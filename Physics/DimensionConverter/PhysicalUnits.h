#pragma once
#include <cmath>

namespace GVis::Physics {

// Коэффициенты перевода "код-единиц" симуляции в физические единицы.
// Формулы взяты из референсной C++/Python программы обработки (l_v, l_r, l_s, l_rho, l_t).
struct PhysicalUnits {
    double lengthScale         = 1.0; // l_r,   kpc  на 1 код-единицу длины
    double velocityScale       = 1.0; // l_v,   km/s
    double surfaceDensityScale = 1.0; // l_s,   Msun/pc^2 (для Extensive-полей: Mass)
    double volumeDensityScale  = 1.0; // l_rho, Msun/pc^3 (для Intensive-поля Density)
    double timeScale           = 1.0; // l_t,   Myr
    double temperatureScale    = 1.0; // l_Te,  K (валиден только если hasTemperature)
    bool   hasTemperature      = false;

    static PhysicalUnits identity() {
        return PhysicalUnits{};
    }

    // Km — x10^10 Msun, Kr — x10 kpc (как в исходных .ini)
    static PhysicalUnits
    fromSimParams(double Km, double Kr, double gamma = 0.0, bool haveGamma = false) {
        PhysicalUnits u;
        u.velocityScale       = 65.76 * std::sqrt(Km / Kr);
        u.lengthScale         = 10.0 * Kr;
        u.surfaceDensityScale = Km / (Kr * Kr) * 100.0;
        u.volumeDensityScale  = Km / (Kr * Kr * Kr) * 0.01;
        u.timeScale           = u.lengthScale / u.velocityScale * 1000.0 * 0.9784;

        if (haveGamma) {
            const double gamma1 = gamma - 1.0;
            u.temperatureScale =
                gamma * gamma1 * 10000.0 / (100.0 / (u.velocityScale * u.velocityScale));
            u.hasTemperature = true;
        }
        return u;
    }
};

// Физический масштаб конкретного поля - определяет, во что домножать
// итоговое значение перед выводом (см. таблицу переводов из PhysicalUnits).
// Синхронизирован с fieldNormolized(): каждому известному полю - своя размерность.
inline double fieldPhysicalScale(const Physics::PhysicalUnits& units, const QString& fieldName) {
    if (fieldName == "Mass")
        return units.surfaceDensityScale; // Extensive -> Msun/pc^2
    if (fieldName == "Density")
        return units.volumeDensityScale; // Intensive -> Msun/pc^3
    if (fieldName == "Velocity")
        return units.velocityScale; // Intensive -> km/s
    if (fieldName == "Energy" && units.hasTemperature)
        return units.temperatureScale; // Intensive -> K

    return 1.0;
}

} // namespace GVis::Physics