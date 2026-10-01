#pragma once

#include "core/IntegratedPigment.h"
#include "core/LatentPlateGraph.h"
#include "core/RegionHierarchy.h"
#include "core/ChunkGradientSynthesis.h"
#include <memory>
#include <vector>

namespace pigment {

// Single-source, single-representation cache. Owned by one node; caller must
// serialize access. Creative/Spill/debug edits do not rerun spectral analysis.
struct Phase4ResearchCache {
  std::vector<double> automaticKey, hierarchyKey, synthesisKey;
  std::unique_ptr<Phase4AutomaticResult> automatic;
  std::unique_ptr<PublicPlateSet> supported;
  std::unique_ptr<Phase4RegionHierarchy> hierarchy;
  std::unique_ptr<Phase4ChunkSynthesis> synthesis;
  size_t automaticBuilds=0, hierarchyBuilds=0, synthesisBuilds=0;
};

struct Phase4RenderInputs {
  ConstImageView source{};
  ImageView destination{};
  RectI renderWindow{};
  IntegratedPigmentParams params{};
  ImageGeometry geometry{};
  const ConstImageView* mask = nullptr;
  Phase4ResearchCache* cache = nullptr;
};

struct Phase4RenderDiagnostics {
  Phase4GateDiagnostics gate{};
  bool reconstructionConverged = true;
};

Phase4RenderDiagnostics processPigmentPhase4(
    const Phase4RenderInputs& inputs, const ExecutionContext& execution = {});

}  // namespace pigment
