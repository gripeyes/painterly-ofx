#include "core/DetailCollapseResearch.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

void near(float actual, float expected, float tolerance,
          const std::string& message) {
  check(std::abs(actual - expected) <= tolerance, message);
}

struct TestImage {
  pigment::RectI bounds;
  int components;
  int padding;
  std::vector<float> pixels;

  TestImage(pigment::RectI b, int c, int p = 0)
      : bounds(b), components(c), padding(p),
        pixels(static_cast<std::size_t>(b.height()) *
               static_cast<std::size_t>(b.width() * c + p), -91.0f) {}

  pigment::ImageView view() {
    return {pixels.data(), bounds.width() * components + padding, bounds, components};
  }
  pigment::ConstImageView view() const {
    return {pixels.data(), bounds.width() * components + padding, bounds, components};
  }
};

void fillImage(TestImage& image, bool premultiplied) {
  auto view = image.view();
  for (int y = view.bounds.y1; y < view.bounds.y2; ++y) {
    for (int x = view.bounds.x1; x < view.bounds.x2; ++x) {
      const float alpha = image.components == 4
          ? 0.2f + 0.1f * static_cast<float>((x - view.bounds.x1) % 5)
          : 1.0f;
      std::array<float, 3> rgb{
          x < (view.bounds.x1 + view.bounds.x2) / 2 ? -0.35f : 3.5f,
          0.12f * static_cast<float>(y - view.bounds.y1),
          ((x + y) % 5 == 0) ? 5.0f : 0.18f};
      float* pixel = view.pixel(x, y);
      for (int channel = 0; channel < 3; ++channel)
        pixel[channel] = premultiplied ? rgb[channel] * alpha : rgb[channel];
      if (image.components == 4) pixel[3] = alpha;
    }
  }
}

pigment::DetailCollapseResearchParams activeParams() {
  pigment::DetailCollapseResearchParams params;
  params.amount = 0.8f;
  params.massScale = 1.5f;
  params.structureScale = 2.0f;
  params.massStrength = 0.65f;
  params.toneSimilarity = 4.0f;
  params.chromaSimilarity = 2.0f;
  params.boundaryPreserve = 0.8f;
  params.internalVariation = 0.2f;
  return params;
}

void testIdentityAndDomain() {
  TestImage source({-3, 4, 14, 15}, 4, 3), output(source.bounds, 4, 5);
  fillImage(source, false);
  pigment::DetailCollapseResearchParams params;
  pigment::processDetailCollapseResearch(
      static_cast<const TestImage&>(source).view(), output.view(), source.bounds,
      params, {}, nullptr);
  for (int y = source.bounds.y1; y < source.bounds.y2; ++y)
    check(std::memcmp(source.view().pixel(source.bounds.x1, y),
                      output.view().pixel(source.bounds.x1, y),
                      static_cast<std::size_t>(source.bounds.width() * 4) * sizeof(float)) == 0,
          "Amount zero Final is bit-exact with unusual origin and stride");

  params.amount = 1.0f;
  params.mix = 0.0f;
  pigment::processDetailCollapseResearch(
      static_cast<const TestImage&>(source).view(), output.view(), source.bounds,
      params, {}, nullptr);
  for (int y = source.bounds.y1; y < source.bounds.y2; ++y)
    check(std::memcmp(source.view().pixel(source.bounds.x1, y),
                      output.view().pixel(source.bounds.x1, y),
                      static_cast<std::size_t>(source.bounds.width() * 4) * sizeof(float)) == 0,
          "Mix zero Final is bit-exact");
  check(pigment::detailCollapseResearchInputDomain().kind ==
            pigment::InputDomainKind::FullRegionOfDefinition,
        "DetailCollapse research reports full-RoD input");
}

void testAllDebugViewsAndAlpha() {
  TestImage source({0, 0, 17, 11}, 4), output(source.bounds, 4);
  fillImage(source, false);
  auto params = activeParams();
  for (int mode = 0; mode <= 14; ++mode) {
    params.debugView = static_cast<pigment::DetailCollapseDebugView>(mode);
    std::fill(output.pixels.begin(), output.pixels.end(), -91.0f);
    pigment::processDetailCollapseResearch(
        static_cast<const TestImage&>(source).view(), output.view(), source.bounds,
        params, {}, nullptr);
    for (int y = source.bounds.y1; y < source.bounds.y2; ++y) {
      for (int x = source.bounds.x1; x < source.bounds.x2; ++x) {
        const float* result = output.view().pixel(x, y);
        for (int channel = 0; channel < 3; ++channel)
          check(std::isfinite(result[channel]), "every debug view produces finite RGB");
        check(result[3] == source.view().pixel(x, y)[3],
              "every debug view preserves alpha exactly");
        if (params.debugView == pigment::DetailCollapseDebugView::StructureGuide ||
            params.debugView == pigment::DetailCollapseDebugView::ProcessingStrength ||
            params.debugView == pigment::DetailCollapseDebugView::BoundaryProtection) {
          near(result[0], result[1], 0.0f, "scalar debug view is grayscale");
          near(result[1], result[2], 0.0f, "scalar debug view is grayscale");
          check(result[0] >= 0.0f && result[0] <= 1.0f,
                "scalar debug view is normalized");
        }
      }
    }
  }
}

void testPremultipliedEquivalenceAndMask() {
  const pigment::RectI bounds{0, 0, 15, 9};
  TestImage straight(bounds, 4), premult(bounds, 4), outStraight(bounds, 4), outPremult(bounds, 4);
  fillImage(straight, false);
  fillImage(premult, true);
  pigment::OwnedPlane mask(bounds, 1.0f);
  for (int y = bounds.y1; y < bounds.y2; ++y)
    for (int x = bounds.x1; x < bounds.x1 + 3; ++x) mask.view().at(x, y) = 0.0f;
  const auto maskView = static_cast<const pigment::OwnedPlane&>(mask).view();

  auto params = activeParams();
  pigment::processDetailCollapseResearch(
      static_cast<const TestImage&>(straight).view(), outStraight.view(), bounds,
      params, {}, &maskView);
  params.premultiplied = true;
  pigment::processDetailCollapseResearch(
      static_cast<const TestImage&>(premult).view(), outPremult.view(), bounds,
      params, {}, &maskView);

  for (int y = bounds.y1; y < bounds.y2; ++y) {
    for (int x = bounds.x1; x < bounds.x2; ++x) {
      const float alpha = outPremult.view().pixel(x, y)[3];
      for (int channel = 0; channel < 3; ++channel)
        near(outPremult.view().pixel(x, y)[channel] / alpha,
             outStraight.view().pixel(x, y)[channel], 8e-5f,
             "premultiplied and straight DetailCollapse agree");
      if (x < bounds.x1 + 3)
        for (int channel = 0; channel < 3; ++channel)
          near(outStraight.view().pixel(x, y)[channel],
               straight.view().pixel(x, y)[channel], 1e-6f,
               "external mask zero preserves original pixel");
    }
  }
}

}  // namespace

int main() {
  testIdentityAndDomain();
  testAllDebugViewsAndAlpha();
  testPremultipliedEquivalenceAndMask();
  if (failures) {
    std::cerr << failures << " DetailCollapse test(s) failed\n";
    return 1;
  }
  std::cout << "All DetailCollapse research tests passed\n";
  return 0;
}
