#include "core/PigmentPhase4.h"

#include "core/ChunkGradientSynthesis.h"
#include "core/ColorSpace.h"
#include "core/PlateSpill.h"
#include "core/RegionHierarchy.h"
#include "core/RegionalEigenField.h"
#include "core/SparseTransitionField.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <cstring>
#include <stdexcept>
#include <chrono>

namespace pigment {
namespace {
float clamp01(float v) noexcept { return std::max(0.0f, std::min(1.0f, v)); }
float maskValue(const ConstImageView *mask, int x, int y, bool invert) {
  float v = mask ? (mask->bounds.contains(x,y)?clamp01(mask->pixel(x, y)[0]):0.0f) : 1.0f;
  return invert ? 1.0f - v : v;
}
YabPixel value(ConstYabPlanes p, int x, int y) {
  return {p.y.at(x, y), p.a.at(x, y), p.b.at(x, y)};
}
YabPixel plateValue(const PublicPlateSet &p, int i, int x, int y) {
  auto a = p.appearance(i);
  return {a.y.at(x, y), a.a.at(x, y), a.b.at(x, y)};
}
YabPixel mix(YabPixel a, YabPixel b, float t) {
  return {a.y + (b.y - a.y) * t, a.a + (b.a - a.a) * t, a.b + (b.b - a.b) * t};
}
} // namespace

Phase4RenderDiagnostics
processPigmentPhase4(const Phase4RenderInputs &in,
                     const ExecutionContext &execution) {
  const RectI b = in.source.bounds;
  using Clock=std::chrono::steady_clock;
  const auto start=Clock::now();auto mark=start;
  Phase4RenderDiagnostics diagnostics;
  auto elapsed=[&](){auto now=Clock::now();double ms=std::chrono::duration<double,std::milli>(now-mark).count();mark=now;return ms;};
  const auto &p = in.params;
  MatrixOpponentTransform transform(p.gamut);
  if (p.debugView == PigmentDebugView::Phase4Source ||
      (p.debugView == PigmentDebugView::Final &&
      (p.amount == 0.0f || p.mix == 0.0f))) {
    execution.parallelRows(
        in.renderWindow.y1, in.renderWindow.y2, [&](int y0, int y1) {
          for (int y = y0; y < y1; ++y)
            for (int x = in.renderWindow.x1; x < in.renderWindow.x2; ++x) {
              const float *s = in.source.pixel(x, y);
              float *d = in.destination.pixel(x, y);
              for (int c = 0; c < in.destination.components; ++c)
                d[c] = s[c];
            }
        });
    return {};
  }
  OwnedYabPlanes original(b);
  OwnedPlane alpha(b, 1.0f);
  auto ov = original.view();
  auto av = alpha.view();
  for (int y = b.y1; y < b.y2; ++y)
    for (int x = b.x1; x < b.x2; ++x) {
      const float *s = in.source.pixel(x, y);
      float a = in.source.components == 4 ? s[3] : 1.0f;
      av.at(x, y) = a;
      std::array<float, 3> rgb{s[0], s[1], s[2]};
      if (p.premultiplied && std::abs(a) > 1e-6f)
        for (float &v : rgb)
          v /= a;
      YabPixel q = transform.toYab(rgb);
      ov.y.at(x, y) = q.y;
      ov.a.at(x, y) = q.a;
      ov.b.at(x, y) = q.b;
    }
  Phase4ResearchCache localCache;
  auto &cache=in.cache?*in.cache:localCache;
  uint64_t hash=1469598103934665603ull;
  for(auto field:{ov.y,ov.a,ov.b})for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){uint32_t bits;float v=field.at(x,y);std::memcpy(&bits,&v,4);hash^=bits;hash*=1099511628211ull;}
  std::vector<double> key{double(hash>>32),double(uint32_t(hash)),double(b.x1),double(b.y1),double(b.x2),double(b.y2),in.geometry.pixelAspect,in.geometry.renderScaleX,in.geometry.renderScaleY,
    double(p.phase4.latentCount),double(p.phase4.plateCount),p.phase4.plateOverlap};
  auto ySupportKey=key;ySupportKey.push_back(phase4SupportRadiusY(p.phase4));
  auto abSupportKey=key;abSupportKey.push_back(phase4SupportRadiusAB(p.phase4));
  if(!cache.automatic || cache.automaticKey!=key){
    auto frozen=p.phase4;frozen.structureRespect=.8f; // approved intrinsic support context; Spill respect is independent
    auto next=std::make_unique<Phase4AutomaticResult>(buildPhase4AutomaticPlates(static_cast<const OwnedYabPlanes&>(original).view(),frozen,in.geometry,execution,&cache.sourceAnalysis));
    if(execution.cancelled())throw std::runtime_error("Phase 4 automatic build cancelled");
    if(!next->diagnostics.eigenspaceFinite || !next->diagnostics.componentsFinite || !next->diagnostics.appearanceFinite)return {next->diagnostics};
    cache.automatic=std::move(next);cache.automaticKey=key;cache.supportKeyY=ySupportKey;cache.supportKeyAB=abSupportKey;++cache.supportBuildsY;++cache.supportBuildsAB;cache.hierarchy.reset();cache.synthesis.reset();++cache.automaticBuilds;
  }
  if(cache.supportKeyY!=ySupportKey || cache.supportKeyAB!=abSupportKey){
    auto frozen=p.phase4;frozen.structureRespect=.8f;
    bool yChanged=cache.supportKeyY!=ySupportKey,abChanged=cache.supportKeyAB!=abSupportKey;
    updatePhase4PlateSupports(*cache.automatic,frozen,yChanged,abChanged,execution);
    if(execution.cancelled())throw std::runtime_error("Phase 4 support cancelled");
    cache.supportKeyY=ySupportKey;cache.supportKeyAB=abSupportKey;cache.supportBuildsY+=yChanged;cache.supportBuildsAB+=abChanged;
  }
  diagnostics.analysisMs=elapsed();
  auto hkey=key;for(float v:{phase4SupportRadiusY(p.phase4),phase4SupportRadiusAB(p.phase4),p.phase4.lumaChromaCoupling,p.phase4.ySupport,p.phase4.abSupport,p.phase4.boundaryLock,p.phase4.mergeSelectivity,p.phase4.internalVariation})hkey.push_back(v);
  if(!cache.hierarchy || cache.hierarchyKey!=hkey){
    auto supported=std::make_unique<PublicPlateSet>(cache.automatic->plates);
    for(int i=0;i<supported->count();++i)for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){supported->supportY(i).at(x,y)=clamp01(supported->supportY(i).at(x,y)*p.phase4.ySupport);supported->supportAB(i).at(x,y)=clamp01(supported->supportAB(i).at(x,y)*p.phase4.abSupport);}
    auto next=std::make_unique<Phase4RegionHierarchy>(buildPhase4RegionHierarchy(static_cast<const OwnedYabPlanes&>(original).view(),*supported,p.phase4,execution));
    if(execution.cancelled())throw std::runtime_error("Phase 4 hierarchy build cancelled");
    cache.supported=std::move(supported);cache.hierarchy=std::move(next);cache.hierarchyKey=hkey;cache.hierarchyCutKey.clear();cache.synthesis.reset();++cache.hierarchyBuilds;
  }
  auto cutKey=hkey;cutKey.push_back(p.phase4.lumaChunkScale);cutKey.push_back(p.phase4.chromaChunkScale);
  if(cache.hierarchyCutKey!=cutKey){cutPhase4RegionHierarchy(*cache.hierarchy,p.phase4.lumaChunkScale,p.phase4.chromaChunkScale);cache.hierarchyCutKey=cutKey;++cache.hierarchyCuts;}
  diagnostics.hierarchyMs=elapsed();
  auto skey=cutKey;skey.push_back(int(p.phase4.representation));skey.push_back(p.phase4.gradientComplexity);
  skey.push_back(p.phase4.yGradientComplexity);skey.push_back(p.phase4.abGradientComplexity);
  auto fieldKeyY=hkey,fieldKeyAB=hkey;
  for(double v:{double(p.phase4.representation),double(p.phase4.lumaChunkScale),double(p.phase4.yGradientComplexity<0?p.phase4.gradientComplexity:p.phase4.yGradientComplexity)})fieldKeyY.push_back(v);
  for(double v:{double(p.phase4.representation),double(p.phase4.chromaChunkScale),double(p.phase4.abGradientComplexity<0?p.phase4.gradientComplexity:p.phase4.abGradientComplexity)})fieldKeyAB.push_back(v);
  const bool rebuildY=!cache.synthesis || cache.fieldKeyY!=fieldKeyY;
  const bool rebuildAB=!cache.synthesis || cache.fieldKeyAB!=fieldKeyAB;
  if(!cache.synthesis || cache.synthesisKey!=skey){
    auto next=std::make_unique<Phase4ChunkSynthesis>(b);
    if(p.phase4.lumaChunkScale==0 && p.phase4.chromaChunkScale==0){
      for(int i=0;i<cache.supported->count();++i){next->plateAppearance.emplace_back(b);auto from=cache.supported->appearance(i);
        auto dst=next->plateAppearance.back().view();for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){dst.y.at(x,y)=from.y.at(x,y);dst.a.at(x,y)=from.a.at(x,y);dst.b.at(x,y)=from.b.at(x,y);}}
    }
    else if(p.phase4.representation==Phase4Representation::Poisson)*next=synthesizePhase4Chunks(static_cast<const OwnedYabPlanes&>(original).view(),*cache.supported,*cache.hierarchy,p.phase4,execution,{},cache.synthesis.get(),rebuildY,rebuildAB);
    else if(p.phase4.representation==Phase4Representation::SecondMoments){
      Phase4BroadFormOptions preserved;preserved.enabled=true;
      preserved.firstStrengthY=20;preserved.firstStrengthAB=2.5;
      preserved.secondStrengthY=20;preserved.secondStrengthAB=1.25;
      *next=synthesizePhase4Chunks(static_cast<const OwnedYabPlanes&>(original).view(),*cache.supported,*cache.hierarchy,p.phase4,execution,preserved);
    }
    else if(p.phase4.representation==Phase4Representation::RegionalEigen){auto eigen=regionalEigenFieldSweep(*cache.supported,*cache.hierarchy,execution,false,true);next->plateAppearance=std::move(eigen.results.front().appearance);}
    else if(p.phase4.representation==Phase4Representation::SparseCurve){auto curves=sparseTransitionField(*cache.supported,*cache.hierarchy,execution);next->plateAppearance=std::move(curves.appearance);}
    else for(int i=0;i<cache.supported->count();++i){next->plateAppearance.emplace_back(b);auto from=cache.supported->appearance(i);auto to=next->plateAppearance.back().view();for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){to.y.at(x,y)=from.y.at(x,y);to.a.at(x,y)=from.a.at(x,y);to.b.at(x,y)=from.b.at(x,y);}}
    // Zero chunk scale is an exact per-family synthesis bypass in every mode.
    for(int i=0;i<cache.supported->count();++i){auto from=cache.supported->appearance(i);auto to=next->plateAppearance[size_t(i)].view();for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){if(p.phase4.lumaChunkScale==0)to.y.at(x,y)=from.y.at(x,y);if(p.phase4.chromaChunkScale==0){to.a.at(x,y)=from.a.at(x,y);to.b.at(x,y)=from.b.at(x,y);}}}
    if(execution.cancelled())throw std::runtime_error("Phase 4 synthesis build cancelled");
    for(const auto &plate:next->solver)for(const auto &solver:plate)if(!solver.converged)return {cache.automatic->diagnostics,false};
    cache.synthesis=std::move(next);cache.synthesisKey=skey;++cache.synthesisBuilds;
    cache.fieldKeyY=fieldKeyY;cache.fieldKeyAB=fieldKeyAB;cache.fieldBuildsY+=rebuildY;cache.fieldBuildsAB+=rebuildAB;
  }
  const auto &automatic=*cache.automatic;
  const auto &plates=*cache.supported;
  const auto &hierarchy=*cache.hierarchy;
  const auto &synthesis=*cache.synthesis;
  diagnostics.synthesisMs=elapsed();
  for(const auto &plate : synthesis.solver)
    for(const auto &solver : plate)
      if(!solver.converged) return {automatic.diagnostics, false};
  auto tkey=key;tkey.push_back(phase4SupportRadiusY(p.phase4));tkey.push_back(phase4SupportRadiusAB(p.phase4));tkey.push_back(p.phase4.ySupport);tkey.push_back(p.phase4.abSupport);
  tkey.push_back(p.phase4.spillReach);tkey.push_back(p.phase4.structureRespect);
  if(!cache.transport || cache.transportKey!=tkey){
    auto next=std::make_unique<Phase4SpillTransport>();
    bool accelerated=in.accelerateTransport && p.phase4.colorInteraction!=ColorInteractionLaw::SpectralPigment &&
      in.accelerateTransport(plates,automatic.analysisGraph,p.phase4,*next);
    if(!accelerated)*next=preparePhase4SpillTransport(plates,automatic.analysisGraph,p.phase4,execution);
    if(execution.cancelled())throw std::runtime_error("Phase 4 transport cancelled");
    cache.transport=std::move(next);cache.transportKey=tkey;++cache.transportBuilds;
  }
  diagnostics.transportMs=elapsed();
  auto ikey=skey;ikey.insert(ikey.end(),tkey.begin(),tkey.end());
  for(double v:{double(p.gamut),double(in.backendIdentity),double(bool(in.accelerateSpill)),double(p.phase4.colorInteraction),double(p.phase4.pigmentDensity),double(p.phase4.spillAmount),double(p.phase4.spillAsymmetry),double(p.phase4.lumaSpill),double(p.phase4.chromaSpill)})ikey.push_back(v);
  for(const auto& plate:p.phase4.plates)for(double v:{double(plate.enabled),double(plate.weight),double(plate.tone),double(plate.biasA),double(plate.biasB),double(plate.spillOut),double(plate.receiveSpill)})ikey.push_back(v);
  if(!cache.interaction || cache.interactionKey!=ikey){
  auto next=std::make_unique<Phase4SpillResult>(b);
  if(in.accelerateSpill && p.phase4.colorInteraction!=ColorInteractionLaw::SpectralPigment && !execution.cancelled()){
    diagnostics.metalAttempted=true;
    diagnostics.metalSpill=in.accelerateSpill(static_cast<const OwnedYabPlanes&>(original).view(),plates,synthesis,automatic.analysisGraph,p.phase4,*cache.transport,p.gamut,*next);
  }
  if(!diagnostics.metalSpill)*next=applyPhase4Spill(static_cast<const OwnedYabPlanes&>(original).view(),plates,synthesis,automatic.analysisGraph,p.phase4,execution,cache.transport.get(),p.gamut);
  if(execution.cancelled())throw std::runtime_error("Phase 4 interaction cancelled");
  cache.interaction=std::move(next);cache.interactionKey=ikey;cache.interactionMetal=diagnostics.metalSpill;++cache.interactionBuilds;
  }else diagnostics.metalSpill=cache.interactionMetal;
  const auto& spill=*cache.interaction;
  diagnostics.interactionMs=elapsed();
  std::optional<Phase4SpillResult> artisticPreSpill;
  if (p.debugView == PigmentDebugView::Phase4PreSpill || p.debugView == PigmentDebugView::Phase4SpillDifference) {
    auto noSpill = p.phase4;
    noSpill.spillAmount = 0.0f;
    artisticPreSpill.emplace(applyPhase4Spill(
        static_cast<const OwnedYabPlanes &>(original).view(),
        plates, synthesis, automatic.analysisGraph, noSpill,
        execution,cache.transport.get(),p.gamut));
  }
  const int plateCount = automatic.plates.count(),
            latentCount = automatic.latent.count();
  execution.parallelRows(
      in.renderWindow.y1, in.renderWindow.y2, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y)
          for (int x = in.renderWindow.x1; x < in.renderWindow.x2; ++x) {
            const float *sp = in.source.pixel(x, y);
            float *dp = in.destination.pixel(x, y);
            if(p.debugView==PigmentDebugView::Final && maskValue(in.mask,x,y,p.invertMask)==0){
              for(int c=0;c<in.destination.components;++c)dp[c]=sp[c];
              continue;
            }
            const float sourceAlpha = av.at(x, y);
            YabPixel source = value(
                         static_cast<const OwnedYabPlanes &>(original).view(),
                         x, y),
                     out = value(
                         static_cast<const OwnedYabPlanes &>(spill.composite)
                             .view(),
                         x, y);
            bool gray = false, composite = false;
            switch (p.debugView) {
            case PigmentDebugView::Phase4Source: out=source; break;
            case PigmentDebugView::Phase4PublicReconstruction: {
              out={0,0,0};for(int i=0;i<plateCount;++i){float a=automatic.plates.alpha(i).at(x,y);auto v=plateValue(automatic.plates,i,x,y);out.y+=a*v.y;out.a+=a*v.a;out.b+=a*v.b;}break;
            }
            case PigmentDebugView::Phase4SpillDifference: {
              auto pre=value(static_cast<const OwnedYabPlanes&>(artisticPreSpill->composite).view(),x,y);
              out={.5f+16*(out.y-pre.y),.5f+16*(out.a-pre.a),.5f+16*(out.b-pre.b)};composite=true;break;
            }
            case PigmentDebugView::Phase4YInfluence:
            case PigmentDebugView::Phase4YTransport:
            case PigmentDebugView::Phase4ABTransport: {
              int i=std::max(0,std::min(plateCount-1,p.phase4.debugPlate));
              const auto &fields=p.debugView==PigmentDebugView::Phase4YInfluence?spill.influenceY:(p.debugView==PigmentDebugView::Phase4YTransport?spill.transportY:spill.transportAB);
              out={fields[size_t(i)].view().at(x,y),0,0};gray=true;break;
            }
            case PigmentDebugView::Phase4SourceBoundaryStrength:
            case PigmentDebugView::Phase4BoundaryHierarchy: {
              float v = hierarchy.boundaryStrength.view().at(x, y);
              out = {v, 0, 0};
              gray = true;
              break;
            }
            case PigmentDebugView::Phase4AtomicRegions: {
              float v = hierarchy.atomicRegionDisplay.view().at(x, y);
              out = {v, 1.0f - v, .25f + .5f * v};
              composite = true;
              break;
            }
            case PigmentDebugView::Phase4LatentComponent: {
              int i =
                  std::max(0, std::min(latentCount - 1, p.phase4.debugLatent));
              float v = automatic.latent.alpha(i).at(x, y);
              out = {v, 0, 0};
              gray = true;
              break;
            }
            case PigmentDebugView::Phase4LatentComposite: {
              float r = 0, g = 0, bb = 0;
              for (int i = 0; i < latentCount; ++i) {
                float v = automatic.latent.alpha(i).at(x, y);
                r += v * float((i * 97 + 29) % 255) / 255.0f;
                g += v * float((i * 57 + 83) % 255) / 255.0f;
                bb += v * float((i * 131 + 17) % 255) / 255.0f;
              }
              out = {r, g, bb};
              composite = true;
              break;
            }
            case PigmentDebugView::Phase4LatentReconstructionError: {
              float v = automatic.latent.reconstructionError().at(x, y);
              out = {v, 0, 0};
              gray = true;
              break;
            }
            case PigmentDebugView::Phase4SpectralResidual: {
              int i =
                  std::max(0, std::min(automatic.latent.spectralModeCount() - 1,
                                       p.phase4.debugLatent));
              float v = .5f + .5f * automatic.latent.spectralMode(i).at(x, y);
              out = {v, 0, 0};
              gray = true;
              break;
            }
            case PigmentDebugView::Phase4ComponentRecoveryError: {
              float v = automatic.latent.recoveryError().at(x, y);
              out = {v, 0, 0};
              gray = true;
              break;
            }
            case PigmentDebugView::Phase4AppearanceUnmixingError: {
              float v = automatic.latent.reconstructionError().at(x, y);
              out = {v, 0, 0};
              gray = true;
              break;
            }
            case PigmentDebugView::Phase4PlateAlpha: {
              int i =
                  std::max(0, std::min(plateCount - 1, p.phase4.debugPlate));
              float v = automatic.plates.alpha(i).at(x, y);
              out = {v, 0, 0};
              gray = true;
              break;
            }
            case PigmentDebugView::Phase4PlateYSupport: {
              int i =
                  std::max(0, std::min(plateCount - 1, p.phase4.debugPlate));
              float v = plates.supportY(i).at(x, y);
              out = {v, 0, 0};
              gray = true;
              break;
            }
            case PigmentDebugView::Phase4PlateABSupport: {
              int i =
                  std::max(0, std::min(plateCount - 1, p.phase4.debugPlate));
              float v = plates.supportAB(i).at(x, y);
              out = {v, 0, 0};
              gray = true;
              break;
            }
            case PigmentDebugView::Phase4PlateYAppearance: {
              int i =
                  std::max(0, std::min(plateCount - 1, p.phase4.debugPlate));
              out = plateValue(automatic.plates, i, x, y);
              out.a = source.a;
              out.b = source.b;
              break;
            }
            case PigmentDebugView::Phase4PlateABAppearance: {
              int i =
                  std::max(0, std::min(plateCount - 1, p.phase4.debugPlate));
              out = plateValue(automatic.plates, i, x, y);
              out.y = source.y;
              break;
            }
            case PigmentDebugView::Phase4PlateOverlapComposite: {
              float r = 0, g = 0, bb = 0;
              for (int i = 0; i < plateCount; ++i) {
                float v = automatic.plates.alpha(i).at(x, y);
                r += v * float((i * 97 + 29) % 255) / 255.0f;
                g += v * float((i * 57 + 83) % 255) / 255.0f;
                bb += v * float((i * 131 + 17) % 255) / 255.0f;
              }
              out = {r, g, bb};
              composite = true;
              break;
            }
            case PigmentDebugView::Phase4YRegionHierarchy:
            case PigmentDebugView::Phase4ABRegionHierarchy:
            case PigmentDebugView::Phase4YChunks:
            case PigmentDebugView::Phase4ABChunks: {
              int i =
                  std::max(0, std::min(plateCount - 1, p.phase4.debugPlate));
              const auto &labels =
                  (p.debugView == PigmentDebugView::Phase4YRegionHierarchy ||
                   p.debugView == PigmentDebugView::Phase4YChunks)
                      ? hierarchy.plates[size_t(i)].yChunk
                      : hierarchy.plates[size_t(i)].abChunk;
              int local = (y - b.y1) * b.width() + x - b.x1;
              unsigned hash = unsigned(labels[size_t(local)] + 1) * 0x9e3779b9u;
              hash ^= hash >> 16;
              out = {float(hash & 255u) / 255.0f,
                     float((hash >> 8) & 255u) / 255.0f,
                     float((hash >> 16) & 255u) / 255.0f};
              composite = true;
              break;
            }
            case PigmentDebugView::Phase4RemovedBoundaries:
            case PigmentDebugView::Phase4RetainedBoundaries: {
              int i =
                  std::max(0, std::min(plateCount - 1, p.phase4.debugPlate));
              const auto &h = hierarchy.plates[size_t(i)];
              bool retained =
                  p.debugView == PigmentDebugView::Phase4RetainedBoundaries;
              float yy = (retained ? h.yRetainedBoundaries
                                   : h.yRemovedBoundaries)
                             .view()
                             .at(x, y);
              float ab = (retained ? h.abRetainedBoundaries
                                   : h.abRemovedBoundaries)
                             .view()
                             .at(x, y);
              out = {yy, ab, ab};
              composite = true;
              break;
            }
            case PigmentDebugView::Phase4SourceGradientField:
            case PigmentDebugView::Phase4SimplifiedGradientField: {
              if(synthesis.sourceGradient.empty()){out={0,0,0};gray=true;break;}
              int i =
                  std::max(0, std::min(plateCount - 1, p.phase4.debugPlate));
              float v =
                  (p.debugView == PigmentDebugView::Phase4SourceGradientField
                       ? synthesis.sourceGradient[size_t(i)]
                       : synthesis.simplifiedGradient[size_t(i)])
                      .view()
                      .at(x, y);
              out = {v, 0, 0};
              gray = true;
              break;
            }
            case PigmentDebugView::Phase4GradientReconstruction: {
              int i =
                  std::max(0, std::min(plateCount - 1, p.phase4.debugPlate));
              out = value(
                  static_cast<const OwnedYabPlanes &>(
                      synthesis.plateAppearance[size_t(i)])
                      .view(),
                  x, y);
              break;
            }
            case PigmentDebugView::Phase4PrimitiveFitError: {
              if(synthesis.fitError.empty()){out={0,0,0};gray=true;break;}
              int i =
                  std::max(0, std::min(plateCount - 1, p.phase4.debugPlate));
              float v = synthesis.fitError[size_t(i)].view().at(x,y);
              out = {v, 0, 0};
              gray = true;
              break;
            }
            case PigmentDebugView::Phase4PreSpill:
              out = value(static_cast<const OwnedYabPlanes &>(
                              artisticPreSpill->composite)
                              .view(),
                          x, y);
              break;
            case PigmentDebugView::Phase4SpillInfluence: {
              int i =
                  std::max(0, std::min(plateCount - 1, p.phase4.debugPlate));
              float v = spill.influence[size_t(i)].view().at(x, y);
              out = {v, 0, 0};
              gray = true;
              break;
            }
            case PigmentDebugView::Phase4PostSpill:
              out = value(
                  static_cast<const OwnedYabPlanes &>(spill.composite).view(),
                  x, y);
              break;
            case PigmentDebugView::Phase4DifferenceFromSource:
              out = {.5f + .25f * (out.y - source.y), .25f * (out.a - source.a),
                     .25f * (out.b - source.b)};
              break;
            default:
              break;
            }
            std::array<float, 3> rgb;
            if (gray)
              rgb = {out.y, out.y, out.y};
            else if (composite)
              rgb = {out.y, out.a, out.b};
            else
              rgb = transform.toRgb(out);
            if (p.debugView == PigmentDebugView::Final) {
              float gate =
                  clamp01(p.amount) * maskValue(in.mask, x, y, p.invertMask);
              rgb = transform.toRgb(mix(source, out, gate));
            }
            const float finalMix =
                p.debugView == PigmentDebugView::Final ? clamp01(p.mix) : 1.0f;
            for (int c = 0; c < 3; ++c) {
              float originalStraight =
                  (p.premultiplied && std::abs(sourceAlpha) > 1e-6f)
                      ? sp[c] / sourceAlpha
                      : sp[c];
              float v =
                  originalStraight + (rgb[c] - originalStraight) * finalMix;
              if (p.premultiplied && std::abs(sourceAlpha) > 1e-6f)
                v *= sourceAlpha;
              if (p.premultiplied && std::abs(sourceAlpha) <= 1e-6f)
                v = sp[c];
              dp[c] = v;
            }
            if (in.destination.components == 4)
              dp[3] = sourceAlpha;
          }
      });
  diagnostics.gate=automatic.diagnostics;diagnostics.finalMs=elapsed();
  diagnostics.totalMs=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
  return diagnostics;
}

} // namespace pigment
