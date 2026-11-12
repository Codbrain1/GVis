#include "Common/Structures/ObjectRegistryStructures.h"
#include "Visualize/Layers/Implementations/SPH/SPHPointsLayerSettings.h"
#include "Visualize/Layers/LayerFactory.h"
#include "Visualize/Views/View3D/AbstractView3D.h"
#include "Visualize/Views/View3D/OpenGL3DWidget/GLViewport.h"
#include "Visualize/Views/ViewFactory.h"
#include "IO/ReaderFactory.h"
#include "IO/SchemePresets.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSurfaceFormat>
#include <QTextStream>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <algorithm>
#include <memory>

namespace {

struct FrameStatistics {
    double averageMs = 0.0;
    double minimumMs = 0.0;
    double maximumMs = 0.0;
};

bool renderAndWait(GVis::Visualize::Views::View3D::GLViewport& viewport) {
    viewport.repaint();

    viewport.makeCurrent();
    QOpenGLContext* context = viewport.context();
    if (!context) {
        viewport.doneCurrent();
        return false;
    }
    context->functions()->glFinish();
    viewport.doneCurrent();
    return true;
}

FrameStatistics measureResidentFrames(GVis::Visualize::Views::View3D::GLViewport& viewport,
                                      int                                         frameCount) {
    QVector<double> frameTimes;
    frameTimes.reserve(frameCount);

    for (int frame = 0; frame < frameCount; ++frame) {
        QElapsedTimer timer;
        timer.start();
        if (!renderAndWait(viewport))
            return {};
        frameTimes.append(timer.nsecsElapsed() / 1'000'000.0);
    }

    const auto [minimum, maximum] = std::minmax_element(frameTimes.cbegin(), frameTimes.cend());
    double totalMs                = 0.0;
    for (const double frameTime : frameTimes)
        totalMs += frameTime;

    return {totalMs / frameTimes.size(), *minimum, *maximum};
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);
    QApplication app(argc, argv);
    QLoggingCategory::setFilterRules("*.debug=false");

    QCommandLineParser parser;
    parser.setApplicationDescription("Measure first-frame GPU upload and resident SPH rendering.");
    parser.addHelpOption();
    parser.addPositionalArgument("directory", "Directory containing GVis BIN V2 files.");
    QCommandLineOption warmupOption({"w", "warmup"},
                                    "Number of unmeasured resident frames.",
                                    "count",
                                    "5");
    QCommandLineOption framesOption({"f", "frames"},
                                    "Number of measured resident frames.",
                                    "count",
                                    "60");
    QCommandLineOption widthOption("width", "Render width in pixels.", "pixels", "1920");
    QCommandLineOption heightOption("height", "Render height in pixels.", "pixels", "1080");
    parser.addOption(warmupOption);
    parser.addOption(framesOption);
    parser.addOption(widthOption);
    parser.addOption(heightOption);
    parser.process(app);

    const QStringList positionalArguments = parser.positionalArguments();
    if (positionalArguments.size() != 1) {
        parser.showHelp(1);
    }

    bool      ok           = false;
    const int warmupFrames = parser.value(warmupOption).toInt(&ok);
    if (!ok || warmupFrames < 0) {
        qCritical("--warmup must be a non-negative integer");
        return 1;
    }
    const int measuredFrames = parser.value(framesOption).toInt(&ok);
    if (!ok || measuredFrames < 1) {
        qCritical("--frames must be a positive integer");
        return 1;
    }
    const int renderWidth = parser.value(widthOption).toInt(&ok);
    if (!ok || renderWidth < 1) {
        qCritical("--width must be a positive integer");
        return 1;
    }
    const int renderHeight = parser.value(heightOption).toInt(&ok);
    if (!ok || renderHeight < 1) {
        qCritical("--height must be a positive integer");
        return 1;
    }

    QDir                inputDirectory(positionalArguments.front());
    const QFileInfoList files =
        inputDirectory.entryInfoList({"*.bin", "*.BIN"}, QDir::Files | QDir::Readable, QDir::Name);
    if (files.isEmpty()) {
        qCritical().noquote() << "No BIN files found in" << inputDirectory.absolutePath();
        return 1;
    }

    auto view =
        GVis::Visualize::Views::ViewFactory::createView(GVis::Visualize::Views::ViewType::OpenGL3D);
    auto  view3d = std::dynamic_pointer_cast<GVis::Visualize::Views::AbstractView3D>(view);
    auto* viewport =
        view3d ? qobject_cast<GVis::Visualize::Views::View3D::GLViewport*>(view3d->getWidget())
               : nullptr;
    if (!viewport) {
        qCritical("Unable to create OpenGL 3D viewport");
        return 1;
    }

