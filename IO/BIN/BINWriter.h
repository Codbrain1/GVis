#include "Common/Enums/IOEnums.h"
#include "Common/Interfaces/IWriter.h"
#include <QFile>
#include <qcontainerfwd.h>
#include <vtkDataSet.h>
#include "Structures/IOStructures.h"

namespace GVis::IO { // TODO: решить в каком формате и что будет записываться в бинарные файлы

class BINWriter : public IWriter {
  public:
    void setPolicy(FilePolicy policy) override {
        m_policy = policy;
    }

    bool write(const QString& path, vtkDataSet* data, const WriteScheme& scheme) override;

  private:
    bool       validate(QFile& file, const WriteScheme& scheme);
    FilePolicy m_policy;
};
} // namespace GVis::IO