#pragma once
#include <QString>
#include <QWidget>

class QMetaProperty;

namespace GVis::UI {

// Единственное место, создающее редакторы под Q_PROPERTY. И AutoSettingsToolBar,
// и попап-формы групп используют ТОЛЬКО эту фабрику — сама компоновка (тулбар vs
// форма) не должна знать, как устроен конкретный виджет-редактор.
class PropertyControlFactory {
  public:
    // iconPrefix + iconLookupKey определяют поиск иконок для bool-свойств:
    //   "<iconPrefix><iconLookupKey>_on.svg" / "_off.svg" -> переключение картинки
    //   "<iconPrefix><iconLookupKey>.svg"                  -> одна иконка + подсветка checked
    //   ничего не найдено                                   -> обычный QCheckBox
    // Для остальных типов iconLookupKey/iconPrefix игнорируются.
    static QWidget* createEditor(QObject*             target,
                                 const QMetaProperty& prop,
                                 QWidget*             parent,
                                 const QString&       iconPrefix    = QString(),
                                 const QString&       iconLookupKey = QString());

    static void syncEditor(QObject* target, const QMetaProperty& prop, QWidget* editor);

  private:
    static QWidget* createBoolEditor(QObject*             target,
                                     const QMetaProperty& prop,
                                     QWidget*             parent,
                                     const QString&       iconPrefix,
                                     const QString&       iconLookupKey);
};

} // namespace GVis::UI