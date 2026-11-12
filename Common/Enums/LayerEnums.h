#pragma once
namespace GVis::Visualize::Layers
{
enum class RenderLayerType
{
  Particles,
  SPH,
  Binning
};
inline QList<RenderLayerType> getAllRenderModes()
{
  return {RenderLayerType::Particles, RenderLayerType::SPH, RenderLayerType::Binning};
}
inline QString rendermodeToString(RenderLayerType mode)
{
  switch (mode)
  {
  case RenderLayerType::Particles:
    return "Particles";
  case RenderLayerType::SPH:
    return "SPH";
  case RenderLayerType::Binning:
    return "Binning";
  default:
    return "Unknown";
  }
}
inline std::optional<RenderLayerType> rendermodeFromString(const QString &s)
{
  if (s == "Particles")
    return RenderLayerType::Particles;
  if (s == "SPH")
    return RenderLayerType::SPH;
  if (s == "Binning")
    return RenderLayerType::Binning;
  return std::nullopt;
}
} // namespace GVis::Visualize::Layers