#pragma once
#include "Common/Enums/IOEnums.h"
#include "Common/Structures/PhysicalStructures.h"
#include <expected>


namespace QSpace::IO {
std::expected<Physics::PhysicalUnitsProfile, IO::ReadStatus> importIniFile(const QString& filePath);

} // namespace QSpace::IO