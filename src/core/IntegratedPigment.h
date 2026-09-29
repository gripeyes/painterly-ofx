#pragma once

#include "core/ColorSpace.h"
#include "core/Execution.h"
#include "core/PictorialPlanes.h"
#include "core/PhotographicDecomposition.h"
#include "core/Phase4Types.h"
#include "core/SoftPictorialPlates.h"
#include "core/Types.h"

#include <cstdint>

namespace pigment {

enum class PigmentComparisonMode {
  Original = 0,
  GuidedDetailCollapse,
  WeightedMeanPigment,
  RepresentativeModePigment,
  PictorialPlanes,
  SoftPictorialPlates,
  AutomaticPlateGraph
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
  DifferenceFromOriginal,
  LocalDensity,
  WinningDominantMode,
  ModeConfidence,
  RepresentativeDistance,
  CandidateCompetition,
  LegacyWeightedMean,
  RepresentativeModeResult,
  RawPlaneMap,
  NormalizedPlaneMembership,
  BaseMembership,
  YTransitionMembership,
  ABTransitionMembership,
  PlaneBroadYTarget,
  PlaneBroadABTarget,
  CombinedYTarget,
  CombinedABTarget,
  PlaneBroadComponent,
  PlaneFineResidual,
  PlaneMediumResidual,
  PlaneExtinctionAmount,
  PlaneStructureProtection,
  PlanePreVeil,
  PlanePreSoftness,
  PlaneFitError,
  PlaneDifferenceFromOriginal,
  AutomaticRawMembership,
  AutomaticNormalizedMembership,
  AutomaticBaseMembership,
  AutomaticPalette,
  HybridCorrectionInfluence,
  AutomaticConfidence,
  AutomaticReconstructionError,
  RgbxyControlMesh,
  RgbxyVertexLayerWeights,
  ReconstructedLayerComposite,
  LayerCompositeDifference,
  PhotographicStructure,
  SmoothShading,
  MediumDescriptiveResidual,
  FineDescriptiveResidual,
  ConditionalPlaneY,
  ConditionalPlaneAB,
  Phase33YReconstruction,
  Phase33ABReconstruction,
  Phase33PreVeil,
  Phase33PreSoftness,
  Phase33DifferenceFromOriginal,
  TransitionSolverResidual,
  Phase4SourceBoundaryStrength,
  Phase4BoundaryHierarchy,
  Phase4AtomicRegions,
  Phase4LatentComponent,
  Phase4LatentComposite,
  Phase4LatentReconstructionError,
  Phase4SpectralResidual,
  Phase4ComponentRecoveryError,
  Phase4AppearanceUnmixingError,
  Phase4PlateAlpha,
  Phase4PlateYSupport,
  Phase4PlateABSupport,
  Phase4PlateYAppearance,
  Phase4PlateABAppearance,
  Phase4PlateOverlapComposite,
  Phase4YRegionHierarchy,
  Phase4ABRegionHierarchy,
  Phase4RemovedBoundaries,
  Phase4RetainedBoundaries,
  Phase4YChunks,
  Phase4ABChunks,
  Phase4SourceGradientField,
  Phase4SimplifiedGradientField,
  Phase4GradientReconstruction,
  Phase4PrimitiveFitError,
  Phase4PreSpill,
  Phase4SpillInfluence,
  Phase4PostSpill,
  Phase4DifferenceFromSource
};

enum class PictorialDebugPlane { PlaneA = 0, PlaneB, PlaneC, PlaneD, Composite };
enum class TransitionSolverMode { JacobiFast = 0, MultigridReference };
enum class PigmentComputeBackend { Auto = 0, Metal, CpuReference };

struct PigmentPhase33Params {
  SoftPlateConstructionParams plates{};
  PhotographicDecompositionParams decomposition{};
  TransitionSolverMode transitionSolver = TransitionSolverMode::MultigridReference;
  PigmentComputeBackend backend = PigmentComputeBackend::Auto;
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
  float modeSelectivity = 0.65f;
  float boundaryScale = 12.0f;
  float veilTonalBias = 0.0f;
  float chromaLumaCoupling = 0.6f;
  WorkingGamut gamut = WorkingGamut::ACEScg;
  bool invertMask = false;
  bool premultiplied = false;
  float mix = 1.0f;
  PigmentComparisonMode comparison = PigmentComparisonMode::RepresentativeModePigment;
  PigmentDebugView debugView = PigmentDebugView::Final;
  PictorialDebugPlane debugPlane = PictorialDebugPlane::Composite;
  PictorialPlanesParams pictorial{};
  PigmentPhase33Params phase33{};
  Phase4Params phase4{};
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

// Density-seeking comparison reference. Candidate density is measured against the
// same physical 9x9 support, then the four strongest source-population candidates
// are blended with a selectivity-controlled softmax rather than averaged globally.
void representativeRegionMassReference(ConstYabPlanes source, YabPlanes destination,
                                       ScalarFieldView processingStrength,
                                       ScalarFieldView boundaryProtection,
                                       const IntegratedPigmentParams& params,
                                       const ExecutionContext& execution = {});

}  // namespace pigment
