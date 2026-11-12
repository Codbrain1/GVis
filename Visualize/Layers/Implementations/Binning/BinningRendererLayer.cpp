#include "BinningRendererLayer.h"
#include "Visualize/ColorMapManager/ColorMapTexture.h"
#include "Visualize/Layers/vtkAdapter.h"
#include <algorithm>
#include <cmath>

namespace GVis::Visualize::Layers {

void BinningRendererLayer::buildShaders() {
    static const char* binVs = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in float aScalar;
        uniform mat4 uMVP;
        uniform bool uShowOutRangeParticles;
        uniform float uRangeMin;
        uniform float uRangeMax;
        out float vScalar;
        void main() {
            bool inRange = aScalar >= uRangeMin && aScalar <= uRangeMax;
            if (!uShowOutRangeParticles && !inRange) {
                gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
                gl_PointSize = 0.0;
            } else {
                gl_Position = uMVP * vec4(aPos, 1.0);
                gl_PointSize = 1.0;
            }
            vScalar = aScalar;
        }
    )";
    static const char* binFs = R"(
        #version 330 core
        in float vScalar;
        out vec4 FragColor;
        void main() {
            FragColor = vec4(vScalar, 1.0, 0.0, 0.0);
        }
    )";
    m_binProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, binVs);
    m_binProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, binFs);
    if (!m_binProgram.link())
        qWarning().noquote() << "Binning shader error:" << m_binProgram.log();

    static const char* resolveVs = R"(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        out vec2 vUV;
        void main() { vUV = aPos * 0.5 + 0.5; gl_Position = vec4(aPos, 0.0, 1.0); }
    )";
    static const char* resolveFs = R"(
        #version 330 core
        in vec2 vUV;
        out vec4 FragColor;
        uniform sampler2D uAccumTex;
        uniform sampler1D uColorMap;
        uniform float uRangeMin;
        uniform float uRangeMax;
        uniform bool  uUseLogScale;
        uniform float uOpacity;
        uniform float uFadeDistance;
        uniform bool  uFadeByLength;
        void main() {
            vec4 acc = texture(uAccumTex, vUV);
            float count = acc.g;
            if (count < 0.5) discard; // Ячейка пуста

            float avgValue = acc.r / count;
            float rMin = uUseLogScale ? log(max(abs(uRangeMin), 1e-30)) / log(10.0) : uRangeMin;
            float rMax = uUseLogScale ? log(max(abs(uRangeMax), 1e-30)) / log(10.0) : uRangeMax;
            float value = uUseLogScale ? log(max(abs(avgValue), 1e-30)) / log(10.0) : avgValue;
            
            float t = clamp((value - rMin) / max(1e-6, rMax - rMin), 0.0, 1.0);
            float alpha = uOpacity;
            if (uFadeByLength) {
                float distFromCenter = length(vUV - vec2(0.5)) * 2.0;
                alpha *= clamp(1.0 - distFromCenter / max(1e-6, uFadeDistance), 0.0, 1.0);
            }

            FragColor = vec4(texture(uColorMap, t).rgb, alpha);
        }
    )";
    m_resolveProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, resolveVs);
    m_resolveProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, resolveFs);
    if (!m_resolveProgram.link())
        qWarning().noquote() << "Binning resolve shader error:" << m_resolveProgram.log();

    static const char* gridVs = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        uniform mat4 uMVP;
        void main() { gl_Position = uMVP * vec4(aPos, 1.0); }
    )";
    static const char* gridFs = R"(
        #version 330 core
        uniform vec4 uColor;
        out vec4 FragColor;
        void main() { FragColor = uColor; }
    )";
    m_gridOverlayProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, gridVs);
    m_gridOverlayProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, gridFs);
    if (!m_gridOverlayProgram.link())
        qWarning().noquote() << "Binning grid overlay shader error:" << m_gridOverlayProgram.log();
}

