#pragma once
#include <QMatrix4x4>
#include <QSize>
#include <QVector3D>

namespace GVis::Visualize {
// нужен всем рендерерам
struct RenderContext {
    QMatrix4x4 viewMatrix; // описывает направление и позицию взгляда и угол обзора
    QMatrix4x4 projMatrix; // Она задает угол обзора (FOV, например, 45°) и соотношение сторон
                           // Qt-виджета
    QMatrix4x4 mvp;        // projMatrix * viewMatrix, посчитан один раз за кадр
    QVector3D  cameraPos;
    QSize      viewportPx;
    float      pixelsPerWorldUnit = 1.0f; // актуально для ортографии
    bool       orthographic =
        true; // переключение между перспективной и ортографической проекциями (параллельная)
};

} // namespace GVis::Visualize