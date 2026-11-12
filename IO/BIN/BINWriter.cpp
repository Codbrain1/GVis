#include "BINWriter.h"
#include "Common/Logger/Logger.h"
#include <qdebug.h>
#include <qlogging.h>
#include <qloggingcategory.h>
#include <vtkType.h>
#include <variant>


namespace GVis::IO {
bool BINWriter::write(const QString& path, vtkDataSet* data, const WriteScheme& scheme) {
    QFile file(path);
    auto  isValide = validate(file, scheme);
    if (!isValide) {
        return false;
    }
    if (!file.open(QIODevice::WriteOnly)) {
        qCCritical(LogIO) << "File is not open: " << file.fileName();
        return false;
    }
    const auto config = std::get<ColumnScheme>(scheme);
}

bool BINWriter::validate(QFile& file, const WriteScheme& scheme) {
    if (!std::holds_alternative<ColumnScheme>(scheme)) {
        qCCritical(LogIO) << "BinWriter requires ColumnScheme! file: " << file.fileName();
    }
    auto config = std::get<ColumnScheme>(scheme);
    for (const auto& col : config.columnsPolicy) {
        if (col.vtkDataType != VTK_DOUBLE && col.vtkDataType != VTK_FLOAT &&
            col.vtkDataType != VTK_INT) {
            qCCritical(LogIO) << "Unsuported type in column: " << col.name;
            return false;
        }
        if (col.name.isEmpty()) {
            qCCritical(LogIO) << "Column name is empty!";
            return false;
        }
        if (col.numberOfComponents < 1) {
            qCCritical(LogIO) << "Number of Components can't less one!";
            return false;
        }
    }
}
} // namespace GVis::IO