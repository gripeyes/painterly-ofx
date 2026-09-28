#pragma once

#include "core/RollingYabMass.h"

namespace pigment::detail {

enum class AcceleratedMassAlgorithm { Guided, DomainTransform };

class AcceleratedYabMassOperator final : public SpatialOperator {
 public:
  AcceleratedYabMassOperator(AcceleratedMassAlgorithm algorithm,
                             RollingYabMassOptions options)
      : algorithm_(algorithm), options_(options) {}

  InputDomainRequest requiredInputDomain(
      const ImageGeometry&) const noexcept override;
  void apply(const SpatialOperation& operation,
             const ExecutionContext& execution) const override;
  void applyWithDebug(const SpatialOperation& operation,
                      const ExecutionContext& execution,
                      const RollingYabMassDebugOutputs& debug) const;

 private:
  AcceleratedMassAlgorithm algorithm_;
  RollingYabMassOptions options_;
};

}  // namespace pigment::detail
