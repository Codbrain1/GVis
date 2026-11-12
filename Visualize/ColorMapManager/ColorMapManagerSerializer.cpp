#include "ColorMapManagerSerializer.h"
#include "Common/Structures/SessionStructures.h"

namespace GVis::Visualize {

GVis::Session::ColorMapManagerDTO ColorMapManagerSerializer::toDTO() {
    GVis::Session::ColorMapManagerDTO dto;
    dto.colorMaps = GVis::Visualize::ColorMapManager::instance().getAllMaps();
    return dto;
}

void ColorMapManagerSerializer::fromDTO(const GVis::Session::ColorMapManagerDTO& dto) {
    for (const auto map : dto.colorMaps) {
        GVis::Visualize::ColorMapManager::instance().AddCustomMap(map);
    }
}

} // namespace GVis::Visualize