#include "core/ChunkGradientSynthesis.h"
#include "core/LatentPlateGraph.h"
#include "core/PigmentPhase4.h"
#include "core/PlateSpill.h"
#include "core/RegionHierarchy.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace {
int failures = 0;
void check(bool v, const char *m) {
  if (!v) {
    ++failures;
    std::cerr << "FAIL: " << m << '\n';
  }
}

pigment::OwnedYabPlanes fixture(pigment::RectI b) {
  pigment::OwnedYabPlanes image(b);
  auto v = image.view();
  for (int y = b.y1; y < b.y2; ++y)
    for (int x = b.x1; x < b.x2; ++x) {
      float fx = float(x - b.x1) / std::max(1, b.width() - 1),
            fy = float(y - b.y1) / std::max(1, b.height() - 1);
      v.y.at(x, y) = -.3f + 2.6f * fx + .07f * std::sin(.7f * x);
      v.a.at(x, y) = .4f * (fy - .5f) + .08f * std::sin(.3f * x);
      v.b.at(x, y) = fx < .45f ? -.3f : .5f;
    }
  return image;
}

void parameterSemantics() {
  pigment::Phase4Params p;
  p.plateScale = 50;
  p.plateOverlap = 0;
  check(pigment::phase4SupportRadiusY(p) == 0 &&
            pigment::phase4SupportRadiusAB(p) == 0,
        "zero overlap has no support expansion");
  p.plateOverlap = .5f;
  p.chromaSupportRatio = 2;
  p.lumaChromaCoupling = 0;
  check(std::abs(pigment::phase4SupportRadiusY(p) - 25) < 1e-6f,
        "Y support radius is Scale times Overlap");
  check(std::abs(pigment::phase4SupportRadiusAB(p) - 50) < 1e-6f,
        "AB support uses independent support ratio");
  float before = pigment::phase4SupportRadiusAB(p);
  p.chromaChunkScale = 400;
  check(std::abs(pigment::phase4SupportRadiusAB(p) - before) < 1e-6f,
        "chunk scale never changes support radius");
  p.lumaChromaCoupling = 1;
  check(std::abs(pigment::phase4SupportRadiusAB(p) - 25) < 1e-6f,
        "full coupling equalizes support radii");
  check(std::abs(pigment::phase4PlateEntropyCoefficient(.5f) - .25f) < 1e-6f,
        "overlap entropy coefficient is squared");
}

