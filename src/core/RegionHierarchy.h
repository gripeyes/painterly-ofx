#pragma once

#include "core/Execution.h"
#include "core/Phase4Types.h"

#include <vector>

namespace pigment {

struct Phase4MergeNode {
  int left = -1, right = -1;
  float level = 0.0f;
};

struct Phase4PlateChunkHierarchy {
  std::vector<int> yChunk;
  std::vector<int> abChunk;
  std::vector<Phase4MergeNode> yTree, abTree;
  // Outgoing right/down grid edges. RoD edges are infinity; edges inside an
  // atomic region are zero; other edges carry their tree LCA level.
  std::vector<float> yEdgeX, yEdgeY, abEdgeX, abEdgeY;
  OwnedPlane yRemovedBoundaries;
  OwnedPlane yRetainedBoundaries;
  OwnedPlane abRemovedBoundaries;
  OwnedPlane abRetainedBoundaries;
  int yChunkCount = 0;
  int abChunkCount = 0;
  float yMinimumDisappearance = 0.0f;
  float abMinimumDisappearance = 0.0f;

  explicit Phase4PlateChunkHierarchy(RectI bounds = {});
};

struct Phase4RegionHierarchy {
  RectI bounds{};
  std::vector<int> atomicRegion;
  int atomicRegionCount = 0;
  OwnedPlane boundaryStrength;
  OwnedPlane atomicRegionDisplay;
  std::vector<Phase4PlateChunkHierarchy> plates;
};

Phase4RegionHierarchy
buildPhase4RegionHierarchy(ConstYabPlanes source, const PublicPlateSet &plates,
                           const Phase4Params &params,
                           const ExecutionContext &execution = {});

} // namespace pigment
