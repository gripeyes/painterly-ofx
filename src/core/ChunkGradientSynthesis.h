#pragma once

#include "core/RegionHierarchy.h"

namespace pigment {

struct Phase4PoissonDiagnostics {
  int iterations = 0;
  double relativeResidual = 0.0;
  bool converged = true;
};

struct Phase4ChunkSynthesis {
  RectI bounds{};
  std::vector<OwnedYabPlanes> plateAppearance;
  OwnedYabPlanes preSpill;
  std::vector<OwnedPlane> sourceGradient;
  std::vector<OwnedPlane> simplifiedGradient;
  std::vector<OwnedPlane> primitiveSelection, fitError;
  std::vector<std::array<Phase4PoissonDiagnostics, 3>> solver;
  explicit Phase4ChunkSynthesis(RectI boundsIn)
      : bounds(boundsIn), preSpill(boundsIn) {}
};

Phase4ChunkSynthesis
synthesizePhase4Chunks(ConstYabPlanes source, const PublicPlateSet &plates,
                       const Phase4RegionHierarchy &hierarchy,
                       const Phase4Params &params,
                       const ExecutionContext &execution = {});

} // namespace pigment