void automaticPlates() {
  pigment::RectI b{0, 0, 48, 36};
  auto image = fixture(b);
  pigment::Phase4Params p;
  p.latentCount = 12;
  p.plateCount = 4;
  p.plateScale = 12;
  p.plateOverlap = .55f;
  p.lumaChunkScale = 8;
  p.chromaChunkScale = 24;
  auto source = static_cast<const pigment::OwnedYabPlanes &>(image).view();
  auto result = pigment::buildPhase4AutomaticPlates(source, p, {});
  check(result.latent.count() <= 12 && result.latent.count() >= 5 &&
            result.plates.count() == 4,
        "latent count is a useful achieved count bounded by the requested "
        "maximum");
  check(result.diagnostics.requestedLatentCount == 12 &&
            result.diagnostics.activeLatentCount == result.latent.count(),
        "requested and achieved latent counts are reported separately");
  double reconstruction = 0;
  for (int y = b.y1; y < b.y2; ++y)
    for (int x = b.x1; x < b.x2; ++x) {
      float ls = 0, ps = 0;
      for (int i = 0; i < result.latent.count(); ++i)
        ls += result.latent.alpha(i).at(x, y);
      for (int i = 0; i < 4; ++i)
        ps += result.plates.alpha(i).at(x, y);
      check(std::abs(ls - 1) < 3e-3f, "latent alpha sums to one");
      check(std::abs(ps - 1) < 3e-3f, "plate alpha sums to one");
      reconstruction += result.latent.reconstructionError().at(x, y);
    }
  check(std::isfinite(reconstruction), "appearance reconstruction is finite");
  check(result.diagnostics.meanEffectiveComponents > 1.0f &&
            result.diagnostics.componentEffectiveRank > 1.0f,
        "component recovery is fuzzy and has nontrivial rank");
  check(result.diagnostics.appearanceUnmixingError < 1e-4f &&
            result.diagnostics.appearanceSpatialVariation > 1e-4f,
        "A3 appearance is spatially varying and reconstructive");
  check(result.diagnostics.fullResolutionReconstructionError < 1e-5f,
        "full-resolution conditional color refinement preserves source reconstruction");
  const auto &distributions = result.latent.distributions();
  check(distributions.components.size() == size_t(result.latent.count()) &&
        distributions.width > 1 && distributions.height > 1,
        "latent local Gaussian distributions remain stored after unmixing");
  for(const auto &component : distributions.components) for(const auto &d : component) {
    check(d.covariance[0]>0 && d.covariance[4]>0 && d.covariance[8]>0,
          "conditional color covariance has positive conditioning");
    check(std::abs(d.covariance[1]-d.covariance[3])<1e-12 &&
          std::abs(d.covariance[2]-d.covariance[6])<1e-12,
          "retained local covariance is symmetric");
  }
  check(result.diagnostics.publicReconstructionError < 1e-4f &&
            result.diagnostics.publicPlateEffectiveRank > 1.1f &&
            result.diagnostics.maximumPublicPlateCorrelation < 0.999f,
        "public plates reconstruct while preserving distinct ownership");
  check(result.diagnostics.componentOccupancy.size() ==
            size_t(result.latent.count()),
        "per-component occupancy is reported");
  for (int row = 0; row < result.analysisGraph.nodeCount(); ++row) {
    double signedSum = 0;
    for (int edge = result.analysisGraph.rowOffsets[row];
         edge < result.analysisGraph.rowOffsets[row + 1]; ++edge) {
      auto value = result.analysisGraph.edges[edge];
      signedSum += value.signedMixtureWeight;
      check(value.weight >= 0 && value.weight <= 1,
            "transport affinity F is nonnegative and bounded");
    }
    check(std::abs(signedSum - 1) < 1e-3,
          "signed W_CMF rows preserve affine reconstruction");
  }
  auto repeated = pigment::buildPhase4AutomaticPlates(source, p, {});
  check(repeated.latent.count() == result.latent.count(),
        "progressive recovery active count is deterministic");
  for (int i = 0; i < result.latent.count(); ++i)
    for (int y = b.y1; y < b.y2; ++y)
      for (int x = b.x1; x < b.x2; ++x)
        check(repeated.latent.alpha(i).at(x, y) ==
                  result.latent.alpha(i).at(x, y),
              "progressive recovery alpha is deterministic");
  check(result.diagnostics.eigenspaceFinite &&
            result.diagnostics.componentsFinite &&
            result.diagnostics.appearanceFinite,
        "Gate A diagnostics are finite");
}