    QSurfaceFormat format = viewport->format();
    format.setSwapInterval(0);
    viewport->setFormat(format);
    viewport->setAttribute(Qt::WA_DontShowOnScreen, true);
    viewport->resize(renderWidth, renderHeight);
    view3d->setGridVisible(false);
    view3d->setAxisVisible(false);
    viewport->show();
    QCoreApplication::processEvents();

    if (!viewport->isValid()) {
        qCritical("Failed to create an OpenGL context. A hardware OpenGL 3.3 context is required.");
        return 1;
    }

    auto reader = GVis::IO::createReader(GVis::IO::FileFormat::BIN);
    if (!reader) {
        qCritical("Failed to create BIN reader");
        return 1;
    }
    reader->setPolicy(GVis::IO::FilePolicy::ForceStandart);

    QTextStream output(stdout);
    output << "Benchmarking " << files.size() << " file(s) at " << renderWidth << 'x'
           << renderHeight << ", " << measuredFrames << " resident frame(s), " << warmupFrames
           << " warm-up frame(s).\n";
    output << "First frame includes GL initialization, CPU-to-GPU upload, and rendering.\n";
    output << "Resident frames exclude file reading and GPU upload. Timings wait for GPU "
              "completion.\n\n";

    const auto presets  = GVis::IO::ReadSchemePresets::getStandardPresets();
    const auto schemeIt = std::find_if(presets.cbegin(), presets.cend(), [](const auto& preset) {
        return preset.first.type == GVis::Visualize::EntityType::DarkMatter &&
               preset.first.format == GVis::IO::FileFormat::BIN &&
               preset.first.mpv == GVis::IO::ModelingProgrammVersion::V2;
    });
    if (schemeIt == presets.cend()) {
        qCritical("DarkMatter BIN V2 scheme is not registered");
        return 1;
    }

    for (const QFileInfo& file : files) {
        QElapsedTimer readTimer;
        readTimer.start();
        auto         readResult = reader->read(file.absoluteFilePath(), schemeIt->second);
        const double readMs     = readTimer.nsecsElapsed() / 1'000'000.0;
        if (!readResult.isSuccess()) {
            output << file.fileName() << ": read failed\n";
            continue;
        }

        auto* polyData = vtkPolyData::SafeDownCast(readResult.data);
        if (!polyData || !polyData->GetPoints() ||
            polyData->GetPoints()->GetNumberOfPoints() == 0) {
            output << file.fileName() << ": no particle positions\n";
            continue;
        }

        const auto dataNode =
            std::make_shared<GVis::Core::DataNode>(vtkSmartPointer<vtkDataSet>(polyData),
                                                   file.fileName(),
                                                   0.0,
                                                   GVis::Visualize::EntityType::DarkMatter);
        auto renderLayer = GVis::Visualize::Layers::LayerFactory::createLayerRenderer(
            dataNode,
            GVis::Visualize::Layers::RenderLayerType::SPH);
        auto settings = std::make_shared<GVis::Visualize::Layers::SPHPointsLayerSettings>();
        settings->setColorByField("Mass");
        settings->setAutoRange(false);
        settings->setRangeMin(0.0);
        settings->setRangeMax(1.0);
        renderLayer->setData(dataNode);
        renderLayer->setSettings(settings);

        const QUuid   layerId = QUuid::createUuid();
        QElapsedTimer firstFrameTimer;
        firstFrameTimer.start();
        view3d->attachRenderLayer(layerId, renderLayer);
        const bool   firstFrameRendered = renderAndWait(*viewport);
        const double firstFrameMs       = firstFrameTimer.nsecsElapsed() / 1'000'000.0;
        if (!firstFrameRendered) {
            output << file.fileName() << ": OpenGL rendering failed\n";
            view3d->detachRenderLayer(layerId);
            continue;
        }

        bool warmupSucceeded = true;
        for (int frame = 0; frame < warmupFrames; ++frame) {
            if (!renderAndWait(*viewport)) {
                warmupSucceeded = false;
                break;
            }
        }
        if (!warmupSucceeded) {
            output << file.fileName() << ": OpenGL warm-up failed\n";
            view3d->detachRenderLayer(layerId);
            continue;
        }

        const FrameStatistics resident = measureResidentFrames(*viewport, measuredFrames);
        view3d->detachRenderLayer(layerId);
        renderLayer.reset();
        QCoreApplication::processEvents();

        output.setRealNumberNotation(QTextStream::FixedNotation);
        output.setRealNumberPrecision(3);
        output << file.fileName() << " | particles=" << polyData->GetPoints()->GetNumberOfPoints()
               << " | read=" << readMs << " ms"
               << " | first-frame=" << firstFrameMs << " ms"
               << " | resident=" << resident.averageMs << " ms (" << (1'000.0 / resident.averageMs)
               << " FPS, min=" << resident.minimumMs << " ms, max=" << resident.maximumMs
               << " ms)\n";
        output.flush();
    }

    return 0;
}
