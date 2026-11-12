#pragma once
#include "ColorMapComboBox.h"
#include "Core/AppCore/AppCore.h"
#include "Models/DataTreeModel/DataTreeModel.h"
#include <QMainWindow>
#include <QObject>
#include <QPushButton>
#include <QVTKOpenGLNativeWidget.h>
#include <QWidget>
#include <qcontainerfwd.h>
#include <qdockwidget.h>
#include <qfiledialog.h>
#include <qfileinfo.h>
#include <qmainwindow.h>
#include <qobject.h>
#include <qpushbutton.h>
#include <qtmetamacros.h>
#include <vtkDataSetAttributes.h>
#include <vtkType.h>

QT_BEGIN_NAMESPACE
namespace Ui {
class PropertyInspector;

}
QT_END_NAMESPACE

namespace GVis::UI {
class SettingsEditorWidget;
class PropertyInspector : public QWidget {
    Q_OBJECT
  public:
    explicit PropertyInspector(GVis::Core::AppCore* app, QWidget* parent = nullptr);
    ~PropertyInspector();
    void setCurrentElement(const QUuid& id, Models::DataTreeItem::Type type);
  private slots:
    // Слот для комбобокса палитр
    void handlePaletteChange(int index);
    void handleCurrentVisualizeColumnChange(int index);

    // Слот для прозрачности (например, если есть слайдер)
    void handleCreateCustomPalete();
    void handleEditPalete();
    void handleColorMapAdded(const Visualize::ColorMap& map);
    // void handleColorCorrectionChange(double val);

  private:
    Ui::PropertyInspector* ui;
    QUuid                  m_currentNodeId;
    QUuid                  m_currentElementId;
    GVis::Core::AppCore*   m_app;
    SettingsEditorWidget*  m_dynamicEditor = nullptr;
    void                   setupUiLogic();  // Начальное заполнение комбобоксов
    void                   updateWidgets(); // Синхронизация UI с данными ноды
};
} // namespace GVis::UI