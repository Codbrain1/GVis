#pragma once
#include "Common/Structures/SessionStructures.h"
#include "Visualize/ColorMapManager/ColorMapManager.h"
#include "colormapmanager_export.h"


namespace GVis::Visualize {

class COLORMAPMANAGER_EXPORT ColorMapManagerSerializer {
  public:
    static GVis::Session::ColorMapManagerDTO toDTO();
    static void                              fromDTO(const GVis::Session::ColorMapManagerDTO& dto);
};
} // namespace GVis::Visualize