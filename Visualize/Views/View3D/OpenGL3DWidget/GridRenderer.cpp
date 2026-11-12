#include "GridRenderer.h"
#include <QVector>

namespace GVis::Visualize::Views::View3D {

GridRenderer::GridRenderer(QObject* parent) : QObject(parent) {
}

void GridRenderer::buildShader() {
    static const char* vs = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        uniform mat4 uMVP;
        void main() { gl_Position = uMVP * vec4(aPos, 1.0); }
    )";
    static const char* fs = R"(
        #version 330 core
        uniform vec4 uColor;
        out vec4 FragColor;
        void main() { FragColor = uColor; }
    )";
    m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, vs);
    m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, fs);
    if (!m_program.link())
        qWarning().noquote() << "GridRenderer shader link error:" << m_program.log();
}

void GridRenderer::initializeGL(QOpenGLFunctions_3_3_Core* gl) {
    Q_UNUSED(gl);
    buildShader();
    m_vao.create();
    m_vao.bind();
    m_vbo.create();
    m_vao.release();
    m_glInitialized = true;
}

void GridRenderer::releaseGL(QOpenGLFunctions_3_3_Core* gl) {
    Q_UNUSED(gl);
    m_vbo.destroy();
    m_vao.destroy();
    m_glInitialized = false;
}

void GridRenderer::rebuildGeometry(QOpenGLFunctions_3_3_Core* gl, GridSettings* settings) {
    const float extent = settings->extent();
    const int   lines  = std::max(1, settings->lineCount());

    QVector<float> verts; // 3 floata на вершину (pos), линии парами
    verts.reserve((lines + 1) * 4 * 3);

    const float step = (2.0f * extent) / float(lines);
    for (int i = 0; i <= lines; ++i) {
        const float v = -extent + i * step;
        // линии вдоль X (постоянный Z)
        verts << v << 0.0f << -extent;
        verts << v << 0.0f << extent;
        // линии вдоль Z (постоянный X)
        verts << -extent << 0.0f << v;
        verts << extent << 0.0f << v;
    }

    m_vertexCount = verts.size() / 3;

    m_vao.bind();
    m_vbo.bind();
    m_vbo.allocate(verts.constData(), verts.size() * int(sizeof(float)));
    gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    gl->glEnableVertexAttribArray(0);
    m_vao.release();

    m_cachedExtent    = extent;
    m_cachedLineCount = lines;
}

void GridRenderer::render(QOpenGLFunctions_3_3_Core*      gl,
                          const Visualize::RenderContext& ctx,
                          GridSettings*                   settings) {
    if (!settings->visible() || !m_glInitialized)
        return;

    // пересобираем геометрию только если параметры реально поменялись —
    // не на каждый кадр, чтобы не гонять память впустую при 60 FPS
    if (!qFuzzyCompare(m_cachedExtent, settings->extent()) ||
        m_cachedLineCount != settings->lineCount()) {
        rebuildGeometry(gl, settings);
    }

    m_program.bind();
    m_program.setUniformValue("uMVP", ctx.mvp);
    const QColor c = settings->color();
    m_program.setUniformValue("uColor", QVector4D(c.redF(), c.greenF(), c.blueF(), c.alphaF()));

    m_vao.bind();
    gl->glDrawArrays(GL_LINES, 0, m_vertexCount);
    m_vao.release();
    m_program.release();
}

} // namespace GVis::Visualize::Views::View3D