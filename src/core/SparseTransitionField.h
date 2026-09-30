#pragma once
#include "core/RegionHierarchy.h"

namespace pigment {
struct SparseTransitionCurve {
  int plate=0,family=0;
  std::vector<std::array<double,2>> points;
  std::array<double,3> negative{},positive{};
  int negativeSamples=0,positiveSamples=0;
  double length=0;
};
struct SparseTransitionSolve {
  int plate=0,family=0,chunk=0,pixels=0,curveConstraints=0,structuralConstraints=0;
  bool solved=false;
  double residual=0;
};
struct SparseTransitionResult {
  std::vector<OwnedYabPlanes> appearance,sideValues;
  std::vector<OwnedPlane> yCurves,abCurves,yConstraints,abConstraints;
  std::vector<SparseTransitionCurve> curves;
  std::vector<SparseTransitionSolve> solves;
  OwnedYabPlanes composite;
  explicit SparseTransitionResult(RectI b):composite(b){}
};
// One isolated value-only raster curve prototype. No OFX integration.
SparseTransitionResult sparseTransitionField(const PublicPlateSet &plates,
    const Phase4RegionHierarchy &hierarchy,const ExecutionContext &execution={});
}
