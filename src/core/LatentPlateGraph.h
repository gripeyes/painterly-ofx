#pragma once

#include "core/Execution.h"
#include "core/Phase4Types.h"
#include <memory>

namespace pigment {
struct Phase4AnalysisCacheData;
// Source-only frozen A1/A2/A3 analysis; grouping/support edits reuse it without
// changing any mathematics or using a reduced/proxy analysis approximation.
struct Phase4AnalysisCache {
  std::vector<double> key;
  std::shared_ptr<Phase4AnalysisCacheData> data;
  size_t builds=0;
};

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
    const ImageGeometry& geometry, const ExecutionContext& execution = {},
    Phase4AnalysisCache* cache=nullptr);

}  // namespace pigment
