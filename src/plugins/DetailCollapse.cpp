#include "plugins/DetailCollapse.h"

#include "core/DetailCollapseResearch.h"
#include "ofx/OfxImageHelpers.h"
#include "ofx/ParameterHelpers.h"
#include "ofxGPURender.h"
#include "ofxsMultiThread.h"
#ifdef PIGMENT_ENABLE_METAL
#include "metal/PigmentMetal.h"
#endif

#include <algorithm>
#include <memory>
#include <sstream>

namespace pigment::plugin {
namespace {

constexpr const char* kAmount = "amount";
constexpr const char* kBackend = "backend";
constexpr const char* kMassScale = "massScale";
constexpr const char* kStructureScale = "structureScale";
constexpr const char* kMassStrength = "massStrength";
constexpr const char* kToneSimilarity = "toneSimilarity";
constexpr const char* kChromaSimilarity = "chromaSimilarity";
constexpr const char* kBoundaryPreserve = "boundaryPreserve";
constexpr const char* kBoundarySoftness = "boundarySoftness";
constexpr const char* kStructurePreserve = "structurePreserve";
constexpr const char* kInternalVariation = "internalVariation";
constexpr const char* kLumaMassing = "lumaMassing";
constexpr const char* kChromaMassing = "chromaMassing";
constexpr const char* kHighlightBias = "highlightBias";
constexpr const char* kMidtoneBias = "midtoneBias";
constexpr const char* kShadowBias = "shadowBias";
constexpr const char* kRangeEnabled = "rangeEnabled";
constexpr const char* kRangeMinimum = "rangeMinimum";
constexpr const char* kRangeMaximum = "rangeMaximum";
constexpr const char* kRangeSoftness = "rangeSoftness";
constexpr const char* kWorkingGamut = "workingGamut";
constexpr const char* kInvertMask = "invertMask";
constexpr const char* kMix = "mix";
constexpr const char* kDebugView = "debugView";
constexpr const char* kMaskClip = "Mask";

class OfxRowProcessor final : public OFX::MultiThread::Processor {
 public:
  OfxRowProcessor(int begin, int end, const RowFunction& function)
      : begin_(begin), end_(end), function_(function) {}
  void multiThreadFunction(unsigned int index, unsigned int count) override {
    const int total = end_ - begin_;
    const int first = begin_ + static_cast<int>(
        (static_cast<long long>(total) * index) / count);
    const int last = begin_ + static_cast<int>(
        (static_cast<long long>(total) * (index + 1)) / count);
    if (first < last) function_(first, last);
  }

