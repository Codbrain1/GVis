#pragma once
#include "Common/Enums/IOEnums.h"
#include "Common/Enums/VisualizeBaseEnums.h"
#include "Common/Interfaces/IReader.h"
#include "Common/Interfaces/IWriter.h"
#include <qcontainerfwd.h>
#include <memory>

namespace GVis::IO {
std::unique_ptr<IReader> createReader(FileFormat format); // строго задаем формат
std::unique_ptr<IWriter> createWriter(FileFormat format); // строго задаем формат файла при записи

namespace Utils {
FileFormat                  getFormat(const QString& path);
GVis::Visualize::EntityType getEntityType(const QString& name);
}; // namespace Utils
} // namespace GVis::IO