void regionHierarchy() {
  pigment::RectI bounds{0, 0, 64, 40};
  auto image = fixture(bounds);
  auto source = static_cast<const pigment::OwnedYabPlanes &>(image).view();
  pigment::Phase4Params parameters;
  parameters.latentCount = 12;
  parameters.plateCount = 4;
  parameters.lumaChunkScale = 0;
  parameters.chromaChunkScale = 0;
  auto automatic = pigment::buildPhase4AutomaticPlates(source, parameters, {});
  auto zero =
      pigment::buildPhase4RegionHierarchy(source, automatic.plates, parameters);
  check(zero.atomicRegionCount > 1, "shared atomic source RAG has regions");
  for (const auto &plate : zero.plates) {
    check(plate.yChunkCount == zero.atomicRegionCount &&
              plate.abChunkCount == zero.atomicRegionCount,
          "Chunk Scale zero is an exact atomic-hierarchy cut");
  }
  parameters.lumaChunkScale = 48;
  parameters.chromaChunkScale = 96;
  auto merged =
      pigment::buildPhase4RegionHierarchy(source, automatic.plates, parameters);
  for (size_t plate = 0; plate < merged.plates.size(); ++plate) {
    check(merged.plates[plate].yChunkCount <= zero.plates[plate].yChunkCount &&
              merged.plates[plate].abChunkCount <=
                  zero.plates[plate].abChunkCount,
          "higher chunk cuts merge regions monotonically");
    const auto &h=merged.plates[plate];
    check(h.yTree.size()==zero.plates[plate].yTree.size(),
          "Chunk Scale only cuts the existing tree");
    for(size_t node=0;node<h.yTree.size();++node) {
      const auto &n=h.yTree[node];
      check(n.left==zero.plates[plate].yTree[node].left &&
            n.right==zero.plates[plate].yTree[node].right &&
            n.level==zero.plates[plate].yTree[node].level,
            "scale changes preserve merge topology and disappearance levels");
      if(n.left>=0) check(n.level>=h.yTree[size_t(n.left)].level &&
                         n.level>=h.yTree[size_t(n.right)].level,
                         "merge levels are nested and monotonic");
    }
    for(int y=0;y<bounds.height();++y) for(int x=0;x<bounds.width();++x) {
      size_t p=size_t(y)*bounds.width()+x;
      if(x+1==bounds.width()) check(std::isinf(h.yEdgeX[p]),"RoD grid edge has infinite level");
      else if(merged.atomicRegion[p]==merged.atomicRegion[p+1])
        check(h.yEdgeX[p]==0,"same-atomic-region edge has zero level");
      else check((h.yEdgeX[p]<=parameters.lumaChunkScale)==(h.yChunk[p]==h.yChunk[p+1]),
                 "LCA edge level agrees with the hierarchy cut");
    }
  }
  bool conditioned = false;
  for (size_t plate = 1; plate < merged.plates.size(); ++plate)
    conditioned |= merged.plates[plate].yChunk != merged.plates[0].yChunk ||
                   merged.plates[plate].abChunk != merged.plates[0].abChunk;
  check(conditioned, "chunk hierarchies are conditioned per public plate");

  auto synthesis = pigment::synthesizePhase4Chunks(source, automatic.plates,
                                                   merged, parameters);
  bool changed = false;
  for (int plate = 0; plate < automatic.plates.count(); ++plate) {
    auto before = automatic.plates.appearance(plate);
    auto after = static_cast<const pigment::OwnedYabPlanes &>(
                     synthesis.plateAppearance[size_t(plate)])
                     .view();
    for (int y = bounds.y1; y < bounds.y2; ++y)
      for (int x = bounds.x1; x < bounds.x2; ++x) {
        check(std::isfinite(after.y.at(x, y)) &&
                  std::isfinite(after.a.at(x, y)) &&
                  std::isfinite(after.b.at(x, y)),
              "chunk synthesis remains finite");
        changed |= std::abs(after.y.at(x, y) - before.y.at(x, y)) > 1e-5f;
      }
  }
  check(changed, "nonzero Chunk Scale synthesizes plate appearance");
  for(const auto &plate:synthesis.solver) for(const auto &solver:plate)
    check(solver.converged && solver.relativeResidual<=1e-5,
          "true bounded Poisson residual satisfies the specified tolerance");
  for(int plate=0;plate<automatic.plates.count();++plate) {
    auto before=automatic.plates.appearance(plate);
    auto after=static_cast<const pigment::OwnedYabPlanes &>(synthesis.plateAppearance[size_t(plate)]).view();
    const auto &h=merged.plates[size_t(plate)];
    for(int y=bounds.y1;y<bounds.y2;++y) for(int x=bounds.x1;x<bounds.x2;++x) {
      bool boundary=h.yRetainedBoundaries.view().at(x,y)>0 ||
                    x==bounds.x1 || x+1==bounds.x2 || y==bounds.y1 || y+1==bounds.y2;
      if(boundary || automatic.plates.supportY(plate).at(x,y)<.02f)
        check(after.y.at(x,y)==before.y.at(x,y),"retained and unsupported values are bit-exact constraints");
    }
  }

  parameters.spillAmount = 0;
  auto completeParams = parameters;
  completeParams.gradientComplexity = 1;
  auto complete = pigment::synthesizePhase4Chunks(source, automatic.plates,
                                                  merged, completeParams);
  for(int plate=0;plate<automatic.plates.count();++plate) {
    auto before=automatic.plates.appearance(plate);
    auto after=static_cast<const pigment::OwnedYabPlanes &>(complete.plateAppearance[size_t(plate)]).view();
    for(int y=bounds.y1;y<bounds.y2;++y) for(int x=bounds.x1;x<bounds.x2;++x)
      check(before.y.at(x,y)==after.y.at(x,y) && before.a.at(x,y)==after.a.at(x,y) &&
            before.b.at(x,y)==after.b.at(x,y),
            "full Gradient Complexity preserves automatic appearance bit-exactly");
  }
  auto noSpill = pigment::applyPhase4Spill(source, automatic.plates, synthesis,
                                           automatic.analysisGraph, parameters);
  parameters.spillAmount = .8f;
  parameters.lumaSpill = .05f;
  parameters.chromaSpill = 1.0f;
  auto spill = pigment::applyPhase4Spill(source, automatic.plates, synthesis,
                                         automatic.analysisGraph, parameters);
  double yChange = 0, abChange = 0;
  auto noSpillView =
      static_cast<const pigment::OwnedYabPlanes &>(noSpill.composite).view();
  auto spillView =
      static_cast<const pigment::OwnedYabPlanes &>(spill.composite).view();
  for (int y = bounds.y1; y < bounds.y2; ++y)
    for (int x = bounds.x1; x < bounds.x2; ++x) {
      yChange += std::abs(spillView.y.at(x, y) - noSpillView.y.at(x, y));
      abChange += std::hypot(spillView.a.at(x, y) - noSpillView.a.at(x, y),
                             spillView.b.at(x, y) - noSpillView.b.at(x, y));
    }
  check(abChange > yChange,
        "directed Spill can reorganize AB more strongly than Y");
  auto disabled=parameters;
  for(auto &control:disabled.plates) control.weight=0;
  auto noOwnership=pigment::applyPhase4Spill(source,automatic.plates,synthesis,
                                             automatic.analysisGraph,disabled);
  auto returned=static_cast<const pigment::OwnedYabPlanes &>(noOwnership.composite).view();
  for(int y=bounds.y1;y<bounds.y2;++y) for(int x=bounds.x1;x<bounds.x2;++x)
    check(returned.y.at(x,y)==source.y.at(x,y) && returned.a.at(x,y)==source.a.at(x,y) &&
          returned.b.at(x,y)==source.b.at(x,y),"zero artist ownership returns original source exactly");

  parameters.lumaChunkScale = 0;
  parameters.chromaChunkScale = 0;
  auto bypassHierarchy =
      pigment::buildPhase4RegionHierarchy(source, automatic.plates, parameters);
  auto bypass = pigment::synthesizePhase4Chunks(source, automatic.plates,
                                                bypassHierarchy, parameters);
  for (int plate = 0; plate < automatic.plates.count(); ++plate) {
    auto before = automatic.plates.appearance(plate);
    auto after = static_cast<const pigment::OwnedYabPlanes &>(
                     bypass.plateAppearance[size_t(plate)])
                     .view();
    for (int y = bounds.y1; y < bounds.y2; ++y)
      for (int x = bounds.x1; x < bounds.x2; ++x)
        check(after.y.at(x, y) == before.y.at(x, y) &&
                  after.a.at(x, y) == before.a.at(x, y) &&
                  after.b.at(x, y) == before.b.at(x, y),
              "Chunk Scale zero is a bit-exact synthesis bypass");
  }
}

