#include "LogManager.h"
#include "Common/Logger/Logger.h"
#include <QCoreApplication>
#include <QDir>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <qcontainerfwd.h>
#include <qcoreapplication.h>
#include <qfileinfo.h>
#include <qloggingcategory.h>
#include <qstandardpaths.h>

namespace GVis::Core {
void LogManager::setup() {
    QString logPath;
#ifdef QT_DEBUG
    logPath = QCoreApplication::applicationDirPath() + "/debug_output.log";
#else
    logPath = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation); // путь к AppData в виндовс потому что именно туда система
                                          // разрешает писать файлы приложениям
    QDir().mkpath(logPath);
    logPath += "/app_log.txt";
#endif

    Common::Logger::init(logPath);
    // Настраиваем правила фильтрации по умолчанию
    // Выключаем debug для всех, оставляем info и выше
    // Но разрешаем Core и IO всегда писать Info
    QString rules = "*debug=false\n"
                    "GVis.*.info=true";
    QLoggingCategory::setFilterRules(rules);
}

void LogManager::setVerbose(bool enabled) {
    if (enabled)
        // Включаем вообще всё для диагностики
        QLoggingCategory::setFilterRules("GVis.*=true");
    else
        QLoggingCategory::setFilterRules("*.debug=false");
}
} // namespace GVis::Core