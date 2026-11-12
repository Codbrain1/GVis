#pragma once
#include "Common/Enums/IOEnums.h"
#include "Common/Structures/FileSchemeStructures.h"
#include "Logger/Logger.h"
#include <Qstring>
#include <qcontainerfwd.h>
#include <qlogging.h>
#include <qtypes.h>
#include <vtkDataObject.h>
#include <vtkDataSet.h>
#include <vtkMultiBlockDataSet.h>

namespace GVis
{
class IWriter
{
public:
  virtual ~IWriter() = default;
  virtual void setPolicy(IO::FilePolicy policy) = 0;
  virtual bool write(const QString &path, vtkDataSet *data, const IO::WriteScheme &scheme) = 0;
  virtual bool writeBatch(const QStringList &paths, vtkMultiBlockDataSet *multiBlock, const IO::WriteScheme &scheme)
  {
    if (!multiBlock || paths.isEmpty())
      return false;

    unsigned int numBlocks = multiBlock->GetNumberOfBlocks();
    int count = std::min(static_cast<qsizetype>(numBlocks), paths.size());
    for (int i = 0; i < count; ++i)
    {
      // Извлекаем блок. Поскольку наш write принимает vtkDataSet*,
      // нужно привести тип (SafeDownCast вернет nullptr, если блок другого типа)
      auto *dataSet = vtkDataSet::SafeDownCast(multiBlock->GetBlock(i));
      if (!dataSet)
      {
        qWarning(LogCommon) << "Data block " << i << " is not read";
        continue;
      }
      if (!this->write(paths[i], dataSet, scheme))
      {
        return false;
      }
    }
    return true;
  }
};
} // namespace GVis