#pragma once
#include <QLoggingCategory>
#include <qloggingcategory.h>
/*_1_*/ Q_DECLARE_LOGGING_CATEGORY(LogSystem) // Запуск приложения, загрузка конфигураций, плагинов, аргументы командной
                                              // строки. То, что происходит 1 раз при старте.
/*_2_*/ Q_DECLARE_LOGGING_CATEGORY(LogCommon) // Утилиты, вспомогательные классы, ошибки в базовых структурах.
/*_3_*/ Q_DECLARE_LOGGING_CATEGORY(LogIO)     // чтение и запись файлов
/*_4_*/ Q_DECLARE_LOGGING_CATEGORY(LogData)   // хранение данных в оперативной памяти и управление ими
/*_5_*/ Q_DECLARE_LOGGING_CATEGORY(LogCore)   // ядро програмы управлющее взаимодействием разных модулей
/*_6_*/ Q_DECLARE_LOGGING_CATEGORY(LogFiltering) // фильтрации по заготовкам из vtk
/*_7_*/ Q_DECLARE_LOGGING_CATEGORY(LogPhysics)   // физика и алгоритмы
/*_8_*/ Q_DECLARE_LOGGING_CATEGORY(LogRemote) // серверное соединение (TODO:будет добавленно на позднем этапе разрабтки)
/*_9_*/ Q_DECLARE_LOGGING_CATEGORY(LogRenderer) // отображение различных приметив и данных считанных с помощью IO
/*_10_*/ Q_DECLARE_LOGGING_CATEGORY(LogUI)      // интерфейс и непосредственное общение с пользователем
/*_11_*/ Q_DECLARE_LOGGING_CATEGORY(LogSession) // работа настройками а и загрузка проектов
namespace GVis::Common {

class Logger {
public:
  static void init(const QString &logName = "GVis.log");
};
} // namespace GVis::Common