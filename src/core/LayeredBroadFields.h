#pragma once
#include "core/RegionHierarchy.h"

namespace pigment {
struct BroadSublayer {
  int family=0; // 0: Y, 1: jointly fitted AB, never shared membership
  bool radial=false;
  double cx=0,cy=0,rx=1,ry=1;
  std::array<std::array<double,3>,2> coefficients{};
  OwnedPlane membership;
  OwnedYabPlanes field;
  explicit BroadSublayer(RectI b):membership(b),field(b){}
};
struct BroadLayerFit {
  int plate=0,family=0,observations=0,layers=0;
  std::vector<double> objectives;
};
struct LayeredBroadResult {
  std::vector<OwnedYabPlanes> appearance,broadTarget,broad,structure,medium,micro;
  std::vector<OwnedPlane> protectionY,protectionAB;
  std::vector<std::vector<BroadSublayer>> layers;
  std::vector<BroadLayerFit> fits;
  OwnedYabPlanes composite;
  explicit LayeredBroadResult(RectI b):composite(b){}
};
struct LayeredBroadOptions {
  bool processY=true,processAB=true;
  int maximumYLayers=4,maximumABLayers=3;
  float observationScaleY=32,observationScaleAB=64;
  float microSurvival=.08f;
};
// Isolated C5 CPU reference. No chunk-domain solves, smoothing, Spill or chemistry.
LayeredBroadResult layeredBroadFields(const PublicPlateSet &plates,
    const Phase4RegionHierarchy &hierarchy,const LayeredBroadOptions &options={},
    const ExecutionContext &execution={});
}