 private:
  int begin_, end_;
  const RowFunction& function_;
};

class DetailCollapseEffect final : public OFX::ImageEffect {
 public:
  explicit DetailCollapseEffect(OfxImageEffectHandle handle)
      : ImageEffect(handle),
        destination_(fetchClip(kOfxImageEffectOutputClipName)),
        source_(fetchClip(kOfxImageEffectSimpleSourceClipName)) {
    if (getContext() == OFX::eContextGeneral) mask_ = fetchClip(kMaskClip);
    amount_ = fetchDoubleParam(kAmount);
    backend_ = fetchChoiceParam(kBackend);
    massScale_ = fetchDoubleParam(kMassScale);
    structureScale_ = fetchDoubleParam(kStructureScale);
    massStrength_ = fetchDoubleParam(kMassStrength);
    toneSimilarity_ = fetchDoubleParam(kToneSimilarity);
    chromaSimilarity_ = fetchDoubleParam(kChromaSimilarity);
    boundaryPreserve_ = fetchDoubleParam(kBoundaryPreserve);
    boundarySoftness_ = fetchDoubleParam(kBoundarySoftness);
    structurePreserve_ = fetchDoubleParam(kStructurePreserve);
    internalVariation_ = fetchDoubleParam(kInternalVariation);
    lumaMassing_ = fetchDoubleParam(kLumaMassing);
    chromaMassing_ = fetchDoubleParam(kChromaMassing);
    highlightBias_ = fetchDoubleParam(kHighlightBias);
    midtoneBias_ = fetchDoubleParam(kMidtoneBias);
    shadowBias_ = fetchDoubleParam(kShadowBias);
    rangeEnabled_ = fetchBooleanParam(kRangeEnabled);
    rangeMinimum_ = fetchDoubleParam(kRangeMinimum);
    rangeMaximum_ = fetchDoubleParam(kRangeMaximum);
    rangeSoftness_ = fetchDoubleParam(kRangeSoftness);
    workingGamut_ = fetchChoiceParam(kWorkingGamut);
    invertMask_ = fetchBooleanParam(kInvertMask);
    mix_ = fetchDoubleParam(kMix);
    debugView_ = fetchChoiceParam(kDebugView);
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
  DetailCollapseResearchParams parameters(double time) const;

  OFX::Clip *destination_ = nullptr, *source_ = nullptr, *mask_ = nullptr;
  OFX::DoubleParam *amount_ = nullptr, *massScale_ = nullptr,
                   *structureScale_ = nullptr, *massStrength_ = nullptr,
                   *toneSimilarity_ = nullptr, *chromaSimilarity_ = nullptr,
                   *boundaryPreserve_ = nullptr, *boundarySoftness_ = nullptr,
                   *structurePreserve_ = nullptr, *internalVariation_ = nullptr,
                   *lumaMassing_ = nullptr, *chromaMassing_ = nullptr,
                   *highlightBias_ = nullptr, *midtoneBias_ = nullptr,
                   *shadowBias_ = nullptr, *rangeMinimum_ = nullptr,
                   *rangeMaximum_ = nullptr, *rangeSoftness_ = nullptr,
                   *mix_ = nullptr;
  OFX::BooleanParam *rangeEnabled_ = nullptr, *invertMask_ = nullptr;
  OFX::ChoiceParam *backend_ = nullptr, *workingGamut_ = nullptr,
                   *debugView_ = nullptr;
#ifdef PIGMENT_ENABLE_METAL
  std::unique_ptr<metal::MetalInstance> metal_;
#endif
};

DetailCollapseResearchParams DetailCollapseEffect::parameters(double time) const {
  DetailCollapseResearchParams p;
  int backend = 0;
  backend_->getValueAtTime(time, backend);
  p.backend = static_cast<DetailCollapseBackend>(std::max(0, std::min(4, backend)));
  p.amount = static_cast<float>(amount_->getValueAtTime(time));
  p.massScale = static_cast<float>(massScale_->getValueAtTime(time));
  p.structureScale = static_cast<float>(structureScale_->getValueAtTime(time));
  p.massStrength = static_cast<float>(massStrength_->getValueAtTime(time));
  p.toneSimilarity = static_cast<float>(toneSimilarity_->getValueAtTime(time));
  p.chromaSimilarity = static_cast<float>(chromaSimilarity_->getValueAtTime(time));
  p.boundaryPreserve = static_cast<float>(boundaryPreserve_->getValueAtTime(time));
  p.boundarySoftness = static_cast<float>(boundarySoftness_->getValueAtTime(time));
  p.structurePreserve = static_cast<float>(structurePreserve_->getValueAtTime(time));
  p.internalVariation = static_cast<float>(internalVariation_->getValueAtTime(time));
  p.lumaMassing = static_cast<float>(lumaMassing_->getValueAtTime(time));
  p.chromaMassing = static_cast<float>(chromaMassing_->getValueAtTime(time));
  p.tonalMask.highlightBias = static_cast<float>(highlightBias_->getValueAtTime(time));
  p.tonalMask.midtoneBias = static_cast<float>(midtoneBias_->getValueAtTime(time));
  p.tonalMask.shadowBias = static_cast<float>(shadowBias_->getValueAtTime(time));
  p.tonalMask.rangeEnabled = rangeEnabled_->getValueAtTime(time);
  p.tonalMask.rangeMinimum = static_cast<float>(rangeMinimum_->getValueAtTime(time));
  p.tonalMask.rangeMaximum = static_cast<float>(rangeMaximum_->getValueAtTime(time));
  p.tonalMask.rangeSoftness = static_cast<float>(rangeSoftness_->getValueAtTime(time));
  p.invertMask = invertMask_->getValueAtTime(time);
  p.mix = static_cast<float>(mix_->getValueAtTime(time));
  int gamut = 0, debug = 0;
  workingGamut_->getValueAtTime(time, gamut);
  debugView_->getValueAtTime(time, debug);
  p.gamut = static_cast<WorkingGamut>(std::max(0, std::min(3, gamut)));
  p.debugView = static_cast<DetailCollapseDebugView>(
      std::max(0, std::min(14, debug)));
  return p;
}

bool DetailCollapseEffect::isIdentity(const OFX::IsIdentityArguments& args,
                                      OFX::Clip*& clip, double& identityTime) {
  const auto p = parameters(args.time);
  if (p.debugView == DetailCollapseDebugView::Original ||
      (p.debugView == DetailCollapseDebugView::Final &&
       (p.amount == 0.0f || p.mix == 0.0f))) {
    clip = source_;
    identityTime = args.time;
    return true;
  }
  return false;
}

bool DetailCollapseEffect::getRegionOfDefinition(
    const OFX::RegionOfDefinitionArguments& args, OfxRectD& rod) {
  rod = source_->getRegionOfDefinition(args.time);
  return true;
}

void DetailCollapseEffect::getRegionsOfInterest(
    const OFX::RegionsOfInterestArguments& args,
    OFX::RegionOfInterestSetter& rois) {
  const OfxRectD fullRod = source_->getRegionOfDefinition(args.time);
  rois.setRegionOfInterest(*source_, fullRod);
  if (mask_) rois.setRegionOfInterest(*mask_, fullRod);
}

void DetailCollapseEffect::render(const OFX::RenderArguments& args) {
  std::unique_ptr<OFX::Image> destination(destination_->fetchImage(args.time));
  std::unique_ptr<OFX::Image> source(source_->fetchImage(args.time));
  if (!destination || !source) OFX::throwSuiteStatusException(kOfxStatFailed);
  if (destination->getPixelDepth() != OFX::eBitDepthFloat ||
      source->getPixelDepth() != OFX::eBitDepthFloat ||
      destination->getPixelComponents() != source->getPixelComponents() ||
      (source->getPixelComponents() != OFX::ePixelComponentRGB &&
       source->getPixelComponents() != OFX::ePixelComponentRGBA))
    OFX::throwSuiteStatusException(kOfxStatErrUnsupported);

  std::unique_ptr<OFX::Image> maskImage;
  ConstFloatPlaneView maskView;
  const ConstFloatPlaneView* maskPointer = nullptr;
  if (mask_ && mask_->isConnected()) {
    maskImage.reset(mask_->fetchImage(args.time));
    if (!maskImage || maskImage->getPixelDepth() != OFX::eBitDepthFloat)
      OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
  }

  auto p = parameters(args.time);
  p.premultiplied = source->getPreMultiplication() == OFX::eImagePreMultiplied;
  const ImageGeometry geometry{source->getPixelAspectRatio(), args.renderScale.x,
                               args.renderScale.y};

#ifdef PIGMENT_ENABLE_METAL
  const bool guidedMetal = p.backend == DetailCollapseBackend::GuidedMetal;
  const bool domainMetal = p.backend == DetailCollapseBackend::DomainTransformMetal;
  const bool nativeIdentity = args.isEnabledMetalRender &&
      p.debugView == DetailCollapseDebugView::Final &&
      (p.amount == 0.0f || p.mix == 0.0f);
  if (args.isEnabledMetalRender && !guidedMetal && !nativeIdentity) {
    if (domainMetal)
      setPersistentMessage(OFX::Message::eMessageWarning, "PigmentMetal",
          "Domain Transform Metal is reserved in this research build; requesting a CPU-backed host retry.");
    OFX::throwSuiteStatusException(kOfxStatGPURenderFailed);
  }
  if (guidedMetal || nativeIdentity) {
    if (!metal_) metal_ = std::make_unique<metal::MetalInstance>();
    const auto makeMetalView = [](OFX::Image& image) {
      const RectI bounds = ofx::toRect(image.getBounds());
      const std::size_t storageBytes = image.getRowBytes() > 0
          ? static_cast<std::size_t>(image.getRowBytes()) * bounds.height() : 0;
      return metal::MetalImageView{image.getPixelData(), storageBytes,
          image.getRowBytes(), bounds, image.getPixelComponentCount()};
    };
    metal::MetalExecutionRequest request;
    request.source = makeMetalView(*source);
    request.destination = makeMetalView(*destination);
    request.nativeHostBuffers = args.isEnabledMetalRender;
    request.hostCommandQueue = args.pMetalCmdQ;
    request.renderWindow = ofx::toRect(args.renderWindow);
    request.params = p;
    request.geometry = geometry;
    if (maskImage) {
      request.mask = makeMetalView(*maskImage);
      request.hasMask = true;
    }
    if (metal_->render(request)) {
      clearPersistentMessage();
      return;
    }
    const auto& diagnostics = metal_->diagnostics();
    if (args.isEnabledMetalRender)
      OFX::throwSuiteStatusException(kOfxStatGPURenderFailed);
    std::ostringstream warning;
    warning << "Guided Metal failed (" << diagnostics.message
            << "); using explicit Guided CPU fallback for this CPU-backed render.";
    setPersistentMessage(OFX::Message::eMessageWarning, "PigmentMetal", warning.str());
    p.backend = DetailCollapseBackend::GuidedCpu;
  } else if (domainMetal) {
    setPersistentMessage(OFX::Message::eMessageWarning, "PigmentMetal",
        "Domain Transform Metal is reserved in this research build; using Domain Transform CPU.");
    p.backend = DetailCollapseBackend::DomainTransformCpu;
  } else {
    clearPersistentMessage();
  }
#else
  if (args.isEnabledMetalRender) OFX::throwSuiteStatusException(kOfxStatGPURenderFailed);
  if (p.backend == DetailCollapseBackend::GuidedMetal) {
    setPersistentMessage(OFX::Message::eMessageWarning, "PigmentMetal",
        "This build has no Metal backend; using Guided CPU.");
    p.backend = DetailCollapseBackend::GuidedCpu;
  } else if (p.backend == DetailCollapseBackend::DomainTransformMetal) {
    setPersistentMessage(OFX::Message::eMessageWarning, "PigmentMetal",
        "This build has no Metal backend; using Domain Transform CPU.");
    p.backend = DetailCollapseBackend::DomainTransformCpu;
  }
#endif

  if (maskImage) {
    maskView = ofx::makeMaskView(*maskImage);
    maskPointer = &maskView;
  }
  ExecutionContext execution;
  execution.cancelled = [this] { return abort(); };
  execution.parallelRows = [](int begin, int end, const RowFunction& function) {
    if (OFX::MultiThread::isSpawnedThread() || end - begin < 8) {
      function(begin, end);
    } else {
      OfxRowProcessor processor(begin, end, function);
      processor.multiThread();
    }
  };
  processDetailCollapseResearch(
      ofx::makeConstImageView(*source), ofx::makeImageView(*destination),
      ofx::toRect(args.renderWindow), p, geometry, maskPointer, execution);
}

void DetailCollapseEffect::purgeCaches() {
#ifdef PIGMENT_ENABLE_METAL
  if (metal_) metal_->releaseTransientResources();
#endif
}

void DetailCollapseEffect::endSequenceRender(
    const OFX::EndSequenceRenderArguments&) {
#ifdef PIGMENT_ENABLE_METAL
  if (metal_) metal_->releaseTransientResources();
#endif
}

void addSupportedComponents(OFX::ClipDescriptor* clip) {
  clip->addSupportedComponent(OFX::ePixelComponentRGB);
  clip->addSupportedComponent(OFX::ePixelComponentRGBA);
  clip->setSupportsTiles(false);
}

void defineWorkingGamut(OFX::ImageEffectDescriptor& descriptor) {
  auto* gamut = descriptor.defineChoiceParam(kWorkingGamut);
  gamut->setLabels("Working Gamut", "Working Gamut", "Working Gamut");
  gamut->setScriptName(kWorkingGamut);
  gamut->setHint("Primaries of the already-linear input; no transfer function is applied");
  gamut->appendOption("ACEScg");
  gamut->appendOption("Linear Rec.709 / sRGB Primaries");
  gamut->appendOption("Linear Rec.2020");
  gamut->appendOption("Display P3 D65 Primaries");
  gamut->setDefault(0);
}

void defineBackend(OFX::ImageEffectDescriptor& descriptor) {
  auto* backend = descriptor.defineChoiceParam(kBackend);
  backend->setLabels("Backend", "Backend", "Backend");
  backend->setScriptName(kBackend);
  backend->setHint("Temporary Stage 2 implementation backend; Reference preserves the original visual checkpoint");
  backend->appendOption("Reference Bilateral CPU");
  backend->appendOption("Guided CPU");
  backend->appendOption("Domain Transform CPU");
  backend->appendOption("Guided Metal");
  backend->appendOption("Domain Transform Metal (CPU fallback)");
  backend->setDefault(0);
}

void defineDebugView(OFX::ImageEffectDescriptor& descriptor) {
  auto* debug = descriptor.defineChoiceParam(kDebugView);
  debug->setLabels("Debug View", "Debug View", "Debug View");
  debug->setScriptName(kDebugView);
  debug->setHint("Temporary research visualization; does not alter the processing path");
  for (const char* option : {
           "Final", "Original", "Consolidation Seed", "Structure Guide",
           "Processing Strength", "Boundary Protection", "Rolling Iteration 1",
           "Rolling Iteration 2", "Rolling Iteration 3", "Rolling Iteration 4",
           "Y Mass Result", "AB Mass Result", "Internal Variation Residual",
           "Pre-Reintegration Mass", "Difference From Original"})
    debug->appendOption(option);
  debug->setDefault(0);
}

}  // namespace

void DetailCollapseFactory::describe(OFX::ImageEffectDescriptor& descriptor) {
  descriptor.setLabels("DetailCollapse", "DetailCollapse",
                       "Pigment DetailCollapse (Research)");
  descriptor.setPluginGrouping("Pigment");
  descriptor.setPluginDescription(
      "Research Rolling YAB Mass implementation for visually evaluating coherent detail consolidation.");
  descriptor.addSupportedContext(OFX::eContextFilter);
  descriptor.addSupportedContext(OFX::eContextGeneral);
  descriptor.addSupportedBitDepth(OFX::eBitDepthFloat);
  descriptor.setSingleInstance(false);
  descriptor.setHostFrameThreading(false);
  descriptor.setSupportsMultiResolution(true);
  descriptor.setSupportsTiles(false);
  descriptor.setTemporalClipAccess(false);
  descriptor.setRenderTwiceAlways(false);
  descriptor.setSupportsMultipleClipDepths(false);
  descriptor.setSupportsMultipleClipPARs(false);
  descriptor.setRenderThreadSafety(OFX::eRenderFullySafe);
#ifdef PIGMENT_ENABLE_METAL
  descriptor.setSupportsMetalRender(true);
#endif
}

void DetailCollapseFactory::describeInContext(OFX::ImageEffectDescriptor& descriptor,
                                               OFX::ContextEnum context) {
  auto* source = descriptor.defineClip(kOfxImageEffectSimpleSourceClipName);
  addSupportedComponents(source);
  source->setTemporalClipAccess(false);
  if (context == OFX::eContextGeneral) {
    auto* mask = descriptor.defineClip(kMaskClip);
    mask->addSupportedComponent(OFX::ePixelComponentAlpha);
    mask->setOptional(true);
    mask->setIsMask(true);
    mask->setSupportsTiles(false);
    mask->setTemporalClipAccess(false);
  }
  auto* output = descriptor.defineClip(kOfxImageEffectOutputClipName);
  addSupportedComponents(output);

  defineBackend(descriptor);
  ofx::defineDouble(descriptor, kAmount, "Amount", 0, 0, 1, 0, 1, 0.01,
                    "Overall Mass Formation processing strength", OFX::eDoubleTypeScale);
  ofx::defineDouble(descriptor, kMassScale, "Mass Scale", 4, 0.25, 128, 0.25, 32,
                    0.25, "Characteristic size of information consolidated, in full-resolution pixels");
  ofx::defineDouble(descriptor, kStructureScale, "Structure Scale", 5, 0, 128, 0, 32,
                    0.25, "Scale at which boundaries become significant enough to protect");
  ofx::defineDouble(descriptor, kMassStrength, "Mass Strength", 0.65, 0, 1, 0, 1,
                    0.01, "Continuous update gain across four fixed rolling iterations",
                    OFX::eDoubleTypeScale);
  ofx::defineDouble(descriptor, kToneSimilarity, "Tone Similarity", 0.35, 0.001, 16,
                    0.01, 2, 0.005, "Y similarity bandwidth; higher values consolidate a wider tonal range");
  ofx::defineDouble(descriptor, kChromaSimilarity, "Chroma Similarity", 0.12, 0.001, 16,
                    0.01, 2, 0.005, "Opponent-chroma similarity bandwidth");
  ofx::defineDouble(descriptor, kBoundaryPreserve, "Boundary Preserve", 0.9, 0, 1,
                    0, 1, 0.01, "Strength of the independent boundary-protection field",
                    OFX::eDoubleTypeScale);
  ofx::defineDouble(descriptor, kBoundarySoftness, "Boundary Softness", 0.08, 0.001, 4,
                    0.005, 1, 0.005, "Transition softness for boundary permeability");
  ofx::defineDouble(descriptor, kStructurePreserve, "Structure Preserve", 1, 0, 1,
                    0, 1, 0.01, "Bias boundary detection toward structure-scale persistent edges",
                    OFX::eDoubleTypeScale);
  ofx::defineDouble(descriptor, kInternalVariation, "Internal Variation", 0.15, 0, 1,
                    0, 1, 0.01, "Reintegrate controlled low-frequency variation inside masses",
                    OFX::eDoubleTypeScale);
  ofx::defineDouble(descriptor, kLumaMassing, "Luma Massing", 1, 0, 1, 0, 1, 0.01,
                    "Y consolidation amount", OFX::eDoubleTypeScale);
  ofx::defineDouble(descriptor, kChromaMassing, "Chroma Massing", 1, 0, 1, 0, 1, 0.01,
                    "Opponent-chroma consolidation amount", OFX::eDoubleTypeScale);
  ofx::defineDouble(descriptor, kHighlightBias, "Highlight Bias", 0, -1, 1, -1, 1,
                    0.01, "Bias processing strength in highlights");
  ofx::defineDouble(descriptor, kMidtoneBias, "Midtone Bias", 0, -1, 1, -1, 1,
                    0.01, "Bias processing strength in midtones");
  ofx::defineDouble(descriptor, kShadowBias, "Shadow Bias", 0, -1, 1, -1, 1,
                    0.01, "Bias processing strength in shadows");
  ofx::defineBoolean(descriptor, kRangeEnabled, "Enable Range", false,
                     "Enable luminance range weighting");
  ofx::defineDouble(descriptor, kRangeMinimum, "Range Minimum", 0, -65504, 65504,
                    -1, 2, 0.01, "Lower luminance range boundary");
  ofx::defineDouble(descriptor, kRangeMaximum, "Range Maximum", 1, -65504, 65504,
                    -1, 16, 0.01, "Upper luminance range boundary");
  ofx::defineDouble(descriptor, kRangeSoftness, "Range Softness", 0.1, 0, 65504,
                    0, 2, 0.01, "Smooth falloff around range boundaries");
  defineWorkingGamut(descriptor);
  ofx::defineBoolean(descriptor, kInvertMask, "Invert Mask", false,
                     "Invert the optional external mask input");
  ofx::defineDouble(descriptor, kMix, "Mix", 1, 0, 1, 0, 1, 0.01,
                    "Final effect mix", OFX::eDoubleTypeScale);
  defineDebugView(descriptor);
}

OFX::ImageEffect* DetailCollapseFactory::createInstance(
    OfxImageEffectHandle handle, OFX::ContextEnum) {
  return new DetailCollapseEffect(handle);
}

}  // namespace pigment::plugin
