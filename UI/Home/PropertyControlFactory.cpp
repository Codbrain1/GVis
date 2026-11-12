#include "PropertyControlFactory.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QMetaProperty>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>

namespace GVis::UI {

static void styleColorButton(QPushButton* btn, const QColor& c) {
    btn->setFixedSize(20, 20);
    btn->setStyleSheet(
        QString("background-color:%1; border:1px solid #1a1a1a; border-radius:3px;").arg(c.name()));
    btn->setToolTip(c.name(QColor::HexArgb));
}

QWidget* PropertyControlFactory::createBoolEditor(QObject*             target,
                                                  const QMetaProperty& prop,
                                                  QWidget*             parent,
                                                  const QString&       iconPrefix,
                                                  const QString&       iconLookupKey) {
    bool initial = prop.read(target).toBool();

    if (!iconLookupKey.isEmpty()) {
        QString iconOnPath  = iconPrefix + iconLookupKey + "_on.svg";
        QString iconOffPath = iconPrefix + iconLookupKey + "_off.svg";
        QIcon   iconOn(iconOnPath);
        QIcon   iconOff(iconOffPath);

        // Режим "переключение картинки" — состояние читается по самой иконке,
        // поэтому checked-подсветка отключается локально: иначе поверх смены
        // иконки ещё добавлялась бы рамка/заливка из глобального QSS.
        if (!iconOn.isNull() && !iconOff.isNull()) {
            auto* btn = new QToolButton(parent);
            btn->setCheckable(true);
            btn->setAutoRaise(true);
            btn->setStyleSheet("QToolButton { background: transparent; border: none; } "
                               "QToolButton:checked { background: transparent; }");
            btn->setToolTip(QString::fromLatin1(prop.name()));
            btn->setChecked(initial);
            btn->setIcon(initial ? iconOn : iconOff);

            // Иконки сохраняются как свойства кнопки, а не только в лямбде —
            // так syncEditor() тоже сможет переключать картинку при внешних изменениях.
            btn->setProperty("iconOn", iconOn);
            btn->setProperty("iconOff", iconOff);

            QObject::connect(btn, &QToolButton::toggled, target, [target, prop, btn](bool v) {
                prop.write(target, v);
                QVariant onVar  = btn->property("iconOn");
                QVariant offVar = btn->property("iconOff");
                btn->setIcon(v ? onVar.value<QIcon>() : offVar.value<QIcon>());
            });
            return btn;
        }

        // Фолбэк: одна иконка + подсветка checked (старое поведение).
        QIcon singleIcon(iconPrefix + iconLookupKey + ".svg");
        if (!singleIcon.isNull()) {
            auto* btn = new QToolButton(parent);
            btn->setCheckable(true);
            btn->setIcon(singleIcon);
            btn->setChecked(initial);
            btn->setToolTip(QString::fromLatin1(prop.name()));
            QObject::connect(btn, &QToolButton::toggled, target, [target, prop](bool v) {
                prop.write(target, v);
            });
            return btn;
        }

        qWarning() << "PropertyControlFactory: no icon resource for bool property" << prop.name() << "(tried"
                   << iconOnPath << "/" << iconOffPath << ") — falling back to checkbox";
    }

    // Финальный фолбэк — обычный чекбокс.
    auto* cb = new QCheckBox(parent);
    cb->setChecked(initial);
    QObject::connect(cb, &QCheckBox::toggled, target, [target, prop](bool v) { prop.write(target, v); });
    return cb;
}

QWidget* PropertyControlFactory::createEditor(QObject*             target,
                                              const QMetaProperty& prop,
                                              QWidget*             parent,
                                              const QString&       iconPrefix,
                                              const QString&       iconLookupKey) {
    QVariant value = prop.read(target);
    auto     type  = static_cast<QMetaType::Type>(prop.metaType().id());

    if (prop.isEnumType()) {
        auto*     combo    = new QComboBox(parent);
        QMetaEnum metaEnum = prop.enumerator();
        for (int i = 0; i < metaEnum.keyCount(); ++i)
            combo->addItem(QString::fromLatin1(metaEnum.key(i)), metaEnum.value(i));
        combo->setCurrentIndex(combo->findData(value.toInt()));
        combo->setMinimumWidth(110);
        QObject::connect(
            combo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            target,
            [target, prop, combo](int idx) { prop.write(target, combo->itemData(idx).toInt()); });
        return combo;
    }

    switch (type) {
        case QMetaType::Bool:
            return createBoolEditor(target, prop, parent, iconPrefix, iconLookupKey);

        case QMetaType::Float:
        case QMetaType::Double: {
            auto* sb = new QDoubleSpinBox(parent);
            sb->setRange(-1e6, 1e6);
            sb->setDecimals(3);
            sb->setSingleStep(0.1);
            sb->setMinimumWidth(70);
            sb->setValue(value.toDouble());
            QObject::connect(sb, &QDoubleSpinBox::valueChanged, target, [target, prop](double v) {
                prop.write(target, v);
            });
            return sb;
        }
        case QMetaType::Int: {
            auto* sb = new QSpinBox(parent);
            sb->setRange(-1000000, 1000000);
            sb->setMinimumWidth(60);
            sb->setValue(value.toInt());
            QObject::connect(sb, QOverload<int>::of(&QSpinBox::valueChanged), target, [target, prop](int v) {
                prop.write(target, v);
            });
            return sb;
        }
        case QMetaType::QColor: {
            auto* btn = new QPushButton(parent);
            styleColorButton(btn, value.value<QColor>());
            QObject::connect(btn, &QPushButton::clicked, target, [target, prop, parent, btn]() {
                QColorDialog dialog(parent);
                dialog.setWindowTitle(QObject::tr("Выберите цвет"));
                dialog.setCurrentColor(prop.read(target).value<QColor>());
                dialog.setOption(QColorDialog::DontUseNativeDialog);
                if (dialog.exec() == QDialog::Accepted) {
                    QColor chosen = dialog.selectedColor();
                    if (chosen.isValid()) {
                        prop.write(target, chosen);
                        styleColorButton(btn, chosen);
                    }
                }
            });
            return btn;
        }
        default:
            return nullptr; // QString и прочее — намеренно не показываем в компактных панелях
    }
}

void PropertyControlFactory::syncEditor(QObject* target, const QMetaProperty& prop, QWidget* editor) {
    QVariant       value = prop.read(target);
    QSignalBlocker block(editor);

    if (auto* cb = qobject_cast<QCheckBox*>(editor))
        cb->setChecked(value.toBool());
    else if (auto* sb = qobject_cast<QDoubleSpinBox*>(editor))
        sb->setValue(value.toDouble());
    else if (auto* sb = qobject_cast<QSpinBox*>(editor))
        sb->setValue(value.toInt());
    else if (auto* combo = qobject_cast<QComboBox*>(editor))
        combo->setCurrentIndex(combo->findData(value.toInt()));
    else if (auto* btn = qobject_cast<QPushButton*>(editor))
        styleColorButton(btn, value.value<QColor>());
    else if (auto* btn = qobject_cast<QToolButton*>(editor)) {
        bool v = value.toBool();
        btn->setChecked(v);
        QVariant onVar  = btn->property("iconOn");
        QVariant offVar = btn->property("iconOff");
        if (onVar.isValid() && offVar.isValid())
            btn->setIcon(v ? onVar.value<QIcon>() : offVar.value<QIcon>());
    }
}

} // namespace GVis::UI