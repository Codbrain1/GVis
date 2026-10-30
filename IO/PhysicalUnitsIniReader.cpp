#include "PhysicalUnitsIniReader.h"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace QSpace::IO {

std::expected<Physics::PhysicalUnitsProfile, IO::ReadStatus>
importIniFile(const QString& filePath) {
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists())
        return std::unexpected(IO::ReadStatus::FileNotFound);
    if (!fileInfo.isFile() || fileInfo.suffix().compare("ini", Qt::CaseInsensitive) != 0)
        return std::unexpected(IO::ReadStatus::InvalidFormat);

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return std::unexpected(IO::ReadStatus::UnknownError);

    Physics::PhysicalUnitsProfile profile;
    profile.haveGamma = false;
    int parsedValues  = 0;
    while (!file.atEnd() && parsedValues < 2) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(';'))
            continue;

        bool         converted = false;
        const double value =
            QLocale::c().toDouble(line.section(QRegularExpression("\\s+"), 0, 0), &converted);
        if (!converted || !std::isfinite(value) || value <= 0.0)
            return std::unexpected(IO::ReadStatus::InvalidFileStructure);

        if (parsedValues == 0)
            profile.Km = value;
        else
            profile.Kr = value;
        ++parsedValues;
    }

    if (parsedValues != 2)
        return std::unexpected(IO::ReadStatus::InvalidFileStructure);

    return profile;
}

} // namespace QSpace::IO