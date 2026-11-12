#pragma once
#include "Core/AppCore/AppCore.h"
#include <QWidget>
QT_BEGIN_NAMESPACE
namespace Ui {
class TimeLineWidget;
}
QT_END_NAMESPACE

namespace GVis::UI {
class TimeLineWidget : public QWidget {
    Q_OBJECT
  public:
    explicit TimeLineWidget(Core::AppCore* app, QWidget* parent = nullptr);
    ~TimeLineWidget();

  signals:
    void currentTimeStampValueChanged(int valueTimestamp);
    void editLayerProperty(const QUuid& layerId);
  private slots:
    void handleToolButtonPrev_clicked();
    void handleToolButtonNext_clicked();
    void handleToolButtonEditLayerSettings();
    void handleTimeSliderChangeValue(int value);

    void handleSnapshotsListChangeSize(const int size);
    void handleChangeEtalonSnapshot(const QList<QUuid>& list);

  private:
    QList<QUuid>        layerList;
    Ui::TimeLineWidget* ui;
    Core::AppCore*      m_app;
    void                setTextTimeLabelTextInternal();
};
} // namespace GVis::UI