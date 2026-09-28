#pragma once
#include "core/Execution.h"
#include "core/Types.h"

namespace pigment {

enum class InputDomainKind { LocalHalo, FullRegionOfDefinition };

struct InputDomainRequest {
  InputDomainKind kind = InputDomainKind::LocalHalo;
  int haloX = 0;
  int haloY = 0;
};

struct SpatialOperation {
  ConstYabPlanes source;
  YabPlanes destination;
  // Independent fields by design: strength controls how far a pixel moves toward
  // the result, while protection controls whether information may cross a boundary.
  ScalarFieldView processingStrength;
  // Values are permeability: 1 crosses freely, 0 is fully protected.
  ScalarFieldView boundaryProtection;
  RectI outputRegion;
  ImageGeometry geometry;
};

class SpatialOperator {
 public:
  virtual ~SpatialOperator() = default;
  virtual InputDomainRequest requiredInputDomain(const ImageGeometry& geometry) const noexcept = 0;
  virtual void apply(const SpatialOperation& operation,
                     const ExecutionContext& execution) const = 0;
};

}  // namespace pigment
