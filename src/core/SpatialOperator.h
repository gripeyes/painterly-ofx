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
  ConstFloatPlaneView control;
  ConstFloatPlaneView boundary;
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

