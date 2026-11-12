#pragma once
#include "Common/Structures/ColormapPresets.h"
#include <QColor>
#include <QMetaProperty>
#include <QObject>
#include <QString>
#include <QUuid>
#include <QVariantMap>
#include "LayersExport.h"
#include <algorithm>
#include <limits>

namespace GVis::Visualize::Layers {

class LAYERS_EXPORT LayerSettings : public QObject {
    Q_OBJECT
  public:
    enum class BlendMode { Alpha = 0, Additive = 1 };
    Q_ENUM(BlendMode)

    struct NumericConstraints {
        double min      = -1000000.0;
        double max      = 1000000.0;
        double step     = 1.0;
        int    decimals = 2;
    };

  private:
    // clang-format off
    Q_PROPERTY(bool          isVisible     READ isVisible     WRITE setVisible       NOTIFY changed)
    Q_PROPERTY(double        opacity       READ opacity       WRITE setOpacity       NOTIFY changed)
    Q_PROPERTY(QString       colorByField  READ colorByField  WRITE setColorByField  NOTIFY changed)
    Q_PROPERTY(QUuid         colorMapId    READ colorMapId    WRITE setColorMapId    NOTIFY changed)
    Q_PROPERTY(bool          useLogScale   READ useLogScale   WRITE setUseLogScale   NOTIFY changed)
    Q_PROPERTY(double        rangeMin      READ rangeMin      WRITE setRangeMin      NOTIFY changed)
    Q_PROPERTY(double        rangeMax      READ rangeMax      WRITE setRangeMax      NOTIFY changed)
    Q_PROPERTY(double        baseRangeMin  READ baseRangeMin                         NOTIFY calculatedRangeChanged)
    Q_PROPERTY(double        baseRangeMax  READ baseRangeMax                         NOTIFY calculatedRangeChanged)
    Q_PROPERTY(bool          autoRange     READ autoRange     WRITE setAutoRange     NOTIFY changed)
    Q_PROPERTY(bool          showOutRangeParticles READ showOutOfRangeParticles WRITE setShowOutOfRangeParticles NOTIFY changed)
    Q_PROPERTY(int           zOrder        READ zOrder        WRITE setZOrder        NOTIFY changed)
    Q_PROPERTY(BlendMode     blendMode     READ blendMode     WRITE setBlendMode     NOTIFY changed)
    Q_PROPERTY(bool show_all_particles  READ showAllParticles    WRITE setShowAllParticles    NOTIFY changed)
    Q_PROPERTY(int  particle_number_min READ particleNumberMin  WRITE setParticleNumberMin  NOTIFY changed)
    Q_PROPERTY(int  particle_number_max READ particleNumberMax  WRITE setParticleNumberMax  NOTIFY changed)
    // clang-format on
  public:
    explicit LayerSettings(QObject* parent = nullptr) : QObject(parent) {
        m_colorMapId = Visualize::ColorMapPresets::getPresetByName("Plasma").id;
    }

    virtual ~LayerSettings() = default;

    bool isVisible() const {
        return m_isVisible;
    }

    void setVisible(bool v) {
        if (v != m_isVisible) {
            m_isVisible = v;
            emit changed();
        }
    }

    double opacity() const {
        return m_opacity;
    }

    void setOpacity(double o) {
        if (!qFuzzyCompare(o, m_opacity)) {
            m_opacity = o;
            emit changed();
        }
    }

    QString colorByField() const {
        return m_colorByField;
    }

    void setColorByField(const QString& f) {
        if (f != m_colorByField) {
            m_colorByField = f;
            emit changed();
        }
    }

    QUuid colorMapId() const {
        return m_colorMapId;
    }

    void setColorMapId(const QUuid& id) {
        if (id != m_colorMapId) {
            m_colorMapId = id;
            emit changed();
        }
    }

    bool useLogScale() const {
        return m_useLogScale;
    }

    void setUseLogScale(bool u) {
        if (u != m_useLogScale) {
            m_useLogScale = u;
            emit changed();
        }
    }

    double rangeMin() const {
        return m_rangeMin;
    }

    void setRangeMin(double v) {
        if (!qFuzzyCompare(v, m_rangeMin)) {
            m_rangeMin = v;
            emit changed();
        }
    }

    double rangeMax() const {
        return m_rangeMax;
    }

    void setRangeMax(double v) {
        if (!qFuzzyCompare(v, m_rangeMax)) {
            m_rangeMax = v;
            emit changed();
        }
    }