void BinningRendererLayer::initializeGL(QOpenGLFunctions_3_3_Core* gl) {
    buildShaders();

    static const float quad[] = {-1, -1, 1, -1, -1, 1, -1, 1, 1, -1, 1, 1};
    m_vaoQuad.create();
    m_vaoQuad.bind();
    m_vboQuad.create();
    m_vboQuad.bind();
    m_vboQuad.allocate(quad, sizeof(quad));
    gl->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    gl->glEnableVertexAttribArray(0);
    m_vaoQuad.release();

    m_vaoParticles.create();
    m_vaoParticles.bind();
    m_vboPos.create();
    m_vboScalar.create();
    m_vaoParticles.release();

    m_vaoGridLines.create();
    m_vaoGridLines.bind();
    m_vboGridLines.create();
    m_vaoGridLines.release();
}

void BinningRendererLayer::uploadBuffersIfDirty(QOpenGLFunctions_3_3_Core* gl) {
    if (!m_dirty)
        return;
    auto node = m_dataNode.lock();
    if (!node)
        return;

    QVector<QVector3D> positions = Visualize::vtkAdapter::extractPositions(node);

    QVector<float> scalars;
    if (m_settings && !m_settings->colorByField().isEmpty()) {
        scalars = Visualize::vtkAdapter::extractScalarField(node, m_settings->colorByField());
    }

    m_particleCount = positions.size();
    m_settings->setParticleCount(m_particleCount);
    if (m_particleCount == 0) {
        m_dirty     = false;
        m_hasBounds = false;
        return;
    }

    m_boundsMin = m_boundsMax = positions[0];
    for (const auto& p : positions) {
        m_boundsMin.setX(std::min(m_boundsMin.x(), p.x()));
        m_boundsMin.setY(std::min(m_boundsMin.y(), p.y()));
        m_boundsMin.setZ(std::min(m_boundsMin.z(), p.z()));
        m_boundsMax.setX(std::max(m_boundsMax.x(), p.x()));
        m_boundsMax.setY(std::max(m_boundsMax.y(), p.y()));
        m_boundsMax.setZ(std::max(m_boundsMax.z(), p.z()));
    }
    m_hasBounds = true;

    m_vaoParticles.bind();
    m_vboPos.bind();
    m_vboPos.allocate(positions.constData(), positions.size() * int(sizeof(QVector3D)));
    gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(QVector3D), nullptr);
    gl->glEnableVertexAttribArray(0);

    m_vboScalar.bind();
    if (!scalars.isEmpty()) {
        m_vboScalar.allocate(scalars.constData(), scalars.size() * int(sizeof(float)));
    } else {
        QVector<float> ones(positions.size(), 1.0f);
        m_vboScalar.allocate(ones.constData(), ones.size() * int(sizeof(float)));
    }
    gl->glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(float), nullptr);
    gl->glEnableVertexAttribArray(1);
    m_vaoParticles.release();

    m_dirty              = false;
    m_cachedGridCellSize = -1.0;
}

void BinningRendererLayer::ensureAccumFBO(const QSize& size, QOpenGLFunctions_3_3_Core* gl) {
    if (m_accumFBO && m_accumFBO->size() == size)
        return;
    QOpenGLFramebufferObjectFormat fmt;
    fmt.setInternalTextureFormat(GL_RGBA32F);
    fmt.setAttachment(QOpenGLFramebufferObject::NoAttachment);
    m_accumFBO.reset(new QOpenGLFramebufferObject(size, fmt));

    // Важно: отключаем интерполяцию для четких границ ячеек биннинга
    gl->glBindTexture(GL_TEXTURE_2D, m_accumFBO->texture());
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl->glBindTexture(GL_TEXTURE_2D, 0);
}

QVector3D BinningRendererLayer::gridCenter() const {
    return (m_boundsMin + m_boundsMax) * 0.5f;
}

