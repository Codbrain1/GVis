#pragma once
#include "Common/Structures/SessionStructures.h"

namespace GVis::Core {
class FileSchemeRegistry;
}

namespace GVis::Session {
class FileSchemeRegistrySerializer {
  public:
    static FileSchemeRegistryDTO toDTO(Core::FileSchemeRegistry* registry);
    bool fromDTO(const FileSchemeRegistryDTO& dto, Core::FileSchemeRegistry* registry);
};
} // namespace GVis::Session