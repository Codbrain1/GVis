#pragma once
#include "Common/Structures/ObjectRegistryStructures.h"

namespace GVis::Physics::Math {
GVis::Core::DataNodeMetaData calculateMetaData(vtkSmartPointer<vtkDataSet> dataSet,
                                               double                      timestamp);
double                       calculateTimestamp(double timestamp);
} // namespace GVis::Physics::Math