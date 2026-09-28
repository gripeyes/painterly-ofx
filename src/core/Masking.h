#pragma once
#include <vector>
#include "core/Execution.h"
#include "core/Types.h"

namespace pigment {

struct BoundaryOptions {
  float protection = 0.5f;
  float softness = 0.25f;
};

struct TonalMaskOptions {
  bool rangeEnabled = false;
  float rangeMinimum = 0.0f;
  float rangeMaximum = 1.0f;
  float rangeSoftness = 0.1f;
  float shadowBias = 0.0f;
  float midtoneBias = 0.0f;
  float highlightBias = 0.0f;
};

void buildBoundaryField(ConstYabPlanes source, FloatPlaneView destination,
                        const BoundaryOptions& options,
                        const ExecutionContext& execution);
float tonalWeight(float y, const TonalMaskOptions& options) noexcept;
void composeControlField(ConstFloatPlaneView luminance,
                         ConstFloatPlaneView boundary,
                         const ConstFloatPlaneView* externalMask,
                         bool invertExternalMask,
                         const TonalMaskOptions& options,
                         FloatPlaneView destination,
                         const ExecutionContext& execution);

}  // namespace pigment

