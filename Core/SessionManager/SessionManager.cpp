#include "SessionManager.h"
#include "Common/Structures/SessionStructures.h"
#include "Core/LayerManager/LayerManagerSerializer.h"
#include "Core/ObjectRegistry/ObjectRegistrySerializer.h"
#include "Core/ViewManager/ViewManager.h"
#include "Core/ViewManager/ViewManagerSerializer.h"
#include "Visualize/ColorMapManager/ColorMapManagerSerializer.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <qfileinfo.h>
#include <qloggingcategory.h>
#include <qstringview.h>
#include "Logger/Logger.h"
#include "Session/ProjectFormatFactory.h"
#include "Session/Reflection.h"
#include <algorithm>


namespace GVis::Core {
namespace {
QJsonObject colorMapToJson(const GVis::Visualize::ColorMap& map) {
    QJsonObject mapJson;
    mapJson.insert(QStringLiteral("id"), map.id.toString());
    mapJson.insert(QStringLiteral("name"), map.name);
    mapJson.insert(QStringLiteral("filePath"), map.filePath);
    mapJson.insert(QStringLiteral("isPreset"), map.isPreset);

    QJsonArray pointsJson;
    for (const auto& point : map.points) {
        QJsonObject pointJson;
        pointJson.insert(QStringLiteral("x"), point.x);
        pointJson.insert(QStringLiteral("r"), point.r);
        pointJson.insert(QStringLiteral("g"), point.g);
        pointJson.insert(QStringLiteral("b"), point.b);
        pointsJson.append(pointJson);
    }
    mapJson.insert(QStringLiteral("points"), pointsJson);
    return mapJson;
}

std::optional<GVis::Visualize::ColorMap> colorMapFromJson(const QJsonObject& json) {
    if (!json.contains(QStringLiteral("id")) || !json.contains(QStringLiteral("name")) ||
        !json.contains(QStringLiteral("points"))) {
        return std::nullopt;
    }

    GVis::Visualize::ColorMap map;
    map.id       = QUuid::fromString(json.value(QStringLiteral("id")).toString());
    map.name     = json.value(QStringLiteral("name")).toString();
    map.filePath = json.value(QStringLiteral("filePath")).toString();
    map.isPreset = json.value(QStringLiteral("isPreset")).toBool(false);

    const QJsonArray pointsJson = json.value(QStringLiteral("points")).toArray();
    map.points.reserve(pointsJson.size());
    for (const auto& pointValue : pointsJson) {
        if (!pointValue.isObject()) {
            return std::nullopt;
        }
        const QJsonObject           pointJson = pointValue.toObject();
        GVis::Visualize::ColorPoint point;
        point.x = pointJson.value(QStringLiteral("x")).toDouble();
        point.r = pointJson.value(QStringLiteral("r")).toDouble();
        point.g = pointJson.value(QStringLiteral("g")).toDouble();
        point.b = pointJson.value(QStringLiteral("b")).toDouble();
        map.points.append(point);
    }

    if (map.id.isNull() || map.name.isEmpty()) {
        return std::nullopt;
    }
    return map;
}
} // namespace

SessionManager::SessionManager(GVis::Core::ObjectRegistry* registry,
                               GVis::Core::LayerManager*   layerManager,
                               GVis::Core::ViewManager*    viewManager,
                               QObject*                    parent)
    : QObject(parent),
      m_registry(registry),
      m_layerManager(layerManager),
      m_viewManager(viewManager) {
    // ВАЖНО: Инициализируем хранилище, иначе будет краш!
    m_storage_session = std::make_unique<GVis::Session::SessionStorage>();
}

void SessionManager::setVideoPlayerStateHandlers(
    std::function<Session::VideoPlayerDTO()>            saveState,
    std::function<QList<QUuid>()>                       playerLayerIds,
    std::function<void(const Session::VideoPlayerDTO&)> restoreState) {
    m_saveVideoPlayerState    = std::move(saveState);
    m_playerLayerIds          = std::move(playerLayerIds);
    m_restoreVideoPlayerState = std::move(restoreState);
}

// ===================================================
// savePallete() -- сохраняет палитру
// loadPallete() -- загружает палитру
// ===================================================
// TODO: переписать на Reflection
bool SessionManager::savePalette(const Visualize::ColorMap& map, const QString& filePath) {
    if (filePath.isEmpty()) {
        emit errorOccurred("Palette file path is empty");
        return false;
    }

    const QJsonObject json = colorMapToJson(map);
    const QByteArray  data = QJsonDocument(json).toJson(QJsonDocument::Indented);
    return m_storage_session->save(filePath, data);
}

// TODO переписать на Reflection
std::optional<Visualize::ColorMap> SessionManager::loadPalette(const QString& filePath) {
    if (filePath.isEmpty()) {
        emit errorOccurred("Palette file path is empty");
        return std::nullopt;
    }

    const auto data = m_storage_session->load(filePath);
    if (!data.has_value()) {
        emit errorOccurred("Failed to load palette file");
        return std::nullopt;
    }

    QJsonParseError parseError;
    const auto      doc = QJsonDocument::fromJson(*data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        emit errorOccurred("Failed to parse palette file");
        return std::nullopt;
    }

    auto colorMap = colorMapFromJson(doc.object());
    if (!colorMap.has_value()) {
        emit errorOccurred("Palette file has invalid schema");
    }
    return colorMap;
}

// ===================================================
// saveProject() -- создает DTO и сохраняет проект
// loadProject() -- загружает DTO и восстанавливает состояние приложения
// ===================================================
bool SessionManager::saveProject(const QString& filePath, GVis::Session::ProjectFormat format) {
    if (filePath.isEmpty()) {
        emit errorOccurred("Project file path is empty");
        return false;
    }

    const GVis::Session::ProjectDTO dto     = createProjectDTO();
    const auto                      dataMap = GVis::Reflection::gadgetToVariantMap(dto);
    const auto                      codec   = GVis::Session::ProjectFormatFactory::create(format);
    if (!codec) {
        emit errorOccurred("Unsupported project format");
        return false;
    }
    const bool saved = m_storage_session->save(filePath, codec->encode(dataMap));
    if (!saved) {
        emit errorOccurred("Failed to save project file");
    }
    return saved;
}

bool SessionManager::loadProject(const QString& filePath) {
    if (filePath.isEmpty()) {
        emit errorOccurred("Project file path is empty");
        return false;
    }

    const auto format = GVis::Session::ProjectFormatFactory::fromFilePath(".json");
    if (!format.has_value()) {
        emit errorOccurred("Unsupported project format");
        return false;
    }

    const auto codec = GVis::Session::ProjectFormatFactory::create(*format);
    if (!codec) {
        emit errorOccurred("Unsupported project format");
        return false;
    }

    const auto data = m_storage_session->load(filePath);
    if (!data.has_value()) {
        emit errorOccurred("Failed to load project file");
        return false;
    }

    const auto map = codec->decode(*data);
    if (!map.has_value()) {
        emit errorOccurred("Failed to decode project file");
        return false;
    }

    static const QStringList requiredFields = {QStringLiteral("objregDTO"),
                                               QStringLiteral("viewmanDTO"),
                                               QStringLiteral("layermanDTO"),
                                               QStringLiteral("colormapmanDTO")};
    for (const auto& field : requiredFields) {
        if (!map->contains(field)) {
            emit errorOccurred("Project file has invalid schema");
            return false;
        }
    }

    const auto dto = GVis::Reflection::variantMapToGadget<GVis::Session::ProjectDTO>(*map);
    if (!fromProjectDTO(dto)) {
        emit errorOccurred("Failed to restore project from DTO");
        return false;
    }

    emit projectLoaded(GVis::Session::ProjectState{});
    return true;
}

GVis::Session::ProjectDTO SessionManager::createProjectDTO() const {
    GVis::Session::ProjectDTO dto;

    if (m_registry) {
        if (const auto registryDto = ObjectRegistrySerializer::toDTO(m_registry))
            dto.objregDTO = *registryDto;
    }
    if (m_layerManager) {
        if (const auto layerDto = LayerManagerSerializer::toDTO(m_layerManager))
            dto.layermanDTO = *layerDto;
    }
    if (m_saveVideoPlayerState && m_playerLayerIds) {
        dto.videoPlayerDTO                 = m_saveVideoPlayerState();
        const QList<QUuid> playerLayerList = m_playerLayerIds();
        const QSet<QUuid>  playerLayerIds(playerLayerList.cbegin(), playerLayerList.cend());
        dto.layermanDTO.layers.erase(
            std::remove_if(dto.layermanDTO.layers.begin(),
                           dto.layermanDTO.layers.end(),
                           [&playerLayerIds](const Session::LayerDTO& layer) {
                               return playerLayerIds.contains(layer.layerId);
                           }),
            dto.layermanDTO.layers.end());
    }
    if (m_viewManager) {
        if (const auto viewDto = ViewManagerSerializer::toDTO(m_viewManager))
            dto.viewmanDTO = *viewDto;
    }
    dto.colormapmanDTO = GVis::Visualize::ColorMapManagerSerializer::toDTO();

    return dto;
}

bool SessionManager::fromProjectDTO(const GVis::Session::ProjectDTO& dto) {
    if (!m_registry || !m_layerManager || !m_viewManager)
        return false;

    // Validate references before mutating the live managers. This keeps a failed
    // restore from leaving a partially restored project behind.
    for (const auto& layer : dto.layermanDTO.layers) {
        const bool hasNode = std::any_of(dto.objregDTO.nodes.cbegin(),
                                         dto.objregDTO.nodes.cend(),
                                         [&](const auto& node) { return node.id == layer.nodeId; });
        const bool hasView = std::any_of(dto.viewmanDTO.views.cbegin(),
                                         dto.viewmanDTO.views.cend(),
                                         [&](const auto& view) { return view.id == layer.viewId; });
        if (!hasNode || !hasView)
            return false;
    }

    // Сначала удаляем старые слои: они владеют ссылками на старые view и
    // должны отсоединиться от OpenGL/Qt до уничтожения представлений.
    m_layerManager->clear();

    bool is_succses = ObjectRegistrySerializer::fromDTO(dto.objregDTO, m_registry);
    is_succses      = is_succses && ViewManagerSerializer::fromDTO(dto.viewmanDTO, m_viewManager);
    GVis::Visualize::ColorMapManagerSerializer::fromDTO(dto.colormapmanDTO);

    LayerManagerSerializer::NodeResolver nodeRes =
        [&](const QUuid& id) -> std::shared_ptr<DataNode> { return m_registry->getNode(id); };

    LayerManagerSerializer::ViewResolver viewRes =
        [&](const QUuid& id) -> std::shared_ptr<GVis::Visualize::Views::AbstractView> {
        return m_viewManager->getView(id);
    };

    is_succses = is_succses &&
                 LayerManagerSerializer::fromDTO(dto.layermanDTO, m_layerManager, nodeRes, viewRes);
    if (is_succses && m_restoreVideoPlayerState)
        m_restoreVideoPlayerState(dto.videoPlayerDTO);
    return is_succses;
}
} // namespace GVis::Core