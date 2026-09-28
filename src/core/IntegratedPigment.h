#pragma once

#include "core/ColorSpace.h"
#include "core/Execution.h"
#include "core/Types.h"

#include <cstdint>

namespace pigment {

enum class PigmentComparisonMode {
  Original = 0,
  GuidedDetailCollapse,
  IntegratedPigment
};

enum class PigmentDebugView {
  Final = 0,
  CoarseStructure,
  VeilSource,
  MassStrengthField,
  BoundaryExtinctionField,
  ChromaMigrationField,
  DetailRetentionField,
  RegionCentroidMode,
  RegionAttractionMagnitude,
  YMass,
  ABMass,
  MassBeforeBoundary,
  BoundaryProtection,
  BoundaryExtinction,
  ChromaMigrationResult,
  FineResidual,
  MediumResidual,
  InternalVariation,
  PreReintegration,
  DifferenceFromOriginal
};

struct IntegratedPigmentParams {
  float amount = 0.7f;
  float massScale = 18.0f;
  float massStrength = 0.55f;
  float toneSimilarity = 0.25f;
  float chromaSimilarity = 0.18f;
  float lumaAttraction = 0.65f;
  float chromaAttraction = 0.8f;

  float structureScale = 10.0f;
  float structurePreserve = 0.8f;
  float boundaryPreserve = 0.75f;
  float boundaryExtinction = 0.35f;
  float boundarySoftness = 0.15f;

  float veilAmount = 0.35f;
  float veilScale = 500.0f;
  float veilIrregularity = 0.4f;
  float veilContrast = 0.3f;
  std::int32_t veilSeed = 1;

  float detailCleanup = 0.1f;
  float fineDetail = 0.05f;
  float mediumDetail = 0.15f;
  float internalVariation = 0.3f;

  float chromaMigration = 0.35f;
  float chromaScale = 24.0f;
  float chromaEdgeRespect = 0.6f;

  float regionSoftness = 0.5f;
  float boundaryScale = 12.0f;
  float veilTonalBias = 0.0f;
  float chromaLumaCoupling = 0.6f;
  WorkingGamut gamut = WorkingGamut::ACEScg;
  bool invertMask = false;
  bool premultiplied = false;
  float mix = 1.0f;
  PigmentComparisonMode comparison = PigmentComparisonMode::IntegratedPigment;
  PigmentDebugView debugView = PigmentDebugView::Final;
};

struct PigmentFieldValues {
  float massStrength = 1.0f;
  float boundaryExtinction = 0.0f;
  float chromaMigration = 1.0f;
  float detailRetention = 1.0f;
};

// Deterministic broad field used by the CPU semantic reference and tests. Coordinates
// are absolute canonical image coordinates, so crops and unusual origins agree.
float painterlyVeilValue(float x, float y, float luminance,
                         const IntegratedPigmentParams& params) noexcept;

// The four controls intentionally use different response curves. A shared Veil
// source never aliases processing strength with boundary protection/extinction.
PigmentFieldValues derivePigmentFields(float veil, float mask,
                                       const IntegratedPigmentParams& params) noexcept;

// Small host-independent visual reference for the region operator. It is intended
// for fixtures and semantics, not full-frame OFX rendering. The fixed 9x9 budget is
// distributed across +/- Mass Scale, rather than representing a 9x9-pixel kernel.
void softRegionMassReference(ConstYabPlanes source, YabPlanes destination,
                             ScalarFieldView processingStrength,
                             ScalarFieldView boundaryProtection,
                             const IntegratedPigmentParams& params,
                             const ExecutionContext& execution = {});

}  // namespace pigment
