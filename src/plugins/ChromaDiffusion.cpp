#include "plugins/ChromaDiffusion.h"

#include "core/ChromaDiffusion.h"
#include "ofx/OfxImageHelpers.h"
#include "ofx/ParameterHelpers.h"
#include "ofxsMultiThread.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>

namespace pigment::plugin {
namespace {

constexpr const char* kAmount = "amount";
constexpr const char* kRadius = "radius";
constexpr const char* kRadiusX = "chromaXRadius";
constexpr const char* kRadiusY = "chromaYRadius";
constexpr const char* kAngle = "angle";
constexpr const char* kLuminancePreservation = "luminancePreservation";
constexpr const char* kEdgeProtection = "edgeProtection";
constexpr const char* kEdgeSoftness = "edgeSoftness";
constexpr const char* kHighlightBias = "highlightBias";
constexpr const char* kMidtoneBias = "midtoneBias";
constexpr const char* kShadowBias = "shadowBias";
constexpr const char* kSaturationCompensation = "saturationCompensation";
constexpr const char* kRangeEnabled = "rangeEnabled";
constexpr const char* kRangeMinimum = "rangeMinimum";
constexpr const char* kRangeMaximum = "rangeMaximum";
constexpr const char* kRangeSoftness = "rangeSoftness";
constexpr const char* kWorkingGamut = "workingGamut";
constexpr const char* kInvertMask = "invertMask";
constexpr const char* kMix = "mix";
constexpr const char* kMaskClip = "Mask";

class OfxRowProcessor final : public OFX::MultiThread::Processor {
 public:
  OfxRowProcessor(int begin, int end, const RowFunction& function)
      : begin_(begin), end_(end), function_(function) {}
  void multiThreadFunction(unsigned int index, unsigned int count) override {
    const int total = end_ - begin_;
    const int first = begin_ + static_cast<int>((static_cast<long long>(total) * index) / count);
    const int last = begin_ + static_cast<int>((static_cast<long long>(total) * (index + 1)) / count);
    if (first < last) function_(first, last);
  }
 private:
  int begin_, end_;
  const RowFunction& function_;
};

class ChromaDiffusionEffect final : public OFX::ImageEffect {
 public:
  explicit ChromaDiffusionEffect(OfxImageEffectHandle handle)
      : ImageEffect(handle), destination_(fetchClip(kOfxImageEffectOutputClipName)),
        source_(fetchClip(kOfxImageEffectSimpleSourceClipName)) {
    if (getContext() == OFX::eContextGeneral) mask_ = fetchClip(kMaskClip);
    amount_ = fetchDoubleParam(kAmount);
    radius_ = fetchDoubleParam(kRadius);
    radiusX_ = fetchDoubleParam(kRadiusX);
    radiusY_ = fetchDoubleParam(kRadiusY);
    angle_ = fetchDoubleParam(kAngle);
    luminancePreservation_ = fetchDoubleParam(kLuminancePreservation);
    edgeProtection_ = fetchDoubleParam(kEdgeProtection);
    edgeSoftness_ = fetchDoubleParam(kEdgeSoftness);
    highlightBias_ = fetchDoubleParam(kHighlightBias);
    midtoneBias_ = fetchDoubleParam(kMidtoneBias);
    shadowBias_ = fetchDoubleParam(kShadowBias);
    saturationCompensation_ = fetchDoubleParam(kSaturationCompensation);
    rangeEnabled_ = fetchBooleanParam(kRangeEnabled);
    rangeMinimum_ = fetchDoubleParam(kRangeMinimum);
    rangeMaximum_ = fetchDoubleParam(kRangeMaximum);
    rangeSoftness_ = fetchDoubleParam(kRangeSoftness);
    workingGamut_ = fetchChoiceParam(kWorkingGamut);
    invertMask_ = fetchBooleanParam(kInvertMask);
    mix_ = fetchDoubleParam(kMix);
  }

  void render(const OFX::RenderArguments& args) override;
  bool isIdentity(const OFX::IsIdentityArguments& args, OFX::Clip*& clip,
                  double& identityTime) override;
  bool getRegionOfDefinition(const OFX::RegionOfDefinitionArguments& args,
                             OfxRectD& rod) override;
  void getRegionsOfInterest(const OFX::RegionsOfInterestArguments& args,
                            OFX::RegionOfInterestSetter& rois) override;

 private:
  ChromaDiffusionParams parameters(double time) const;

