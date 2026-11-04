
#include "ParticleRendererLayer.h"
#include "Common/Structures/RenderContext.h"
#include "Visualize/ColorMapManager/ColorMapTexture.h"
#include "Visualize/Layers/vtkAdapter.h"
#include <algorithm>

namespace QSpace::Visualize::Layers {

void ParticleRendererLayer::buildShader() {
    // TODO перенести шейдеры в отдельные файлы
    static const char* vs = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in float aScalar;
        
        uniform mat4 uMVP;

        // оперделяем общий размер частицы или варируем в диапазоно в зависимости от aScalar
        uniform bool uAutoSizePoint;
        uniform float uPointSize;
        uniform float uMinPointSize;
        uniform float uMaxPointSize;

        // диапазон значений aScalar и показывать ли частицы вне него
        uniform bool uShowOutRangeParticles;
        uniform float uRangeMin;
        uniform float uRangeMax;
        uniform bool uUseLogScale;

        out float vNorm;

       void main() {
        // 1. Фильтрация (с исправленными знаками для допуска)
        float eps = 1e-12;
        bool inRange = (aScalar >= uRangeMin - eps && aScalar <= uRangeMax + eps);

        if (!uShowOutRangeParticles && !inRange) {
            gl_Position = vec4(9.9, 9.9, 9.9, 1.0); 
            gl_PointSize = 0.0;
        } else {
            gl_Position = uMVP * vec4(aPos, 1.0);
        }

        // 2. Расчет нормализованного t с учетом логарифма для границ
        float val = aScalar;
        float rMin = uRangeMin;
        float rMax = uRangeMax;

        if (uUseLogScale) {
            val  = log(max(abs(aScalar), 1e-30)) / log(10.0);
            rMin = log(max(abs(uRangeMin), 1e-30)) / log(10.0);
            rMax = log(max(abs(uRangeMax), 1e-30)) / log(10.0);
        }

        // 3. Безопасный расчет t для сверхмалых чисел
        float rangeDist = rMax - rMin;
        float t = 0.0;
        
        // Используем 1e-20 чтобы не терять разницу сверхмалых масс
        if (rangeDist > 1e-20) { 
            t = clamp((val - rMin) / rangeDist, 0.0, 1.0);
        } else {
            // Если минимум и максимум равны (например, все частицы одной массы),
            // присваиваем им цвет из середины палитры
            t = 0.5; 
        }
        
        gl_PointSize = uAutoSizePoint ? mix(uMinPointSize, uMaxPointSize, t) : uPointSize;
        vNorm = t;
    }
    )";
    static const char* fs = R"(
        #version 330 core
        in float vNorm;
        out vec4 FragColor;
        
        uniform sampler1D uColorMap;
        uniform bool uShadeAsSphere;
        uniform float uOpacity;

        void main() {
            vec2 coord = gl_PointCoord * 2.0 - 1.0;
            float r2 = dot(coord, coord);
            if (r2 > 1.0) discard;
            vec3 color = texture(uColorMap, vNorm).rgb;
            if (uShadeAsSphere) {
                float shade = sqrt(1.0 - r2);
                color *= (0.5 + 0.5 * shade);
            }
            FragColor = vec4(color, uOpacity);
        }
    )";
    m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, vs);
    m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, fs);

    if (!m_program.link()) // собирает шейдеры в единую программу
        qWarning().noquote() << "ParticleRendererLayer shader error:" << m_program.log();
}

void ParticleRendererLayer::initializeGL(QOpenGLFunctions_3_3_Core* gl) {
    Q_UNUSED(gl);
    buildShader();
    m_vao.create();
    m_vao.bind();
    m_vboPosition.create();
    m_vboScalar.create();
    m_vao.release();
}

