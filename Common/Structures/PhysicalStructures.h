#pragma once
#include <QMetaType>

namespace GVis::Physics {
struct PhysicalUnitsProfile {
    Q_GADGET
    Q_PROPERTY(double Km MEMBER Km)
    Q_PROPERTY(double Kr MEMBER Kr)
    Q_PROPERTY(double gamma MEMBER gamma)
    Q_PROPERTY(bool haveGamma MEMBER haveGamma)

  public:
    double Km        = 3.72;
    double Kr        = 0.9;
    double gamma     = 5.0 / 3.0;
    bool   haveGamma = true;
};

} // namespace GVis::Physics