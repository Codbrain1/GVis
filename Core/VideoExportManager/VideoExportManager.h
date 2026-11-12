#pragma once

#include "Common/Interfaces/IRenderLayer.h"
#include "Core/DataManager/DataManager.h"
#include "Visualize/VideoExporter/VideoExporter.h"
#include <QMap>
#include <QObject>
#include <QStringList>
#include <quuid.h>
#include <memory>

namespace QSpace::Core {

class [[deprecated]] VideoExportManager : public QObject {
    Q_OBJECT
  public:
    VideoExportManager(QSpace::Core::DataManager*                        dataManager,
                       std::shared_ptr<QSpace::Visualize::IRenderLayer>  targetLayer,
                       std::shared_ptr<QSpace::Visualize::VideoExporter> exporter,
                       QObject*                                          parent = nullptr);

    // stride: 1 = каждый файл, 2 = через один, 10 = каждый десятый
    void start(const QStringList&            files,
               const QSpace::IO::ReadScheme& scheme,
               QSpace::IO::FileFormat        format,
               int                           stride = 1);

  signals:
    void progressUpdated(int currentFrame, int totalFrames);
    void exportFinished(bool success);
  public slots:
    void cancel(); // Слот для отмены

  private slots:
    void fillBuffer();
    void processReadyFrames();
    void handleFileReady(const QUuid& taskId, QSpace::IO::ReadResult result);

  private:
    bool                                              m_isCancelled = false; // Флаг отмены
    QSpace::Core::DataManager*                        m_dataManager;
    std::shared_ptr<QSpace::Visualize::IRenderLayer>  m_targetLayer;
    std::shared_ptr<QSpace::Visualize::VideoExporter> m_exporter;

    QStringList            m_files;
    QSpace::IO::ReadScheme m_scheme;
    QSpace::IO::FileFormat m_format;

    int m_stride;
    int m_totalFramesToRender;
    int m_framesRendered;

    // Контроль потокового чтения (Look-ahead)
    int m_maxConcurrentReads = 3; // Держим в RAM не более 3 файлов одновременно
    int m_nextFrameToRead;        // Индекс кадра, который нужно отправить на чтение
    int m_nextFrameToRender;      // Индекс кадра, который мы ждем для отрисовки в видео

    QMap<QUuid, int>          m_pendingTasks; // Связь TaskID -> Индекс кадра
    QMap<int, IO::ReadResult> m_readyFrames;  // Отсортированный буфер готовых данных
};

} // namespace QSpace::Core