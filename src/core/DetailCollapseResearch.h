#pragma once

#include "core/ColorSpace.h"
#include "core/Execution.h"
#include "core/Masking.h"
#include "core/SpatialOperator.h"
#include "core/Types.h"

namespace pigment {

enum class DetailCollapseDebugView {
  Final = 0,
  Original,
  ConsolidationSeed,
  StructureGuide,
  ProcessingStrength,
  BoundaryProtection,
  RollingIteration1,
  RollingIteration2,
  RollingIteration3,
  RollingIteration4,
  YMassResult,
  ABMassResult,
  InternalVariationResidual,
  PreReintegrationMass,
  DifferenceFromOriginal
};

enum class DetailCollapseBackend {
  ReferenceBilateralCpu = 0,
  GuidedCpu,
  DomainTransformCpu,
  GuidedMetal,
  DomainTransformMetal
};

struct DetailCollapseResearchParams {
  DetailCollapseBackend backend = DetailCollapseBackend::ReferenceBilateralCpu;
  float amount = 0.0f;
  float massScale = 4.0f;
  float structureScale = 5.0f;
  float massStrength = 0.65f;
  float toneSimilarity = 0.35f;
  float chromaSimilarity = 0.12f;
  float boundaryPreserve = 0.9f;
  float boundarySoftness = 0.08f;
  float structurePreserve = 1.0f;
  float internalVariation = 0.15f;
  float lumaMassing = 1.0f;
  float chromaMassing = 1.0f;
  TonalMaskOptions tonalMask{};
  bool invertMask = false;
  float mix = 1.0f;
  bool premultiplied = false;
  WorkingGamut gamut = WorkingGamut::ACEScg;
  DetailCollapseDebugView debugView = DetailCollapseDebugView::Final;
};

InputDomainRequest detailCollapseResearchInputDomain() noexcept;

void processDetailCollapseResearch(
    ConstImageView source, ImageView destination, RectI outputRegion,
    const DetailCollapseResearchParams& params, const ImageGeometry& geometry,
    const ConstFloatPlaneView* externalMask,
    const ExecutionContext& execution = {});

}  // namespace pigment
