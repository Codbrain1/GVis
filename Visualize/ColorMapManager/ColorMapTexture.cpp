#include "ColorMapTexture.h"
#include "Common/Structures/ColormapPresets.h"
#include <QOpenGLFunctions>
#include <QVector>
#include "ColorMapManager.h"
#include <algorithm>

namespace GVis::Visualize::Layers {

QMap<QOpenGLContext*, ColorMapTexture::ContextCache> ColorMapTexture::s_caches;

QOpenGLContext* ColorMapTexture::cacheKeyFor(QOpenGLContext* ctx) {
    // если контексты расшарены (AA_ShareOpenGLContexts=true), у всех есть
    // общий shareContext() — используем его как единый ключ кэша.
    // Если sharing не установлен (fallback/отладка), каждый контекст кэшируется отдельно.
    return ctx->shareContext() ? ctx->shareContext() : ctx;
}

GLuint ColorMapTexture::getOrCreate(QOpenGLFunctions_3_3_Core* gl, const QUuid& colorMapId) {
    QOpenGLContext* current = QOpenGLContext::currentContext();
    if (!current) {
        qWarning() << "ColorMapTexture::getOrCreate called without a current GL context";
        return 0;
    }
    QOpenGLContext* key = cacheKeyFor(current);

    auto cacheIt = s_caches.find(key);
    if (cacheIt == s_caches.end()) {
        // подписываемся на уничтожение контекста, чтобы вычистить кэш и не течь GPU-памятью
        QObject::connect(key, &QOpenGLContext::aboutToBeDestroyed, key, [key]() {
            releaseForContext(key);
        });
        cacheIt = s_caches.insert(key, ContextCache{});
    }

    auto texIt = cacheIt->textures.find(colorMapId);
    if (texIt != cacheIt->textures.end())
        return texIt.value();

    const auto mapOpt = Visualize::ColorMapManager::instance().getMap(colorMapId);
    if (!mapOpt.has_value()) {
        qWarning() << "ColorMapTexture: colormap not found for id" << colorMapId
                   << "— falling back to Plasma preset";
        return getOrCreate(gl, Visualize::ColorMapPresets::getPresetByName("Plasma").id);
    }

    const Visualize::ColorMap& map = mapOpt.value();

    QVector<Visualize::ColorPoint> sortedPoints = map.points;
    std::sort(
        sortedPoints.begin(),
        sortedPoints.end(),
        [](const Visualize::ColorPoint& a, const Visualize::ColorPoint& b) { return a.x < b.x; });

    if (sortedPoints.isEmpty()) {
        qWarning() << "ColorMapTexture: colormap" << map.name << "has no points";
        return 0;
    }

    constexpr int  kResolution = 256;
    QVector<float> pixels(kResolution * 4);

    for (int i = 0; i < kResolution; ++i) {
        const double t = double(i) / double(kResolution - 1);

        int hi = 0;
        while (hi < sortedPoints.size() - 1 && sortedPoints[hi].x < t)
            ++hi;
        const int lo = std::max(0, hi - 1);

        double r, g, b;
        if (sortedPoints[hi].x <= sortedPoints[lo].x + 1e-12) {
            r = sortedPoints[hi].r;
            g = sortedPoints[hi].g;
            b = sortedPoints[hi].b;
        } else {
            const double frac =
                std::clamp((t - sortedPoints[lo].x) / (sortedPoints[hi].x - sortedPoints[lo].x),
                           0.0,
                           1.0);
            r = sortedPoints[lo].r + (sortedPoints[hi].r - sortedPoints[lo].r) * frac;
            g = sortedPoints[lo].g + (sortedPoints[hi].g - sortedPoints[lo].g) * frac;
            b = sortedPoints[lo].b + (sortedPoints[hi].b - sortedPoints[lo].b) * frac;
        }

        pixels[i * 4 + 0] = float(r);
        pixels[i * 4 + 1] = float(g);
        pixels[i * 4 + 2] = float(b);
        pixels[i * 4 + 3] = 1.0f;
    }

    GLuint tex = 0;
    gl->glGenTextures(1, &tex);
    gl->glBindTexture(GL_TEXTURE_1D, tex);
    gl->glTexImage1D(GL_TEXTURE_1D,
                     0,
                     GL_RGBA32F,
                     kResolution,
                     0,
                     GL_RGBA,
                     GL_FLOAT,
                     pixels.constData());
    gl->glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->glBindTexture(GL_TEXTURE_1D, 0);

    cacheIt->textures.insert(colorMapId, tex);
    return tex;
}

void ColorMapTexture::invalidate(const QUuid& colorMapId) {
    // сбрасываем во ВСЕХ context-кэшах сразу, т.к. пользователь мог отредактировать
    // палитру, которая используется в нескольких окнах одновременно
    for (auto& cache : s_caches) {
        auto it = cache.textures.find(colorMapId);
        if (it != cache.textures.end()) {
            // ВАЖНО: удаление текстуры требует активного контекста, к которому она
            // принадлежит. Здесь предполагается вызов из слота, где нужный контекст
            // уже сделан текущим (см. примечание ниже по интеграции).
            GLuint tex = it.value();
            if (auto* gl = QOpenGLContext::currentContext()
                               ? QOpenGLContext::currentContext()->functions()
                               : nullptr) {
                gl->glDeleteTextures(1, &tex);
            }
            cache.textures.erase(it);
        }
    }
}

void ColorMapTexture::releaseForContext(QOpenGLContext* ctx) {
    auto it = s_caches.find(ctx);
    if (it == s_caches.end())
        return;

    if (ctx->makeCurrent(nullptr)) { /* контекст может быть уже недоступен для surface */
    }

    auto* gl = ctx->functions();
    for (GLuint tex : it->textures)
        gl->glDeleteTextures(1, &tex);

    s_caches.erase(it);
}

} // namespace GVis::Visualize::Layers