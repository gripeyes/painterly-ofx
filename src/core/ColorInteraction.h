#pragma once
#include "core/ColorSpace.h"
#include <array>

namespace pigment {
enum class ColorInteractionLaw { LinearYAB=0, Density, SpectralPigment };
constexpr int kInteractionSamples=21;
struct InteractionMaterial {
  double magnitude=0;
  std::array<double,3> residual{}, coefficients{};
  std::array<double,kInteractionSamples> absorption{};
  double fitError=0, residualMagnitude=0;
};
// Transient material state: no spectral image or alteration of spatial data.
class ColorInteraction {
 public:
  explicit ColorInteraction(WorkingGamut gamut);
  InteractionMaterial encode(YabPixel color,ColorInteractionLaw law) const;
  std::array<double,3> reconstruct(const InteractionMaterial&,ColorInteractionLaw) const;
  YabPixel mix(const InteractionMaterial *materials,const float *weights,int count,
               YabPixel linear,ColorInteractionLaw law,float density) const;
 private:
  MatrixOpponentTransform opponent_;
  std::array<std::array<double,kInteractionSamples>,3> integration_{};
  std::array<double,3> integrate(const std::array<double,kInteractionSamples>&) const;
};
}
