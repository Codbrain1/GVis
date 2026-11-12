#pragma once
#include "Common/Structures/RenderContext.h"
#include <QObject>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QString>
#include <QVector3D>
#include <QVector>
#include "../View3DSettings.h"

namespace GVis::Visualize::Views::View3D {

// Позиция и текст деления оси в мировых координатах — используется GLViewport,
// чтобы спроецировать точку в 2D и нарисовать подпись через QPainter поверх OpenGL-кадра.
struct AxisTick {
    QVector3D worldPos;
    QString   text;
    QColor    color;
};

// Рисует три цветные линии осей (X/Y/Z) через GL_LINES и предоставляет список
// делений с мировыми координатами для последующей отрисовки подписей QPainter'ом —
// сам AxisRenderer текст не рисует, т.к. это требует QPainter, а не GL-контекста.
class AxisRenderer : public QObject {
    Q_OBJECT
  public:
    explicit AxisRenderer(QObject* parent = nullptr);
    ~AxisRenderer() override = default;

    void initializeGL(QOpenGLFunctions_3_3_Core* gl);
    void releaseGL(QOpenGLFunctions_3_3_Core* gl);

    // extent — длина осей в мировых единицах (обычно берётся из GridSettings::extent()
    // или из bounding box данных, который вычисляет GLViewport)
    void render(QOpenGLFunctions_3_3_Core*      gl,
                const Visualize::RenderContext& ctx,
                AxisSettings*                   settings,
                float                           extent);

    // вызывается GLViewport при построении подписей делений для QPainter-оверлея
    QVector<AxisTick> buildTicks(AxisSettings* settings, float extent) const;

  private:
    void rebuildGeometry(QOpenGLFunctions_3_3_Core* gl, float extent, AxisSettings* settings);
    void buildShader();

    QOpenGLShaderProgram     m_program;
    QOpenGLBuffer            m_vbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject m_vao;
    int                      m_vertexCount     = 0;
    bool                     m_glInitialized   = false;
    float                    m_cachedExtent    = -1.0f;
    int                      m_cachedTickCount = -1;
    QColor                   m_cachedColorX, m_cachedColorY, m_cachedColorZ;
};

} // namespace GVis::Visualize::Views::View3D