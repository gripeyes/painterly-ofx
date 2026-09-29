#include "plugins/Pigment.h"

#include "core/IntegratedPigment.h"
#include "core/PigmentPhase33.h"
#include "core/PigmentPhase4.h"
#include "ofx/OfxImageHelpers.h"
#include "ofx/ParameterHelpers.h"
#include "ofxGPURender.h"
#ifdef PIGMENT_ENABLE_METAL
#include "metal/PigmentMetal.h"
#endif

#include <algorithm>
#include <array>
#include <memory>
#include <sstream>

namespace pigment::plugin {
namespace {

constexpr const char* kMaskClip = "Mask";
constexpr const char* kPlaneMapClip = "PlaneMap";
constexpr const char* kAmount = "amount";
constexpr const char* kMassScale = "massScale";
constexpr const char* kMassStrength = "massStrength";
constexpr const char* kToneSimilarity = "toneSimilarity";
constexpr const char* kChromaSimilarity = "chromaSimilarity";
constexpr const char* kLumaAttraction = "lumaAttraction";
constexpr const char* kChromaAttraction = "chromaAttraction";
constexpr const char* kStructureScale = "structureScale";
constexpr const char* kStructurePreserve = "structurePreserve";
constexpr const char* kBoundaryPreserve = "boundaryPreserve";
constexpr const char* kBoundaryExtinction = "boundaryExtinction";
constexpr const char* kBoundarySoftness = "boundarySoftness";
constexpr const char* kVeilAmount = "veilAmount";
constexpr const char* kVeilScale = "veilScale";
constexpr const char* kVeilIrregularity = "veilIrregularity";
constexpr const char* kVeilContrast = "veilContrast";
constexpr const char* kVeilSeed = "veilSeed";
constexpr const char* kDetailCleanup = "detailCleanup";
constexpr const char* kFineDetail = "fineDetail";
constexpr const char* kMediumDetail = "mediumDetail";
constexpr const char* kInternalVariation = "internalVariation";
constexpr const char* kChromaMigration = "chromaMigration";
constexpr const char* kChromaScale = "chromaScale";
constexpr const char* kChromaEdgeRespect = "chromaEdgeRespect";
constexpr const char* kRegionSoftness = "regionSoftness";
constexpr const char* kModeSelectivity = "modeSelectivity";
constexpr const char* kBoundaryScale = "boundaryScale";
constexpr const char* kVeilTonalBias = "veilTonalBias";
constexpr const char* kChromaLumaCoupling = "chromaLumaCoupling";
constexpr const char* kWorkingGamut = "workingGamut";
constexpr const char* kInvertMask = "invertMask";
constexpr const char* kComparison = "comparisonMode";
constexpr const char* kDebug = "debugView";
constexpr const char* kMix = "mix";
constexpr const char* kFineExtinction = "fineExtinction";
constexpr const char* kMediumExtinction = "mediumExtinction";
constexpr const char* kBroadRetention = "broadRetention";
constexpr const char* kDetailStructurePreserve = "detailStructurePreserve";
constexpr const char* kYTransitionWidth = "yTransitionWidth";
constexpr const char* kABTransitionWidth = "abTransitionWidth";
constexpr const char* kTransitionStructureRespect = "transitionStructureRespect";
constexpr const char* kLocalSoftness = "localSoftness";
constexpr const char* kDebugPlane = "debugPlane";
constexpr const char* kPlaneSource = "phase33PlaneSource";
constexpr const char* kAutomaticOccupancy = "phase33AutomaticOccupancy";
constexpr const char* kAutomaticPlaneStrength = "phase33AutomaticPlaneStrength";
constexpr const char* kSpatialCoherence = "phase33SpatialCoherence";
constexpr const char* kColorCoherence = "phase33ColorCoherence";
constexpr const char* kHybridGuidance = "phase33HybridGuidance";
constexpr const char* kShadingModel = "phase33ShadingModel";
constexpr const char* kSmoothSimplification = "phase33SmoothSimplification";
constexpr const char* kShadingStructurePreserve = "phase33ShadingStructurePreserve";
constexpr const char* kChromaShadingRetention = "phase33ChromaShadingRetention";
constexpr const char* kPhase33StructurePreserve = "phase33StructurePreserve";
constexpr const char* kTransitionSolver = "phase33TransitionSolver";
constexpr const char* kComputeBackend = "phase33ComputeBackend";
constexpr const char* kPhase4PlateCount = "phase4PlateCount";
constexpr const char* kPhase4LatentCount = "phase4LatentCount";
constexpr const char* kPhase4PlateScale = "phase4PlateScale";
constexpr const char* kPhase4PlateOverlap = "phase4PlateOverlap";
constexpr const char* kPhase4BoundaryLock = "phase4BoundaryLock";
constexpr const char* kPhase4Coupling = "phase4LumaChromaCoupling";
constexpr const char* kPhase4LumaChunkScale = "phase4LumaChunkScale";
constexpr const char* kPhase4ChromaChunkScale = "phase4ChromaChunkScale";
constexpr const char* kPhase4MergeSelectivity = "phase4MergeSelectivity";
constexpr const char* kPhase4InternalVariation = "phase4InternalVariation";
constexpr const char* kPhase4GradientComplexity = "phase4GradientComplexity";
constexpr const char* kPhase4SpillAmount = "phase4SpillAmount";
constexpr const char* kPhase4SpillReach = "phase4SpillReach";
constexpr const char* kPhase4SpillAsymmetry = "phase4SpillAsymmetry";
constexpr const char* kPhase4ChromaSpill = "phase4ChromaSpill";
constexpr const char* kPhase4LumaSpill = "phase4LumaSpill";
constexpr const char* kPhase4StructureRespect = "phase4StructureRespect";
constexpr const char* kPhase4DebugLatent = "phase4DebugLatent";
constexpr const char* kPhase4DebugPlate = "phase4DebugPlate";
constexpr std::array<const char*, 8> kPhase4PlateEnable{"phase4PlateAEnable","phase4PlateBEnable","phase4PlateCEnable","phase4PlateDEnable","phase4PlateEEnable","phase4PlateFEnable","phase4PlateGEnable","phase4PlateHEnable"};
constexpr std::array<const char*, 8> kPhase4PlateWeight{"phase4PlateAWeight","phase4PlateBWeight","phase4PlateCWeight","phase4PlateDWeight","phase4PlateEWeight","phase4PlateFWeight","phase4PlateGWeight","phase4PlateHWeight"};
constexpr std::array<const char*, 8> kPhase4PlateTone{"phase4PlateATone","phase4PlateBTone","phase4PlateCTone","phase4PlateDTone","phase4PlateETone","phase4PlateFTone","phase4PlateGTone","phase4PlateHTone"};
constexpr std::array<const char*, 8> kPhase4PlateBiasA{"phase4PlateABiasA","phase4PlateBBiasA","phase4PlateCBiasA","phase4PlateDBiasA","phase4PlateEBiasA","phase4PlateFBiasA","phase4PlateGBiasA","phase4PlateHBiasA"};
constexpr std::array<const char*, 8> kPhase4PlateBiasB{"phase4PlateABiasB","phase4PlateBBiasB","phase4PlateCBiasB","phase4PlateDBiasB","phase4PlateEBiasB","phase4PlateFBiasB","phase4PlateGBiasB","phase4PlateHBiasB"};
constexpr std::array<const char*, 8> kPhase4PlateSpillOut{"phase4PlateASpillOut","phase4PlateBSpillOut","phase4PlateCSpillOut","phase4PlateDSpillOut","phase4PlateESpillOut","phase4PlateFSpillOut","phase4PlateGSpillOut","phase4PlateHSpillOut"};
constexpr std::array<const char*, 8> kPhase4PlateReceive{"phase4PlateAReceive","phase4PlateBReceive","phase4PlateCReceive","phase4PlateDReceive","phase4PlateEReceive","phase4PlateFReceive","phase4PlateGReceive","phase4PlateHReceive"};
constexpr std::array<const char*, 4> kPlaneEnable{
    "planeAEnable", "planeBEnable", "planeCEnable", "planeDEnable"};
constexpr std::array<const char*, 4> kPlaneAmount{
    "planeAAmount", "planeBAmount", "planeCAmount", "planeDAmount"};
constexpr std::array<const char*, 4> kPlaneSourceMix{
    "planeASourceMix", "planeBSourceMix", "planeCSourceMix", "planeDSourceMix"};
constexpr std::array<const char*, 4> kPlaneManualTarget{
    "planeAManualTarget", "planeBManualTarget", "planeCManualTarget", "planeDManualTarget"};
constexpr std::array<const char*, 4> kPlaneYOffset{
    "planeAYOffset", "planeBYOffset", "planeCYOffset", "planeDYOffset"};
constexpr std::array<const char*, 4> kPlaneToneInfluence{
    "planeAToneInfluence", "planeBToneInfluence", "planeCToneInfluence", "planeDToneInfluence"};
constexpr std::array<const char*, 4> kPlaneABias{
    "planeAChromaBiasA", "planeBChromaBiasA", "planeCChromaBiasA", "planeDChromaBiasA"};
constexpr std::array<const char*, 4> kPlaneBBias{
    "planeAChromaBiasB", "planeBChromaBiasB", "planeCChromaBiasB", "planeDChromaBiasB"};
constexpr std::array<const char*, 4> kPlaneChromaInfluence{
    "planeAChromaInfluence", "planeBChromaInfluence", "planeCChromaInfluence", "planeDChromaInfluence"};

class PigmentEffect final : public OFX::ImageEffect {
 public:
  explicit PigmentEffect(OfxImageEffectHandle handle)
      : ImageEffect(handle), destination_(fetchClip(kOfxImageEffectOutputClipName)),
        source_(fetchClip(kOfxImageEffectSimpleSourceClipName)) {
    if (getContext() == OFX::eContextGeneral) {
      mask_ = fetchClip(kMaskClip);
      planeMap_ = fetchClip(kPlaneMapClip);
    }
#define FETCH_DOUBLE(member, name) member = fetchDoubleParam(name)
    FETCH_DOUBLE(amount_, kAmount); FETCH_DOUBLE(massScale_, kMassScale);
    FETCH_DOUBLE(massStrength_, kMassStrength); FETCH_DOUBLE(toneSimilarity_, kToneSimilarity);
    FETCH_DOUBLE(chromaSimilarity_, kChromaSimilarity); FETCH_DOUBLE(lumaAttraction_, kLumaAttraction);
    FETCH_DOUBLE(chromaAttraction_, kChromaAttraction); FETCH_DOUBLE(structureScale_, kStructureScale);
    FETCH_DOUBLE(structurePreserve_, kStructurePreserve); FETCH_DOUBLE(boundaryPreserve_, kBoundaryPreserve);
    FETCH_DOUBLE(boundaryExtinction_, kBoundaryExtinction); FETCH_DOUBLE(boundarySoftness_, kBoundarySoftness);
    FETCH_DOUBLE(veilAmount_, kVeilAmount); FETCH_DOUBLE(veilScale_, kVeilScale);
    FETCH_DOUBLE(veilIrregularity_, kVeilIrregularity); FETCH_DOUBLE(veilContrast_, kVeilContrast);
    FETCH_DOUBLE(detailCleanup_, kDetailCleanup); FETCH_DOUBLE(fineDetail_, kFineDetail);
    FETCH_DOUBLE(mediumDetail_, kMediumDetail); FETCH_DOUBLE(internalVariation_, kInternalVariation);
    FETCH_DOUBLE(chromaMigration_, kChromaMigration); FETCH_DOUBLE(chromaScale_, kChromaScale);
    FETCH_DOUBLE(chromaEdgeRespect_, kChromaEdgeRespect); FETCH_DOUBLE(regionSoftness_, kRegionSoftness);
    FETCH_DOUBLE(modeSelectivity_, kModeSelectivity);
    FETCH_DOUBLE(boundaryScale_, kBoundaryScale); FETCH_DOUBLE(veilTonalBias_, kVeilTonalBias);
    FETCH_DOUBLE(chromaLumaCoupling_, kChromaLumaCoupling); FETCH_DOUBLE(mix_, kMix);
#undef FETCH_DOUBLE
    veilSeed_ = fetchIntParam(kVeilSeed);
    invertMask_ = fetchBooleanParam(kInvertMask);
    gamut_ = fetchChoiceParam(kWorkingGamut);
    comparison_ = fetchChoiceParam(kComparison);
    debug_ = fetchChoiceParam(kDebug);
    debugPlane_ = fetchChoiceParam(kDebugPlane);
    fineExtinction_ = fetchDoubleParam(kFineExtinction);
    mediumExtinction_ = fetchDoubleParam(kMediumExtinction);
    broadRetention_ = fetchDoubleParam(kBroadRetention);
    detailStructurePreserve_ = fetchDoubleParam(kDetailStructurePreserve);
    yTransitionWidth_ = fetchDoubleParam(kYTransitionWidth);
    abTransitionWidth_ = fetchDoubleParam(kABTransitionWidth);
    transitionStructureRespect_ = fetchDoubleParam(kTransitionStructureRespect);
    localSoftness_ = fetchDoubleParam(kLocalSoftness);
    planeSource_ = fetchChoiceParam(kPlaneSource);
    automaticOccupancy_ = fetchDoubleParam(kAutomaticOccupancy);
    automaticPlaneStrength_ = fetchDoubleParam(kAutomaticPlaneStrength);
    spatialCoherence_ = fetchDoubleParam(kSpatialCoherence);
    colorCoherence_ = fetchDoubleParam(kColorCoherence);
    hybridGuidance_ = fetchDoubleParam(kHybridGuidance);
    shadingModel_ = fetchChoiceParam(kShadingModel);
    smoothSimplification_ = fetchDoubleParam(kSmoothSimplification);
    shadingStructurePreserve_ = fetchDoubleParam(kShadingStructurePreserve);
    chromaShadingRetention_ = fetchDoubleParam(kChromaShadingRetention);
    phase33StructurePreserve_ = fetchDoubleParam(kPhase33StructurePreserve);
    transitionSolver_ = fetchChoiceParam(kTransitionSolver);
    computeBackend_ = fetchChoiceParam(kComputeBackend);
    phase4PlateCount_ = fetchIntParam(kPhase4PlateCount);
    phase4LatentCount_ = fetchIntParam(kPhase4LatentCount);
    phase4DebugLatent_ = fetchIntParam(kPhase4DebugLatent);
    phase4DebugPlate_ = fetchIntParam(kPhase4DebugPlate);
#define FETCH_PHASE4(member, name) member = fetchDoubleParam(name)
    FETCH_PHASE4(phase4PlateScale_, kPhase4PlateScale); FETCH_PHASE4(phase4PlateOverlap_, kPhase4PlateOverlap);
    FETCH_PHASE4(phase4BoundaryLock_, kPhase4BoundaryLock); FETCH_PHASE4(phase4Coupling_, kPhase4Coupling);
    FETCH_PHASE4(phase4LumaChunkScale_, kPhase4LumaChunkScale); FETCH_PHASE4(phase4ChromaChunkScale_, kPhase4ChromaChunkScale);
    FETCH_PHASE4(phase4MergeSelectivity_, kPhase4MergeSelectivity); FETCH_PHASE4(phase4InternalVariation_, kPhase4InternalVariation);
    FETCH_PHASE4(phase4GradientComplexity_, kPhase4GradientComplexity); FETCH_PHASE4(phase4SpillAmount_, kPhase4SpillAmount);
    FETCH_PHASE4(phase4SpillReach_, kPhase4SpillReach); FETCH_PHASE4(phase4SpillAsymmetry_, kPhase4SpillAsymmetry);
    FETCH_PHASE4(phase4ChromaSpill_, kPhase4ChromaSpill); FETCH_PHASE4(phase4LumaSpill_, kPhase4LumaSpill);
    FETCH_PHASE4(phase4StructureRespect_, kPhase4StructureRespect);
#undef FETCH_PHASE4
    for(int i=0;i<8;++i){phase4PlateEnable_[i]=fetchBooleanParam(kPhase4PlateEnable[i]);phase4PlateWeight_[i]=fetchDoubleParam(kPhase4PlateWeight[i]);phase4PlateTone_[i]=fetchDoubleParam(kPhase4PlateTone[i]);phase4PlateBiasA_[i]=fetchDoubleParam(kPhase4PlateBiasA[i]);phase4PlateBiasB_[i]=fetchDoubleParam(kPhase4PlateBiasB[i]);phase4PlateSpillOut_[i]=fetchDoubleParam(kPhase4PlateSpillOut[i]);phase4PlateReceive_[i]=fetchDoubleParam(kPhase4PlateReceive[i]);}
    for (int i = 0; i < 4; ++i) {
      planeEnable_[i] = fetchBooleanParam(kPlaneEnable[i]);
      planeAmount_[i] = fetchDoubleParam(kPlaneAmount[i]);
      planeSourceMix_[i] = fetchDoubleParam(kPlaneSourceMix[i]);
      planeManualTarget_[i] = fetchRGBParam(kPlaneManualTarget[i]);
      planeYOffset_[i] = fetchDoubleParam(kPlaneYOffset[i]);
      planeToneInfluence_[i] = fetchDoubleParam(kPlaneToneInfluence[i]);
      planeABias_[i] = fetchDoubleParam(kPlaneABias[i]);
      planeBBias_[i] = fetchDoubleParam(kPlaneBBias[i]);
      planeChromaInfluence_[i] = fetchDoubleParam(kPlaneChromaInfluence[i]);
    }
  }

