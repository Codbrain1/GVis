#include "AutoSettingsToolBar.h"
#include "PropertyControlFactory.h"
#include <QFormLayout>
#include <QMenu>
#include <QMetaProperty>
#include <QWidgetAction>

namespace GVis::UI {

AutoSettingsToolBar::AutoSettingsToolBar(QWidget* parent) : QToolBar(parent) {
    setMovable(false);
    setFloatable(false);
    setIconSize(QSize(16, 16));
    setToolButtonStyle(Qt::ToolButtonIconOnly);
}

void AutoSettingsToolBar::setSettings(QObject* settings, const QString& iconPrefix) {
    if (m_settings)
        m_settings->disconnect(this);

    m_settings   = settings;
    m_iconPrefix = iconPrefix;
    rebuild();
}

void AutoSettingsToolBar::setQuickAccessProperties(const QStringList& qualifiedNames) {
    m_quickAccessProperties = QSet<QString>(qualifiedNames.begin(), qualifiedNames.end());
    if (m_settings)
        rebuild();
}

bool AutoSettingsToolBar::isQuickAccess(const QString& qualifiedName) const {
    return m_quickAccessProperties.contains(qualifiedName);
}

AutoSettingsToolBar::ResolvedProperty
AutoSettingsToolBar::resolveQualifiedProperty(const QString& qualifiedName) const {
    if (!m_settings)
        return {};

    QStringList parts   = qualifiedName.split('.', Qt::SkipEmptyParts);
    QObject*    current = m_settings;

    // Проходим по всем сегментам, кроме последнего — это путь через вложенные QObject*.
    for (int i = 0; i < parts.size() - 1; ++i) {
        const QMetaObject* mo  = current->metaObject();
        int                idx = mo->indexOfProperty(parts[i].toUtf8().constData());
        if (idx < 0)
            return {};
        QObject* nested = mo->property(idx).read(current).value<QObject*>();
        if (!nested)
            return {};
        current = nested;
    }

    const QMetaObject* mo  = current->metaObject();
    int                idx = mo->indexOfProperty(parts.last().toUtf8().constData());
    if (idx < 0)
        return {};

    return {current, mo->property(idx), true};
}

void AutoSettingsToolBar::rebuild() {
    clear();
    m_boundEditors.clear();

    if (!m_settings)
        return;

    // Проход 1: исключительные (quick-access) свойства — сразу в полосу.
    addQuickAccessControls();
    if (!m_quickAccessProperties.isEmpty())
        addSeparator();

    // Проход 2: обычная автогенерация — скаляры верхнего уровня и группы.
    const QMetaObject* mo = m_settings->metaObject();
    for (int i = QObject::staticMetaObject.propertyCount(); i < mo->propertyCount(); ++i) {
        QMetaProperty prop         = mo->property(i);
        QString       propertyName = QString::fromLatin1(prop.name());

        if (isQuickAccess(propertyName))
            continue; // уже вынесено в проходе 1

        if (prop.metaType().flags().testFlag(QMetaType::PointerToQObject)) {
            QObject* nested = prop.read(m_settings).value<QObject*>();
            if (nested)
                addNestedGroup(prop, nested, propertyName);
        } else {
            addScalarProperty(m_settings, prop, propertyName);
        }
    }

    int sigIdx = mo->indexOfSignal("changed()");
    if (sigIdx < 0)
        sigIdx = mo->indexOfSignal("anyChanged()");
    if (sigIdx >= 0) {
        QMetaMethod signal = mo->method(sigIdx);
        QMetaMethod slot   = metaObject()->method(metaObject()->indexOfSlot("syncAll()"));
        connect(m_settings, signal, this, slot);
    }
}

void AutoSettingsToolBar::addQuickAccessControls() {
    for (const QString& qualifiedName : std::as_const(m_quickAccessProperties)) {
        ResolvedProperty resolved = resolveQualifiedProperty(qualifiedName);
        if (!resolved.valid) {
            qWarning() << "AutoSettingsToolBar: quick-access property not found:" << qualifiedName;
            continue;
        }

        QString  iconKey = QString(qualifiedName).replace('.', '_');
        QWidget* editor =
            PropertyControlFactory::createEditor(resolved.target, resolved.prop, this, m_iconPrefix, iconKey);
        if (!editor)
            continue;

        addWidget(editor);
        m_boundEditors.push_back({resolved.target, resolved.prop, editor});
    }
}

void AutoSettingsToolBar::addScalarProperty(QObject*             target,
                                            const QMetaProperty& prop,
                                            const QString&       iconLookupKey) {
    QWidget* editor = PropertyControlFactory::createEditor(target, prop, this, m_iconPrefix, iconLookupKey);
    if (!editor)
        return; // тип не поддержан для компактного отображения (например QString)

    addWidget(editor);
    m_boundEditors.push_back({target, prop, editor});
}

void AutoSettingsToolBar::addNestedGroup(const QMetaProperty& prop,
                                         QObject*             nested,
                                         const QString&       groupPath) {
    QString iconPath = m_iconPrefix + groupPath + ".svg";
    QIcon   icon(iconPath);
    if (icon.isNull())
        qWarning() << "AutoSettingsToolBar: missing group icon resource" << iconPath;

    QAction* groupAction = addAction(icon, groupPath);
    groupAction->setToolTip(groupPath);

    auto* menu = new QMenu(this);
    menu->setMinimumWidth(220);

    auto* popupWidget = new QWidget(menu);
    auto* form        = new QFormLayout(popupWidget);
    form->setContentsMargins(8, 6, 8, 6);
    form->setSpacing(4);

    populateFormForObject(nested, form, groupPath);

    // Если все свойства группы оказались quick-access (например, у viewport
    // остались бы только orthographic, а он уже вынесен) — попап окажется
    // пустым. В этом случае кнопку группы просто отключаем.
    if (form->rowCount() == 0) {
        popupWidget->deleteLater();
        menu->deleteLater();
        groupAction->setEnabled(false);
        return;
    }

    auto* widgetAction = new QWidgetAction(menu);
    widgetAction->setDefaultWidget(popupWidget);
    menu->addAction(widgetAction);

    connect(groupAction, &QAction::triggered, this, [this, groupAction, menu]() {
        QWidget* anchor = widgetForAction(groupAction);
        QPoint   pos =
            anchor ? anchor->mapToGlobal(QPoint(0, anchor->height())) : mapToGlobal(QPoint(0, height()));
        menu->popup(pos);
    });
}

void AutoSettingsToolBar::populateFormForObject(QObject*       target,
                                                QFormLayout*   form,
                                                const QString& groupPath) {
    const QMetaObject* mo = target->metaObject();

    for (int i = QObject::staticMetaObject.propertyCount(); i < mo->propertyCount(); ++i) {
        QMetaProperty prop          = mo->property(i);
        QString       propertyName  = QString::fromLatin1(prop.name());
        QString       qualifiedName = groupPath + "." + propertyName;

        if (isQuickAccess(qualifiedName))
            continue; // вынесено в основную полосу — не дублируем в попапе

        if (prop.metaType().flags().testFlag(QMetaType::PointerToQObject)) {
            QObject* deeperNested = prop.read(target).value<QObject*>();
            if (!deeperNested)
                continue;
            auto* nestedForm = new QFormLayout();
            populateFormForObject(deeperNested, nestedForm, qualifiedName);
            form->addRow(propertyName, nestedForm);
            continue;
        }

        QString iconLookupKey = qualifiedName;
        iconLookupKey.replace('.', '_');

        QWidget* editor = PropertyControlFactory::createEditor(target,
                                                               prop,
                                                               form->parentWidget(),
                                                               m_iconPrefix,
                                                               iconLookupKey);
        if (!editor)
            continue;

        form->addRow(propertyName, editor);
        m_boundEditors.push_back({target, prop, editor});
    }
}

void AutoSettingsToolBar::syncAll() {
    for (auto& e : m_boundEditors)
        PropertyControlFactory::syncEditor(e.target, e.prop, e.widget);
}

} // namespace GVis::UI