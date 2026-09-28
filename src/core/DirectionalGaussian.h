#pragma once
#include "core/SpatialOperator.h"

namespace pigment {

struct DirectionalGaussianOptions {
  float radius = 12.0f;
  float xScale = 1.0f;
  float yScale = 1.0f;
  float angleDegrees = 0.0f;
  float edgeProtection = 0.5f;
  float edgeSoftness = 0.25f;
};

class DirectionalGaussianOperator final : public SpatialOperator {
 public:
  explicit DirectionalGaussianOperator(DirectionalGaussianOptions options)
      : options_(options) {}
  InputDomainRequest requiredInputDomain(const ImageGeometry& geometry) const noexcept override;
  void apply(const SpatialOperation& operation,
             const ExecutionContext& execution) const override;
 private:
  DirectionalGaussianOptions options_;
};

}  // namespace pigment

