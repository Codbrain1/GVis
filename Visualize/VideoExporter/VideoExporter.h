#pragma once

#include <QObject>
#include <QString>
#include <vtkOggTheoraWriter.h>
#include <vtkSmartPointer.h>
#include <vtkWindowToImageFilter.h>

namespace GVis::Visualize {

class VideoExporter : public QObject {
    Q_OBJECT
  public:
    explicit VideoExporter(VtkView* view, QObject* parent = nullptr);
    ~VideoExporter();
    /**
     * @brief инициализирует создание видеофайла
     */
    bool startExport(const QString& filepath, int fps = 30);
    void captureFrame();
    void finishExport();

  private:
    VtkView*                                m_vtkView;
    vtkSmartPointer<vtkWindowToImageFilter> m_windowToImage;
    vtkSmartPointer<vtkOggTheoraWriter>     m_writer;
    bool                                    m_isRecording = false;
};

} // namespace GVis::Visualize