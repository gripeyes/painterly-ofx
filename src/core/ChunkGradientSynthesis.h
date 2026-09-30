#pragma once

#include "core/RegionHierarchy.h"

namespace pigment {

// Standalone CPU research controls. No OFX UI/default-mode migration.
struct Phase4BroadFormOptions {
  // Opt-in until the photographic Gate C accepts this experiment.
  bool enabled = false;
  float spacingY = 32.0f;
  float spacingAB = 64.0f;
  float strengthY = 20.0f;
  float strengthAB = 5.0f;
  float firstStrengthY = 0.0f;
  float firstStrengthAB = 0.0f;
};

struct Phase4PoissonDiagnostics {
  int iterations = 0;
  double relativeResidual = 0.0;
  bool converged = true;
  int broadConstraints = 0;
  double broadResultRmse = 0.0;
  int firstConstraints = 0;
  double firstResultRmse = 0.0;
};

struct Phase4ChunkSynthesis {
  RectI bounds{};
  std::vector<OwnedYabPlanes> plateAppearance;
  OwnedYabPlanes preSpill;
  std::vector<OwnedPlane> sourceGradient;
  std::vector<OwnedPlane> simplifiedGradient;
  std::vector<OwnedPlane> primitiveSelection, fitError;
  std::vector<OwnedYabPlanes> broadConstraintTargets;
  std::vector<OwnedYabPlanes> broadConstraintInfluence;
  std::vector<std::array<Phase4PoissonDiagnostics, 3>> solver;
  explicit Phase4ChunkSynthesis(RectI boundsIn)
      : bounds(boundsIn), preSpill(boundsIn) {}
};

Phase4ChunkSynthesis
synthesizePhase4Chunks(ConstYabPlanes source, const PublicPlateSet &plates,
                       const Phase4RegionHierarchy &hierarchy,
                       const Phase4Params &params,
                       const ExecutionContext &execution = {},
                       const Phase4BroadFormOptions &broadForm = {});

} // namespace pigment
