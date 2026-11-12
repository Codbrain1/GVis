#pragma once
#include <QObject>
namespace GVis::Visualize::Views
{
namespace View3D
{

enum class CameraViewType
{
  XY_Top,   // Сверху
  XZ_Front, // Спереди
  YZ_Right, // Справа
  Iso       // Изометрия
  // MINOR:: добавить стандратные позиции
};
} // namespace View3D
namespace View2D
{

}
enum class ViewType
{
  OpenGL3D,
  QCustonPlot2D
};

} // namespace GVis::Visualize::Views