#pragma once

#include "core/ChunkGradientSynthesis.h"

namespace pigment {

struct Phase4SpillResult {
  RectI bounds{};
  std::vector<OwnedYabPlanes> plateAppearance;
  std::vector<OwnedPlane> influence;
  OwnedYabPlanes composite;
  explicit Phase4SpillResult(RectI boundsIn)
      : bounds(boundsIn), composite(boundsIn) {}
};

Phase4SpillResult applyPhase4Spill(ConstYabPlanes original,
                                   const PublicPlateSet &plates,
                                   const Phase4ChunkSynthesis &synthesis,
                                   const SparseAffinityGraph &graph,
                                   const Phase4Params &params,
                                   const ExecutionContext &execution = {});

} // namespace pigment
