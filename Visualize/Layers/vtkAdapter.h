#pragma once
#include "Common/Structures/ObjectRegistryStructures.h"
#include <QVector3D>
#include <QVector>
#include <qloggingcategory.h>
#include <qvectornd.h>
#include <vtkDataArray.h>
#include <vtkDataSetAttributes.h>
#include <vtkIdList.h>
#include <vtkKdTreePointLocator.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPointSet.h>
#include <vtkPoints.h>
#include "Implementations/SPH/SPHPointsLayerSettings.h"
#include "Logger/Logger.h"
#include "Physics/DimensionConverter/PhysicalUnits.h"
#include <cmath>
#include <utility>

// TODO временный адаптер для vtk массивов
namespace GVis::Visualize::vtkAdapter {

// vtkDataSet сам по себе не гарантирует наличие точек (например, vtkImageData),
// поэтому кастуем к vtkPointSet — базовому классу для vtkPolyData/vtkUnstructuredGrid,
// которые реально хранят частицы как точки.
inline QVector<QVector3D> extractPositions(const std::shared_ptr<Core::DataNode>& node) {
    QVector<QVector3D> result;
    if (!node || !node->data)
        return result;

    vtkPointSet* pointSet = vtkPointSet::SafeDownCast(node->data);
    if (!pointSet || !pointSet->GetPoints())
        return result;

    vtkPoints*      points = pointSet->GetPoints();
    const vtkIdType n      = points->GetNumberOfPoints();
    result.reserve(int(n));

    for (vtkIdType i = 0; i < n; ++i) {
        double p[3];
        points->GetPoint(i, p);
        result.append(QVector3D(float(p[0]), float(p[1]), float(p[2])));
    }
    return result;
}

inline std::pair<QVector3D, QVector3D> getBounds(const std::shared_ptr<Core::DataNode>& node) {
    std::pair<QVector3D, QVector3D> result{{-10, -10, -10}, {10, 10, 10}};

    if (!node || !node->data)
        return result;

    vtkPointSet* pointSet = vtkPointSet::SafeDownCast(node->data);
    if (!pointSet || !pointSet->GetPoints())
        return result;

    double* bounds = pointSet->GetBounds();
    result.first   = QVector3D(bounds[0], bounds[2], bounds[4]);
    result.second  = QVector3D(bounds[1], bounds[3], bounds[5]);
    return result;
}

inline QVector<float> extractScalarField(const std::shared_ptr<Core::DataNode>& node,
                                         const QString&                         fieldName) {
    QVector<float> result;
    if (!node || !node->data || fieldName.isEmpty())
        return result;

    vtkPointData* pointData = node->data->GetPointData();
    if (!pointData->HasArray(fieldName.toUtf8().constData()))
        return result;

    vtkDataArray* array = pointData->GetArray(fieldName.toUtf8().constData());
    if (!array)
        return result;

    const vtkIdType n       = array->GetNumberOfTuples();
    const int       numComp = array->GetNumberOfComponents(); // Смотрим, сколько компонент в поле

    result.reserve(int(n));

    if (numComp == 1) {
        // 1. Обычный скаляр (Масса, Плотность, Давление)
        for (vtkIdType i = 0; i < n; ++i) {
            result.append(static_cast<float>(array->GetTuple1(i)));
        }
        auto range = array->GetRange();
        qCDebug(LogRenderer) << array->GetName() << " min: " << range[0] << "max: " << range[1];
    } else if (numComp == 3) {
        // 2. Векторное поле (Скорость / Velocity, Сила, Ускорение)
        // Выделяем на стеке небольшой буфер под 3 координаты
        double v[3] = {0.0, 0.0, 0.0};

        for (vtkIdType i = 0; i < n; ++i) {
            // Быстро вытаскиваем сразу весь вектор (Vx, Vy, Vz)
            array->GetTuple(i, v);

            // Вычисляем модуль скорости: sqrt(Vx^2 + Vy^2 + Vz^2)
            float speedMagnitude = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

            result.append(speedMagnitude);
        }
    } else {
        // 3. Резервный вариант (если компонент 2, 4 или больше)
        // Берем только первую компоненту, чтобы избежать паники VTK
        for (vtkIdType i = 0; i < n; ++i) {
            result.append(static_cast<float>(array->GetComponent(i, 0)));
        }
    }

    return result;
}

inline std::pair<double, double> getScalarRange(const std::shared_ptr<Core::DataNode>& node,
                                                const QString&                         fieldName) {
    if (!node || !node->data || fieldName.isEmpty())
        return std::pair(0.0, 0.0);
    vtkDataArray* array = node->data->GetPointData()->GetArray(fieldName.toUtf8().constData());
    if (!array)
        return std::pair(0.0, 0.0);
    double range[2];
    array->GetRange(range);
    return std::pair(range[0], range[1]);
}

// список имён доступных скалярных полей — нужен для UI (выбор colorByField)
inline QStringList availableScalarFields(const std::shared_ptr<Core::DataNode>& node) {
    QStringList names;
    if (!node || !node->data)
        return names;

    vtkPointData* pd = node->data->GetPointData();
    for (int i = 0; i < pd->GetNumberOfArrays(); ++i)
        names << QString::fromUtf8(pd->GetArrayName(i));
    return names;
}

inline vtkPointSet* getPointSet(const std::shared_ptr<Core::DataNode>& node) {
    if (!node || !node->data)
        return nullptr;
    return vtkPointSet::SafeDownCast(node->data);
}

// h_i = eta * (m_i / rho_i)^(1/3) — формула из референсного Python-скрипта (const_eh).
// Годится, когда в данных есть и масса, и плотность каждой частицы (обычно — газ).
inline QVector<float> computeSmoothingLength_FromDensity(const QVector<float>& mass,
                                                         const QVector<float>& density,
                                                         float                 eta = 1.333333f) {
    QVector<float> h(mass.size(), 0.0f);
    for (int i = 0; i < mass.size(); ++i) {
        const float rho = density[i];
        h[i]            = (rho > 1e-30f) ? eta * std::cbrt(mass[i] / rho) : 0.0f;
    }
    return h;
}

// Fallback для полей без встроенной плотности (типично — тёмная материя):
// h_i = расстояние до k-го ближайшего соседа. Тяжёлая операция (O(N log N)),
// поэтому должна вызываться только при загрузке/смене данных, не каждый кадр.
inline QVector<float> computeSmoothingLength_KNN(vtkPointSet* pointSet, int kNeighbors = 32) {
    QVector<float> h;
    if (!pointSet || !pointSet->GetPoints())
        return h;

    vtkNew<vtkKdTreePointLocator> locator;
    locator->SetDataSet(pointSet);
    locator->BuildLocator();

    const vtkIdType n = pointSet->GetNumberOfPoints();
    h.resize(int(n));

    vtkNew<vtkIdList> ids;
    for (vtkIdType i = 0; i < n; ++i) {
        double p[3];
        pointSet->GetPoint(i, p);
        locator->FindClosestNPoints(kNeighbors, p, ids);

        double maxDist2 = 0.0;
        for (vtkIdType j = 0; j < ids->GetNumberOfIds(); ++j) {
            double q[3];
            pointSet->GetPoint(ids->GetId(j), q);
            const double dx = p[0] - q[0], dy = p[1] - q[1], dz = p[2] - q[2];
            maxDist2 = std::max(maxDist2, dx * dx + dy * dy + dz * dz);
        }
        h[int(i)] = float(std::sqrt(maxDist2));
    }
    return h;
}

inline Layers::SPHPointsLayerSettings::NormalizationMode
fieldNormolized(const std::shared_ptr<Core::DataNode>& node, const QString& fieldName) {
    if (!node || fieldName.isEmpty())
        return Layers::SPHPointsLayerSettings::NormalizationMode::Unknown;
    if (fieldName == "Mass") {
        return Layers::SPHPointsLayerSettings::NormalizationMode::Extensive;
    } else if (fieldName == "Density" || fieldName == "Energy" || fieldName == "Velocity") {
        return Layers::SPHPointsLayerSettings::NormalizationMode::Intensive;
    }
    return Layers::SPHPointsLayerSettings::NormalizationMode::Unknown;
}
} // namespace GVis::Visualize::vtkAdapter