  OFX::Clip *destination_ = nullptr, *source_ = nullptr, *mask_ = nullptr;
  OFX::DoubleParam *amount_ = nullptr, *radius_ = nullptr, *radiusX_ = nullptr,
                   *radiusY_ = nullptr, *angle_ = nullptr,
                   *luminancePreservation_ = nullptr, *edgeProtection_ = nullptr,
                   *edgeSoftness_ = nullptr, *highlightBias_ = nullptr,
                   *midtoneBias_ = nullptr, *shadowBias_ = nullptr,
                   *saturationCompensation_ = nullptr, *rangeMinimum_ = nullptr,
                   *rangeMaximum_ = nullptr, *rangeSoftness_ = nullptr, *mix_ = nullptr;
  OFX::BooleanParam *rangeEnabled_ = nullptr, *invertMask_ = nullptr;
  OFX::ChoiceParam* workingGamut_ = nullptr;
};

ChromaDiffusionParams ChromaDiffusionEffect::parameters(double time) const {
  ChromaDiffusionParams p;
  p.amount = static_cast<float>(amount_->getValueAtTime(time));
  p.diffusion.radius = static_cast<float>(radius_->getValueAtTime(time));
  p.diffusion.xScale = static_cast<float>(radiusX_->getValueAtTime(time));
  p.diffusion.yScale = static_cast<float>(radiusY_->getValueAtTime(time));
  p.diffusion.angleDegrees = static_cast<float>(angle_->getValueAtTime(time));
  p.luminancePreservation = static_cast<float>(luminancePreservation_->getValueAtTime(time));
  p.diffusion.edgeProtection = static_cast<float>(edgeProtection_->getValueAtTime(time));
  p.diffusion.edgeSoftness = static_cast<float>(edgeSoftness_->getValueAtTime(time));
  p.tonalMask.highlightBias = static_cast<float>(highlightBias_->getValueAtTime(time));
  p.tonalMask.midtoneBias = static_cast<float>(midtoneBias_->getValueAtTime(time));
  p.tonalMask.shadowBias = static_cast<float>(shadowBias_->getValueAtTime(time));
  p.saturationCompensationStops = static_cast<float>(saturationCompensation_->getValueAtTime(time));
  p.tonalMask.rangeEnabled = rangeEnabled_->getValueAtTime(time);
  p.tonalMask.rangeMinimum = static_cast<float>(rangeMinimum_->getValueAtTime(time));
  p.tonalMask.rangeMaximum = static_cast<float>(rangeMaximum_->getValueAtTime(time));
  p.tonalMask.rangeSoftness = static_cast<float>(rangeSoftness_->getValueAtTime(time));
  int gamut = 0;
  workingGamut_->getValueAtTime(time, gamut);
  p.gamut = static_cast<WorkingGamut>(std::max(0, std::min(3, gamut)));
  p.invertMask = invertMask_->getValueAtTime(time);
  p.mix = static_cast<float>(mix_->getValueAtTime(time));
  return p;
}

bool ChromaDiffusionEffect::isIdentity(const OFX::IsIdentityArguments& args,
                                       OFX::Clip*& clip, double& time) {
  const auto p = parameters(args.time);
  if (p.amount == 0.0f || p.mix == 0.0f) {
    clip = source_; time = args.time; return true;
  }
  return false;
}

bool ChromaDiffusionEffect::getRegionOfDefinition(
    const OFX::RegionOfDefinitionArguments& args, OfxRectD& rod) {
  rod = source_->getRegionOfDefinition(args.time);
  return true;
}

void ChromaDiffusionEffect::getRegionsOfInterest(
    const OFX::RegionsOfInterestArguments& args, OFX::RegionOfInterestSetter& rois) {
  const auto p = parameters(args.time);
  ImageGeometry geometry{source_->getPixelAspectRatio(), args.renderScale.x, args.renderScale.y};
  const auto request = chromaDiffusionInputDomain(p, geometry);
  if (request.kind == InputDomainKind::FullRegionOfDefinition) {
    rois.setRegionOfInterest(*source_, source_->getRegionOfDefinition(args.time));
  } else {
    const double x = request.haloX * source_->getPixelAspectRatio() /
                     std::max(args.renderScale.x, 1e-9);
    const double y = request.haloY / std::max(args.renderScale.y, 1e-9);
    rois.setRegionOfInterest(*source_, ofx::expandedCanonical(args.regionOfInterest, x, y));
  }
  if (mask_) rois.setRegionOfInterest(*mask_, args.regionOfInterest);
}

void ChromaDiffusionEffect::render(const OFX::RenderArguments& args) {
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
    maskView = ofx::makeMaskView(*maskImage);
    maskPointer = &maskView;
  }

  auto p = parameters(args.time);
  p.premultiplied = source->getPreMultiplication() == OFX::eImagePreMultiplied;
  const auto srcView = ofx::makeConstImageView(*source);
  auto dstView = ofx::makeImageView(*destination);
  const ImageGeometry geometry{source->getPixelAspectRatio(), args.renderScale.x,
                               args.renderScale.y};
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
  processChromaDiffusion(srcView, dstView, ofx::toRect(args.renderWindow), p,
                         geometry, maskPointer, execution);
}

void addSupportedComponents(OFX::ClipDescriptor* clip) {
  clip->addSupportedComponent(OFX::ePixelComponentRGB);
  clip->addSupportedComponent(OFX::ePixelComponentRGBA);
  clip->setSupportsTiles(true);
}

}  // namespace

