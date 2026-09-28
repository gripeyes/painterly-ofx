#pragma once
#include <vector>
#include "core/Execution.h"
#include "core/Types.h"

namespace pigment {

struct BoundaryOptions {
  float protection = 0.5f;
  float softness = 0.25f;
};

struct StructureBoundaryOptions {
  // Independent from Mass Scale. The guide is simplified at this scale before
  // measuring boundaries, allowing small high-contrast detail to disappear.
  // The value is in pixels of the supplied planes; a future host wrapper is
  // responsible for render-scale and pixel-aspect conversion.
  float structureScale = 4.0f;
  float protection = 0.75f;
  float softness = 0.15f;
  float luminanceWeight = 1.0f;
  float axisAWeight = 0.0f;
  float axisBWeight = 0.0f;
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
void buildStructureBoundaryField(ConstYabPlanes source, FloatPlaneView destination,
                                 const StructureBoundaryOptions& options,
                                 const ExecutionContext& execution = {});
float tonalWeight(float y, const TonalMaskOptions& options) noexcept;
void composeProcessingStrengthField(ConstFloatPlaneView luminance,
                                    const ConstFloatPlaneView* externalMask,
                                    bool invertExternalMask,
                                    const TonalMaskOptions& options,
                                    FloatPlaneView destination,
                                    const ExecutionContext& execution = {});

// Stage 1 compatibility helper. New region-aware operators should use
// composeProcessingStrengthField and pass boundary protection independently.
void composeControlField(ConstFloatPlaneView luminance,
                         ConstFloatPlaneView boundary,
                         const ConstFloatPlaneView* externalMask,
                         bool invertExternalMask,
                         const TonalMaskOptions& options,
                         FloatPlaneView destination,
                         const ExecutionContext& execution);

}  // namespace pigment
