#pragma once
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <qcombobox.h>
#include <qobject.h>
#include <qpushbutton.h>
#include <qtmetamacros.h>
#include <qwidget.h>

namespace GVis::UI {
class ColorMapComboBox : public QComboBox {
    Q_OBJECT
  public:
    explicit ColorMapComboBox(QWidget* parent = nullptr);
  signals:
    void newButtonClicked();
    void editButtonClicked();

  protected:
    void showPopup() override;
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    QPushButton* m_newButton  = nullptr;
    QPushButton* m_editButton = nullptr;
};
} // namespace GVis::UI