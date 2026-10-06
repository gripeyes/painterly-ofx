#pragma once
#include "core/RegionHierarchy.h"
#include <array>

namespace pigment {
struct RegionalModeDiagnostic {
  int plate=0, family=0, chunk=0, component=0, interior=0, mode=0;
  double eigenvalue=0, residual=0;
  std::array<double,3> coefficient{}, gradientEnergy{};
};
struct RegionalFitDiagnostic {
  int plate=0,family=0,chunk=0,component=0,interior=0,requested=0,used=0;
  double mass=0;
  std::array<double,3> rmse{};
};
struct RegionalEigenResult {
  std::vector<OwnedYabPlanes> appearance;
  OwnedYabPlanes composite;
  explicit RegionalEigenResult(RectI bounds):composite(bounds){}
};
struct RegionalEigenSweep {
  std::vector<RegionalEigenResult> results;
  // Presentation-only per-component peak normalization, zero on boundaries.
  std::vector<std::vector<OwnedPlane>> yModeAtlas,abModeAtlas;
  std::vector<RegionalModeDiagnostic> modes;
  std::vector<RegionalFitDiagnostic> fits;
  // Diagnostic-only boundary appearance, separate from fitted interiors.
  std::vector<OwnedYabPlanes> boundaryAppearance;
};
// Isolated CPU experiment. No source-gradient survival or moment constraints.
RegionalEigenSweep regionalEigenFieldSweep(const PublicPlateSet &plates,
    const Phase4RegionHierarchy &hierarchy,
    const ExecutionContext &execution={}, bool broadSideBoundary=false,
    bool lowBudgetOnly=false);
} // namespace pigment
