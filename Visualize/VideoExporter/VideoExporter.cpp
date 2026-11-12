#include "VideoExporter.h"
#include "Common/Logger/Logger.h"
#include <vtkGenericOpenGLRenderWindow.h>

namespace GVis::Visualize {

VideoExporter::VideoExporter(VtkView* vtkview, QObject* parent)
    : QObject(parent), m_vtkView(vtkview) {
    m_windowToImage = vtkSmartPointer<vtkWindowToImageFilter>::New();
    m_windowToImage->SetInput(m_vtkView->getRenderWindow());
    m_windowToImage->SetInputBufferTypeToRGB();
    m_windowToImage->ReadFrontBufferOff(); // Важно: читаем из заднего буфера (скрытого)

    m_writer = vtkSmartPointer<vtkOggTheoraWriter>::New();
    m_writer->SetInputConnection(m_windowToImage->GetOutputPort());
}

VideoExporter::~VideoExporter() {
    finishExport();
}

bool VideoExporter::startExport(const QString& filepath, int fps) {
    if (!m_vtkView || !m_vtkView->getRenderWindow()) {
        qCCritical(LogRenderer) << "VideoExporter: Render window is null!";
        return false;
    }
    m_windowToImage->SetInput(m_vtkView->getRenderWindow());
    m_writer->SetFileName(filepath.toStdString().c_str());
    m_writer->SetRate(fps);
    m_writer->Start();
    m_isRecording = true;

    qCInfo(LogRenderer) << "Started video export to:" << filepath << "at" << fps << "FPS";
    return true;
}

void VideoExporter::captureFrame() {
    if (!m_isRecording)
        return;

    // Принудительно рендерим кадр в VTK
    m_vtkView->getRenderWindow()->Render();

    // Захватываем пиксели и пишем
    m_windowToImage->Modified();
    m_windowToImage->Update();
    m_writer->Write();
}

void VideoExporter::finishExport() {
    if (m_isRecording) {
        m_writer->End();
        m_isRecording = false;
        qCInfo(LogRenderer) << "Video export finished.";
    }
}

} // namespace GVis::Visualize