void primitiveGradientSurvival() {
  // An accepted affine hypothesis must not erase the source residual.  All
  // interior edges here have ell=0, so the approved survival gain is exactly C.
  pigment::RectI bounds{0,0,32,32};
  pigment::OwnedYabPlanes image(bounds);
  pigment::PublicPlateSet plates(bounds,4);
  pigment::Phase4RegionHierarchy hierarchy;
  hierarchy.bounds=bounds;
  hierarchy.atomicRegion.assign(32*32,0);
  hierarchy.atomicRegionCount=1;
  for(int i=0;i<4;++i) {
    hierarchy.plates.emplace_back(bounds);
    auto &h=hierarchy.plates.back();
    h.yChunk.assign(32*32,0);h.abChunk=h.yChunk;
    h.yChunkCount=h.abChunkCount=1;
    h.yEdgeX.assign(32*32,0);h.yEdgeY=h.yEdgeX;
    h.abEdgeX=h.yEdgeX;h.abEdgeY=h.yEdgeX;
    auto value=plates.appearance(i);
    for(int y=0;y<32;++y) for(int x=0;x<32;++x) {
      float v=.5f+.01f*x+(((x+y)&1)?-.0002f:.0002f);
      image.view().y.at(x,y)=value.y.at(x,y)=v;
      value.a.at(x,y)=value.b.at(x,y)=0;
      plates.alpha(i).at(x,y)=.25f;
      plates.supportY(i).at(x,y)=plates.supportAB(i).at(x,y)=1;
    }
  }
  pigment::Phase4Params params;
  params.gradientComplexity=.35f;
  auto source=static_cast<const pigment::OwnedYabPlanes &>(image).view();
  auto result=pigment::synthesizePhase4Chunks(source,plates,hierarchy,params);
  auto reconstructed=static_cast<const pigment::OwnedYabPlanes &>(result.plateAppearance[0]).view();
  double originalResidual=0,retainedResidual=0;
  for(int y=8;y<24;++y) for(int x=8;x<24;++x) {
    double sign=((x+y)&1)?-1:1;
    originalResidual+=sign*(source.y.at(x,y)-(.5+.01*x));
    retainedResidual+=sign*(reconstructed.y.at(x,y)-(.5+.01*x));
  }
  check(std::abs(retainedResidual/originalResidual-.35)<.02,
        "accepted affine candidate retains Complexity fraction of fine gradients");
  check(result.primitiveSelection[0].view().at(16,16)>0 &&
        result.primitiveSelection[0].view().at(16,16)<.5f,
        "gradient-survival fixture actually selects an affine candidate");
  for(int i=0;i<4;++i) for(int y=0;y<32;++y) for(int x=0;x<32;++x) {
    float v=.5f+(((x+y)&1)?-.0002f:.0002f);
    image.view().y.at(x,y)=plates.appearance(i).y.at(x,y)=v;
  }
  auto flat=pigment::synthesizePhase4Chunks(source,plates,hierarchy,params);
  check(flat.primitiveSelection[0].view().at(16,16)==0,
        "a flat broad field can qualify as solid despite fine oscillation");
  auto flatResult=static_cast<const pigment::OwnedYabPlanes &>(flat.plateAppearance[0]).view();
  double flatResidual=0;
  for(int y=8;y<24;++y) for(int x=8;x<24;++x)
    flatResidual+=(((x+y)&1)?-1:1)*(flatResult.y.at(x,y)-.5f);
  check(std::abs(flatResidual/originalResidual-.35)<.02,
        "solid qualification changes analysis only, not approved gradient survival");
}

