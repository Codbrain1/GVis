#pragma once
#include "Common/Enums/IOEnums.h"
#include "Common/Structures/PhysicalStructures.h"
#include <expected>

namespace GVis::IO {
std::expected<Physics::PhysicalUnitsProfile, IO::ReadStatus> importIniFile(const QString& filePath);

} // namespace GVis::IO