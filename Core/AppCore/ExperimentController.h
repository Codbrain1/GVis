#pragma once

#include "Common/Structures/ObjectRegistryStructures.h"
#include "Common/Structures/PhysicalStructures.h"
#include <QList>
#include <QObject>
#include <memory>


namespace GVis::Core {
class ObjectRegistry;
class LayerManager;
class ViewManager;
} // namespace GVis::Core

namespace GVis::Core::Controllers {

class ExperimentController final : public QObject {
    Q_OBJECT
  public:
    explicit ExperimentController(Core::ObjectRegistry* objectRegistry,
                                  Core::LayerManager*   layerManager,
                                  Core::ViewManager*    viewManager,
                                  QObject*              parent = nullptr);

    std::shared_ptr<Core::Experiment>        getExperimentById(const QUuid& id) const;
    QList<std::shared_ptr<Core::Experiment>> getExperiments() const;
    bool updatePhysicalUnits(const QUuid& id, const Physics::PhysicalUnitsProfile& profile);

  signals:
    void experimentChanged(const QUuid& experimentId);
    void markSessionDirty();

  private:
    Core::ObjectRegistry* m_objectRegistry;
    Core::LayerManager*   m_layerManager;
    Core::ViewManager*    m_viewManager;
};

} // namespace GVis::Core::Controllers