void interiorBroadForm() {
  pigment::RectI bounds{0,0,96,96};
  pigment::OwnedYabPlanes image(bounds);
  pigment::PublicPlateSet plates(bounds,4);
  pigment::Phase4RegionHierarchy hierarchy;
  hierarchy.bounds=bounds;
  hierarchy.atomicRegion.assign(96*96,0);
  hierarchy.atomicRegionCount=1;
  std::vector<float> form(96*96);
  constexpr double pi=3.141592653589793;
  for(int i=0;i<4;++i) {
    hierarchy.plates.emplace_back(bounds);
    auto &h=hierarchy.plates.back();
    h.yChunk.assign(96*96,0);h.abChunk=h.yChunk;
    h.yChunkCount=h.abChunkCount=1;
    h.yEdgeX.assign(96*96,0);h.yEdgeY=h.yEdgeX;
    h.abEdgeX=h.yEdgeX;h.abEdgeY=h.yEdgeX;
    auto app=plates.appearance(i);
    for(int y=0;y<96;++y) for(int x=0;x<96;++x) {
      double fx=x/95.0,fy=y/95.0;
      float broad=float(-.7+2.6*std::sin(pi*fx)*std::sin(pi*fy)+
                       .7*std::sin(2*pi*fx)*std::sin(pi*fy));
      form[size_t(y)*96+x]=broad;
      float value=broad+(((x+y)&1)?-.02f:.02f)+.015f*std::sin(float(2*pi*x/9));
      image.view().y.at(x,y)=app.y.at(x,y)=value;
      app.a.at(x,y)=app.b.at(x,y)=0;
      plates.alpha(i).at(x,y)=.25f;
      plates.supportY(i).at(x,y)=plates.supportAB(i).at(x,y)=1;
    }
  }
  pigment::Phase4Params params;
  params.gradientComplexity=.05f;
  auto source=static_cast<const pigment::OwnedYabPlanes &>(image).view();
  pigment::Phase4BroadFormOptions disabled;disabled.enabled=false;
  auto baseline=pigment::synthesizePhase4Chunks(source,plates,hierarchy,params,{},disabled);
  pigment::Phase4BroadFormOptions enabled;enabled.enabled=true;
  auto constrained=pigment::synthesizePhase4Chunks(source,plates,hierarchy,params,{},enabled);
  auto b=static_cast<const pigment::OwnedYabPlanes &>(baseline.plateAppearance[0]).view();
  auto c=static_cast<const pigment::OwnedYabPlanes &>(constrained.plateAppearance[0]).view();
  double oldError=0,newError=0,oldTexture=0,newTexture=0;
  for(int y=16;y<80;++y) for(int x=16;x<80;++x) {
    float reference=form[size_t(y)*96+x];
    oldError+=std::pow(b.y.at(x,y)-reference,2);
    newError+=std::pow(c.y.at(x,y)-reference,2);
    // Second differences distinguish known analytical broad form from texture.
    double exact=form[size_t(y)*96+x-1]-2*reference+form[size_t(y)*96+x+1];
    oldTexture+=std::pow(source.y.at(x-1,y)-2*source.y.at(x,y)+source.y.at(x+1,y)-exact,2);
    newTexture+=std::pow(c.y.at(x-1,y)-2*c.y.at(x,y)+c.y.at(x+1,y)-exact,2);
    check(c.a.at(x,y)==0 && c.b.at(x,y)==0,"interior constraints preserve neutral AB");
  }
  check(newError<.65*oldError,"regional interior constraints improve curved broad form at strong simplification");
  check(newTexture<.1*oldTexture,"broad constraints do not restore oscillatory source description");
  check(constrained.solver[0][0].broadConstraints>0 &&
        constrained.solver[0][0].converged,
        "interior moment constraints participate in converged bounded solve");
  for(int y=0;y<96;++y) for(int x=0;x<96;++x)
    if(x==0 || y==0 || x==95 || y==95)
      check(c.y.at(x,y)==source.y.at(x,y),"interior moments leave retained contour values bit-exact");
  auto onlyY=pigment::Phase4BroadFormOptions{};
  onlyY.enabled=true;
  onlyY.strengthY*=2;
  auto changed=pigment::synthesizePhase4Chunks(source,plates,hierarchy,params,{},onlyY);
  auto next=static_cast<const pigment::OwnedYabPlanes &>(changed.plateAppearance[0]).view();
  for(int y=0;y<96;++y) for(int x=0;x<96;++x)
    check(next.a.at(x,y)==c.a.at(x,y) && next.b.at(x,y)==c.b.at(x,y),
          "Y interior constraint strength does not alter AB reconstruction");
  auto repeat=pigment::synthesizePhase4Chunks(source,plates,hierarchy,params,{},enabled);
  auto repeated=static_cast<const pigment::OwnedYabPlanes &>(repeat.plateAppearance[0]).view();
  for(int y=0;y<96;++y) for(int x=0;x<96;++x)
    check(repeated.y.at(x,y)==c.y.at(x,y),"interior constraint sites and solve are deterministic");
  params.lumaChunkScale=0;params.chromaChunkScale=0;
  auto bypass=pigment::synthesizePhase4Chunks(source,plates,hierarchy,params,{},enabled);
  auto untouched=static_cast<const pigment::OwnedYabPlanes &>(bypass.plateAppearance[0]).view();
  for(int y=0;y<96;++y) for(int x=0;x<96;++x)
    check(untouched.y.at(x,y)==source.y.at(x,y),"zero chunk scale bypasses interior constraints exactly");
}

