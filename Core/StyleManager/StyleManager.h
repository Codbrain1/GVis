#pragma once
#include "Core/LayerManager/LayerManager.h"
#include "Visualize/Layers/Layer.h"
#include <memory>

namespace GVis::Core {

class StyleManager {
  public:
    // Сценарий 1: Привязать слой к общим настройкам узла (Master)
    static void syncLayerWithMaster(std::shared_ptr<Visualize::Layers::Layer> layer,
                                    std::shared_ptr<DataNode>                 node);

    // Сценарий 2: Сделать настройки слоя уникальными (отвязать)
    static void makeLayerUnique(std::shared_ptr<Visualize::Layers::Layer> layer);

    // Сценарий 4: Скопировать стиль с одного слоя на другой
    static void copyStyle(std::shared_ptr<Visualize::Layers::Layer> source,
                          std::shared_ptr<Visualize::Layers::Layer> target,
                          std::shared_ptr<DataNode>                 targetNode);
};

} // namespace GVis::Core