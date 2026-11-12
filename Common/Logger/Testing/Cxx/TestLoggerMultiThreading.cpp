#include "Common/Logger/Logger.h"
#include <QFuture>
#include <QTest>
#include <QtConcurrent/QtConcurrent>
#include <QtConcurrent/qtconcurrentrun.h>
#include <qlist.h>
#include <qlogging.h>
#include <qobject.h>
#include <qtmetamacros.h>

class TestLoggerMultiThreading : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() {
    QString testLogName = "test_output.log";
    QFile::remove(testLogName);
    GVis::Common::Logger::init(testLogName);
  };
  void testMultiThreadWrite() {
    const int iterationsThread = 100;                              // число записей одним потоком
    const int ThreadCount = 20;                                    // число потоков
    const int expectedTotalLines = iterationsThread * ThreadCount; // общее число строк
    auto writeTask = [iterationsThread](int threadID) {
      for (int i = 0; i < iterationsThread; ++i) {
        qInfo(LogCommon) << "this thread:" << threadID << ", iteration: " << i;
      }
    };
    QList<QFuture<void>> futures;
    for (int i = 0; i < ThreadCount; ++i) {
      futures.append(QtConcurrent::run(writeTask, i));
    }
    for (auto &f : futures) {
      f.waitForFinished();
    }
    QFile file("test_output.log");
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream in(&file);
    int lineCount = 0;
    while (!in.atEnd()) {
      in.readLine();
      lineCount++;
    }
    file.close();

    // Если логгер не потокобезопасен (нет мьютекса),
    // записи могут перемешаться или потеряться, и счетчик будет неверным.
    QCOMPARE(lineCount, expectedTotalLines + 1);
  }
  void cleanupTestCase() { QFile::remove("test_output.log"); }
};
QTEST_MAIN(TestLoggerMultiThreading)
#include "TestLoggerMultiThreading.moc"