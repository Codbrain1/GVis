#pragma once
#include <QMap>
#include <QPointer>
#include <QWidget>

class QFormLayout;
class QMetaProperty;
class QPushButton;
class QColor;

namespace GVis::Visualize::Layers {
class LayerSettings;
}
namespace GVis::UI {
class SettingsEditorWidget : public QWidget {
    Q_OBJECT
  public:
    explicit SettingsEditorWidget(QWidget* parent = nullptr);
    ~SettingsEditorWidget() = default;

    // Назначает текущий объект настроек и полностью перестраивает UI формы
    void setSettings(GVis::Visualize::Layers::LayerSettings* settings);

  private:
    void createRowForProperty(const QMetaProperty& prop);
    void updateColorButton(QPushButton* btn, const QColor& color);
    void updateUiValues();
    void clearEditor();

    QFormLayout*                                     m_layout = nullptr;
    QPointer<GVis::Visualize::Layers::LayerSettings> m_settings;
    QMap<QString, QWidget*>                          m_editors;
};
} // namespace GVis::UI