#pragma once

#include "core/IntegratedPigment.h"
#include "core/LatentPlateGraph.h"
#include "core/RegionHierarchy.h"
#include "core/ChunkGradientSynthesis.h"
#include "core/PlateSpill.h"
#include <functional>
#include <memory>
#include <vector>

namespace pigment {

// Single-source, single-representation cache. Owned by one node; caller must
// serialize access. Creative/Spill/debug edits do not rerun spectral analysis.
struct Phase4ResearchCache {
  Phase4AnalysisCache sourceAnalysis;
  std::vector<double> automaticKey, hierarchyKey, synthesisKey;
  std::unique_ptr<Phase4AutomaticResult> automatic;
  std::unique_ptr<PublicPlateSet> supported;
  std::unique_ptr<Phase4RegionHierarchy> hierarchy;
  std::unique_ptr<Phase4ChunkSynthesis> synthesis;
  size_t automaticBuilds=0, hierarchyBuilds=0, synthesisBuilds=0;
  std::vector<double> transportKey;
  std::unique_ptr<Phase4SpillTransport> transport;
  size_t transportBuilds=0;
};

struct Phase4RenderInputs {
  ConstImageView source{};
  ImageView destination{};
  RectI renderWindow{};
  IntegratedPigmentParams params{};
  ImageGeometry geometry{};
  const ConstImageView* mask = nullptr;
  Phase4ResearchCache* cache = nullptr;
  // Optional hybrid accelerator. False means unsupported/failure and the
  // unchanged CPU reference runs. Spectral must stay CPU-only.
  std::function<bool(ConstYabPlanes,const PublicPlateSet&,const Phase4ChunkSynthesis&,
      const SparseAffinityGraph&,const Phase4Params&,const Phase4SpillTransport&,
      WorkingGamut,Phase4SpillResult&)> accelerateSpill;
  std::function<bool(const PublicPlateSet&,const SparseAffinityGraph&,const Phase4Params&,
      Phase4SpillTransport&)> accelerateTransport;
};

struct Phase4RenderDiagnostics {
  Phase4GateDiagnostics gate{};
  bool reconstructionConverged = true;
  double analysisMs=0, hierarchyMs=0, synthesisMs=0, transportMs=0, interactionMs=0, finalMs=0, totalMs=0;
  bool metalSpill=false, metalAttempted=false;
};

Phase4RenderDiagnostics processPigmentPhase4(
    const Phase4RenderInputs& inputs, const ExecutionContext& execution = {});

}  // namespace pigment
