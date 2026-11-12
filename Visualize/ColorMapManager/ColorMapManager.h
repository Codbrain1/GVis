#pragma once
#include "Common/Structures/ColormapPresets.h"
#include <QApplication>
#include <QColor>
#include <QIcon>
#include <QLinearGradient>
#include <QList>
#include <QMap>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QUuid>
#include <qlogging.h>
#include <qobject.h>
#include <qtmetamacros.h>
#include <quuid.h>
#include "colormapmanager_export.h"


namespace GVis::Visualize {
class COLORMAPMANAGER_EXPORT ColorMapManager : public QObject {
    Q_OBJECT
  public:
    static ColorMapManager& instance();

    // Поиск по ID — теперь максимально быстрый
    std::optional<ColorMap> getMap(const QUuid& id) const;

    // Для заполнения UI (список всех палитр)
    QList<ColorMap> getAllMaps() const;

    void  AddCustomMap(const ColorMap& map);
    void  removeCustomMap(const QUuid& id);
    bool  contains(const QUuid& id);
    QIcon createColorMapIcon(const GVis::Visualize::ColorMap& map, QSize size = QSize(80, 16));
  signals:
    void paleteAdded(const ColorMap& colorMap);

  private:
    ColorMapManager(QObject* parent = nullptr);
    ColorMapManager& operator=(const ColorMapManager&) = delete;
    // Храним мапу для быстрого доступа по UUID
    QMap<QUuid, ColorMap> m_availableMaps;
};
} // namespace GVis::Visualize