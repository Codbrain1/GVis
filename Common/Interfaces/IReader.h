#pragma once
#include "Common/Enums/IOEnums.h"
#include "Common/Structures/FileSchemeStructures.h"
#include "IO/ReadResult.h"
#include <QString>
#include <vtkDataSet.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkSmartPointer.h>

namespace GVis
{
class IReader
{
public:
  virtual ~IReader() = default;
  virtual void setPolicy(IO::FilePolicy policy) = 0;
  virtual IO::ReadResult read(const QString &path, const IO::ReadScheme &scheme) const = 0;

  // чтение пакета данных одного временного снимка
  virtual IO::BatchResult readBatch(const QList<IO::BatchTask> &tasks) const
  {
    IO::BatchResult batchResult;
    auto multi = vtkSmartPointer<vtkMultiBlockDataSet>::New();
    multi->SetNumberOfBlocks(tasks.size());

    // последовательная реализация (внутри модуля IO может быть переопределна)
    for (int i = 0; i < tasks.size(); ++i)
    {
      auto res = this->read(tasks[i].path, tasks[i].scheme);
      if (res.isSuccess())
      {
        multi->SetBlock(i, res.data);
        batchResult.succes_count++;
      }
      else
      {
        batchResult.fail_count++;
        batchResult.errMessages.append(tasks[i].path + ":" + res.errMessage);
      }
    }
    batchResult.data = multi;
    return batchResult;
  }
};
} // namespace GVis