    double baseRangeMin() const {
        return m_baseRangeMin;
    }

    void setBaseRangeMin(double v) {
        if (!qFuzzyCompare(v, m_baseRangeMin)) {
            m_baseRangeMin = v;
            emit changed();
        }
    }

    double baseRangeMax() const {
        return m_baseRangeMax;
    }

    void setBaseRangeMax(double v) {
        if (!qFuzzyCompare(v, m_baseRangeMax)) {
            m_baseRangeMax = v;
            emit changed();
        }
    }

    // Обновляет диапазон, рассчитанный рендерером после проецирования на FBO.
    // Это не пользовательское изменение настроек: общий сигнал changed()
    // не испускается, поэтому Layer::update() не запускает рекурсивный рендер.
    void setCalculatedRange(double min, double max) {
        const bool minChanged = !qFuzzyCompare(m_baseRangeMin, min);
        const bool maxChanged = !qFuzzyCompare(m_baseRangeMax, max);
        if (!minChanged && !maxChanged)
            return;

        m_baseRangeMin = min;
        m_baseRangeMax = max;
        QMetaObject::invokeMethod(
            this,
            [this]() { emit calculatedRangeChanged(); },
            Qt::QueuedConnection);
    }

    bool autoRange() const {
        return m_autoRange;
    }

    double physicalKm() const {
        return m_physicalKm;
    }

    double physicalKr() const {
        return m_physicalKr;
    }

    double physicalGamma() const {
        return m_physicalGamma;
    }

    bool hasPhysicalGamma() const {
        return m_hasPhysicalGamma;
    }

    void setPhysicalUnitsParameters(double Km, double Kr, double gamma, bool haveGamma) {
        if (m_physicalKm != Km || m_physicalKr != Kr || m_physicalGamma != gamma ||
            m_hasPhysicalGamma != haveGamma) {
            m_physicalKm       = Km;
            m_physicalKr       = Kr;
            m_physicalGamma    = gamma;
            m_hasPhysicalGamma = haveGamma;
            emit changed();
        }
    }

    void setAutoRange(bool a) {
        if (a != m_autoRange) {
            m_autoRange = a;
            emit changed();
        }
    }

    bool showOutOfRangeParticles() const {
        return m_showOutOfRangeParticles;
    }

    void setShowOutOfRangeParticles(bool value) {
        if (m_showOutOfRangeParticles != value) {
            m_showOutOfRangeParticles = value;
            emit changed();
        }
    }

    int zOrder() const {
        return m_zOrder;
    }

    void setZOrder(int z) {
        if (m_zOrder != z) {
            m_zOrder = z;
            emit changed();
        }
    }

    BlendMode blendMode() const {
        return m_blendMode;
    }

    void setBlendMode(BlendMode m) {
        if (m_blendMode != m) {
            m_blendMode = m;
            emit changed();
        }
    }

    bool showAllParticles() const {
        return m_showAllParticles;
    }

    void setShowAllParticles(bool value) {
        if (m_showAllParticles != value) {
            m_showAllParticles = value;
            emit changed();
        }
    }

    int particleNumberMin() const {
        return m_particleNumberMin;
    }

    void setParticleNumberMin(int value) {
        value = std::max(0, value);
        if (m_particleNumberMin == value)
            return;

        m_particleNumberMin = value;
        if (m_particleNumberMax < value)
            m_particleNumberMax = value;
        emit changed();
    }

    int particleNumberMax() const {
        return m_particleNumberMax;
    }

    void setParticleNumberMax(int value) {
        value = std::max(0, value);
        if (m_particleNumberMax == value)
            return;

        m_particleNumberMax = value;
        if (m_particleNumberMin > value)
            m_particleNumberMin = value;
        emit changed();
    }

    void setParticleCount(int count) {
        count = std::max(0, count);
        if (m_particleCount == count)
            return;

        m_particleCount = count;
        emit calculatedRangeChanged();
    }

    int firstVisibleParticle(int count) const {
        if (m_showAllParticles || count <= 0)
            return 0;
        return std::clamp(m_particleNumberMin, 0, count);
    }

    int visibleParticleCount(int count) const {
        if (count <= 0)
            return 0;
        if (m_showAllParticles)
            return count;

        const int first = firstVisibleParticle(count);
        const int last  = std::clamp(m_particleNumberMax, 0, count - 1);
        return std::max(0, last - first + 1);
    }

