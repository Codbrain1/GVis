#pragma once
#include <QColor>
#include <QObject>

namespace GVis::Visualize::Views::View2D {

// ============================================================
// Сетка на графике (major/minor gridlines QCustomPlot)
// ============================================================
class GridSettings : public QObject {
    Q_OBJECT
    // clang-format off
    Q_PROPERTY(bool   majorVisible READ majorVisible WRITE setMajorVisible NOTIFY changed)
    Q_PROPERTY(bool   minorVisible READ minorVisible WRITE setMinorVisible NOTIFY changed)
    Q_PROPERTY(QColor majorColor   READ majorColor   WRITE setMajorColor   NOTIFY changed)
    Q_PROPERTY(QColor minorColor   READ minorColor   WRITE setMinorColor   NOTIFY changed)
    Q_PROPERTY(float  majorLineWidth READ majorLineWidth WRITE setMajorLineWidth NOTIFY changed)
public:
    explicit GridSettings(QObject* parent = nullptr) : QObject(parent) {}

    bool majorVisible() const { return m_majorVisible; }
    void setMajorVisible(bool v) { if (m_majorVisible != v) { m_majorVisible = v; emit changed(); } }

    bool minorVisible() const { return m_minorVisible; }
    void setMinorVisible(bool v) { if (m_minorVisible != v) { m_minorVisible = v; emit changed(); } }

    QColor majorColor() const { return m_majorColor; }
    void setMajorColor(const QColor& c) { if (m_majorColor != c) { m_majorColor = c; emit changed(); } }

    QColor minorColor() const { return m_minorColor; }
    void setMinorColor(const QColor& c) { if (m_minorColor != c) { m_minorColor = c; emit changed(); } }

    float majorLineWidth() const { return m_majorLineWidth; }
    void setMajorLineWidth(float w) { if (!qFuzzyCompare(m_majorLineWidth, w)) { m_majorLineWidth = w; emit changed(); } }

    // clang-format on

  signals:
    void changed();

  private:
    bool   m_majorVisible   = true;
    bool   m_minorVisible   = false;
    QColor m_majorColor     = QColor(80, 80, 80);
    QColor m_minorColor     = QColor(50, 50, 50);
    float  m_majorLineWidth = 1.0f;
};

// ============================================================
// Оси графика (подписи, диапазоны, шкала)
// ============================================================
class AxisSettings : public QObject {
    Q_OBJECT
    // clang-format off
    Q_PROPERTY(QString xLabel   READ xLabel   WRITE setXLabel   NOTIFY changed)
    Q_PROPERTY(QString yLabel   READ yLabel   WRITE setYLabel   NOTIFY changed)
    Q_PROPERTY(bool    xLogScale READ xLogScale WRITE setXLogScale NOTIFY changed)
    Q_PROPERTY(bool    yLogScale READ yLogScale WRITE setYLogScale NOTIFY changed)
    Q_PROPERTY(bool    autoScale READ autoScale WRITE setAutoScale NOTIFY changed)
public:
    explicit AxisSettings(QObject* parent = nullptr) : QObject(parent) {}

    QString xLabel() const { return m_xLabel; }
    void setXLabel(const QString& l) { if (m_xLabel != l) { m_xLabel = l; emit changed(); } }

    QString yLabel() const { return m_yLabel; }
    void setYLabel(const QString& l) { if (m_yLabel != l) { m_yLabel = l; emit changed(); } }

    bool xLogScale() const { return m_xLogScale; }
    void setXLogScale(bool v) { if (m_xLogScale != v) { m_xLogScale = v; emit changed(); } }

    bool yLogScale() const { return m_yLogScale; }
    void setYLogScale(bool v) { if (m_yLogScale != v) { m_yLogScale = v; emit changed(); } }

    bool autoScale() const { return m_autoScale; }
    void setAutoScale(bool v) { if (m_autoScale != v) { m_autoScale = v; emit changed(); } }

    // clang-format on
  signals:
    void changed();

  private:
    QString m_xLabel    = "X";
    QString m_yLabel    = "Y";
    bool    m_xLogScale = false;
    bool    m_yLogScale = false;
    bool    m_autoScale = true;
};

// ============================================================
// Общий вид графика
// ============================================================
class ViewportSettings : public QObject {
    Q_OBJECT
    // clang-format off
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY changed)
    Q_PROPERTY(bool   legendVisible   READ legendVisible   WRITE setLegendVisible   NOTIFY changed)
    Q_PROPERTY(bool   antialiased     READ antialiased     WRITE setAntialiased     NOTIFY changed)
    public:
    explicit ViewportSettings(QObject* parent = nullptr) : QObject(parent) {}
    
    QColor backgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(const QColor& c) { if (m_backgroundColor != c) { m_backgroundColor = c; emit changed(); } }
    
    bool legendVisible() const { return m_legendVisible; }
    void setLegendVisible(bool v) { if (m_legendVisible != v) { m_legendVisible = v; emit changed(); } }
    
    bool antialiased() const { return m_antialiased; }
    void setAntialiased(bool v) { if (m_antialiased != v) { m_antialiased = v; emit changed(); } }

    // clang-format on

  signals:
    void changed();

  private:
    QColor m_backgroundColor = QColor(255, 255, 255);
    bool   m_legendVisible   = true;
    bool   m_antialiased     = true;
};

class View2DSettings : public QObject {
    Q_OBJECT
    // clang-format off
    Q_PROPERTY(GridSettings*     grid     READ grid     CONSTANT)
    Q_PROPERTY(AxisSettings*     axis     READ axis     CONSTANT)
    Q_PROPERTY(ViewportSettings* viewport READ viewport CONSTANT)
    // clang-format on
  public:
    explicit View2DSettings(QObject* parent = nullptr)
        : QObject(parent),
          m_grid(new GridSettings(this)),
          m_axis(new AxisSettings(this)),
          m_viewport(new ViewportSettings(this)) {
        connect(m_grid, &GridSettings::changed, this, &View2DSettings::anyChanged);
        connect(m_axis, &AxisSettings::changed, this, &View2DSettings::anyChanged);
        connect(m_viewport, &ViewportSettings::changed, this, &View2DSettings::anyChanged);
    }

    GridSettings* grid() const {
        return m_grid;
    }

    AxisSettings* axis() const {
        return m_axis;
    }

    ViewportSettings* viewport() const {
        return m_viewport;
    }

  signals:
    void anyChanged();

  private:
    GridSettings*     m_grid;
    AxisSettings*     m_axis;
    ViewportSettings* m_viewport;
};

} // namespace GVis::Visualize::Views::View2D