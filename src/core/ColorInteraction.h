#pragma once
#include "core/ColorSpace.h"
#include <array>

namespace pigment {
enum class ColorInteractionLaw { LinearYAB=0, Density, SpectralPigment };
constexpr int kInteractionSamples=21;
// Optional diagnostics only. Null traces retain the existing arithmetic path.
struct SpectralEncodeTrace {
  std::array<double,3> sceneRGB{},materialRGB{},negativeResidual{},fitResidual{},decodedRGB{};
  std::array<double,kInteractionSamples> reflectance{},safeReflectance{},ks{};
  int greyClamp=0,reflectanceFloor=0,coefficientBound=0,invalid=0;
  int iterations=0,rejectedSteps=0;
};
struct SpectralMixTrace {
  double magnitude=0;
  std::array<double,kInteractionSamples> ks{},reflectance{};
  std::array<double,3> xyz{},materialRGB{},sceneBeforeResidual{},residual{},nonlinearRGB{};
  YabPixel nonlinearYab{},finalYab{};
  int densityClamp=0,invalid=0,bypass=0;
};
struct InteractionMaterial {
  double magnitude=0;
  std::array<double,3> residual{}, coefficients{};
  std::array<double,kInteractionSamples> absorption{};
  double fitError=0, residualMagnitude=0;
};
// Transient material state: no spectral image or alteration of spatial data.
class ColorInteraction {
 public:
  explicit ColorInteraction(WorkingGamut gamut,bool spectralSceneMass=false);
  InteractionMaterial encode(YabPixel color,ColorInteractionLaw law,SpectralEncodeTrace *trace=nullptr) const;
  std::array<double,3> reconstruct(const InteractionMaterial&,ColorInteractionLaw) const;
  YabPixel mix(const InteractionMaterial *materials,const float *weights,int count,
               YabPixel linear,ColorInteractionLaw law,float density,SpectralMixTrace *trace=nullptr) const;
 private:
  MatrixOpponentTransform opponent_;
  bool spectralSceneMass_=false; // Isolated CPU diagnosis, never a UI/default change.
  std::array<std::array<double,kInteractionSamples>,3> integration_{};
  std::array<std::array<double,kInteractionSamples>,3> xyzIntegration_{};
  std::array<double,3> integrate(const std::array<double,kInteractionSamples>&) const;
};
}