void BinningRendererLayer::rebuildGridOverlayIfNeeded(QOpenGLFunctions_3_3_Core* gl) {
    if (!m_settings || !m_hasBounds)
        return;

    const double cellSize = m_settings->cellSize();
    if (qFuzzyCompare(cellSize, m_cachedGridCellSize))
        return;

    QVector<float> verts;
    const float    minX = m_boundsMin.x(), maxX = m_boundsMax.x();
    const float    minZ = m_boundsMin.z(), maxZ = m_boundsMax.z();
    const float    y = (m_boundsMin.y() + m_boundsMax.y()) * 0.5f;

    const float cs = float(std::max(cellSize, 1e-6));
    for (float x = minX; x <= maxX + cs * 0.5f; x += cs) {
        verts << x << y << minZ;
        verts << x << y << maxZ;
    }
    for (float z = minZ; z <= maxZ + cs * 0.5f; z += cs) {
        verts << minX << y << z;
        verts << maxX << y << z;
    }

    m_gridLineVertexCount = verts.size() / 3;

    m_vaoGridLines.bind();
    m_vboGridLines.bind();
    m_vboGridLines.allocate(verts.constData(), verts.size() * int(sizeof(float)));
    gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    gl->glEnableVertexAttribArray(0);
    m_vaoGridLines.release();

    m_cachedGridCellSize = cellSize;
}

