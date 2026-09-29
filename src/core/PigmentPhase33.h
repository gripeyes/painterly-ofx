#pragma once

#include "core/IntegratedPigment.h"

namespace pigment {

struct Phase33RenderInputs {
  ConstImageView source{};
  ImageView destination{};
  RectI renderWindow{};
  IntegratedPigmentParams params{};
  ImageGeometry geometry{};
  const ConstImageView* mask = nullptr;
  const ConstImageView* planeMap = nullptr;
};

// Full-frame deterministic CPU reference and unsupported-Metal fallback.
void processPigmentPhase33(const Phase33RenderInputs& inputs,
                           const ExecutionContext& execution = {});

}  // namespace pigment
