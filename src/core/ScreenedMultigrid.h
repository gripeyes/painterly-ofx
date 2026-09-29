#pragma once

#include "core/Execution.h"
#include "core/Types.h"

namespace pigment {

struct ScreenedMultigridParams {
  float lambdaX = 1.0f;
  float lambdaY = 1.0f;
  int vCycles = 3;
  int preRelaxations = 4;
  int postRelaxations = 4;
  int coarseRelaxations = 24;
  int maximumLevels = 7;
  int minimumCoarseExtent = 16;
  float relaxation = 2.0f / 3.0f;
};

// Solves c(x)(u-f)-div(lambda*g*grad(u))=0. Conductance is sampled at
// edge endpoints and combined conservatively. Source, confidence and
// conductance may be arbitrary scalar fields.
void solveScreenedMultigrid(ScalarFieldView source,
                            ScalarFieldView confidence,
                            ScalarFieldView conductance,
                            FloatPlaneView destination,
                            const ScreenedMultigridParams& params = {},
                            const ExecutionContext& execution = {});

// Computes the RMS residual of the same discrete operator. Primarily used by
// parity/convergence diagnostics.
float screenedResidualRms(ConstFloatPlaneView solution,
                          ScalarFieldView source,
                          ScalarFieldView confidence,
                          ScalarFieldView conductance,
                          float lambdaX, float lambdaY) noexcept;

}  // namespace pigment
