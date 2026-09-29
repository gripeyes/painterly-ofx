#pragma once

#include "core/Execution.h"
#include "core/PictorialPlanes.h"
#include "core/ScreenedMultigrid.h"
#include "core/Types.h"

#include <array>

namespace pigment {

enum class PhotographicShadingModel { QuadraticPhase32 = 0, SourceSmooth, TgvRegularized };

struct PhotographicDecompositionParams {
  PhotographicShadingModel model = PhotographicShadingModel::SourceSmooth;
  float smoothSimplification = 0.55f;
  float shadingStructurePreserve = 0.8f;
  float chromaShadingRetention = 0.65f;
  float fineExtinction = 0.9f;
  float mediumExtinction = 0.8f;
  float detailStructurePreserve = 0.7f;
  float structurePreserve = 0.9f;
};

struct TgvDiagnostics {
  float gradientNormSquaredBound = 0.0f;
  float operatorNormSquaredBound = 0.0f;
  float tau = 0.0f, sigma = 0.0f;
  float initialEnergy = 0.0f, finalEnergy = 0.0f;
  float primalChangeRms = 0.0f;
  bool finite = true;
};

struct PhotographicComponents {
  OwnedPlane smooth, structure, medium, fine;
  explicit PhotographicComponents(RectI bounds)
      : smooth(bounds), structure(bounds), medium(bounds), fine(bounds) {}
};

void sourceSmoothWls(ConstFloatPlaneView source,
                     ScalarFieldView structureProtection,
                     float simplification, float structurePreserve,
                     const ImageGeometry& geometry,
                     FloatPlaneView destination,
                     const ExecutionContext& execution = {});

TgvDiagnostics tgvRegularizedSmooth(ConstFloatPlaneView source,
                                    ScalarFieldView structureProtection,
                                    float simplification, float structurePreserve,
                                    const ImageGeometry& geometry,
                                    FloatPlaneView destination,
                                    const ExecutionContext& execution = {});

void decomposePhotographicLuminance(ConstFloatPlaneView source,
                                    ConstFloatPlaneView smooth,
                                    ScalarFieldView structureProtection,
                                    PhotographicComponents& result,
                                    const ExecutionContext& execution = {});

// Conditional WLS fits the actual signal with membership-weighted fidelity;
// callers mix its result by membership exactly once.
void conditionalSmoothField(ConstFloatPlaneView signal,
                            ConstFloatPlaneView membership,
                            ScalarFieldView edgeConductance,
                            float lambda,
                            FloatPlaneView destination,
                            const ExecutionContext& execution = {});

float tgvOperatorNormSquaredBound(float spacingX, float spacingY) noexcept;

}  // namespace pigment