  void render(const OFX::RenderArguments& args) override;
  bool isIdentity(const OFX::IsIdentityArguments& args, OFX::Clip*& clip,
                  double& identityTime) override;
  bool getRegionOfDefinition(const OFX::RegionOfDefinitionArguments& args,
                             OfxRectD& rod) override;
  void getRegionsOfInterest(const OFX::RegionsOfInterestArguments& args,
                            OFX::RegionOfInterestSetter& rois) override;
  void purgeCaches() override;
  void endSequenceRender(const OFX::EndSequenceRenderArguments&) override;

 private:
  IntegratedPigmentParams parameters(double time) const;
  OFX::Clip *destination_ = nullptr, *source_ = nullptr, *mask_ = nullptr, *planeMap_ = nullptr;
  OFX::DoubleParam *amount_ = nullptr, *massScale_ = nullptr, *massStrength_ = nullptr,
      *toneSimilarity_ = nullptr, *chromaSimilarity_ = nullptr, *lumaAttraction_ = nullptr,
      *chromaAttraction_ = nullptr, *structureScale_ = nullptr, *structurePreserve_ = nullptr,
      *boundaryPreserve_ = nullptr, *boundaryExtinction_ = nullptr, *boundarySoftness_ = nullptr,
      *veilAmount_ = nullptr, *veilScale_ = nullptr, *veilIrregularity_ = nullptr,
      *veilContrast_ = nullptr, *detailCleanup_ = nullptr, *fineDetail_ = nullptr,
      *mediumDetail_ = nullptr, *internalVariation_ = nullptr, *chromaMigration_ = nullptr,
      *chromaScale_ = nullptr, *chromaEdgeRespect_ = nullptr, *regionSoftness_ = nullptr,
      *modeSelectivity_ = nullptr,
      *boundaryScale_ = nullptr, *veilTonalBias_ = nullptr, *chromaLumaCoupling_ = nullptr,
      *mix_ = nullptr;
  OFX::DoubleParam *fineExtinction_ = nullptr, *mediumExtinction_ = nullptr,
      *broadRetention_ = nullptr, *detailStructurePreserve_ = nullptr,
      *yTransitionWidth_ = nullptr, *abTransitionWidth_ = nullptr,
      *transitionStructureRespect_ = nullptr, *localSoftness_ = nullptr;
  OFX::DoubleParam *automaticOccupancy_ = nullptr, *automaticPlaneStrength_ = nullptr,
      *spatialCoherence_ = nullptr, *colorCoherence_ = nullptr,
      *hybridGuidance_ = nullptr, *smoothSimplification_ = nullptr,
      *shadingStructurePreserve_ = nullptr, *chromaShadingRetention_ = nullptr,
      *phase33StructurePreserve_ = nullptr;
  std::array<OFX::BooleanParam*, 4> planeEnable_{};
  std::array<OFX::DoubleParam*, 4> planeAmount_{}, planeSourceMix_{}, planeYOffset_{},
      planeToneInfluence_{}, planeABias_{}, planeBBias_{}, planeChromaInfluence_{};
  std::array<OFX::RGBParam*, 4> planeManualTarget_{};
  OFX::IntParam* veilSeed_ = nullptr;
  OFX::IntParam *phase4PlateCount_ = nullptr, *phase4LatentCount_ = nullptr,
      *phase4DebugLatent_ = nullptr, *phase4DebugPlate_ = nullptr;
  OFX::BooleanParam* invertMask_ = nullptr;
  OFX::ChoiceParam *gamut_ = nullptr, *comparison_ = nullptr, *debug_ = nullptr,
      *debugPlane_ = nullptr, *planeSource_ = nullptr, *shadingModel_ = nullptr,
      *transitionSolver_ = nullptr, *computeBackend_ = nullptr;
  OFX::DoubleParam *phase4PlateScale_ = nullptr, *phase4PlateOverlap_ = nullptr,
      *phase4BoundaryLock_ = nullptr, *phase4Coupling_ = nullptr,
      *phase4LumaChunkScale_ = nullptr, *phase4ChromaChunkScale_ = nullptr,
      *phase4MergeSelectivity_ = nullptr, *phase4InternalVariation_ = nullptr,
      *phase4GradientComplexity_ = nullptr, *phase4SpillAmount_ = nullptr,
      *phase4SpillReach_ = nullptr, *phase4SpillAsymmetry_ = nullptr,
      *phase4ChromaSpill_ = nullptr, *phase4LumaSpill_ = nullptr,
      *phase4StructureRespect_ = nullptr;
  std::array<OFX::BooleanParam*,8> phase4PlateEnable_{};
  std::array<OFX::DoubleParam*,8> phase4PlateWeight_{},phase4PlateTone_{},phase4PlateBiasA_{},
      phase4PlateBiasB_{},phase4PlateSpillOut_{},phase4PlateReceive_{};
#ifdef PIGMENT_ENABLE_METAL
  std::unique_ptr<metal::MetalInstance> metal_;
#endif
};

IntegratedPigmentParams PigmentEffect::parameters(double time) const {
  IntegratedPigmentParams p;
#define VALUE(member, field) p.field = static_cast<float>(member->getValueAtTime(time))
  VALUE(amount_, amount); VALUE(massScale_, massScale); VALUE(massStrength_, massStrength);
  VALUE(toneSimilarity_, toneSimilarity); VALUE(chromaSimilarity_, chromaSimilarity);
  VALUE(lumaAttraction_, lumaAttraction); VALUE(chromaAttraction_, chromaAttraction);
  VALUE(structureScale_, structureScale); VALUE(structurePreserve_, structurePreserve);
  VALUE(boundaryPreserve_, boundaryPreserve); VALUE(boundaryExtinction_, boundaryExtinction);
  VALUE(boundarySoftness_, boundarySoftness); VALUE(veilAmount_, veilAmount);
  VALUE(veilScale_, veilScale); VALUE(veilIrregularity_, veilIrregularity);
  VALUE(veilContrast_, veilContrast); VALUE(detailCleanup_, detailCleanup);
  VALUE(fineDetail_, fineDetail); VALUE(mediumDetail_, mediumDetail);
  VALUE(internalVariation_, internalVariation); VALUE(chromaMigration_, chromaMigration);
  VALUE(chromaScale_, chromaScale); VALUE(chromaEdgeRespect_, chromaEdgeRespect);
  VALUE(regionSoftness_, regionSoftness); VALUE(boundaryScale_, boundaryScale);
  VALUE(modeSelectivity_, modeSelectivity);
  VALUE(veilTonalBias_, veilTonalBias); VALUE(chromaLumaCoupling_, chromaLumaCoupling);
  VALUE(mix_, mix);
#undef VALUE
  p.veilSeed = veilSeed_->getValueAtTime(time);
  p.invertMask = invertMask_->getValueAtTime(time);
  int value = 0;
  gamut_->getValueAtTime(time, value);
  p.gamut = static_cast<WorkingGamut>(std::max(0, std::min(3, value)));
  comparison_->getValueAtTime(time, value);
  p.comparison = static_cast<PigmentComparisonMode>(std::max(0, std::min(6, value)));
  debug_->getValueAtTime(time, value);
  p.debugView = static_cast<PigmentDebugView>(std::max(0, std::min(96, value)));
  debugPlane_->getValueAtTime(time, value);
  p.debugPlane = static_cast<PictorialDebugPlane>(std::max(0, std::min(4, value)));
  p.pictorial.fineExtinction = static_cast<float>(fineExtinction_->getValueAtTime(time));
  p.pictorial.mediumExtinction = static_cast<float>(mediumExtinction_->getValueAtTime(time));
  p.pictorial.broadRetention = static_cast<float>(broadRetention_->getValueAtTime(time));
  p.pictorial.detailStructurePreserve = static_cast<float>(detailStructurePreserve_->getValueAtTime(time));
  p.pictorial.yTransitionWidth = static_cast<float>(yTransitionWidth_->getValueAtTime(time));
  p.pictorial.abTransitionWidth = static_cast<float>(abTransitionWidth_->getValueAtTime(time));
  p.pictorial.transitionStructureRespect = static_cast<float>(transitionStructureRespect_->getValueAtTime(time));
  p.pictorial.localSoftness = static_cast<float>(localSoftness_->getValueAtTime(time));
  planeSource_->getValueAtTime(time, value);
  p.phase33.plates.source = static_cast<PlaneSourceMode>(std::max(0, std::min(3, value)));
  p.phase33.plates.automaticOccupancy = static_cast<float>(automaticOccupancy_->getValueAtTime(time));
  p.phase33.plates.automaticPlaneStrength = static_cast<float>(automaticPlaneStrength_->getValueAtTime(time));
  p.phase33.plates.spatialCoherence = static_cast<float>(spatialCoherence_->getValueAtTime(time));
  p.phase33.plates.colorCoherence = static_cast<float>(colorCoherence_->getValueAtTime(time));
  p.phase33.plates.hybridGuidance = static_cast<float>(hybridGuidance_->getValueAtTime(time));
  shadingModel_->getValueAtTime(time, value);
  p.phase33.decomposition.model = static_cast<PhotographicShadingModel>(std::max(0, std::min(2, value)));
  p.phase33.decomposition.smoothSimplification = static_cast<float>(smoothSimplification_->getValueAtTime(time));
  p.phase33.decomposition.shadingStructurePreserve = static_cast<float>(shadingStructurePreserve_->getValueAtTime(time));
  p.phase33.decomposition.chromaShadingRetention = static_cast<float>(chromaShadingRetention_->getValueAtTime(time));
  p.phase33.decomposition.fineExtinction = p.pictorial.fineExtinction;
  p.phase33.decomposition.mediumExtinction = p.pictorial.mediumExtinction;
  p.phase33.decomposition.detailStructurePreserve = p.pictorial.detailStructurePreserve;
  p.phase33.decomposition.structurePreserve = static_cast<float>(phase33StructurePreserve_->getValueAtTime(time));
  transitionSolver_->getValueAtTime(time, value);
  p.phase33.transitionSolver = static_cast<TransitionSolverMode>(std::max(0, std::min(1, value)));
  computeBackend_->getValueAtTime(time, value);
  p.phase33.backend = static_cast<PigmentComputeBackend>(std::max(0, std::min(2, value)));
  p.phase4.plateCount = std::max(4, std::min(8, phase4PlateCount_->getValueAtTime(time)));
  p.phase4.latentCount = std::max(12, std::min(24, phase4LatentCount_->getValueAtTime(time)));
  p.phase4.debugLatent = std::max(0, std::min(23, phase4DebugLatent_->getValueAtTime(time)-1));
  p.phase4.debugPlate = std::max(0, std::min(7, phase4DebugPlate_->getValueAtTime(time)));
#define VALUE_PHASE4(member, field) p.phase4.field = static_cast<float>(member->getValueAtTime(time))
  VALUE_PHASE4(phase4PlateScale_, plateScale); VALUE_PHASE4(phase4PlateOverlap_, plateOverlap);
  VALUE_PHASE4(phase4BoundaryLock_, boundaryLock); VALUE_PHASE4(phase4Coupling_, lumaChromaCoupling);
  VALUE_PHASE4(phase4LumaChunkScale_, lumaChunkScale); VALUE_PHASE4(phase4ChromaChunkScale_, chromaChunkScale);
  VALUE_PHASE4(phase4MergeSelectivity_, mergeSelectivity); VALUE_PHASE4(phase4InternalVariation_, internalVariation);
  VALUE_PHASE4(phase4GradientComplexity_, gradientComplexity); VALUE_PHASE4(phase4SpillAmount_, spillAmount);
  VALUE_PHASE4(phase4SpillReach_, spillReach); VALUE_PHASE4(phase4SpillAsymmetry_, spillAsymmetry);
  VALUE_PHASE4(phase4ChromaSpill_, chromaSpill); VALUE_PHASE4(phase4LumaSpill_, lumaSpill);
  VALUE_PHASE4(phase4StructureRespect_, structureRespect);
#undef VALUE_PHASE4
  for(int i=0;i<8;++i){auto&control=p.phase4.plates[i];control.enabled=phase4PlateEnable_[i]->getValueAtTime(time);control.weight=float(phase4PlateWeight_[i]->getValueAtTime(time));control.tone=float(phase4PlateTone_[i]->getValueAtTime(time));control.biasA=float(phase4PlateBiasA_[i]->getValueAtTime(time));control.biasB=float(phase4PlateBiasB_[i]->getValueAtTime(time));control.spillOut=float(phase4PlateSpillOut_[i]->getValueAtTime(time));control.receiveSpill=float(phase4PlateReceive_[i]->getValueAtTime(time));}
  MatrixOpponentTransform opponent(p.gamut);
  for (int i = 0; i < 4; ++i) {
    auto& plane = p.pictorial.planes[i];
    plane.enabled = planeEnable_[i]->getValueAtTime(time);
    plane.amount = static_cast<float>(planeAmount_[i]->getValueAtTime(time));
    plane.sourceMix = static_cast<float>(planeSourceMix_[i]->getValueAtTime(time));
    double r = 0.18, g = 0.18, b = 0.18;
    planeManualTarget_[i]->getValueAtTime(time, r, g, b);
    plane.manualTarget = opponent.toYab({static_cast<float>(r), static_cast<float>(g),
                                         static_cast<float>(b)});
    plane.yOffset = static_cast<float>(planeYOffset_[i]->getValueAtTime(time));
    plane.toneInfluence = static_cast<float>(planeToneInfluence_[i]->getValueAtTime(time));
    plane.aBias = static_cast<float>(planeABias_[i]->getValueAtTime(time));
    plane.bBias = static_cast<float>(planeBBias_[i]->getValueAtTime(time));
    plane.chromaInfluence = static_cast<float>(planeChromaInfluence_[i]->getValueAtTime(time));
  }
  return p;
}

bool PigmentEffect::isIdentity(const OFX::IsIdentityArguments& args, OFX::Clip*& clip,
                               double& identityTime) {
  const auto p = parameters(args.time);
  if (p.comparison == PigmentComparisonMode::Original ||
      (p.comparison == PigmentComparisonMode::PictorialPlanes &&
       (!planeMap_ || !planeMap_->isConnected()) && p.debugView == PigmentDebugView::Final) ||
      (p.comparison == PigmentComparisonMode::SoftPictorialPlates &&
       p.phase33.plates.source == PlaneSourceMode::Manual &&
       (!planeMap_ || !planeMap_->isConnected()) && p.debugView == PigmentDebugView::Final) ||
      (p.debugView == PigmentDebugView::Final && (p.amount == 0.0f || p.mix == 0.0f))) {
    clip = source_; identityTime = args.time; return true;
  }
  return false;
}

bool PigmentEffect::getRegionOfDefinition(const OFX::RegionOfDefinitionArguments& args,
                                          OfxRectD& rod) {
  rod = source_->getRegionOfDefinition(args.time); return true;
}

void PigmentEffect::getRegionsOfInterest(const OFX::RegionsOfInterestArguments& args,
                                         OFX::RegionOfInterestSetter& rois) {
  const OfxRectD rod = source_->getRegionOfDefinition(args.time);
  rois.setRegionOfInterest(*source_, rod);
  if (mask_) rois.setRegionOfInterest(*mask_, rod);
  if (planeMap_ && parameters(args.time).comparison != PigmentComparisonMode::AutomaticPlateGraph)
    rois.setRegionOfInterest(*planeMap_, rod);
}

void PigmentEffect::render(const OFX::RenderArguments& args) {
  std::unique_ptr<OFX::Image> destination(destination_->fetchImage(args.time));
  std::unique_ptr<OFX::Image> source(source_->fetchImage(args.time));
  if (!destination || !source) OFX::throwSuiteStatusException(kOfxStatFailed);
  if (source->getPixelDepth() != OFX::eBitDepthFloat ||
      destination->getPixelDepth() != OFX::eBitDepthFloat ||
      source->getPixelComponents() != destination->getPixelComponents() ||
      (source->getPixelComponents() != OFX::ePixelComponentRGB &&
       source->getPixelComponents() != OFX::ePixelComponentRGBA))
    OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
  std::unique_ptr<OFX::Image> maskImage;
  if (mask_ && mask_->isConnected()) {
    maskImage.reset(mask_->fetchImage(args.time));
    if (!maskImage || maskImage->getPixelDepth() != OFX::eBitDepthFloat)
      OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
  }
  auto p = parameters(args.time);
  p.premultiplied = source->getPreMultiplication() == OFX::eImagePreMultiplied;
  std::unique_ptr<OFX::Image> planeMapImage;
  if (p.comparison != PigmentComparisonMode::AutomaticPlateGraph &&
      planeMap_ && planeMap_->isConnected()) {
    planeMapImage.reset(planeMap_->fetchImage(args.time));
    if (!planeMapImage || planeMapImage->getPixelDepth() != OFX::eBitDepthFloat ||
        planeMapImage->getPixelComponents() != OFX::ePixelComponentRGBA ||
        planeMapImage->getPixelAspectRatio() != source->getPixelAspectRatio() ||
        ofx::toRect(planeMapImage->getBounds()).x1 != ofx::toRect(source->getBounds()).x1 ||
        ofx::toRect(planeMapImage->getBounds()).y1 != ofx::toRect(source->getBounds()).y1 ||
        ofx::toRect(planeMapImage->getBounds()).x2 != ofx::toRect(source->getBounds()).x2 ||
        ofx::toRect(planeMapImage->getBounds()).y2 != ofx::toRect(source->getBounds()).y2) {
      setPersistentMessage(OFX::Message::eMessageError, "PigmentPlaneMap",
                           "Plane Map must be float RGBA with the same bounds and PAR as Source.");
      OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
    }
  }
  if (p.comparison == PigmentComparisonMode::AutomaticPlateGraph) {
    if (args.isEnabledMetalRender)
      OFX::throwSuiteStatusException(kOfxStatGPURenderFailed);
    const auto sourceBounds = ofx::toRect(source->getBounds());
    const auto destinationBounds = ofx::toRect(destination->getBounds());
    ConstImageView sourceView{static_cast<const float*>(source->getPixelData()),
        source->getRowBytes() / static_cast<int>(sizeof(float)), sourceBounds,
        source->getPixelComponentCount()};
    ImageView destinationView{static_cast<float*>(destination->getPixelData()),
        destination->getRowBytes() / static_cast<int>(sizeof(float)), destinationBounds,
        destination->getPixelComponentCount()};
    ConstImageView maskView{};
    if (maskImage) maskView = {static_cast<const float*>(maskImage->getPixelData()),
        maskImage->getRowBytes() / static_cast<int>(sizeof(float)),
        ofx::toRect(maskImage->getBounds()), maskImage->getPixelComponentCount()};
    Phase4RenderInputs cpu{sourceView, destinationView, ofx::toRect(args.renderWindow), p,
        {source->getPixelAspectRatio(), args.renderScale.x, args.renderScale.y},
        maskImage ? &maskView : nullptr};
    const auto diagnostics = processPigmentPhase4(
        cpu, {[this] { return abort(); }, serialRows, nullptr});
    if (!diagnostics.gate.eigenspaceFinite || !diagnostics.gate.componentsFinite ||
        !diagnostics.gate.appearanceFinite) {
      setPersistentMessage(OFX::Message::eMessageError, "PigmentPhase4GateA",
                           "Phase 4 Gate A produced non-finite automatic-plate diagnostics.");
      OFX::throwSuiteStatusException(kOfxStatFailed);
    }
    clearPersistentMessage();
    return;
  }
  if (p.comparison == PigmentComparisonMode::SoftPictorialPlates) {
    if (args.isEnabledMetalRender)
      OFX::throwSuiteStatusException(kOfxStatGPURenderFailed);
    if (p.phase33.backend == PigmentComputeBackend::Metal) {
      setPersistentMessage(OFX::Message::eMessageError, "PigmentPhase33Metal",
                           "Phase 3.3 Metal is not available for this build; choose Auto or CPU Reference.");
      OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
    }
    const auto sourceBounds = ofx::toRect(source->getBounds());
    const auto destinationBounds = ofx::toRect(destination->getBounds());
    ConstImageView sourceView{static_cast<const float*>(source->getPixelData()),
        source->getRowBytes() / static_cast<int>(sizeof(float)), sourceBounds,
        source->getPixelComponentCount()};
    ImageView destinationView{static_cast<float*>(destination->getPixelData()),
        destination->getRowBytes() / static_cast<int>(sizeof(float)), destinationBounds,
        destination->getPixelComponentCount()};
    ConstImageView maskView{}, mapView{};
    if (maskImage) maskView = {static_cast<const float*>(maskImage->getPixelData()),
        maskImage->getRowBytes() / static_cast<int>(sizeof(float)), ofx::toRect(maskImage->getBounds()),
        maskImage->getPixelComponentCount()};
    if (planeMapImage) mapView = {static_cast<const float*>(planeMapImage->getPixelData()),
        planeMapImage->getRowBytes() / static_cast<int>(sizeof(float)), ofx::toRect(planeMapImage->getBounds()),
        planeMapImage->getPixelComponentCount()};
    Phase33RenderInputs cpu{sourceView, destinationView, ofx::toRect(args.renderWindow), p,
        {source->getPixelAspectRatio(), args.renderScale.x, args.renderScale.y},
        maskImage ? &maskView : nullptr, planeMapImage ? &mapView : nullptr};
    processPigmentPhase33(cpu, { [this] { return abort(); }, serialRows, nullptr });
    clearPersistentMessage();
    return;
  }
#ifdef PIGMENT_ENABLE_METAL
  if (!metal_) metal_ = std::make_unique<metal::MetalInstance>();
  const auto makeView = [](OFX::Image& image) {
    const RectI bounds = ofx::toRect(image.getBounds());
    const std::size_t bytes = image.getRowBytes() > 0
        ? static_cast<std::size_t>(image.getRowBytes()) * bounds.height() : 0;
    return metal::MetalImageView{image.getPixelData(), bytes, image.getRowBytes(),
                                bounds, image.getPixelComponentCount()};
  };
  metal::IntegratedMetalExecutionRequest request;
  request.source = makeView(*source); request.destination = makeView(*destination);
  request.nativeHostBuffers = args.isEnabledMetalRender;
  request.hostCommandQueue = args.pMetalCmdQ;
  request.renderWindow = ofx::toRect(args.renderWindow);
  request.params = p;
  request.geometry = {source->getPixelAspectRatio(), args.renderScale.x, args.renderScale.y};
  if (maskImage) { request.mask = makeView(*maskImage); request.hasMask = true; }
  if (planeMapImage) {
    request.planeMap = makeView(*planeMapImage); request.hasPlaneMap = true;
  }
  if (metal_->renderIntegrated(request)) { clearPersistentMessage(); return; }
  const auto& diagnostics = metal_->diagnostics();
  if (args.isEnabledMetalRender) OFX::throwSuiteStatusException(kOfxStatGPURenderFailed);
  std::ostringstream message;
  message << "Integrated Pigment requires Metal; execution failed: " << diagnostics.message;
  setPersistentMessage(OFX::Message::eMessageError, "PigmentMetal", message.str());
  OFX::throwSuiteStatusException(kOfxStatFailed);
#else
  (void)p;
  setPersistentMessage(OFX::Message::eMessageError, "PigmentMetal",
                       "Pigment (Research) requires an Apple Metal build.");
  OFX::throwSuiteStatusException(args.isEnabledMetalRender ? kOfxStatGPURenderFailed
                                                           : kOfxStatErrUnsupported);
#endif
}

void PigmentEffect::purgeCaches() {
#ifdef PIGMENT_ENABLE_METAL
  if (metal_) metal_->releaseTransientResources();
#endif
}
void PigmentEffect::endSequenceRender(const OFX::EndSequenceRenderArguments&) { purgeCaches(); }

OFX::GroupParamDescriptor* group(OFX::ImageEffectDescriptor& d, const char* name,
                                 const char* label, bool open = true) {
  auto* value = d.defineGroupParam(name); value->setLabels(label, label, label);
  value->setOpen(open); return value;
}

OFX::DoubleParamDescriptor* number(OFX::ImageEffectDescriptor& d,
    OFX::GroupParamDescriptor& parent, const char* name, const char* label,
    double value, double minimum, double maximum, double displayMaximum,
    const char* hint, OFX::DoubleTypeEnum type = OFX::eDoubleTypePlain) {
  auto* parameter = ofx::defineDouble(d, name, label, value, minimum, maximum,
      minimum, displayMaximum, 0.01, hint, type);
  parameter->setParent(parent); return parameter;
}

void addComponents(OFX::ClipDescriptor* clip) {
  clip->addSupportedComponent(OFX::ePixelComponentRGB);
  clip->addSupportedComponent(OFX::ePixelComponentRGBA);
  clip->setSupportsTiles(false);
}

}  // namespace

void PigmentFactory::describe(OFX::ImageEffectDescriptor& d) {
  d.setLabels("Pigment", "Pigment", "Pigment (Research)");
  d.setPluginGrouping("Pigment");
  d.setPluginDescription("Integrated GPU-resident pictorial image reorganization in YAB space.");
  d.addSupportedContext(OFX::eContextFilter); d.addSupportedContext(OFX::eContextGeneral);
  d.addSupportedBitDepth(OFX::eBitDepthFloat); d.setSingleInstance(false);
  d.setHostFrameThreading(false); d.setSupportsMultiResolution(true); d.setSupportsTiles(false);
  d.setTemporalClipAccess(false); d.setRenderTwiceAlways(false);
  d.setSupportsMultipleClipDepths(false); d.setSupportsMultipleClipPARs(false);
  d.setRenderThreadSafety(OFX::eRenderFullySafe);
#ifdef PIGMENT_ENABLE_METAL
  d.setSupportsMetalRender(true);
#endif
}

void PigmentFactory::describeInContext(OFX::ImageEffectDescriptor& d,
                                       OFX::ContextEnum context) {
  auto* source = d.defineClip(kOfxImageEffectSimpleSourceClipName); addComponents(source);
  if (context == OFX::eContextGeneral) {
    auto* mask = d.defineClip(kMaskClip); mask->addSupportedComponent(OFX::ePixelComponentAlpha);
    mask->setOptional(true); mask->setIsMask(true); mask->setSupportsTiles(false);
    auto* planeMap = d.defineClip(kPlaneMapClip);
    planeMap->addSupportedComponent(OFX::ePixelComponentRGBA);
    planeMap->setOptional(true); planeMap->setIsMask(false); planeMap->setSupportsTiles(false);
  }
  auto* output = d.defineClip(kOfxImageEffectOutputClipName); addComponents(output);

  auto* painterly = group(d, "painterlyGroup", "Painterly");
  number(d, *painterly, kAmount, "Amount", 0.7, 0, 1, 1, "Overall integrated processing strength", OFX::eDoubleTypeScale);
  number(d, *painterly, kMassScale, "Mass Scale", 18, 0.25, 256, 96, "Physical radius across which soft regions can form");
  number(d, *painterly, kMassStrength, "Mass Strength", 0.55, 0, 1, 1, "Continuous attraction toward local soft modes", OFX::eDoubleTypeScale);
  number(d, *painterly, kToneSimilarity, "Tone Similarity", 0.25, 0.001, 16, 2, "Y feature bandwidth");
  number(d, *painterly, kChromaSimilarity, "Chroma Similarity", 0.18, 0.001, 16, 2, "Joint opponent-chroma bandwidth");
  number(d, *painterly, kLumaAttraction, "Luma Attraction", 0.65, 0, 1, 1, "Luminance movement toward region modes", OFX::eDoubleTypeScale);
  number(d, *painterly, kChromaAttraction, "Chroma Attraction", 0.8, 0, 1, 1, "Opponent-chroma movement toward region modes", OFX::eDoubleTypeScale);

  auto* structure = group(d, "structureGroup", "Structure");
  number(d, *structure, kStructureScale, "Structure Scale", 10, 0.25, 256, 64, "Scale used to recognize pictorially persistent structure");
  number(d, *structure, kStructurePreserve, "Structure Preserve", 0.8, 0, 1, 1, "Weight of multiscale-persistent boundaries", OFX::eDoubleTypeScale);
  number(d, *structure, kBoundaryPreserve, "Boundary Preserve", 0.75, 0, 1, 1, "Independent protection of significant boundaries", OFX::eDoubleTypeScale);
  number(d, *structure, kBoundaryExtinction, "Boundary Extinction", 0.35, 0, 1, 1, "Selective dissolution of boundaries", OFX::eDoubleTypeScale);
  number(d, *structure, kBoundarySoftness, "Boundary Softness", 0.15, 0.001, 4, 1, "Transition into boundary protection");

  auto* spatial = group(d, "spatialGroup", "Spatial Variation");
  number(d, *spatial, kVeilAmount, "Veil Amount", 0.35, 0, 1, 1, "Spatial unevenness of abstraction", OFX::eDoubleTypeScale);
  number(d, *spatial, kVeilScale, "Veil Scale", 500, 8, 4096, 1500, "Characteristic size of broad irregular fields");
  number(d, *spatial, kVeilIrregularity, "Veil Irregularity", 0.4, 0, 1, 1, "Broad field octave and warp contribution", OFX::eDoubleTypeScale);
  number(d, *spatial, kVeilContrast, "Veil Contrast", 0.3, -2, 2, 1, "Contrast of the broad field");
  auto* seed = d.defineIntParam(kVeilSeed); seed->setLabels("Veil Seed", "Veil Seed", "Veil Seed");
  seed->setScriptName(kVeilSeed); seed->setDefault(1); seed->setRange(-1000000, 1000000); seed->setParent(*spatial);

  auto* detail = group(d, "detailGroup", "Detail");
  number(d, *detail, kDetailCleanup, "Detail Cleanup", 0.1, 0, 1, 1, "Low-strength Guided Metal cleanup", OFX::eDoubleTypeScale);
  number(d, *detail, kFineDetail, "Fine Detail", 0.05, 0, 1, 1, "Fine residual reintegration", OFX::eDoubleTypeScale);
  number(d, *detail, kMediumDetail, "Medium Detail", 0.15, 0, 1, 1, "Medium residual reintegration", OFX::eDoubleTypeScale);
  number(d, *detail, kInternalVariation, "Internal Variation", 0.3, 0, 1, 1, "Broad internal variation reintegration", OFX::eDoubleTypeScale);

  auto* chroma = group(d, "chromaGroup", "Chroma");
  number(d, *chroma, kChromaMigration, "Chroma Migration", 0.35, 0, 1, 1, "Independent spatial migration of opponent chroma", OFX::eDoubleTypeScale);
  number(d, *chroma, kChromaScale, "Chroma Scale", 24, 0.25, 512, 128, "Physical scale of chroma migration");
  number(d, *chroma, kChromaEdgeRespect, "Chroma Edge Respect", 0.6, 0, 1, 1, "How strongly significant boundaries inhibit chroma", OFX::eDoubleTypeScale);

  auto* outputGroup = group(d, "outputGroup", "Output");
  number(d, *outputGroup, kMix, "Mix", 1, 0, 1, 1, "Final straight-RGB blend", OFX::eDoubleTypeScale);

  auto* planes = group(d, "pictorialPlanesGroup", "Pictorial Planes", false);
  constexpr std::array<const char*, 4> planeLabels{"Plane A", "Plane B", "Plane C", "Plane D"};
  for (int i = 0; i < 4; ++i) {
    const std::string groupName = std::string("pictorialPlane") + char('A' + i) + "Group";
    auto* planeGroup = group(d, groupName.c_str(), planeLabels[i], false);
    planeGroup->setParent(*planes);
    auto* enable = ofx::defineBoolean(d, kPlaneEnable[i], "Enable", true,
                                     "Enable this Plane Map membership channel");
    enable->setParent(*planeGroup);
    number(d, *planeGroup, kPlaneAmount[i], "Amount", 1, 0, 1, 1,
           "Scale this plane's membership", OFX::eDoubleTypeScale);
    number(d, *planeGroup, kPlaneSourceMix[i], "Source-Derived Target Mix", 1, 0, 1, 1,
           "Blend from the manual constant target to the robust source-derived field",
           OFX::eDoubleTypeScale);
    auto* target = d.defineRGBParam(kPlaneManualTarget[i]);
    target->setLabels("Manual Target Color", "Manual Target Color", "Manual Target Color");
    target->setScriptName(kPlaneManualTarget[i]); target->setDefault(0.18, 0.18, 0.18);
    target->setRange(-16, -16, -16, 16, 16, 16);
    target->setDisplayRange(0, 0, 0, 2, 2, 2); target->setParent(*planeGroup);
    number(d, *planeGroup, kPlaneYOffset[i], "Tone Offset", 0, -16, 16, 2,
           "Additive unclipped offset in internal Y");
    number(d, *planeGroup, kPlaneToneInfluence[i], "Tone Influence", 1, 0, 1, 1,
           "Influence of the plane's Y target", OFX::eDoubleTypeScale);
    number(d, *planeGroup, kPlaneABias[i], "Chroma Bias A", 0, -4, 4, 1,
           "Additive opponent-axis A bias");
    number(d, *planeGroup, kPlaneBBias[i], "Chroma Bias B", 0, -4, 4, 1,
           "Additive opponent-axis B bias");
    number(d, *planeGroup, kPlaneChromaInfluence[i], "Chroma Influence", 1, 0, 1, 1,
           "Influence of the plane's opponent-chroma target", OFX::eDoubleTypeScale);
  }

  auto* information = group(d, "pictorialInformationGroup", "Pictorial Information", false);
  number(d, *information, kFineExtinction, "Fine Extinction", 0.9, 0, 1, 1,
         "Remove the fine source residual inside plane ownership", OFX::eDoubleTypeScale);
  number(d, *information, kMediumExtinction, "Medium Extinction", 0.8, 0, 1, 1,
         "Remove the medium source residual inside plane ownership", OFX::eDoubleTypeScale);
  number(d, *information, kBroadRetention, "Broad Retention", 0.2, 0, 1, 1,
         "Retain source broad shading instead of the fitted plane target", OFX::eDoubleTypeScale);
  number(d, *information, kDetailStructurePreserve, "Detail Structure Preserve", 0.7, 0, 1, 1,
         "Protect residual detail only at scale-persistent source structure",
         OFX::eDoubleTypeScale);

  auto* transitions = group(d, "pictorialTransitionsGroup", "Plane Transitions", false);
  number(d, *transitions, kYTransitionWidth, "Y Transition Width", 12, 0, 512, 128,
         "Full-resolution 10-90 percent luminance-membership transition width");
  number(d, *transitions, kABTransitionWidth, "Chroma Transition Width", 48, 0, 1024, 256,
         "Full-resolution 10-90 percent opponent-chroma transition width");
  number(d, *transitions, kTransitionStructureRespect, "Transition Structure Respect", 0.7,
         0, 1, 1, "Source-structure permeability for membership propagation",
         OFX::eDoubleTypeScale);
  number(d, *transitions, kLocalSoftness, "Local Softness", 0, 0, 1, 1,
         "Optional downstream optical softness; zero fully bypasses it",
         OFX::eDoubleTypeScale);

  auto* phase33 = group(d, "phase33Group", "Soft Pictorial Plates (Phase 3.3)", false);
  auto* construction = group(d, "phase33ConstructionGroup", "Plane Construction", false);
  construction->setParent(*phase33);
  auto* planeSource = d.defineChoiceParam(kPlaneSource);
  planeSource->setLabels("Plane Source", "Plane Source", "Plane Source");
  planeSource->setScriptName(kPlaneSource); planeSource->appendOption("Manual");
  planeSource->appendOption("Auto Convex Palette");
  planeSource->appendOption("Auto Spatial Layers"); planeSource->appendOption("Hybrid");
  planeSource->setDefault(1); planeSource->setParent(*construction);
  number(d, *construction, kAutomaticOccupancy, "Automatic Occupancy", 1, 0, 1, 1,
         "Explicit automatic plane coverage; confidence never changes it", OFX::eDoubleTypeScale);
  number(d, *construction, kAutomaticPlaneStrength, "Automatic Plane Strength", 1, 0, 1, 1,
         "Processing influence of automatic ownership", OFX::eDoubleTypeScale);
  number(d, *construction, kSpatialCoherence, "Spatial Coherence", 0.5, 0, 1, 1,
         "Spatial coherence or RGBXY mesh scale", OFX::eDoubleTypeScale);
  number(d, *construction, kColorCoherence, "Color Coherence", 0.5, 0, 1, 1,
         "Compactness of convex palette contributions", OFX::eDoubleTypeScale);
  number(d, *construction, kHybridGuidance, "Hybrid Guidance Amount", 0.75, 0, 1, 1,
         "Influence and strong-paint locking of the RGBA Plane Map", OFX::eDoubleTypeScale);

  auto* shading = group(d, "phase33ShadingGroup", "Photographic Shading", false);
  shading->setParent(*phase33);
  auto* shadingModel = d.defineChoiceParam(kShadingModel);
  shadingModel->setLabels("Shading Model", "Shading Model", "Shading Model");
  shadingModel->setScriptName(kShadingModel); shadingModel->appendOption("Quadratic (Phase 3.2)");
  shadingModel->appendOption("Source Smooth"); shadingModel->appendOption("TGV Regularized");
  shadingModel->setDefault(1); shadingModel->setParent(*shading);
  number(d, *shading, kSmoothSimplification, "Smooth Simplification", 0.55, 0, 1, 1,
         "Regularization of source-derived smooth photographic shading", OFX::eDoubleTypeScale);
  number(d, *shading, kShadingStructurePreserve, "Shading Structure Preserve", 0.8, 0, 1, 1,
         "Protect scale-persistent geometry during shading regularization", OFX::eDoubleTypeScale);
  number(d, *shading, kChromaShadingRetention, "Chroma Shading Retention", 0.65, 0, 1, 1,
         "Retain conditional broad chroma variation within each plate", OFX::eDoubleTypeScale);
  number(d, *shading, kPhase33StructurePreserve, "Structure Preserve", 0.9, 0, 1, 1,
         "Survival of spatially fixed structural residuals", OFX::eDoubleTypeScale);

  auto* solver = group(d, "phase33SolverGroup", "Phase 3.3 Solver", false);
  solver->setParent(*phase33);
  auto* transitionSolver = d.defineChoiceParam(kTransitionSolver);
  transitionSolver->setLabels("Transition Solver", "Transition Solver", "Transition Solver");
  transitionSolver->setScriptName(kTransitionSolver); transitionSolver->appendOption("Jacobi Fast");
  transitionSolver->appendOption("Multigrid Reference"); transitionSolver->setDefault(1);
  transitionSolver->setParent(*solver);
  auto* backend = d.defineChoiceParam(kComputeBackend);
  backend->setLabels("Compute Backend", "Compute Backend", "Compute Backend");
  backend->setScriptName(kComputeBackend); backend->appendOption("Auto");
  backend->appendOption("Metal"); backend->appendOption("CPU Reference");
  backend->setDefault(0); backend->setParent(*solver);

  auto* phase4 = group(d, "phase4Group", "Automatic Plate Graph (Phase 4)", false);
  auto* phase4Auto = group(d, "phase4AutoGroup", "Auto Plates", true); phase4Auto->setParent(*phase4);
  auto* plateCount = d.defineIntParam(kPhase4PlateCount); plateCount->setLabels("Plate Count","Plate Count","Plate Count"); plateCount->setDefault(6); plateCount->setRange(4,8); plateCount->setDisplayRange(4,8); plateCount->setParent(*phase4Auto);
  number(d,*phase4Auto,kPhase4PlateScale,"Plate Scale",48,4,256,128,"Graph-geodesic component adjacency scale; never a blur radius");
  number(d,*phase4Auto,kPhase4PlateOverlap,"Plate Overlap",.55,0,1,1,"Latent grouping entropy and graph-support budget",OFX::eDoubleTypeScale);
  number(d,*phase4Auto,kPhase4BoundaryLock,"Boundary Lock",.75,0,1,1,"Raises persistent-boundary disappearance levels",OFX::eDoubleTypeScale);
  number(d,*phase4Auto,kPhase4Coupling,"Luma/Chroma Coupling",.35,0,1,1,"Equalizes AB organization toward Y",OFX::eDoubleTypeScale);
  auto* phase4Chunk=group(d,"phase4ChunkGroup","Chunk Formation",false);phase4Chunk->setParent(*phase4);
  number(d,*phase4Chunk,kPhase4LumaChunkScale,"Luma Chunk Scale",24,0,256,128,"Y hierarchy cut in canonical pixels; zero bypasses synthesis");
  number(d,*phase4Chunk,kPhase4ChromaChunkScale,"Chroma Chunk Scale",64,0,512,256,"AB hierarchy cut in canonical pixels; zero bypasses synthesis");
  number(d,*phase4Chunk,kPhase4MergeSelectivity,"Merge Selectivity",.6,0,1,1,"Sensitivity to appearance and gradient disagreement",OFX::eDoubleTypeScale);
  number(d,*phase4Chunk,kPhase4InternalVariation,"Internal Variation",.45,0,1,1,"Variation tolerated during region merging",OFX::eDoubleTypeScale);
  number(d,*phase4Chunk,kPhase4GradientComplexity,"Gradient Complexity",.35,0,1,1,"Primitive acceptance and residual-gradient survival",OFX::eDoubleTypeScale);
  auto* phase4Spill=group(d,"phase4SpillGroup","Plate Interaction",false);phase4Spill->setParent(*phase4);
  number(d,*phase4Spill,kPhase4SpillAmount,"Spill Amount",.25,0,1,1,"Directed graph-based plate interaction",OFX::eDoubleTypeScale);
  number(d,*phase4Spill,kPhase4SpillReach,"Spill Reach",48,0,256,128,"Maximum information-flow graph path budget");
  number(d,*phase4Spill,kPhase4SpillAsymmetry,"Spill Asymmetry",.5,0,1,1,"Blend from symmetric to directed information flow",OFX::eDoubleTypeScale);
  number(d,*phase4Spill,kPhase4ChromaSpill,"Chroma Spill",.75,0,1,1,"AB interaction strength",OFX::eDoubleTypeScale);
  number(d,*phase4Spill,kPhase4LumaSpill,"Luma Spill",.15,0,1,1,"Y interaction strength",OFX::eDoubleTypeScale);
  number(d,*phase4Spill,kPhase4StructureRespect,"Structure Respect",.8,0,1,1,"Retained-boundary capacity in support and spill graphs",OFX::eDoubleTypeScale);
  constexpr std::array<const char*,8> phase4Labels{"Plate A","Plate B","Plate C","Plate D","Plate E","Plate F","Plate G","Plate H"};
  for(int i=0;i<8;++i){std::string name="phase4Plate";name+=char('A'+i);name+="Group";auto*pg=group(d,name.c_str(),phase4Labels[i],false);pg->setParent(*phase4);auto*enabled=ofx::defineBoolean(d,kPhase4PlateEnable[i],"Enable",true,"Enable reconstruction and interaction for this plate");enabled->setParent(*pg);number(d,*pg,kPhase4PlateWeight[i],"Weight",1,0,4,2,"Final reconstruction ownership only");number(d,*pg,kPhase4PlateTone[i],"Tone",0,-16,16,2,"Additive unclipped Y adjustment after chunk synthesis");number(d,*pg,kPhase4PlateBiasA[i],"Chroma A",0,-4,4,1,"Additive opponent A adjustment after chunk synthesis");number(d,*pg,kPhase4PlateBiasB[i],"Chroma B",0,-4,4,1,"Additive opponent B adjustment after chunk synthesis");number(d,*pg,kPhase4PlateSpillOut[i],"Spill Out",1,0,1,1,"Directed donor strength",OFX::eDoubleTypeScale);number(d,*pg,kPhase4PlateReceive[i],"Receive Spill",1,0,1,1,"Directed receiver strength",OFX::eDoubleTypeScale);}
  auto* phase4Research=group(d,"phase4ResearchGroup","Phase 4 Research Advanced",false);phase4Research->setParent(*phase4);
  auto* latentCount=d.defineIntParam(kPhase4LatentCount);latentCount->setLabels("Latent Components","Latent Components","Latent Components");latentCount->setDefault(16);latentCount->setRange(12,24);latentCount->setDisplayRange(12,24);latentCount->setParent(*phase4Research);
  auto* debugLatent=d.defineIntParam(kPhase4DebugLatent);debugLatent->setLabels("Debug Latent","Debug Latent","Debug Latent");debugLatent->setDefault(1);debugLatent->setRange(1,24);debugLatent->setDisplayRange(1,24);debugLatent->setParent(*phase4Research);
  auto* debugPlate=d.defineIntParam(kPhase4DebugPlate);debugPlate->setLabels("Debug Plate","Debug Plate","Debug Plate");debugPlate->setDefault(0);debugPlate->setRange(0,7);debugPlate->setDisplayRange(0,7);debugPlate->setParent(*phase4Research);

  auto* advanced = group(d, "advancedGroup", "Research Advanced", false);
  number(d, *advanced, kRegionSoftness, "Region Softness", 0.5, 0.1, 4, 2, "Tail softness of joint spatial/color attraction");
  number(d, *advanced, kModeSelectivity, "Mode Selectivity", 0.65, 0, 1, 1,
         "Low softly mixes candidate populations; high favors the dominant coherent mode",
         OFX::eDoubleTypeScale);
  number(d, *advanced, kBoundaryScale, "Boundary Scale", 12, 0.25, 256, 64, "Independent support for boundary extinction");
  number(d, *advanced, kVeilTonalBias, "Veil Tonal Bias", 0, -1, 1, 1, "Bias the broad field using unclipped luminance");
  number(d, *advanced, kChromaLumaCoupling, "Chroma/Luma Coupling", 0.6, 0, 1, 1, "Amount of Y structure guiding chroma migration", OFX::eDoubleTypeScale);
  auto* gamut = d.defineChoiceParam(kWorkingGamut); gamut->setLabels("Working Gamut", "Working Gamut", "Working Gamut");
  gamut->setScriptName(kWorkingGamut); gamut->appendOption("ACEScg");
  gamut->appendOption("Linear Rec.709 / sRGB Primaries"); gamut->appendOption("Linear Rec.2020");
  gamut->appendOption("Display P3 D65 Primaries"); gamut->setDefault(0); gamut->setParent(*advanced);
  auto* invert = ofx::defineBoolean(d, kInvertMask, "Invert Mask", false, "Invert the optional external mask");
  invert->setParent(*advanced);
  auto* comparison = d.defineChoiceParam(kComparison); comparison->setLabels("Comparison", "Comparison", "Comparison");
  comparison->setScriptName(kComparison); comparison->appendOption("Original");
  comparison->appendOption("Current Guided DetailCollapse");
  comparison->appendOption("Weighted Mean (legacy research)");
  comparison->appendOption("Representative Mode");
  comparison->appendOption("Pictorial Planes (Phase 3.2)");
  comparison->appendOption("Soft Pictorial Plates (Phase 3.3)");
  comparison->appendOption("Automatic Plate Graph (Phase 4)");
  comparison->setDefault(3); comparison->setParent(*advanced);
  auto* debug = d.defineChoiceParam(kDebug); debug->setLabels("Debug View", "Debug View", "Debug View");
  debug->setScriptName(kDebug);
  for (const char* option : {"Final", "Coarse Structure", "Veil Source", "Mass Strength Field",
      "Boundary Extinction Field", "Chroma Migration Field", "Detail Retention Field",
      "Region Centroid / Mode", "Region Attraction Magnitude", "Y Mass", "AB Mass",
      "Mass Result Before Boundary Processing", "Boundary Protection", "Boundary Extinction",
      "Chroma Migration Result", "Fine Residual", "Medium Residual", "Internal Variation",
      "Pre-Reintegration", "Difference From Original", "Local Density",
      "Winning / Dominant Mode", "Mode Confidence", "Representative Distance",
      "Candidate Competition", "Legacy Weighted Mean", "Representative Mode Result",
      "Raw Plane Map", "Normalized Plane Membership", "Base / Unassigned Membership",
      "Y Transition Membership", "AB Transition Membership", "Plane Broad Y Target",
      "Plane Broad AB Target", "Combined Y Target", "Combined AB Target",
      "Broad Component", "Fine Residual", "Medium Residual", "Extinction Amount",
      "Structure / Protection", "Pre-Veil Result", "Pre-Softness Result", "Fit Error",
      "Plane Difference From Original"})
    debug->appendOption(option);
  for (const char* option : {"Automatic Raw Membership", "Automatic Normalized Membership",
      "Automatic Base / Unassigned", "Automatic Palette", "Hybrid Correction Influence",
      "Automatic Confidence", "Automatic Reconstruction Error", "RGBXY Control Mesh",
      "RGBXY Vertex-to-Layer Weights", "Reconstructed Layer Composite",
      "Layer Composite Difference", "Structural Component", "Smooth Shading",
      "Medium Descriptive Residual", "Fine Descriptive Residual", "Conditional Plane Y",
      "Conditional Plane AB", "Phase 3.3 Y Reconstruction", "Phase 3.3 AB Reconstruction",
      "Phase 3.3 Pre-Veil", "Phase 3.3 Pre-Softness",
      "Phase 3.3 Difference From Original", "Transition Solver Residual"})
    debug->appendOption(option);
  for(const char* option:{"Phase 4 Source Boundary Strength","Phase 4 Boundary Hierarchy / UCM","Phase 4 Atomic Regions","Phase 4 Latent Fuzzy Component","Phase 4 Latent Composite","Phase 4 Latent Reconstruction Error","Phase 4 Spectral Eigenspace Residual","Phase 4 Component Recovery Projection Error","Phase 4 Appearance-Unmixing Error","Phase 4 Artist Plate Alpha","Phase 4 Artist Plate Y Support","Phase 4 Artist Plate AB Support","Phase 4 Plate Y Appearance","Phase 4 Plate AB Appearance","Phase 4 Plate Overlap Composite","Phase 4 Y Region Hierarchy","Phase 4 AB Region Hierarchy","Phase 4 Removed Boundaries","Phase 4 Retained Boundaries","Phase 4 Y Chunks","Phase 4 AB Chunks","Phase 4 Source Gradient Field","Phase 4 Simplified Gradient Field","Phase 4 Gradient Reconstruction","Phase 4 Primitive Selection / Fit Error","Phase 4 Pre-Spill Result","Phase 4 Spill Influence Per Plate","Phase 4 Post-Spill Result","Phase 4 Difference From Source"})debug->appendOption(option);
  debug->setDefault(0); debug->setParent(*advanced);
  auto* debugPlane = d.defineChoiceParam(kDebugPlane);
  debugPlane->setLabels("Debug Plane", "Debug Plane", "Debug Plane");
  debugPlane->setScriptName(kDebugPlane);
  for (const char* option : {"Plane A", "Plane B", "Plane C", "Plane D", "Composite"})
    debugPlane->appendOption(option);
  debugPlane->setDefault(4); debugPlane->setParent(*advanced);
}

OFX::ImageEffect* PigmentFactory::createInstance(OfxImageEffectHandle handle,
                                                  OFX::ContextEnum) {
  return new PigmentEffect(handle);
}

}  // namespace pigment::plugin
