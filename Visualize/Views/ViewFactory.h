#pragma once
#include "Common/Enums/ViewEnums.h"
#include "views_export.h"

namespace GVis::Visualize::Views {
class AbstractView;

namespace ViewFactory {

VIEWS_EXPORT std::shared_ptr<AbstractView> createView(ViewType type, QObject* parent = nullptr);

}; // namespace ViewFactory
} // namespace GVis::Visualize::Views