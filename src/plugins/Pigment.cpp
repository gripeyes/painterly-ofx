#include "plugins/Pigment.h"

#include "core/IntegratedPigment.h"
#include "ofx/OfxImageHelpers.h"
#include "ofx/ParameterHelpers.h"
#include "ofxGPURender.h"
#ifdef PIGMENT_ENABLE_METAL
#include "metal/PigmentMetal.h"
#endif

#include <algorithm>
#include <memory>
#include <sstream>

namespace pigment::plugin {
namespace {

constexpr const char* kMaskClip = "Mask";
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
constexpr const char* kBoundaryScale = "boundaryScale";
constexpr const char* kVeilTonalBias = "veilTonalBias";
constexpr const char* kChromaLumaCoupling = "chromaLumaCoupling";
constexpr const char* kWorkingGamut = "workingGamut";
constexpr const char* kInvertMask = "invertMask";
constexpr const char* kComparison = "comparisonMode";
constexpr const char* kDebug = "debugView";
constexpr const char* kMix = "mix";

class PigmentEffect final : public OFX::ImageEffect {
 public:
  explicit PigmentEffect(OfxImageEffectHandle handle)
      : ImageEffect(handle), destination_(fetchClip(kOfxImageEffectOutputClipName)),
        source_(fetchClip(kOfxImageEffectSimpleSourceClipName)) {
    if (getContext() == OFX::eContextGeneral) mask_ = fetchClip(kMaskClip);
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
    FETCH_DOUBLE(boundaryScale_, kBoundaryScale); FETCH_DOUBLE(veilTonalBias_, kVeilTonalBias);
    FETCH_DOUBLE(chromaLumaCoupling_, kChromaLumaCoupling); FETCH_DOUBLE(mix_, kMix);
#undef FETCH_DOUBLE
    veilSeed_ = fetchIntParam(kVeilSeed);
    invertMask_ = fetchBooleanParam(kInvertMask);
    gamut_ = fetchChoiceParam(kWorkingGamut);
    comparison_ = fetchChoiceParam(kComparison);
    debug_ = fetchChoiceParam(kDebug);
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
  OFX::Clip *destination_ = nullptr, *source_ = nullptr, *mask_ = nullptr;
  OFX::DoubleParam *amount_ = nullptr, *massScale_ = nullptr, *massStrength_ = nullptr,
      *toneSimilarity_ = nullptr, *chromaSimilarity_ = nullptr, *lumaAttraction_ = nullptr,
      *chromaAttraction_ = nullptr, *structureScale_ = nullptr, *structurePreserve_ = nullptr,
      *boundaryPreserve_ = nullptr, *boundaryExtinction_ = nullptr, *boundarySoftness_ = nullptr,
      *veilAmount_ = nullptr, *veilScale_ = nullptr, *veilIrregularity_ = nullptr,
      *veilContrast_ = nullptr, *detailCleanup_ = nullptr, *fineDetail_ = nullptr,
      *mediumDetail_ = nullptr, *internalVariation_ = nullptr, *chromaMigration_ = nullptr,
      *chromaScale_ = nullptr, *chromaEdgeRespect_ = nullptr, *regionSoftness_ = nullptr,
      *boundaryScale_ = nullptr, *veilTonalBias_ = nullptr, *chromaLumaCoupling_ = nullptr,
      *mix_ = nullptr;
  OFX::IntParam* veilSeed_ = nullptr;
  OFX::BooleanParam* invertMask_ = nullptr;
  OFX::ChoiceParam *gamut_ = nullptr, *comparison_ = nullptr, *debug_ = nullptr;
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
  VALUE(veilTonalBias_, veilTonalBias); VALUE(chromaLumaCoupling_, chromaLumaCoupling);
  VALUE(mix_, mix);
#undef VALUE
  p.veilSeed = veilSeed_->getValueAtTime(time);
  p.invertMask = invertMask_->getValueAtTime(time);
  int value = 0;
  gamut_->getValueAtTime(time, value);
  p.gamut = static_cast<WorkingGamut>(std::max(0, std::min(3, value)));
  comparison_->getValueAtTime(time, value);
  p.comparison = static_cast<PigmentComparisonMode>(std::max(0, std::min(2, value)));
  debug_->getValueAtTime(time, value);
  p.debugView = static_cast<PigmentDebugView>(std::max(0, std::min(19, value)));
  return p;
}

bool PigmentEffect::isIdentity(const OFX::IsIdentityArguments& args, OFX::Clip*& clip,
                               double& identityTime) {
  const auto p = parameters(args.time);
  if (p.comparison == PigmentComparisonMode::Original ||
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

  auto* advanced = group(d, "advancedGroup", "Research Advanced", false);
  number(d, *advanced, kRegionSoftness, "Region Softness", 0.5, 0.1, 4, 2, "Tail softness of joint spatial/color attraction");
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
  comparison->appendOption("Current Guided DetailCollapse"); comparison->appendOption("Integrated Pigment");
  comparison->setDefault(2); comparison->setParent(*advanced);
  auto* debug = d.defineChoiceParam(kDebug); debug->setLabels("Debug View", "Debug View", "Debug View");
  debug->setScriptName(kDebug);
  for (const char* option : {"Final", "Coarse Structure", "Veil Source", "Mass Strength Field",
      "Boundary Extinction Field", "Chroma Migration Field", "Detail Retention Field",
      "Region Centroid / Mode", "Region Attraction Magnitude", "Y Mass", "AB Mass",
      "Mass Result Before Boundary Processing", "Boundary Protection", "Boundary Extinction",
      "Chroma Migration Result", "Fine Residual", "Medium Residual", "Internal Variation",
      "Pre-Reintegration", "Difference From Original"}) debug->appendOption(option);
  debug->setDefault(0); debug->setParent(*advanced);
}

OFX::ImageEffect* PigmentFactory::createInstance(OfxImageEffectHandle handle,
                                                  OFX::ContextEnum) {
  return new PigmentEffect(handle);
}

}  // namespace pigment::plugin
