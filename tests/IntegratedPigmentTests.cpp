#include "core/IntegratedPigment.h"

#include <cmath>
#include <iostream>

namespace {

int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

void testVeilAndFields() {
  pigment::IntegratedPigmentParams p;
  const float a = pigment::painterlyVeilValue(120.0f, -45.0f, 0.4f, p);
  const float b = pigment::painterlyVeilValue(120.0f, -45.0f, 0.4f, p);
  const float c = pigment::painterlyVeilValue(121.0f, -45.0f, 0.4f, p);
  check(a == b, "Veil is deterministic");
  check(std::abs(a - c) < 0.03f, "Veil is continuous at neighboring pixels");
  p.veilSeed = 9;
  const float seeded = pigment::painterlyVeilValue(120.0f, -45.0f, 0.4f, p);
  check(std::abs(a - seeded) > 1.0e-5f, "Veil seed changes the field");

  p.veilAmount = 1.0f;
  p.boundaryExtinction = 0.8f;
  p.chromaMigration = 0.7f;
  const auto low = pigment::derivePigmentFields(0.15f, 1.0f, p);
  const auto high = pigment::derivePigmentFields(0.85f, 1.0f, p);
  check(high.massStrength != low.massStrength, "Veil modulates mass strength");
  check(high.boundaryExtinction != low.boundaryExtinction,
        "Boundary extinction has an independent response");
  check(std::abs(high.chromaMigration - low.chromaMigration) <
            std::abs(high.boundaryExtinction - low.boundaryExtinction),
        "Chroma response is not an alias of boundary response");
  check(high.detailRetention < low.detailRetention,
        "Detail retention uses its own inverse response");
}

void testPhysicalMassScaleSupport() {
  const pigment::RectI bounds{0, 0, 41, 9};
  pigment::OwnedYabPlanes source(bounds), smallResult(bounds), largeResult(bounds);
  auto s = source.view();
  for (int y = 0; y < 9; ++y) {
    for (int x = 0; x < 41; ++x) {
      s.y.at(x, y) = x < 20 ? 0.2f : 0.8f;
      s.a.at(x, y) = x < 20 ? -0.05f : 0.08f;
      s.b.at(x, y) = 0.02f;
    }
  }
  pigment::IntegratedPigmentParams p;
  p.massStrength = 1.0f;
  p.toneSimilarity = 20.0f;
  p.chromaSimilarity = 20.0f;
  p.lumaAttraction = 1.0f;
  p.chromaAttraction = 1.0f;
  p.boundaryPreserve = 0.0f;
  p.massScale = 2.0f;
  pigment::softRegionMassReference(pigment::asConst(s), smallResult.view(), 1.0f, 0.0f, p);
  p.massScale = 14.0f;
  pigment::softRegionMassReference(pigment::asConst(s), largeResult.view(), 1.0f, 0.0f, p);
  const float smallChange = std::abs(smallResult.view().y.at(14, 4) - 0.2f);
  const float largeChange = std::abs(largeResult.view().y.at(14, 4) - 0.2f);
  check(largeChange > smallChange + 0.01f,
        "Mass Scale expands physical support beyond a fixed 9x9-pixel footprint");
  check(std::isfinite(largeResult.view().a.at(14, 4)),
        "Soft region reference remains finite");
}

void testRepresentativePopulationSelection() {
  const pigment::RectI bounds{0, 0, 25, 9};
  pigment::OwnedYabPlanes source(bounds), legacy(bounds), representative(bounds), softRepresentative(bounds);
  auto s = source.view();
  for (int y = 0; y < bounds.y2; ++y) for (int x = 0; x < bounds.x2; ++x) {
    const bool dominantGreen = x < 17;
    s.y.at(x, y) = dominantGreen ? 0.55f : 0.31f;
    s.a.at(x, y) = dominantGreen ? -0.12f : 0.16f;
    s.b.at(x, y) = dominantGreen ? 0.10f : -0.08f;
  }
  pigment::IntegratedPigmentParams p;
  p.massScale = 12.0f; p.massStrength = 1.0f;
  p.toneSimilarity = 12.0f; p.chromaSimilarity = 2.0f;
  p.lumaAttraction = 1.0f; p.chromaAttraction = 1.0f;
  p.boundaryPreserve = 0.0f; p.modeSelectivity = 1.0f;
  pigment::softRegionMassReference(pigment::asConst(s), legacy.view(), 1.0f, 0.0f, p);
  pigment::representativeRegionMassReference(
      pigment::asConst(s), representative.view(), 1.0f, 0.0f, p);
  p.modeSelectivity = 0.0f;
  pigment::representativeRegionMassReference(
      pigment::asConst(s), softRepresentative.view(), 1.0f, 0.0f, p);
  const int x = 12, y = 4;
  const auto distanceToGreen = [&](pigment::YabPlanes view) {
    const float dy = view.y.at(x, y) - 0.55f;
    const float da = view.a.at(x, y) + 0.12f;
    const float db = view.b.at(x, y) - 0.10f;
    return std::sqrt(dy * dy + da * da + db * db);
  };
  check(distanceToGreen(representative.view()) < distanceToGreen(legacy.view()),
        "Representative mode stays closer to the dominant source population than the mean");
  check(distanceToGreen(representative.view()) < distanceToGreen(softRepresentative.view()),
        "Mode Selectivity continuously strengthens dominant-population attraction");
  check(std::isfinite(representative.view().y.at(x, y)),
        "Representative mode remains deterministic and finite");
}

}  // namespace

int main() {
  testVeilAndFields();
  testPhysicalMassScaleSupport();
  testRepresentativePopulationSelection();
  if (failures) {
    std::cerr << failures << " integrated Pigment test(s) failed\n";
    return 1;
  }
  std::cout << "All integrated Pigment tests passed\n";
  return 0;
}