void ChromaDiffusionFactory::describe(OFX::ImageEffectDescriptor& d) {
  d.setLabels("ChromaDiffusion", "ChromaDiffusion", "Pigment ChromaDiffusion");
  d.setPluginGrouping("Pigment");
  d.setPluginDescription("Spatially diffuses opponent chroma independently of luminance.");
  d.addSupportedContext(OFX::eContextFilter);
  d.addSupportedContext(OFX::eContextGeneral);
  d.addSupportedBitDepth(OFX::eBitDepthFloat);
  d.setSingleInstance(false);
  d.setHostFrameThreading(false);
  d.setSupportsMultiResolution(true);
  d.setSupportsTiles(true);
  d.setTemporalClipAccess(false);
  d.setRenderTwiceAlways(false);
  d.setSupportsMultipleClipDepths(false);
  d.setSupportsMultipleClipPARs(false);
  d.setRenderThreadSafety(OFX::eRenderFullySafe);
}

void ChromaDiffusionFactory::describeInContext(OFX::ImageEffectDescriptor& d,
                                                OFX::ContextEnum context) {
  auto* source = d.defineClip(kOfxImageEffectSimpleSourceClipName);
  addSupportedComponents(source);
  source->setTemporalClipAccess(false);

  if (context == OFX::eContextGeneral) {
    auto* mask = d.defineClip(kMaskClip);
    mask->addSupportedComponent(OFX::ePixelComponentAlpha);
    mask->setOptional(true);
    mask->setIsMask(true);
    mask->setSupportsTiles(true);
    mask->setTemporalClipAccess(false);
  }

  auto* output = d.defineClip(kOfxImageEffectOutputClipName);
  addSupportedComponents(output);

  ofx::defineDouble(d,kAmount,"Amount",0,0,1,0,1,0.01,"Diffusion strength",OFX::eDoubleTypeScale);
  ofx::defineDouble(d,kRadius,"Radius",12,0,250,0,100,0.25,"Master diffusion radius in full-resolution pixels");
  ofx::defineDouble(d,kRadiusX,"Chroma X Radius",1,0,4,0,2,0.01,"Multiplier along the primary diffusion axis",OFX::eDoubleTypeScale);
  ofx::defineDouble(d,kRadiusY,"Chroma Y Radius",1,0,4,0,2,0.01,"Multiplier along the secondary diffusion axis",OFX::eDoubleTypeScale);
  ofx::defineDouble(d,kAngle,"Angle",0,-180,180,-180,180,0.1,"Orientation of the primary diffusion axis",OFX::eDoubleTypeAngle);
  ofx::defineDouble(d,kLuminancePreservation,"Luminance Preservation",1,0,1,0,1,0.01,"At 100%, retain original luminance exactly",OFX::eDoubleTypeScale);
  ofx::defineDouble(d,kEdgeProtection,"Edge Protection",0.5,0,1,0,1,0.01,"Protect significant luminance boundaries",OFX::eDoubleTypeScale);
  ofx::defineDouble(d,kEdgeSoftness,"Edge Softness",0.25,0.001,4,0.01,1,0.01,"Softness of luminance boundary discrimination");
  ofx::defineDouble(d,kHighlightBias,"Highlight Bias",0,-1,1,-1,1,0.01,"Bias processing strength in highlights");
  ofx::defineDouble(d,kMidtoneBias,"Midtone Bias",0,-1,1,-1,1,0.01,"Bias processing strength in midtones");
  ofx::defineDouble(d,kShadowBias,"Shadow Bias",0,-1,1,-1,1,0.01,"Bias processing strength in shadows");
  ofx::defineDouble(d,kSaturationCompensation,"Saturation Compensation",0,-2,2,-1,1,0.01,"Opponent-chroma gain in stops after diffusion");
  ofx::defineBoolean(d,kRangeEnabled,"Enable Range",false,"Enable luminance range weighting");
  ofx::defineDouble(d,kRangeMinimum,"Range Minimum",0,-65504,65504,-1,2,0.01,"Lower luminance range boundary");
  ofx::defineDouble(d,kRangeMaximum,"Range Maximum",1,-65504,65504,-1,16,0.01,"Upper luminance range boundary");
  ofx::defineDouble(d,kRangeSoftness,"Range Softness",0.1,0,65504,0,2,0.01,"Smooth falloff around range boundaries");
  auto* gamut = d.defineChoiceParam(kWorkingGamut);
  gamut->setLabels("Working Gamut","Working Gamut","Working Gamut");
  gamut->setScriptName(kWorkingGamut);
  gamut->setHint("Primaries of the already-linear input; no transfer function is applied");
  gamut->appendOption("ACEScg");
  gamut->appendOption("Linear Rec.709 / sRGB Primaries");
  gamut->appendOption("Linear Rec.2020");
  gamut->appendOption("Display P3 D65 Primaries");
  gamut->setDefault(0);
  ofx::defineBoolean(d,kInvertMask,"Invert Mask",false,"Invert the external mask input");
  ofx::defineDouble(d,kMix,"Mix",1,0,1,0,1,0.01,"Final effect mix",OFX::eDoubleTypeScale);
}

OFX::ImageEffect* ChromaDiffusionFactory::createInstance(OfxImageEffectHandle handle,
                                                          OFX::ContextEnum) {
  return new ChromaDiffusionEffect(handle);
}

}  // namespace pigment::plugin
