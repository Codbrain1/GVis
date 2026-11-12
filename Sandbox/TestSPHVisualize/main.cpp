#include "Common/Enums/LayerEnums.h"
#include "Common/Structures/ObjectRegistryStructures.h"
#include "Visualize/Layers/Implementations/Binning/BinningPointsLayerSettings.h"
#include "Visualize/Layers/Implementations/Particle/ParticlePointsLayerSettings.h"
#include "Visualize/Layers/Implementations/SPH/SPHPointsLayerSettings.h"
#include "Visualize/Layers/LayerFactory.h"
#include "Visualize/Views/View3D/OpenGL3DWidget/OpenGL3DWidget.h"
#include "Visualize/Views/ViewFactory.h"
#include "IO/ReaderFactory.h"
#include "IO/SchemeFactory.h"
#include <clocale>
#include <locale.h>


#include <QApplication>
#include <QElapsedTimer>
#include <QMainWindow>
#include <vtkPoints.h>
#include <vtkPolyData.h>

int main(int argc, char* argv[]) {
    std::setlocale(LC_ALL, "Rus");
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts,
                               true); // до QApplication — многооконность
    QApplication app(argc, argv);

    const QString dataPath = "D:/NIR/NIR_6_semestr/GVis/Sandbox/TEST_DATA/TF250_v2/G_    0.bin";

    // ================ Чтение файла — без изменений от вашего пайплайна ================
    auto reader = GVis::IO::createReader(GVis::IO::FileFormat::BIN);
    if (!reader) {
        qCritical() << "Failed to create reader";
        return 1;
    }
    reader->setPolicy(GVis::IO::FilePolicy::ForceStandart);

    auto scheme = GVis::IO::SchemeFactory::createScheme_v2(GVis::Visualize::EntityType::Gas,
                                                           GVis::IO::FileFormat::BIN);

    QElapsedTimer readTimer;
    readTimer.start();
    auto readResult = reader->read(dataPath, scheme);
    qDebug().noquote() << QString("Чтение файла заняло %1 мс").arg(readTimer.elapsed());

    if (!readResult.isSuccess()) {
        qCritical() << "Failed to read data";
        return 1;
    }

    vtkPolyData* polyData = vtkPolyData::SafeDownCast(readResult.data);
    if (!polyData || !polyData->GetPoints() || polyData->GetPoints()->GetNumberOfPoints() <= 0) {
        qCritical() << "Invalid polyData or empty points";
        return 1;
    }
    qDebug() << "Точек прочитано:" << polyData->GetPoints()->GetNumberOfPoints();

    // ================ Оборачиваем в DataNode — узел модели данных ================
    auto dataNode = std::make_shared<GVis::Core::DataNode>(
        vtkSmartPointer<vtkDataSet>(polyData), // vtkPolyData наследует vtkDataSet
        "DarkMatter snapshot 0",
        0.0,
        GVis::Visualize::EntityType::Gas);

    // ================ Создаём рендер-слой через LayerFactory ================
    auto renderLayer = GVis::Visualize::Layers::LayerFactory::createLayerRenderer(
        dataNode,
        GVis::Visualize::Layers::RenderLayerType::SPH);

    if (!renderLayer) {
        qCritical() << "Failed to create render layer";
        return 1;
    }

    // ---- настройки слоя — конкретно ParticlePointsLayerSettings ----
    auto layerSettings = std::make_shared<GVis::Visualize::Layers::SPHPointsLayerSettings>();
    layerSettings->setColorByField(
        "Density"); // если поля Mass/Density нет — просто одноцветные точки
    layerSettings->setUseLogScale(false);

    renderLayer->setData(dataNode);
    renderLayer->setSettings(layerSettings);

    // ================ Создаём View через ViewFactory ================
    auto view =
        GVis::Visualize::Views::ViewFactory::createView(GVis::Visualize::Views::ViewType::OpenGL3D);
    if (!view) {
        qCritical() << "Failed to create view";
        return 1;
    }

    auto view3d = std::dynamic_pointer_cast<GVis::Visualize::Views::AbstractView3D>(view);
    if (!view3d) {
        qCritical() << "View is not a 3D view";
        return 1;
    }

    // ---- привязка слоя к окну через layerId (см. наше обсуждение LayerManager) ----
    const QUuid layerId = QUuid::createUuid();
    view3d->attachRenderLayer(layerId, renderLayer);

    // ---- настройки сцены: включаем сетку/оси, чтобы было видно ориентацию ----
    view3d->sceneSettings().grid()->setVisible(true);
    view3d->sceneSettings().axis()->setVisible(true);
    view3d->sceneSettings().axis()->setUnitLabel("kpc");

    // ================ Отображаем окно ================
    QMainWindow window;
    window.setCentralWidget(view->getWidget());
    window.resize(1024, 768);
    window.setWindowTitle("SPH Particles Test — Dark Matter");
    window.show();

    return app.exec();
}