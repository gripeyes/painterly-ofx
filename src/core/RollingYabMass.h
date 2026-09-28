#pragma once

#include "core/Similarity.h"
#include "core/SpatialOperator.h"

#include <array>

namespace pigment::detail {

// Internal DetailCollapse backend. This is not an OFX effect or plug-in factory.
struct RollingYabMassOptions {
  float massScale = 8.0f;
  float massStrength = 0.5f;
  float internalVariation = 0.15f;
  float lumaMassing = 1.0f;
  float chromaMassing = 1.0f;
  SimilarityWeights similarity{};
};

struct RollingYabMassDebugOutputs {
  YabPlanes* consolidationSeed = nullptr;
  std::array<YabPlanes*, 4> rollingIterations{};
  YabPlanes* preReintegrationMass = nullptr;
  YabPlanes* internalVariationResidual = nullptr;
};

class RollingYabMassOperator final : public SpatialOperator {
 public:
  explicit RollingYabMassOperator(RollingYabMassOptions options)
      : options_(options) {}

  InputDomainRequest requiredInputDomain(
      const ImageGeometry& geometry) const noexcept override;
  void apply(const SpatialOperation& operation,
             const ExecutionContext& execution) const override;
  void applyWithDebug(const SpatialOperation& operation,
                      const ExecutionContext& execution,
                      const RollingYabMassDebugOutputs& debug) const;

 private:
  RollingYabMassOptions options_;
};

}  // namespace pigment::detail
