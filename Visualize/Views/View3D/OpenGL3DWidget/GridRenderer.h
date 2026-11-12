#pragma once
#include "Common/Structures/RenderContext.h"
#include <QObject>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include "../View3DSettings.h"

namespace GVis::Visualize::Views::View3D {

// Рисует плоскую сетку (GL_LINES) на плоскости XZ. Геометрия перестраивается лениво —
// только когда extent/lineCount реально изменился, а не каждый кадр.
class GridRenderer : public QObject {
    Q_OBJECT
  public:
    explicit GridRenderer(QObject* parent = nullptr);
    ~GridRenderer() override = default;

    void initializeGL(QOpenGLFunctions_3_3_Core* gl);
    void releaseGL(QOpenGLFunctions_3_3_Core* gl);

    void render(QOpenGLFunctions_3_3_Core*      gl,
                const Visualize::RenderContext& ctx,
                GridSettings*                   settings);

  private:
    void rebuildGeometry(QOpenGLFunctions_3_3_Core* gl, GridSettings* settings);
    void buildShader();

    QOpenGLShaderProgram     m_program;
    QOpenGLBuffer            m_vbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject m_vao;
    int                      m_vertexCount   = 0;
    bool                     m_glInitialized = false;

    // кэш параметров, под которые построена текущая геометрия —
    // чтобы не пересобирать буфер каждый кадр без необходимости
    float m_cachedExtent    = -1.0f;
    int   m_cachedLineCount = -1;
};

} // namespace GVis::Visualize::Views::View3D