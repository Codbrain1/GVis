#pragma once
#include "AutoSettingsToolBar.h"
#include <QWidget>

namespace GVis::UI {

class ViewContainerWidget : public QWidget {
    Q_OBJECT
  public:
    // viewWidget — уже существующий GL-виджет (или любой другой) конкретного view.
    // settingsObject — QObject с Q_PROPERTY для автогенерации тулбара; может быть nullptr,
    // если у типа view нет настроек, отображаемых в шапке (тогда шапка скрывается).
    ViewContainerWidget(QWidget* viewWidget, QObject* settingsObject, QWidget* parent = nullptr);

    AutoSettingsToolBar* header() const {
        return m_header;
    }
    QWidget* viewWidget() const {
        return m_viewWidget;
    }

    // Позволяет переподключить настройки постфактум (например, если view
    // подгружает свой settings-объект асинхронно, уже после создания контейнера).
    void setSettingsObject(QObject* settingsObject);

  private:
    AutoSettingsToolBar* m_header     = nullptr;
    QWidget*             m_viewWidget = nullptr;
};

} // namespace GVis::UI