void ParticleRendererLayer::uploadBuffersIfDirty(QOpenGLFunctions_3_3_Core* gl) {
    if (!m_dirty)
        return;
    auto node = m_dataNode.lock();
    if (!node)
        return;

    QVector<QVector3D> positions = Visualize::vtkAdapter::extractPositions(node);
    QVector<float>     scalars;
    if (m_settings && !m_settings->colorByField().isEmpty()) {
        scalars = Visualize::vtkAdapter::extractScalarField(node, m_settings->colorByField());
    } else {
        scalars = QVector<float>();
    }

    m_particleCount = positions.size();
    m_settings->setParticleCount(m_particleCount);
    if (m_particleCount == 0) {
        qWarning() << "ParticleRendererLayer: DataNode contains no points";
        m_dirty = false;
        return;
    }

    m_vao.bind();
    m_vboPosition.bind();
    m_vboPosition.allocate(positions.constData(), positions.size() * int(sizeof(QVector3D)));
    gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(QVector3D), nullptr);
    gl->glEnableVertexAttribArray(0);

    m_vboScalar.bind();
    if (!scalars.isEmpty()) {
        m_vboScalar.allocate(scalars.constData(), scalars.size() * int(sizeof(float)));
    } else {
        QVector<float> zeros(positions.size(), 0.0f);
        m_vboScalar.allocate(zeros.constData(), zeros.size() * int(sizeof(float)));
    }

    gl->glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(float), nullptr);
    gl->glEnableVertexAttribArray(1);
    m_vao.release();

    m_dirty = false;

    if (!scalars.isEmpty())
        computeScalarBoundsIfNeeded(scalars);
}

void ParticleRendererLayer::computeScalarBoundsIfNeeded(const QVector<float>& scalars) {
    if (!m_settings || !m_settings->autoRange())
        return;

    float max = scalars[0];
    float min = scalars[0];

    for (const auto& val : scalars) {
        max = std::max(max, val);
        min = std::min(min, val);
    }

    m_settings->setBaseRangeMin(min);
    m_settings->setBaseRangeMax(max);

    if (m_settings->autoRange()) {
        m_settings->setRangeMin(min);
        m_settings->setRangeMax(max);
    }
}

void ParticleRendererLayer::render(QOpenGLFunctions_3_3_Core*      gl,
                                   const Visualize::RenderContext& ctx) {
    if (!m_settings)
        return;
    // загружаем данные на видеокарту если были внесены какие либо изменения
    uploadBuffersIfDirty(gl);
    if (m_particleCount == 0)
        return;

    gl->glEnable(GL_BLEND);

    if (static_cast<LayerSettings::BlendMode>(m_settings->blendMode()) ==
        LayerSettings::BlendMode::Additive)
        gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    else
        gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const GLuint colorMapTex = ColorMapTexture::getOrCreate(gl, m_settings->colorMapId());
    gl->glActiveTexture(GL_TEXTURE0);
    gl->glBindTexture(GL_TEXTURE_1D, colorMapTex);

    m_program.bind();
    m_program.setUniformValue("uMVP", ctx.mvp);
    m_program.setUniformValue("uPointSize", m_settings->pointSizePx());
    m_program.setUniformValue("uMinPointSize", m_settings->minPointSizePx());
    m_program.setUniformValue("uMaxPointSize", m_settings->maxPointSizePx());
    m_program.setUniformValue("uAutoSizePoint", m_settings->autoSizePoint());
    m_program.setUniformValue("uRangeMin", float(m_settings->rangeMin()));
    m_program.setUniformValue("uRangeMax", float(m_settings->rangeMax()));
    m_program.setUniformValue("uUseLogScale", m_settings->useLogScale());
    m_program.setUniformValue("uShadeAsSphere", m_settings->shadeAsSphere());
    m_program.setUniformValue("uOpacity", float(m_settings->opacity()));
    m_program.setUniformValue("uAutoSizePoint", m_settings->autoSizePoint());
    m_program.setUniformValue("uShowOutRangeParticles", m_settings->showOutOfRangeParticles());
    m_program.setUniformValue("uColorMap", 0);

    const int firstParticle = m_settings->firstVisibleParticle(m_particleCount);
    const int particleCount = m_settings->visibleParticleCount(m_particleCount);
    m_vao.bind();
    gl->glDrawArrays(GL_POINTS, firstParticle, particleCount);
    m_vao.release();
    m_program.release();

    gl->glBindTexture(GL_TEXTURE_1D, 0);
    gl->glDisable(GL_BLEND);
}

void ParticleRendererLayer::releaseGL(QOpenGLFunctions_3_3_Core*) {
    m_vboPosition.destroy();
    m_vboScalar.destroy();
    m_vao.destroy();
}

bool ParticleRendererLayer::boundingBox(QVector3D&, QVector3D&) const {
    return false;
}

} // namespace QSpace::Visualize::Layers