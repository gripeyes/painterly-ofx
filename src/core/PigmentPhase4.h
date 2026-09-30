#pragma once

#include "core/IntegratedPigment.h"
#include "core/LatentPlateGraph.h"

namespace pigment {

struct Phase4RenderInputs {
  ConstImageView source{};
  ImageView destination{};
  RectI renderWindow{};
  IntegratedPigmentParams params{};
  ImageGeometry geometry{};
  const ConstImageView* mask = nullptr;
};

struct Phase4RenderDiagnostics {
  Phase4GateDiagnostics gate{};
  bool reconstructionConverged = true;
};

Phase4RenderDiagnostics processPigmentPhase4(
    const Phase4RenderInputs& inputs, const ExecutionContext& execution = {});

}  // namespace pigment