void renderIdentityAndAlpha() {
  pigment::RectI b{-2, 3, 18, 17};
  int stride = b.width() * 4 + 3;
  std::vector<float> src(size_t(stride) * b.height(), -5), dst(src.size(), -9);
  for (int y = b.y1; y < b.y2; ++y)
    for (int x = b.x1; x < b.x2; ++x) {
      size_t i = size_t(y - b.y1) * stride + (x - b.x1) * 4;
      float a = .2f + .8f * float((x + y + 20) % 9) / 8;
      src[i] = a * (.1f + .03f * x);
      src[i + 1] = a * (-.2f + .04f * y);
      src[i + 2] = a * ((x % 5) ? 0.3f : 2.0f);
      src[i + 3] = a;
    }
  pigment::IntegratedPigmentParams p;
  p.comparison = pigment::PigmentComparisonMode::AutomaticPlateGraph;
  p.premultiplied = true;
  p.phase4.latentCount = 12;
  p.phase4.plateCount = 4;
  p.phase4.plateScale = 8;
  p.amount = 0;
  pigment::Phase4RenderInputs in{
      {src.data(), stride, b, 4}, {dst.data(), stride, b, 4}, b, p, {}};
  pigment::processPigmentPhase4(in);
  for (int y = b.y1; y < b.y2; ++y)
    for (int x = b.x1; x < b.x2; ++x) {
      size_t i = size_t(y - b.y1) * stride + (x - b.x1) * 4;
      for (int c = 0; c < 4; ++c)
        check(dst[i + c] == src[i + c],
              "Amount zero is bit-exact and preserves alpha");
    }
}
} // namespace
int main() {
  parameterSemantics();
  automaticPlates();
  regionHierarchy();
  primitiveGradientSurvival();
  interiorBroadForm();
  renderIdentityAndAlpha();
  if (failures)
    return 1;
  std::cout << "All Phase 4 CPU tests passed (photographic gates are evaluated separately)\n";
}
