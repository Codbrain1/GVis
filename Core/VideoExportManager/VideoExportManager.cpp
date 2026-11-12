#include "VideoExportManager.h"
#include "Common/Enums/IOEnums.h"
#include "Common/Logger/Logger.h"
#include "Interfaces/IOFactory.h"
#include <QFileInfo>

namespace GVis::Core {

VideoExportManager::VideoExportManager(DataManager*                                    dataManager,
                                       std::shared_ptr<GVis::Visualize::IRenderLayer>  targetLayer,
                                       std::shared_ptr<GVis::Visualize::VideoExporter> exporter,
                                       QObject*                                        parent)
    : QObject(parent),
      m_dataManager(dataManager),
      m_targetLayer(targetLayer),
      m_exporter(exporter) {
    // Подписываемся на ответы от DataManager
    connect(m_dataManager, &DataManager::fileReady, this, &VideoExportManager::handleFileReady);
}

void VideoExportManager::start(const QStringList&    files,
                               const IO::ReadScheme& scheme,
                               IO::FileFormat        format,
                               int                   stride) {
    m_files  = files;
    m_scheme = scheme;
    m_format = format;
    m_stride = (stride > 0) ? stride : 1;

    m_framesRendered    = 0;
    m_nextFrameToRead   = 0;
    m_nextFrameToRender = 0;
    m_pendingTasks.clear();
    m_readyFrames.clear();

    m_totalFramesToRender = (m_files.size() + m_stride - 1) / m_stride;

    qCInfo(LogCore) << "Starting video export. Files:" << m_files.size() << "Stride:" << m_stride
                    << "Frames to render:" << m_totalFramesToRender;

    // Даем стартовый пинок конвейеру
    fillBuffer();
}

void VideoExportManager::fillBuffer() {
    if (m_isCancelled)
        return;
    // Поддерживаем очередь чтения. Если задач меньше максимума - добавляем.
    while (m_pendingTasks.size() < m_maxConcurrentReads && m_nextFrameToRead < m_files.size()) {
        QString path = m_files[m_nextFrameToRead];

        // Отправляем в TaskManager и запоминаем какой ID какому кадру принадлежит
        QUuid taskId = m_dataManager->importDataAsync(path, m_scheme, IO::ImportRole::FullData);
        m_pendingTasks.insert(taskId, m_nextFrameToRead);

        m_nextFrameToRead += m_stride;
    }
}

void VideoExportManager::handleFileReady(const QUuid& taskId, IO::ReadResult result) {
    // Проверяем, относится ли этот файл к нашему процессу экспорта
    if (!m_pendingTasks.contains(taskId))
        return;

    int frameIndex = m_pendingTasks.take(taskId);

    if (!result.isSuccess()) {
        qCCritical(LogCore) << "Video export failed on frame" << frameIndex << ":"
                            << result.errMessage;
        m_exporter->finishExport();
        emit exportFinished(false);
        return;
    }

    // Кладем результат в отсортированный буфер
    m_readyFrames.insert(frameIndex, result);

    // Пробуем отрендерить кадры по порядку
    processReadyFrames();

    // Буфер освободился - запрашиваем следующий файл
    fillBuffer();
}

void VideoExportManager::cancel() {
    m_isCancelled = true;
}

void VideoExportManager::processReadyFrames() {
    // Цикл работает только если в буфере лежит ТОТ САМЫЙ кадр, чья очередь подошла
    while (m_readyFrames.contains(m_nextFrameToRender)) {
        if (m_isCancelled) {
            m_exporter->finishExport();
            emit exportFinished(false);
            return; // Экстренный выход
        }
        IO::ReadResult result     = m_readyFrames.take(m_nextFrameToRender);
        auto           fileName   = QFileInfo(result.path).fileName();
        auto           entityType = IO::Utils::getEntityType(fileName);
        // 1. Создаем временную ноду для визуализатора
        auto tempNode = std::make_shared<DataNode>(result.data,
                                                   QFileInfo(result.path).fileName(),
                                                   result.timestamp,
                                                   entityType);

        // 2. Подменяем данные. Так как targetLayer это интерфейс, безопасно кастуем его
        m_targetLayer->setData(tempNode);

        // 3. Синхронный рендер кадра в файл
        m_exporter->captureFrame();

        m_nextFrameToRender += m_stride;
        m_framesRendered++;

        emit progressUpdated(m_framesRendered, m_totalFramesToRender);

        // 4. ОЧИСТКА ПАМЯТИ: В этот момент result выходит из области видимости,
        // счетчик vtkSmartPointer падает до 0, и тяжелые гигабайты памяти очищаются.
    }

    // Проверка на завершение
    if (m_framesRendered >= m_totalFramesToRender) {
        m_exporter->finishExport();
        emit exportFinished(true);
    }
}

} // namespace GVis::Core