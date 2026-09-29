#include "core/PictorialPlanes.h"

#include <cmath>
#include <iostream>

namespace {
int failures = 0;
void check(bool value, const char* message) {
  if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}

void membershipTests() {
  const std::array<bool, 4> enabled{true, true, true, true};
  const std::array<float, 4> amount{1, 1, 1, 1};
  const auto half = pigment::normalizePlaneMembership({0.5f, 0.5f, 0, 0}, enabled, amount);
  check(std::abs(half.plane[0] - 0.5f) < 1e-6f &&
        std::abs(half.plane[1] - 0.5f) < 1e-6f && half.base == 0.0f,
        "50/50 overlap has no base contribution");
  const auto partial = pigment::normalizePlaneMembership({0.25f, 0, 0, 0}, enabled, amount);
  check(std::abs(partial.plane[0] - 0.25f) < 1e-6f &&
        std::abs(partial.base - 0.75f) < 1e-6f, "partial coverage retains base");
  const auto saturated = pigment::normalizePlaneMembership({2, 1, 1, -4}, enabled, amount);
  float sum = saturated.base;
  for (float value : saturated.plane) sum += value;
  check(std::abs(sum - 1.0f) < 1e-6f && saturated.base == 0.0f,
        "overlap saturates and normalizes");
}

void fitTests() {
  const pigment::RectI bounds{0, 0, 48, 36};
  pigment::OwnedYabPlanes source(bounds);
  pigment::OwnedPlane membership(bounds, 1.0f);
  auto s = source.view();
  for (int y = 0; y < bounds.y2; ++y) for (int x = 0; x < bounds.x2; ++x) {
    const float u = (x - 23.5f) / 14.0f, v = (y - 17.5f) / 10.0f;
    s.y.at(x, y) = 0.4f + 0.12f * u - 0.08f * v + 0.07f * u * u + 0.03f * u * v;
    s.a.at(x, y) = -0.1f + 0.04f * v;
    s.b.at(x, y) = 0.03f - 0.02f * u;
  }
  s.y.at(4, 4) = 50.0f;  // Robust fitting must not follow one HDR outlier.
  const auto model = pigment::fitQuadraticPlane(
      pigment::asConst(s), static_cast<const pigment::OwnedPlane&>(membership).view());
  const auto center = pigment::evaluateQuadraticPlane(model, 24, 18);
  check(model.valid && model.order == 2, "quadratic plane model is selected");
  check(std::abs(center.y - 0.4f) < 0.03f, "robust fit rejects isolated HDR event");
  check(std::abs(center.a + 0.1f) < 0.02f, "opponent A fit is stable");
}

void transitionTests() {
  check(std::abs(pigment::screenedTransitionLambda(3.2188758f) - 1.0f) < 1e-5f,
        "10-90 transition width uses screened-Poisson calibration");

  const pigment::RectI bounds{0, 0, 192, 3};
  pigment::OwnedPlaneMemberships source(bounds), result(bounds);
  auto sourceView = source.view();
  for (int y = bounds.y1; y < bounds.y2; ++y) {
    for (int x = bounds.x1; x < bounds.x2; ++x) {
      const bool left = x < 96;
      sourceView.plane[0].at(x, y) = left ? 1.0f : 0.0f;
      for (int plane = 1; plane < 4; ++plane) sourceView.plane[plane].at(x, y) = 0.0f;
      sourceView.base.at(x, y) = left ? 0.0f : 1.0f;
    }
  }
  auto measuredTransition = [&](float parameterWidth, pigment::ImageGeometry geometry) {
    pigment::propagatePlaneMemberships(
        static_cast<const pigment::OwnedPlaneMemberships&>(source).view(), 0.0f,
        parameterWidth, 0.0f, geometry, result.view());
    const auto output = static_cast<const pigment::OwnedPlaneMemberships&>(result).view();
    auto crossing = [&](float threshold) {
    for (int x = 1; x < bounds.x2; ++x) {
      const float before = output.plane[0].at(x - 1, 1);
      const float after = output.plane[0].at(x, 1);
      if (before >= threshold && after <= threshold) {
        const float t = (before - threshold) / std::max(before - after, 1.0e-8f);
        return static_cast<float>(x - 1) + t;
      }
    }
    return -1000.0f;
    };
    return crossing(0.1f) - crossing(0.9f);
  };
  check(std::abs(measuredTransition(12.0f, {}) - 12.0f) <= 0.6f,
        "canonical transition width measures within five percent");
  check(std::abs(measuredTransition(24.0f, {1.0, 0.5, 0.5}) - 12.0f) <= 0.6f,
        "transition calibration follows proxy render scale");
  check(std::abs(measuredTransition(24.0f, {2.0, 1.0, 1.0}) - 12.0f) <= 0.6f,
        "transition calibration follows pixel aspect ratio");

  check(pigment::residualSurvival(1, 1, 1, 1, 1) == 1.0f,
        "detail structure preserve is independent and can retain a boundary");
  check(pigment::residualSurvival(1, 1, 1, 1, 0) == 0.0f,
        "detail extinction remains independent from transition permeability");
}
}

int main() {
  membershipTests(); fitTests(); transitionTests();
  if (failures) return 1;
  std::cout << "All pictorial-plane tests passed\n";
  return 0;
}
