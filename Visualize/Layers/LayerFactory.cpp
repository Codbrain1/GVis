#include "LayerFactory.h"
#include "Common/Enums/LayerEnums.h"
#include "Implementations/Binning/BinningRendererLayer.h"
#include "Implementations/Particle/ParticleRendererLayer.h"
#include "Implementations/SPH/SPHRendererLayer.h"

namespace GVis::Visualize::Layers::LayerFactory {

namespace {} // namespace

std::shared_ptr<IRenderLayer> createLayerRenderer(const std::shared_ptr<Core::DataNode> node,
                                                  Layers::RenderLayerType               type) {
    if (!node) {
        qWarning() << "LayerFactory::createLayerRenderer: node is null";
        return nullptr;
    }

    switch (type) {
        case Layers::RenderLayerType::Particles: {
            auto layer = std::make_shared<ParticleRendererLayer>();
            layer->setData(node);
            layer->setSettings(std::make_shared<ParticlePointsLayerSettings>());
            return layer;
        }
        case Layers::RenderLayerType::SPH: {
            auto layer = std::make_shared<SPHRendererLayer>();
            layer->setData(node);
            layer->setSettings(std::make_shared<SPHPointsLayerSettings>());
            return layer;
        }
        case Layers::RenderLayerType::Binning: {
            auto layer = std::make_shared<BinningRendererLayer>();
            layer->setData(node);
            layer->setSettings(std::make_shared<BinningPointsLayerSettings>());
            return layer;
        }
    }

    qWarning() << "LayerFactory::createLayerRenderer: unknown RenderLayerType";
    return nullptr;
}

std::shared_ptr<IRenderLayer> createLayerRenderer(Layers::RenderLayerType type) {
    switch (type) {
        case Layers::RenderLayerType::Particles: {
            auto layer = std::make_shared<ParticleRendererLayer>();
            layer->setSettings(std::make_shared<ParticlePointsLayerSettings>());
            return layer;
        }
        case Layers::RenderLayerType::SPH: {
            auto layer = std::make_shared<SPHRendererLayer>();
            layer->setSettings(std::make_shared<SPHPointsLayerSettings>());
            return layer;
        }
        case Layers::RenderLayerType::Binning: {
            auto layer = std::make_shared<BinningRendererLayer>();
            layer->setSettings(std::make_shared<BinningPointsLayerSettings>());
            return layer;
        }
    }

    qWarning() << "LayerFactory::createLayerRenderer: unknown RenderLayerType";
    return nullptr;
}
} // namespace GVis::Visualize::Layers::LayerFactory