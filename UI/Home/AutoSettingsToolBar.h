#pragma once
#include <QMetaProperty>
#include <QSet>
#include <QStringList>
#include <QToolBar>
#include <QVector>

class QMenu;
class QFormLayout;

namespace GVis::UI {

class AutoSettingsToolBar : public QToolBar {
    Q_OBJECT
  public:
    explicit AutoSettingsToolBar(QWidget* parent = nullptr);

    void setSettings(QObject* settings, const QString& iconPrefix = ":/icons/");

    // "Исключительные" свойства, которые выносятся прямо в полосу тулбара,
    // минуя попап своей группы. Формат: "группа.свойство" для вложенных
    // (например "viewport.orthographic"), либо просто "свойство" для свойств
    // верхнего уровня. Такие свойства ИСКЛЮЧАЮТСЯ из попапа своей группы —
    // не дублируются. Можно вызывать до или после setSettings().
    void setQuickAccessProperties(const QStringList& qualifiedNames);

  private slots:
    void syncAll();

  private:
    struct BoundEditor {
        QObject*      target;
        QMetaProperty prop;
        QWidget*      widget;
    };

    struct ResolvedProperty {
        QObject*      target = nullptr;
        QMetaProperty prop;
        bool          valid = false;
    };

    void rebuild();
    void addQuickAccessControls();
    void addScalarProperty(QObject* target, const QMetaProperty& prop, const QString& iconLookupKey);
    void addNestedGroup(const QMetaProperty& prop, QObject* nested, const QString& groupPath);
    void populateFormForObject(QObject* target, QFormLayout* form, const QString& groupPath);

    ResolvedProperty resolveQualifiedProperty(const QString& qualifiedName) const;
    bool             isQuickAccess(const QString& qualifiedName) const;

    QObject*      m_settings = nullptr;
    QString       m_iconPrefix;
    QSet<QString> m_quickAccessProperties;

    QVector<BoundEditor> m_boundEditors;
};

} // namespace GVis::UI