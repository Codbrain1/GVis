#include "Logger.h"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <iostream>
#include <memory>
#include <qcontainerfwd.h>
#include <qdebug.h>
#include <qfiledevice.h>
#include <qlogging.h>
#include <qloggingcategory.h>
#include <qmutex.h>

Q_LOGGING_CATEGORY(LogSystem, "GVis.System")
Q_LOGGING_CATEGORY(LogCommon, "GVis.Common")
Q_LOGGING_CATEGORY(LogIO, "GVis.IO")
Q_LOGGING_CATEGORY(LogData, "GVis.Data")
Q_LOGGING_CATEGORY(LogCore, "GVis.Core")
Q_LOGGING_CATEGORY(LogFiltering, "GVis.Filtering")
Q_LOGGING_CATEGORY(LogPhysics, "GVis.Physics")
Q_LOGGING_CATEGORY(LogRemote, "GVis.Remote")
Q_LOGGING_CATEGORY(LogRenderer, "GVis.Renderer")
Q_LOGGING_CATEGORY(LogUI, "GVis.UI")
Q_LOGGING_CATEGORY(LogSession, "GVis.Session")

static std::unique_ptr<QFile> m_log_file;
static QMutex m_mutex;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
  // выбираем тип лога
  QString levelText;
  switch (type) {
    case QtDebugMsg:    levelText = "[DBG]"; break;
    case QtInfoMsg:     levelText = "[INF]"; break;
    case QtWarningMsg:  levelText = "[WRN]"; break;
    case QtCriticalMsg: levelText = "[CRT]"; break;
    case QtFatalMsg:    levelText = "[FTL]"; break;
  }
  // формируем дату и время, и категорию
  QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
  QString category = QString::fromUtf8(context.category);

  QString location;
  if (context.file) {
    QString filename = QFileInfo(QString::fromUtf8(context.file)).fileName(); // путь к файлу с ошибкой
    location = QString("(%1:%2)").arg(filename).arg(context.line);            // номер строки с ошибкой
  }
  // Собираем итоговую строку
  //  Пример: [2023-10-05 12:00:00.123] [WRN] [GVis.IO] File header corrupted! (BinaryReader.cpp:45)
  QString formatMsg = QString("%1 %2 [%3] %4 %5")
                          .arg(timeStr)   // создания записи
                          .arg(levelText) // тип ошибки
                          .arg(category)  // модуль откуда прила ошибка
                          .arg(msg)       // сообщение от отправителя
                          .arg(location);

  if (type == QtCriticalMsg || type == QtFatalMsg) {
    std::cerr << formatMsg.toStdString() << std::endl;
  } else {
    std::cout << formatMsg.toStdString() << std::endl;
  }
  QMutexLocker loker(&m_mutex);
  if (m_log_file && m_log_file->isOpen()) {
    QTextStream stream(m_log_file.get());
    stream << formatMsg << "\n";
  }
}

namespace GVis::Common {
void Logger::init(const QString &logName) {
  m_log_file = std::make_unique<QFile>(logName);
  if (m_log_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    qInstallMessageHandler(messageHandler);
    qInfo(LogCommon) << "=== Logger Initialized === ";
  } else {
    std::cerr << "Failed to open log file: " << logName.toStdString() << std::endl;
  }
}
} // namespace GVis::Common