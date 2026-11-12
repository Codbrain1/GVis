#pragma once
#include "Common/Interfaces/IRenderLayer.h"
#include "Common/Structures/RenderContext.h"
#include <QMap>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLWidget>
#include <QSet>
#include <QUuid>
#include "../View3DSettings.h"
#include "AxisRenderer.h"
#include "Enums/ViewEnums.h"
#include "GridRenderer.h"
#include <memory>

namespace GVis::Visualize::Views::View3D {

class GLViewport : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT
  public:
    explicit GLViewport(View3DSettings* settings, QWidget* parent = nullptr);
    ~GLViewport() override;

    void attachRenderLayer(const QUuid&                                   layerId,
                           std::shared_ptr<Visualize::IOpenGLRenderLayer> layer);
    void detachRenderLayer(const QUuid& layerId);

    void setBackgroundColor(double r, double g, double b);
    void resetCamera();
    void setCameraPreset(View3D::CameraViewType presetIndex);
    void forceFullRedraw();

  signals:
    // испускается когда любой видимый слой готов сообщить актуальный диапазон/палитру —
    // внешний colorbar (QCustomPlotView2D) подписывается на это, чтобы синхронизировать
    // QCPColorScale::setDataRange()/градиент без прямой зависимости от GLViewport.
    // ПРЕДПОЛОЖЕНИЕ: сигнатура минимальна (id слоя); сам диапазон/colorMapId colorbar
    // читает через LayerSettings, на который у него уже есть shared_ptr от LayerManager.
    void layerVisualsChanged(const QUuid& layerId);

  protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void paintEvent(QPaintEvent* event) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

  private:
    Visualize::RenderContext buildRenderContext();
    void                     fitCameraToLayers();
    void                     processPendingColorMapInvalidations();

    float normalizeAngle(float angle) {
        while (angle > M_PI)
            angle -= 2.0f * M_PI;
        while (angle < -M_PI)
            angle += 2.0f * M_PI;
        return angle;
    }

    View3DSettings* m_settings; // не владеем

    QMap<QUuid, std::shared_ptr<Visualize::IOpenGLRenderLayer>> m_layers;

    GridRenderer m_gridRenderer;
    AxisRenderer m_axisRenderer;

    Visualize::RenderContext m_lastContext;
    float                    m_axisExtent = 10.0f;

    QVector3D m_center{0, 0, 0};
    float     m_distance = 10.0f; // радиус орбиты (дистанция до объекта).
    float     m_yaw      = 0.0f;  // поворот мыши по горизонтали
    float     m_pitch    = 0.0f;  // наклон мыши по вертикали
    float     m_roll     = 0.0f;
    QPoint    m_lastMousePos; // последняя позиция мыши
    bool      m_dragging = false;

    QColor m_backgroundColor{13, 13, 20};
    bool   m_needsCameraFit = true; // триггер для автоматического наведения камеры на объект

    // ---- отложенная инвалидация текстур палитр ----
    // GL-контекст не гарантированно активен в слоте, вызванном сигналом
    // ColorMapManager::paleteAdded (тот эмитится из произвольного места UI),
    // поэтому сами ID палитр только накапливаем здесь, а реальный
    // ColorMapTexture::invalidate() вызывается внутри paintGL(), где контекст точно активен.
    QSet<QUuid> m_pendingColorMapInvalidations;
};

} // namespace GVis::Visualize::Views::View3D