#pragma once

#include "core/Execution.h"
#include "core/Phase4Types.h"

namespace pigment {

struct Phase4AutomaticResult {
  LatentComponentSet latent;
  PublicPlateSet plates;
  SparseAffinityGraph analysisGraph;
  Phase4GateDiagnostics diagnostics{};
  Phase4AutomaticResult(RectI bounds, int latentCount, int plateCount)
      : latent(bounds, latentCount), plates(bounds, plateCount) {}
};

Phase4AutomaticResult buildPhase4AutomaticPlates(
    ConstYabPlanes source, const Phase4Params& params,
    const ImageGeometry& geometry, const ExecutionContext& execution = {});

}  // namespace pigment
