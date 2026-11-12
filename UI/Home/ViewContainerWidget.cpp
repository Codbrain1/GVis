#include "ViewContainerWidget.h"
#include "AutoSettingsToolBar.h"
#include <QVBoxLayout>
#include <QWidget>

namespace GVis::UI {

ViewContainerWidget::ViewContainerWidget(QWidget* viewWidget, QObject* settingsObject, QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_header = new AutoSettingsToolBar(this);
    layout->addWidget(m_header);

    // Репарентим существующий виджет view внутрь контейнера.
    // Важно: viewWidget может быть nullptr только если вызывающий код уже это проверил —
    // здесь мы намеренно НЕ делаем повторную проверку, чтобы не маскировать баг молча.
    viewWidget->setParent(this);
    layout->addWidget(viewWidget, 1);

    m_viewWidget = viewWidget;

    setSettingsObject(settingsObject);
}
// Позволяет переподключить настройки постфактум (например, если view
// подгружает свой settings-объект асинхронно, уже после создания контейнера).
void ViewContainerWidget::setSettingsObject(QObject* settingsObject) {
    m_header->setSettings(settingsObject);
    // Если у view вообще нет отображаемых настроек — не занимаем место пустой полосой.
    m_header->setVisible(settingsObject != nullptr);
}

} // namespace GVis::UI