void BinningRendererLayer::render(QOpenGLFunctions_3_3_Core*      gl,
                                  const Visualize::RenderContext& ctx) {
    if (!m_settings)
        return;
    uploadBuffersIfDirty(gl);
    if (m_particleCount == 0)
        return;

    int   cSize = std::max(1, static_cast<int>(m_settings->cellSize()));
    QSize gridPx(std::max(1, ctx.viewportPx.width() / cSize),
                 std::max(1, ctx.viewportPx.height() / cSize));

    ensureAccumFBO(gridPx, gl);

    // ========================================================
    // Этап 1: Биннинг частиц в уменьшенную FBO-сетку
    // ========================================================
    m_accumFBO->bind();
    gl->glViewport(0, 0, m_accumFBO->width(), m_accumFBO->height());
    gl->glClearColor(0, 0, 0, 0);
    gl->glClear(GL_COLOR_BUFFER_BIT);
    gl->glDisable(GL_DEPTH_TEST);
    gl->glEnable(GL_BLEND);
    gl->glBlendFunc(GL_ONE, GL_ONE);

    m_binProgram.bind();
    m_binProgram.setUniformValue("uMVP", ctx.mvp);
    m_binProgram.setUniformValue("uRangeMin", float(m_settings->rangeMin()));
    m_binProgram.setUniformValue("uRangeMax", float(m_settings->rangeMax()));
    m_binProgram.setUniformValue("uShowOutRangeParticles", m_settings->showOutOfRangeParticles());

    const int firstParticle = m_settings->firstVisibleParticle(m_particleCount);
    const int particleCount = m_settings->visibleParticleCount(m_particleCount);
    m_vaoParticles.bind();
    gl->glDrawArrays(GL_POINTS, firstParticle, particleCount);
    m_vaoParticles.release();
    m_binProgram.release();

    // ========================================================
    // Этап 1.5: Расчет диапазона скаляра (Readback из FBO)
    // ========================================================
    int                fboW = m_accumFBO->width();
    int                fboH = m_accumFBO->height();
    std::vector<float> accumPixels(fboW * fboH * 4); // FBO имеет формат GL_RGBA32F

    // Читаем данные пока FBO еще привязан
    gl->glReadPixels(0, 0, fboW, fboH, GL_RGBA, GL_FLOAT, accumPixels.data());
    m_accumFBO->release(); // Теперь можно отвязать

    float calcMin       = std::numeric_limits<float>::max();
    float calcMax       = std::numeric_limits<float>::lowest();
    bool  hasValidCells = false;

    for (size_t i = 0; i < accumPixels.size(); i += 4) {
        float sum   = accumPixels[i];     // acc.r (сумма скаляров)
        float count = accumPixels[i + 1]; // acc.g (количество частиц)

        if (count >= 0.5f) { // Ячейка не пуста
            // ВАЖНО: Эта формула должна в точности зеркалить логику resolveFs!
            float normalizedValue = sum / count;

            // Если вы перейдете на поверхностную плотность, логика будет такой:
            // float normalizedValue = sum / (cSize * cSize);

            calcMin = std::min(calcMin, normalizedValue);
            calcMax = std::max(calcMax, normalizedValue);
            // m_settings->setRangeMin(calcMin);
            // m_settings->setRangeMax(calcMax);
            hasValidCells = true;
        }
    }

    if (!hasValidCells) {
        calcMin = 0.0f;
        calcMax = 1.0f; // Заглушка, если все частицы вне экрана
    }

    // ========================================================
    // Этап 2: Рендер результата на полный экран
    // ========================================================
    gl->glViewport(0, 0, ctx.viewportPx.width(), ctx.viewportPx.height());
    gl->glEnable(GL_BLEND);
    gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    gl->glActiveTexture(GL_TEXTURE0);
    gl->glBindTexture(GL_TEXTURE_2D, m_accumFBO->texture());
    gl->glActiveTexture(GL_TEXTURE1);
    gl->glBindTexture(GL_TEXTURE_1D, ColorMapTexture::getOrCreate(gl, m_settings->colorMapId()));

    m_resolveProgram.bind();
    m_resolveProgram.setUniformValue("uAccumTex", 0);
    m_resolveProgram.setUniformValue("uColorMap", 1);

    // Передаем динамически рассчитанные значения напрямую!
    m_resolveProgram.setUniformValue("uRangeMin", calcMin);
    m_resolveProgram.setUniformValue("uRangeMax", calcMax);

    m_resolveProgram.setUniformValue("uUseLogScale", m_settings->useLogScale());
    m_resolveProgram.setUniformValue("uOpacity", float(m_settings->opacity()));
    m_resolveProgram.setUniformValue("uFadeByLength", m_settings->fadeByLength());
    m_resolveProgram.setUniformValue("uFadeDistance", m_settings->maxDistance());

    m_vaoQuad.bind();
    gl->glDrawArrays(GL_TRIANGLES, 0, 6);
    m_vaoQuad.release();
    m_resolveProgram.release();

    // Этап 3: Оверлей мировой сетки (опционально)
    if (m_settings->lineWidth() > 0.0f) {
        rebuildGridOverlayIfNeeded(gl);
        if (m_gridLineVertexCount > 0) {
            gl->glLineWidth(m_settings->lineWidth());
            m_gridOverlayProgram.bind();
            m_gridOverlayProgram.setUniformValue("uMVP", ctx.mvp);
            const QColor c = m_settings->lineColor();
            m_gridOverlayProgram.setUniformValue(
                "uColor",
                QVector4D(c.redF(), c.greenF(), c.blueF(), c.alphaF()));

            m_vaoGridLines.bind();
            gl->glDrawArrays(GL_LINES, 0, m_gridLineVertexCount);
            m_vaoGridLines.release();
            m_gridOverlayProgram.release();
            gl->glLineWidth(1.0f);
        }
    }

    gl->glDisable(GL_BLEND);
}

void BinningRendererLayer::releaseGL(QOpenGLFunctions_3_3_Core*) {
    m_vboPos.destroy();
    m_vboScalar.destroy();
    m_vaoParticles.destroy();
    m_vboQuad.destroy();
    m_vaoQuad.destroy();
    m_vboGridLines.destroy();
    m_vaoGridLines.destroy();
    m_accumFBO.reset();
}

bool BinningRendererLayer::boundingBox(QVector3D& outMin, QVector3D& outMax) const {
    if (!m_hasBounds)
        return false;
    outMin = m_boundsMin;
    outMax = m_boundsMax;
    return true;
}

} // namespace GVis::Visualize::Layers