
#include "ColorMapManager.h"
#include "Common/Logger/Logger.h"
#include <QList>
#include <QMetaObject>
#include <QThread>
#include <qobject.h>
#include <quuid.h>

namespace QSpace::Visualize {
ColorMapManager& ColorMapManager::instance() {
    static ColorMapManager _instance;
    return _instance;
}

// Поиск по ID — теперь максимально быстрый
std::optional<ColorMap> ColorMapManager::getMap(const QUuid& id) const {
    if (m_availableMaps.contains(id))
        return m_availableMaps.value(id);
    return std::nullopt;
}

// Для заполнения UI (список всех палитр)
QList<ColorMap> ColorMapManager::getAllMaps() const {
    return m_availableMaps.values();
}

void ColorMapManager::AddCustomMap(const ColorMap& map) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(
            this, [this, map]() { AddCustomMap(map); }, Qt::QueuedConnection);
        return;
    }

    m_availableMaps.insert(map.id, map);
    emit paleteAdded(map);
}

void ColorMapManager::removeCustomMap(const QUuid& id) {
    auto it = m_availableMaps.find(id);
    if (it != m_availableMaps.end() && !it->isPreset) {
        m_availableMaps.erase(it);
    } else {
        qCritical(LogRenderer) << "Attempted to remove a preset or non-existent colormap with ID:"
                               << id;
    }
}

ColorMapManager::ColorMapManager(QObject* parent) : QObject(parent) {
    qRegisterMetaType<ColorMap>("QSpace::Visualize::ColorMap");

    // При инициализации заполняем карту пресетами
    auto presets = ColorMapPresets::getStandardPresets();
    for (const auto& map : presets) {
        m_availableMaps.insert(map.id, map);
    }
}

bool ColorMapManager::contains(const QUuid& id) {
    return m_availableMaps.contains(id);
}

QIcon ColorMapManager::createColorMapIcon(const QSpace::Visualize::ColorMap& map, QSize size) {
    QPixmap  pix(size);
    QPainter painter(&pix);

    // Рисуем градиент
    QLinearGradient grad(0, 0, size.width(), 0);
    for (const auto& pt : map.points) {
        grad.setColorAt(pt.x, QColor::fromRgbF(pt.r, pt.g, pt.b));
    }

    painter.fillRect(pix.rect(), grad);

    // Адаптивная рамка: берем цвет текста из системной палитры,
    // но делаем его полупрозрачным
    QColor borderColor = qApp->palette().color(QPalette::WindowText);
    borderColor.setAlpha(60);

    painter.setPen(borderColor);
    painter.drawRect(pix.rect().adjusted(0, 0, -1, -1));

    return QIcon(pix);
}

} // namespace QSpace::Visualize