    virtual QString propertyDisplayName(const QString& propName) const {
        static const QMap<QString, QString> baseNames = {
            {"isVisible", "Видимость слоя"},
            {"opacity", "Непрозрачность частиц"},
            {"colorByField", "Окрашивать по полю"},
            {"colorMapId", "Цветовая карта"},
            {"useLogScale", "Логарифмическая шкала"},
            {"rangeMin", "Мин. значение диапазона"},
            {"rangeMax", "Макс. значение диапазона"},
            {"baseRangeMin", "Глобальный минимум"},
            {"baseRangeMax", "Глобальный максимум"},
            {"autoRange", "Авто-диапазон"},
            {"showOutRangeParticles", "Отображать частицы вне диапазона значений"},
            {"show_all_particles", "Показать все частицы"},
            {"particle_number_min", "Первый номер частицы"},
            {"particle_number_max", "Последний номер частицы"},
            {"zOrder", "Порядок отображения (Z)"},
            {"blendMode", "Режим смешивания"}};
        //  {"pickable", "Доступен для выбора"}};
        return baseNames.value(propName, propName); // Если не нашли, вернем английское имя
    }

    virtual QString enumValueDisplayName(const QString& propName, const QString& enumKey) const {
        if (propName == "blendMode") {
            if (enumKey == "Alpha")
                return "Альфа-смешивание";
            if (enumKey == "Additive")
                return "Аддитивное (Добавление)";
        }
        return enumKey;
    }

    virtual NumericConstraints propertyConstraints(const QString& propName) const {
        NumericConstraints c;
        if (propName == "opacity") {
            c.min      = 0.0;
            c.max      = 1.0;
            c.step     = 0.05;
            c.decimals = 4;
        } else if (propName == "particle_number_min" || propName == "particle_number_max") {
            c.min  = 0.0;
            c.max  = m_particleCount > 0 ? m_particleCount - 1 : std::numeric_limits<int>::max();
            c.step = 1.0;
            c.decimals = 0;
        } else if (propName == "rangeMin" || propName == "rangeMax" || propName == "baseRangeMin" ||
                   propName == "baseRangeMax") {
            // ДИНАМИЧЕСКИЙ РАСЧЕТ: шаг — это 1% от текущего глобального диапазона
            double delta = m_baseRangeMax - m_baseRangeMin;
            if (delta <= 0.0)
                delta = 1.0;

            c.step     = delta / 100.0;
            c.decimals = 8;

            c.min = m_baseRangeMin - delta * 10; // Позволяем крутить чуть шире диапазона
            c.max = m_baseRangeMax + delta * 10;
        }
        return c;
    }

    virtual bool isPropertyEnabled(const QString& propName) const {
        if (propName == "particle_number_min" || propName == "particle_number_max") {
            return !showAllParticles();
        } else if (propName == "rangeMin") {
            return !autoRange();
        } else if (propName == "rangeMax") {
            return !autoRange();
        } else if (propName == "baseRangeMin") {
            return false;
        } else if (propName == "baseRangeMax") {
            return false;
        }
        return true; // По умолчанию всё доступно
    }

  signals:
    void changed();
    void calculatedRangeChanged();

  private:
    bool      m_isVisible = true; // видимость слоя
    double    m_opacity   = 1.0;  // непрозрачномсть частиц
    QString   m_colorByField;     // имя поля, по которому окрашиваются частицы
    QUuid     m_colorMapId; // идентификатор цветовой карты, используемой для окрашивания частиц
    bool      m_useLogScale  = true; // использовать ли логарифмическую шкалу для окрашивания частиц
    double    m_rangeMin     = 0.0;  // минимальное значение диапазона окрашивания частиц
    double    m_rangeMax     = 100.0; // максимальное значение диапазона окрашивания частиц
    double    m_baseRangeMin = 0.0;   // глобальное минимальное значение
    double    m_baseRangeMax = 100.0; // глобальное максимальное значение
    bool      m_autoRange    = true;  // автоматически ли подбирать диапазон окрашивания частиц
    bool      m_showOutOfRangeParticles = false;
    int       m_zOrder                  = 0;
    BlendMode m_blendMode               = BlendMode::Alpha;
    double    m_physicalKm              = 3.72;
    double    m_physicalKr              = 0.9;
    double    m_physicalGamma           = 5.0 / 3.0;
    bool      m_showAllParticles        = true;
    int       m_particleNumberMin       = 0;
    int       m_particleNumberMax       = std::numeric_limits<int>::max();
    int       m_particleCount           = 0;

    bool m_hasPhysicalGamma = true;
};

} // namespace GVis::Visualize::Layers