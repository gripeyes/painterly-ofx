#include "core/ChromaDiffusion.h"
#include "core/Similarity.h"
#include "core/SpatialOperator.h"

#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
int failures = 0;

void check(bool condition, const std::string& message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

void near(float actual, float expected, float tolerance, const std::string& message) {
  check(std::abs(actual - expected) <= tolerance,
        message + " (actual=" + std::to_string(actual) +
        ", expected=" + std::to_string(expected) + ")");
}

struct TestImage {
  pigment::RectI bounds;
  int components;
  int padding;
  std::vector<float> pixels;
  TestImage(pigment::RectI b, int c, int pad = 0)
      : bounds(b), components(c), padding(pad),
        pixels(static_cast<std::size_t>(b.height()) * (b.width() * c + pad), -99.0f) {}
  pigment::ImageView view() {
    return {pixels.data(), bounds.width() * components + padding, bounds, components};
  }
  pigment::ConstImageView view() const {
    return {pixels.data(), bounds.width() * components + padding, bounds, components};
  }
};

void testColorRoundTrip() {
  const std::array<pigment::WorkingGamut, 4> gamuts{
      pigment::WorkingGamut::ACEScg, pigment::WorkingGamut::LinearRec709,
      pigment::WorkingGamut::LinearRec2020, pigment::WorkingGamut::DisplayP3D65};
  const std::array<std::array<float,3>, 5> colors{{
      {0.18f,0.18f,0.18f}, {1.0f,0.0f,0.0f}, {-0.2f,0.5f,4.0f},
      {16.0f,2.0f,0.1f}, {0.0f,0.0f,0.0f}}};
  for (auto gamut : gamuts) {
    pigment::MatrixOpponentTransform transform(gamut);
    for (const auto& rgb : colors) {
      const auto yab = transform.toYab(rgb);
      const auto result = transform.toRgb(yab);
      for (int c = 0; c < 3; ++c) near(result[c], rgb[c], 3e-5f, "gamut round trip");
    }
    const auto neutral = transform.toYab({0.42f,0.42f,0.42f});
    near(neutral.a, 0.0f, 2e-7f, "neutral A");
    near(neutral.b, 0.0f, 2e-7f, "neutral B");
  }
}

void testSimilarity() {
  const pigment::YabPixel x{0.2f, 0.1f, -0.2f};
  const pigment::YabPixel y{0.4f, 0.2f, -0.1f};
  pigment::SimilarityWeights weights;
  near(pigment::yabDistanceSquared(x,y,weights),
       pigment::yabDistanceSquared(y,x,weights), 1e-7f, "similarity symmetry");
  weights.luminance = 0.0f;
  const float chromaOnly = pigment::yabDistanceSquared(x,y,weights);
  weights.axisA = weights.axisB = 0.0f;
  near(pigment::yabDistanceSquared(x,y,weights), 0.0f, 1e-7f, "disabled axes");
  check(chromaOnly > 0.0f, "chroma axes contribute");
  weights = {};
  const float unitScale = pigment::yabDistanceSquared(x,y,weights);
  weights.luminanceScale = weights.chromaScale = 2.0f;
  near(pigment::yabDistanceSquared(x,y,weights), unitScale * 0.25f, 1e-7f,
       "similarity scales distance quadratically");
  check(pigment::gaussianSimilarity(x,x,weights) == 1.0f,
        "identical colors have unit similarity");
}

void fillPattern(TestImage& image) {
  auto v = image.view();
  for (int y = v.bounds.y1; y < v.bounds.y2; ++y) {
    for (int x = v.bounds.x1; x < v.bounds.x2; ++x) {
      float* p = v.pixel(x,y);
      p[0] = 0.1f * (x - v.bounds.x1) - 0.2f;
      p[1] = 0.07f * (y - v.bounds.y1) + 0.1f;
      p[2] = (x == y) ? 4.0f : 0.3f;
      if (v.components == 4) p[3] = 0.25f + 0.05f * (x - v.bounds.x1);
    }
  }
}

void testIdentityAndAlpha() {
  TestImage source({-2,3,7,10}, 4, 3), destination({-2,3,7,10}, 4, 5);
  fillPattern(source);
  pigment::ChromaDiffusionParams params;
  params.amount = 0.0f;
  pigment::processChromaDiffusion(static_cast<const TestImage&>(source).view(), destination.view(), source.bounds,
                                  params, {}, nullptr);
  for (int y = source.bounds.y1; y < source.bounds.y2; ++y)
    check(std::memcmp(source.view().pixel(source.bounds.x1,y),
                      destination.view().pixel(source.bounds.x1,y),
                      source.bounds.width()*4*sizeof(float)) == 0,
          "amount zero is bit-exact with padded rows");

  params.amount = 1.0f;
  params.diffusion.radius = 1.5f;
  params.luminancePreservation = 1.0f;
  pigment::processChromaDiffusion(static_cast<const TestImage&>(source).view(), destination.view(), source.bounds,
                                  params, {}, nullptr);
  for (int y = source.bounds.y1; y < source.bounds.y2; ++y)
    for (int x = source.bounds.x1; x < source.bounds.x2; ++x)
      check(source.view().pixel(x,y)[3] == destination.view().pixel(x,y)[3],
            "alpha is exact");

  params.mix = 0.0f;
  std::fill(destination.pixels.begin(), destination.pixels.end(), -99.0f);
  pigment::processChromaDiffusion(static_cast<const TestImage&>(source).view(), destination.view(), source.bounds,
                                  params, {}, nullptr);
  for (int y = source.bounds.y1; y < source.bounds.y2; ++y)
    check(std::memcmp(source.view().pixel(source.bounds.x1,y),
                      destination.view().pixel(source.bounds.x1,y),
                      source.bounds.width()*4*sizeof(float)) == 0,
          "mix zero is bit-exact with padded rows");
}

void testNeutralAndLuminance() {
  TestImage source({0,0,9,9}, 3), destination({0,0,9,9}, 3);
  auto sv = source.view();
  for (int y=0; y<9; ++y) for (int x=0; x<9; ++x) {
    const float g = 0.05f + 0.1f*x;
    float* p=sv.pixel(x,y); p[0]=p[1]=p[2]=g;
  }
  pigment::ChromaDiffusionParams params;
  params.amount=1.0f; params.diffusion.radius=2.0f; params.diffusion.edgeProtection=0.0f;
  pigment::processChromaDiffusion(static_cast<const TestImage&>(source).view(),destination.view(),source.bounds,params,{},nullptr);
  for (int y=0; y<9; ++y) for (int x=0; x<9; ++x) {
    const float* p=destination.view().pixel(x,y);
    near(p[0],p[1],2e-6f,"neutral remains neutral");
    near(p[1],p[2],2e-6f,"neutral remains neutral");
    near(p[0],source.view().pixel(x,y)[0],2e-5f,"preserved neutral luminance");
  }
}

void testPremultiplication() {
  TestImage straight({0,0,11,7},4), premult({0,0,11,7},4);
  for (int y=0;y<7;++y) for (int x=0;x<11;++x) {
    const float alpha=0.15f+0.07f*(x%5);
    const std::array<float,3> rgb{0.1f*x,0.2f+0.05f*y,(x<5)?2.0f:-0.25f};
    auto* s=straight.view().pixel(x,y); auto* m=premult.view().pixel(x,y);
    for(int c=0;c<3;++c){s[c]=rgb[c];m[c]=rgb[c]*alpha;} s[3]=m[3]=alpha;
  }
  TestImage outStraight(straight.bounds,4), outPremult(straight.bounds,4);
  pigment::ChromaDiffusionParams p; p.amount=1.0f; p.diffusion.radius=1.25f;
  pigment::processChromaDiffusion(static_cast<const TestImage&>(straight).view(),outStraight.view(),straight.bounds,p,{},nullptr);
  p.premultiplied=true;
  pigment::processChromaDiffusion(static_cast<const TestImage&>(premult).view(),outPremult.view(),premult.bounds,p,{},nullptr);
  for (int y=0;y<7;++y) for (int x=0;x<11;++x) {
    const float a=outStraight.view().pixel(x,y)[3];
    for(int c=0;c<3;++c)
      near(outPremult.view().pixel(x,y)[c]/a,outStraight.view().pixel(x,y)[c],4e-5f,
           "premultiplied and straight processing agree");
    check(outPremult.view().pixel(x,y)[3]==premult.view().pixel(x,y)[3],"premult alpha exact");
  }
}

void testTilingAndThreadDeterminism() {
  TestImage source({0,0,31,19},4), full(source.bounds,4), threaded(source.bounds,4), tiled(source.bounds,4);
  fillPattern(source);
  pigment::ChromaDiffusionParams p; p.amount=1.0f; p.diffusion.radius=1.0f;
  p.diffusion.edgeProtection=0.2f; p.diffusion.angleDegrees=31.0f;
  const auto src=static_cast<const TestImage&>(source).view();
  pigment::processChromaDiffusion(src,full.view(),source.bounds,p,{},nullptr);

  pigment::ExecutionContext execution;
  execution.parallelRows=[](int begin,int end,const pigment::RowFunction& fn){
    const int middle=begin+(end-begin)/2;
    std::thread first([&]{fn(begin,middle);});
    std::thread second([&]{fn(middle,end);});
    first.join(); second.join();
  };
  pigment::processChromaDiffusion(src,threaded.view(),source.bounds,p,{},nullptr,execution);
  for(int y=0;y<19;++y)
    check(std::memcmp(full.view().pixel(0,y),threaded.view().pixel(0,y),31*4*sizeof(float))==0,
          "thread scheduling is bit deterministic");

  const auto halo=pigment::chromaDiffusionInputDomain(p,{});
  for (pigment::RectI tile : {pigment::RectI{0,0,15,19},pigment::RectI{15,0,31,19}}) {
    const auto crop=pigment::intersect(pigment::expand(tile,halo.haloX,halo.haloY),source.bounds);
    pigment::ConstImageView cropped{src.pixel(crop.x1,crop.y1),src.rowStride,crop,4};
    pigment::processChromaDiffusion(cropped,tiled.view(),tile,p,{},nullptr);
  }
  for(int y=0;y<19;++y) for(int x=0;x<31;++x) for(int c=0;c<4;++c)
    near(tiled.view().pixel(x,y)[c],full.view().pixel(x,y)[c],3e-5f,"tiled render matches full render");
}

void testControlFieldAndBoundary() {
  pigment::RectI b{0,0,5,3};
  pigment::OwnedYabPlanes planes(b);
  auto p=planes.view();
  for(int y=0;y<3;++y) for(int x=0;x<5;++x) {
    p.y.at(x,y)=x<2?0.1f:1.0f; p.a.at(x,y)=p.b.at(x,y)=0.0f;
  }
  pigment::OwnedPlane boundary(b), control(b), mask(b,0.25f);
  pigment::buildBoundaryField(pigment::asConst(p),boundary.view(),{1.0f,0.1f},{});
  check(boundary.view().at(1,1)<boundary.view().at(0,1),"boundary detects transition");
  const auto pc = pigment::asConst(p);
  const auto bc = static_cast<const pigment::OwnedPlane&>(boundary).view();
  const auto mc = static_cast<const pigment::OwnedPlane&>(mask).view();
  pigment::composeControlField(pc.y,bc,&mc,false,{},control.view(),{});
  near(control.view().at(0,1),0.25f*boundary.view().at(0,1),1e-6f,"external mask composes");
  pigment::composeProcessingStrengthField(pc.y,&mc,false,{},control.view(),{});
  near(control.view().at(0,1),0.25f,1e-6f,
       "processing strength composes independently from boundary protection");
  pigment::composeControlField(pc.y,bc,&mc,true,{},control.view(),{});
  near(control.view().at(0,1),0.75f*boundary.view().at(0,1),1e-6f,"mask inversion composes");

  pigment::TonalMaskOptions range;
  range.rangeEnabled=true; range.rangeMinimum=0.25f; range.rangeMaximum=0.75f;
  range.rangeSoftness=0.05f;
  check(pigment::tonalWeight(0.5f,range)>0.99f,"range mask includes interior");
  check(pigment::tonalWeight(0.0f,range)<1e-6f,"range mask excludes exterior");
}

void testDirectionalResponseAndCancellation() {
  const pigment::RectI b{0,0,17,17};
  pigment::OwnedYabPlanes source(b), destination(b);
  pigment::OwnedPlane boundary(b,1.0f), control(b,1.0f);
  auto src=source.view();
  for(int y=0;y<17;++y) for(int x=0;x<17;++x) src.y.at(x,y)=0.5f;
  src.a.at(8,8)=1.0f;
  pigment::DirectionalGaussianOptions options;
  options.radius=2.0f; options.xScale=2.0f; options.yScale=0.25f;
  options.edgeProtection=0.0f;
  pigment::DirectionalGaussianOperator op(options);
  auto dc=static_cast<const pigment::OwnedPlane&>(control).view();
  auto db=static_cast<const pigment::OwnedPlane&>(boundary).view();
  op.apply({static_cast<const pigment::OwnedYabPlanes&>(source).view(),destination.view(),dc,db,b,{}},{});
  const auto out=static_cast<const pigment::OwnedYabPlanes&>(destination).view();
  check(out.a.at(10,8)>out.a.at(8,10),"directional impulse spreads farther on X axis");

  TestImage input({0,0,8,8},3), cancelled({0,0,8,8},3);
  fillPattern(input);
  pigment::ChromaDiffusionParams p; p.amount=1.0f;
  int checks=0;
  pigment::ExecutionContext execution;
  execution.cancelled=[&]{++checks; return true;};
  pigment::processChromaDiffusion(static_cast<const TestImage&>(input).view(),cancelled.view(),input.bounds,p,{},nullptr,execution);
  check(checks>0,"cancellation is polled by processing kernels");
}

class NonConvolutionOperator final : public pigment::SpatialOperator {
 public:
  pigment::InputDomainRequest requiredInputDomain(const pigment::ImageGeometry&) const noexcept override {
    return {pigment::InputDomainKind::FullRegionOfDefinition,0,0};
  }
  void apply(const pigment::SpatialOperation&, const pigment::ExecutionContext&) const override {}
};

void testOperatorContract() {
  pigment::DirectionalGaussianOptions o; o.radius=3.0f; o.xScale=2.0f; o.yScale=1.0f;
  pigment::DirectionalGaussianOperator gaussian(o);
  const auto halo=gaussian.requiredInputDomain({1.0,1.0,1.0});
  check(halo.kind==pigment::InputDomainKind::LocalHalo && halo.haloX>0,"Gaussian reports halo");
  NonConvolutionOperator region;
  check(region.requiredInputDomain({}).kind==pigment::InputDomainKind::FullRegionOfDefinition,
        "operator contract permits non-convolution domain");
}
}  // namespace

int main() {
  try {
    testColorRoundTrip(); testSimilarity(); testIdentityAndAlpha();
    testNeutralAndLuminance(); testPremultiplication(); testTilingAndThreadDeterminism();
    testControlFieldAndBoundary(); testDirectionalResponseAndCancellation();
    testOperatorContract();
  } catch (const std::exception& e) {
    std::cerr << "UNCAUGHT: " << e.what() << '\n'; return 2;
  }
  if (failures) { std::cerr << failures << " test(s) failed\n"; return 1; }
  std::cout << "All Pigment core tests passed\n";
  return 0;
}
