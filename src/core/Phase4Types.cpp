#include "core/Phase4Types.h"

#include <algorithm>
#include <cmath>

namespace pigment {

LatentComponentSet::LatentComponentSet(RectI bounds, int count)
    : bounds_(bounds),
      count_(std::max(1, std::min(kPhase4LatentCapacity, count))),
      confidence_(bounds), reconstructionError_(bounds),
      spectralResidual_(bounds), recoveryError_(bounds) {
  alpha_.reserve(count_);
  appearance_.reserve(count_);
  for (int i = 0; i < count_; ++i) {
    alpha_.emplace_back(bounds);
    appearance_.emplace_back(bounds);
  }
}

void LatentComponentSet::allocateSpectralModes(int count) {
  spectralModes_.clear();
  spectralModes_.reserve(std::max(0, count));
  for (int i = 0; i < count; ++i)
    spectralModes_.emplace_back(bounds_);
}

PublicPlateSet::PublicPlateSet(RectI bounds, int count)
    : bounds_(bounds),
      count_(std::max(1, std::min(kPhase4PlateCapacity, count))) {
  alpha_.reserve(count_);
  supportY_.reserve(count_);
  supportAB_.reserve(count_);
  appearance_.reserve(count_);
  for (int i = 0; i < count_; ++i) {
    alpha_.emplace_back(bounds);
    supportY_.emplace_back(bounds);
    supportAB_.emplace_back(bounds);
    appearance_.emplace_back(bounds);
  }
}

float phase4PlateEntropyCoefficient(float overlap) noexcept {
  const float value = std::max(0.0f, std::min(1.0f, overlap));
  return value * value;
}

float phase4SupportRadiusY(const Phase4Params &params) noexcept {
  return std::max(0.0f, params.plateScale) *
         std::max(0.0f, std::min(1.0f, params.plateOverlap));
}

float phase4SupportRadiusAB(const Phase4Params &params) noexcept {
  const float base = phase4SupportRadiusY(params);
  if (base == 0.0f)
    return 0.0f;
  const float ratio =
      std::max(0.25f, std::min(4.0f, params.chromaSupportRatio));
  const float coupling =
      std::max(0.0f, std::min(1.0f, params.lumaChromaCoupling));
  return base * (coupling + (1.0f - coupling) * ratio);
}

} // namespace pigment
