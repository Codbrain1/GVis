#include "VideoController.h"
#include "Common/Logger/Logger.h"
#include "Core/LayerManager/LayerManager.h"
#include "Core/ObjectRegistry/ObjectRegistry.h"
#include "Visualize/Layers/Layer.h"
#include "Visualize/Views/View3D/AbstractView3D.h"
#include <qtmetamacros.h>
#include <quuid.h>
#include "Session/Reflection.h"
#include <memory>


namespace GVis::Core::Controllers {

VideoController::VideoController(Core::ObjectRegistry* objectRegistry,
                                 Core::ViewManager*    viewManager,
                                 Core::LayerManager*   layerManager,
                                 QObject*              parent)
    : QObject(parent),
      m_objectRegistry(objectRegistry),
      m_viewManager(viewManager),
      m_layerManager(layerManager) {
}

void VideoController::initialize() {
}

Session::VideoPlayerDTO VideoController::playerState() const {
    Session::VideoPlayerDTO state;
    state.experimentId        = m_targetExperimentId;
    state.referenceSnapshotId = m_referenceSnapshotId;
    state.currentSnapshotId   = m_currentVisualizeSnapshotId;

    for (const auto& layers : m_snapshotPlayerSate) {
        for (const auto& layer : layers) {
            if (!layer || !m_playerLayerSources.contains(layer->layerId()))
                continue;

            auto settings = layer->getSettings();
            if (!settings)
                continue;

            state.layers.append({m_playerLayerSources.value(layer->layerId()),
                                 Reflection::QObjectToVariantMap(settings.get())});
        }
    }
    return state;
}

QList<QUuid> VideoController::playerLayerIds() const {
    QList<QUuid> layerIds;
    for (const auto& layers : m_snapshotPlayerSate) {
        for (const auto& layer : layers) {
            if (layer)
                layerIds.append(layer->layerId());
        }
    }
    return layerIds;
}

void VideoController::clearPlayerLayers() {
    for (const auto& layers : m_snapshotPlayerSate) {
        for (const auto& layer : layers) {
            if (layer)
                m_layerManager->removeLayer(layer->layerId());
        }
    }
    m_snapshotPlayerSate.clear();
    m_playerLayerSources.clear();
}

void VideoController::handleNodeDataLoaded(const QUuid& nodeId) {
    restorePlayerLayersWhenReady();
    for (const auto& layers : m_snapshotPlayerSate) {
        for (const auto& layer : layers) {
            if (!layer || layer->dataNodeId() != nodeId)
                continue;

            layer->update();
        }
    }
}

void VideoController::handleTargetExperimentChange(const QUuid& experimentId) {
    // проверяем чт эксперимент существует
    if (!m_objectRegistry->containsExperiment(experimentId)) {
        qCWarning(LogCore) << "Experiment with QUuid: " << experimentId.toString()
                           << " don't exist!";
        return;
    }

    m_targetExperimentId      = experimentId;
    auto       experiment_ptr = m_objectRegistry->getExperiment(experimentId);
    const auto size           = experiment_ptr->snapshots.size();

    m_timeSliderSnapshots.clear();
    m_timeSliderSnapshots.reserve(size);

    // заполняем доступные для отображения снимки
    for (const auto& snapshot : experiment_ptr->snapshots)
        m_timeSliderSnapshots.emplace_back(snapshot->id, snapshot->timestamp);

    // сортируем по времени
    std::sort(m_timeSliderSnapshots.begin(), m_timeSliderSnapshots.end());

    if (m_timeSliderSnapshots.empty()) {
        qCWarning(LogCore) << "Experiment" << experimentId.toString() << "has no snapshots";
        return;
    }
    emit snapshotsListSizeChanged(int(size));

    // ---- по умолчанию эталон — первый снимок (как вы и просили) ----
    handleActivateVisualizeSnapshot(1);
    handleFixedEtalonSnapshot(m_timeSliderSnapshots[0].targetSnapshot);
}

void VideoController::handleFixedEtalonSnapshot(const QUuid& referenceSnapshotId) {
    auto snapshot = m_objectRegistry->getSnapshot(referenceSnapshotId);
    if (!snapshot || !snapshot->isLoaded()) {
        qCWarning(LogCore) << "VideoController: reference snapshot not found:"
                           << referenceSnapshotId;
        return;
    }
    clearPlayerLayers();
    m_referenceSnapshotId = referenceSnapshotId;

    // копируем настройки снимка для последующего повторения
    for (const auto& node : snapshot->components) {
        if (!node)
            continue;

        auto layers = m_layerManager->getLayersForNode(node->id);
        for (const auto& layer : layers) {
            if (!layer) {
                qCWarning(LogCore)
                    << "VideoController: failed to create layer for node" << node->id;
                continue;
            }
            auto newLayer = m_layerManager->createLayerCopy(layer->layerId());
            if (!newLayer) {
                qCWarning(LogCore)
                    << "VideoController: failed to create layer for node" << node->id;
                continue;
            }
            m_snapshotPlayerSate[node->type].append(newLayer);
            m_playerLayerSources.insert(newLayer->layerId(), layer->layerId());
            layer->setVisible(false);
        }
    }
    const auto restoredLayers = std::move(m_restoredPlayerLayers);
    for (const auto& restoredLayer : restoredLayers) {
        for (const auto& layers : m_snapshotPlayerSate) {
            for (const auto& layer : layers) {
                if (!layer ||
                    m_playerLayerSources.value(layer->layerId()) != restoredLayer.sourceLayerId)
                    continue;

                if (const auto settings = layer->getSettings()) {
                    const auto result =
                        Reflection::variantMapToQObject(restoredLayer.settings, settings.get());
                    if (!result.success) {
                        qCWarning(LogCore)
                            << "VideoController: failed to restore player layer settings"
                            << layer->layerId();
                        continue;
                    }
                    layer->setVisible(settings->isVisible());
                }
            }
        }
    }

    auto currentSnapshot = m_objectRegistry->getSnapshot(m_currentVisualizeSnapshotId);
    if (!currentSnapshot) {
        qCWarning(LogCore) << "VideoController: snapshot not found:"
                           << m_currentVisualizeSnapshotId;
        return;
    }

    QMap<GVis::Visualize::EntityType, std::shared_ptr<DataNode>> nodesByType;
    for (const auto& node : currentSnapshot->components) {
        if (node) {
            nodesByType.insert(node->type, node);
        }
    }

    // подменяем данные в уже существующих слоях — настройки (settings) не трогаем,
    // именно поэтому цвет/диапазон/радиус сглаживания и т.д. остаются как были
    for (auto it = m_snapshotPlayerSate.begin(); it != m_snapshotPlayerSate.end(); ++it) {
        auto nodeIt = nodesByType.find(it.key());
        if (nodeIt == nodesByType.end()) {
            qCWarning(LogCore) << "VideoController: snapshot" << m_currentVisualizeSnapshotId
                               << "has no component of type" << int(it.key());
            continue;
        }
        for (auto& layer : it.value())
            if (layer && nodeIt.value())
                layer->setData(nodeIt.value());
    }

    QList<QUuid> list;
    for (auto it = m_snapshotPlayerSate.begin(); it != m_snapshotPlayerSate.end(); ++it) {
        for (auto& layer : it.value())
            if (layer) {
                list.append(layer->layerId());
            }
    }
    emit changedEtalonLayer(list);
}

void VideoController::handleActivateVisualizeSnapshot(int index) {
    // index приходит 1-based из QSlider (диапазон задан как setRange(1, size))
    const int vecIndex = index - 1;
    if (vecIndex < 0 || vecIndex >= static_cast<int>(m_timeSliderSnapshots.size())) {
        qCWarning(LogCore) << "VideoController: slider index out of range:" << index;
        return;
    }
    if (m_timeSliderSnapshots.empty())
        return;

    const QUuid targetSnapshotId = m_timeSliderSnapshots[vecIndex].targetSnapshot;
    auto        snapshot         = m_objectRegistry->getSnapshot(targetSnapshotId);
    if (!snapshot) {
        qCWarning(LogCore) << "VideoController: snapshot not found:" << targetSnapshotId;
        return;
    }

    QMap<GVis::Visualize::EntityType, std::shared_ptr<DataNode>> nodesByType;
    for (const auto& node : snapshot->components) {
        if (node) {
            nodesByType.insert(node->type, node);
        }
    }

    // подменяем данные в уже существующих слоях — настройки (settings) не трогаем,
    // именно поэтому цвет/диапазон/радиус сглаживания и т.д. остаются как были
    for (auto it = m_snapshotPlayerSate.begin(); it != m_snapshotPlayerSate.end(); ++it) {
        auto nodeIt = nodesByType.find(it.key());
        if (nodeIt == nodesByType.end()) {
            qCWarning(LogCore) << "VideoController: snapshot" << targetSnapshotId
                               << "has no component of type" << int(it.key());
            continue;
        }
        for (auto& layer : it.value())
            if (layer) {
                layer->setData(nodeIt.value());
                layer->update();
            }
    }
    m_currentVisualizeSnapshotId = targetSnapshotId;
    requestLoadFloatWindow(index);
}

void VideoController::restorePlayerState(const Session::VideoPlayerDTO& state) {
    clearPlayerLayers();
    m_targetExperimentId         = state.experimentId;
    m_referenceSnapshotId        = state.referenceSnapshotId;
    m_currentVisualizeSnapshotId = state.currentSnapshotId;
    m_restoredPlayerLayers       = state.layers;
    m_timeSliderSnapshots.clear();
    m_restorePending = !m_targetExperimentId.isNull() && !m_referenceSnapshotId.isNull() &&
                       !m_currentVisualizeSnapshotId.isNull();

    if (!m_restorePending || !m_objectRegistry->containsExperiment(m_targetExperimentId)) {
        m_restorePending = false;
        m_restoredPlayerLayers.clear();
        return;
    }

    const auto experiment = m_objectRegistry->getExperiment(m_targetExperimentId);
    for (const auto& snapshot : experiment->snapshots) {
        if (snapshot)
            m_timeSliderSnapshots.emplace_back(snapshot->id, snapshot->timestamp);
    }
    std::sort(m_timeSliderSnapshots.begin(), m_timeSliderSnapshots.end());
    if (m_timeSliderSnapshots.empty()) {
        m_restorePending = false;
        m_restoredPlayerLayers.clear();
        return;
    }

    emit snapshotsListSizeChanged(static_cast<int>(m_timeSliderSnapshots.size()));
    restorePlayerLayersWhenReady();
}

void VideoController::restorePlayerLayersWhenReady() {
    if (!m_restorePending)
        return;

    const auto referenceSnapshot = m_objectRegistry->getSnapshot(m_referenceSnapshotId);
    if (!referenceSnapshot || !referenceSnapshot->isLoaded())
        return;

    m_restorePending = false;
    handleFixedEtalonSnapshot(m_referenceSnapshotId);

    const auto currentIndex =
        std::find_if(m_timeSliderSnapshots.cbegin(),
                     m_timeSliderSnapshots.cend(),
                     [this](const SnapshotToTimestamp& snapshot) {
                         return snapshot.targetSnapshot == m_currentVisualizeSnapshotId;
                     });
    if (currentIndex != m_timeSliderSnapshots.cend())
        handleActivateVisualizeSnapshot(
            static_cast<int>(std::distance(m_timeSliderSnapshots.cbegin(), currentIndex)) + 1);
}

void VideoController::requestLoadFloatWindow(int index) {
    if (m_timeSliderSnapshots.empty()) {
        return;
    }

    const int size = static_cast<int>(m_timeSliderSnapshots.size());

    // 1. Безопасно вычисляем границы (от 0 до size - 1)
    const int startIndex = std::max(0, index - 5);
    const int endIndex   = std::min(size - 1, index + 5);

    // 3. Извлекаем нужные ID (targetSnapshot)
    for (int i = startIndex; i <= endIndex; ++i) {
        auto snapshot = m_objectRegistry->getSnapshot(m_timeSliderSnapshots[i].targetSnapshot);
        if (!snapshot)
            continue;

        for (const auto& node : snapshot->components) {
            if (node && node->data == nullptr) {
                emit requestNodeLoad(node->id);
            }
        }
    }
}
} // namespace GVis::Core::Controllers