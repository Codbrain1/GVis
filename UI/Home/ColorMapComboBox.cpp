#include "ColorMapComboBox.h"
#include <QAbstractItemView>
#include <QComboBox>
#include <QFrame>
#include <QMouseEvent>
#include <qabstractitemmodel.h>
#include <qboxlayout.h>
#include <qcombobox.h>
#include <qframe.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qpushbutton.h>
#include <qwidget.h>

namespace GVis::UI {
ColorMapComboBox::ColorMapComboBox(QWidget* parent) : QComboBox(parent) {
    this->view()->setAutoScroll(false); // отключаем автоскролл, чтобы кнопка не уезжала при открытии меню
    this->view()->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    this->view()->viewport()->installEventFilter(this);
}
void ColorMapComboBox::showPopup() {
    QComboBox::showPopup();
    QWidget* popup = this->view()->window(); // получение контейнера выпадающего окна
    if (!m_newButton || !m_editButton) {
        // настраиваем выравнивание на дне виджета
        QBoxLayout* existingLayout = qobject_cast<QBoxLayout*>(popup->layout());
        if (existingLayout) {
            // рисуем линию для отделения кнопки от пунктов меню
            QFrame* line = new QFrame(popup);
            line->setFrameShape(QFrame::HLine);
            line->setFrameShadow(QFrame::Sunken);

            QHBoxLayout* layout = new QHBoxLayout();
            layout->setContentsMargins(5, 5, 5, 5);

            m_newButton = new QPushButton("New", popup); // создаем кнопку
            m_newButton->setFlat(true);                  // убирает рамки
            m_newButton->setContentsMargins(5, 5, 5, 5);

            m_editButton = new QPushButton("Edit", popup);
            m_editButton->setFlat(true); // убирает рамки
            m_editButton->setContentsMargins(5, 5, 5, 5);

            layout->addWidget(m_newButton);
            layout->addWidget(m_editButton);

            existingLayout->addWidget(line);
            existingLayout->addLayout(layout);

            popup->setLayout(existingLayout);
            connect(m_newButton, &QPushButton::clicked, this, [this]() {
                this->hidePopup();
                emit newButtonClicked();
            });
            connect(m_editButton, &QPushButton::clicked, this, [this]() {
                this->hidePopup();
                emit editButtonClicked();
            });
        }
    }
}
bool ColorMapComboBox::eventFilter(QObject* obj, QEvent* event) {
    if (obj == this->view()->viewport()) {
        if (event->type() == QEvent::MouseButtonRelease) {
            auto        mouseEvent = static_cast<QMouseEvent*>(event);
            QModelIndex index      = this->view()->indexAt(mouseEvent->pos());
            if (index.isValid()) {
                // Клик по элементу списка, обрабатываем как обычно
                this->setCurrentIndex(index.row());
                emit activated(index.row());
                return true;
            }
        }
    }
    return QComboBox::eventFilter(obj, event);
}
} // namespace GVis::UI