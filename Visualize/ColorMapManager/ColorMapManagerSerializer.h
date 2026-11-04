#pragma once
#include "Common/Structures/SessionStructures.h"
#include "colormapmanager_export.h"
#include "Visualize/ColorMapManager/ColorMapManager.h"

namespace QSpace::Visualize {

class COLORMAPMANAGER_EXPORT ColorMapManagerSerializer {
  public:
    static QSpace::Session::ColorMapManagerDTO toDTO();
    static void fromDTO(const QSpace::Session::ColorMapManagerDTO& dto);
};
} // namespace QSpace::Visualize