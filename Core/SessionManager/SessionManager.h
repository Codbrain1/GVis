#pragma once
#include "Core/LayerManager/LayerManager.h"
#include "Core/ObjectRegistry/ObjectRegistry.h"
#include "Core/ViewManager/ViewManager.h"
#include <QObject>
#include "Session/SessionStorage.h"
#include "Structures/SessionStructures.h"
#include <functional>
#include <memory>
#include <optional>

namespace GVis::Core {

class SessionManager : public QObject { // Исправлена опечатка в имени
    Q_OBJECT
  public:
    explicit SessionManager(GVis::Core::ObjectRegistry* registry,
                            GVis::Core::LayerManager*   layerManager,
                            GVis::Core::ViewManager*    viewManager,
                            QObject*                    parent = nullptr);
    // Сохранение и загрузка проекта
    bool saveProject(const QString& filePath, GVis::Session::ProjectFormat format);
    bool loadProject(const QString& filePath);
    void
    setVideoPlayerStateHandlers(std::function<Session::VideoPlayerDTO()>            saveState,
                                std::function<QList<QUuid>()>                       playerLayerIds,
                                std::function<void(const Session::VideoPlayerDTO&)> restoreState);

    // сохранение и загрузка палитр
    bool savePalette(const Visualize::ColorMap& map, const QString& filePath);
    std::optional<Visualize::ColorMap> loadPalette(const QString& filePath);

  signals:
    [[deprecated]]
    void projectLoaded(const GVis::Session::ProjectState& state);
    void errorOccurred(const QString& msg);

  private:
    GVis::Session::ProjectDTO                createProjectDTO() const;
    bool                                     fromProjectDTO(const GVis::Session::ProjectDTO& dto);
    GVis::Core::ObjectRegistry*              m_registry;
    GVis::Core::LayerManager*                m_layerManager;
    GVis::Core::ViewManager*                 m_viewManager;
    std::function<Session::VideoPlayerDTO()> m_saveVideoPlayerState;
    std::function<QList<QUuid>()>            m_playerLayerIds;
    std::function<void(const Session::VideoPlayerDTO&)> m_restoreVideoPlayerState;
    std::unique_ptr<GVis::Session::SessionStorage>      m_storage_session;
};
} // namespace GVis::Core