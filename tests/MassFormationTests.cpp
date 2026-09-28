#include "core/Masking.h"
#include "core/Filtering.h"
#include "core/RollingYabMass.h"
#include "core/ScratchArena.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

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

pigment::detail::RollingYabMassOptions referenceOptions() {
  pigment::detail::RollingYabMassOptions options;
  options.massScale = 2.5f;
  options.massStrength = 0.8f;
  options.internalVariation = 0.1f;
  options.similarity.luminanceScale = 10.0f;
  options.similarity.chromaScale = 10.0f;
  return options;
}

void fillFields(pigment::OwnedYabPlanes& planes) {
  auto p = planes.view();
  const auto bounds = p.y.bounds;
  for (int y = bounds.y1; y < bounds.y2; ++y) {
    for (int x = bounds.x1; x < bounds.x2; ++x) {
      p.y.at(x, y) = x < (bounds.x1 + bounds.x2) / 2 ? -0.5f : 3.0f;
      p.a.at(x, y) = ((x + y) & 1) ? -1.25f : 1.75f;
      p.b.at(x, y) = 0.2f * static_cast<float>(y - bounds.y1);
    }
  }
}

void testStructureScaleRejectsSmallStrongEdges() {
  const pigment::RectI bounds{0, 0, 65, 33};
  pigment::OwnedYabPlanes source(bounds);
  auto src = source.view();
  for (int y = 0; y < 33; ++y) {
    for (int x = 0; x < 65; ++x) {
      src.y.at(x, y) = x < 33 ? 0.2f : 0.8f;
      src.a.at(x, y) = src.b.at(x, y) = 0.0f;
    }
  }
  src.y.at(12, 16) = 8.0f;  // tiny but much higher contrast than the silhouette

  pigment::OwnedPlane fine(bounds), broad(bounds);
  pigment::StructureBoundaryOptions options;
  options.protection = 1.0f;
  options.softness = 0.08f;
  options.structureScale = 0.25f;
  pigment::buildStructureBoundaryField(pigment::asConst(src), fine.view(), options);
  options.structureScale = 4.0f;
  pigment::buildStructureBoundaryField(pigment::asConst(src), broad.view(), options);

  const float fineSpike = fine.view().at(11, 16);
  const float broadSpike = broad.view().at(11, 16);
  const float broadSilhouette = broad.view().at(32, 16);
  check(fineSpike < 0.25f, "raw-scale guide detects the small strong edge");
  check(broadSpike > fineSpike + 0.35f,
        "Structure Scale extinguishes protection around small strong detail");
  check(broadSilhouette < broadSpike,
        "large silhouette remains more protected than isolated detail");
}

void testReusableFilteringPrimitives() {
  const pigment::RectI bounds{-2, 3, 7, 10};
  pigment::OwnedPlane source(bounds, 2.5f), box(bounds), gaussian(bounds);
  const auto src = static_cast<const pigment::OwnedPlane&>(source).view();
  pigment::boxBlurPlane(src, box.view(), 3, 2);
  pigment::gaussianBlurPlane(src, gaussian.view(), 2.0f, 1.0f);
  for (int y = bounds.y1; y < bounds.y2; ++y)
    for (int x = bounds.x1; x < bounds.x2; ++x) {
      near(box.view().at(x, y), 2.5f, 1e-6f,
           "box statistics preserve a constant field at borders");
      near(gaussian.view().at(x, y), 2.5f, 1e-6f,
           "Gaussian scale seed preserves a constant field at borders");
    }
}

void testIndependentStrengthAndBoundaryFields() {
  const pigment::RectI bounds{-4, 2, 17, 9};
  pigment::OwnedYabPlanes source(bounds), openResult(bounds), protectedResult(bounds);
  fillFields(source);
  pigment::OwnedPlane strength(bounds, 1.0f), open(bounds, 1.0f), protectedField(bounds, 1.0f);
  const int split = (bounds.x1 + bounds.x2) / 2;
  for (int y = bounds.y1; y < bounds.y2; ++y) {
    for (int x = bounds.x1; x < split; ++x) strength.view().at(x, y) = 0.0f;
    protectedField.view().at(split - 1, y) = 0.0f;
    protectedField.view().at(split, y) = 0.0f;
  }

  pigment::detail::RollingYabMassOperator op(referenceOptions());
  const auto src = static_cast<const pigment::OwnedYabPlanes&>(source).view();
  const auto strengthView = static_cast<const pigment::OwnedPlane&>(strength).view();
  const auto openView = static_cast<const pigment::OwnedPlane&>(open).view();
  const auto protectedView = static_cast<const pigment::OwnedPlane&>(protectedField).view();
  op.apply({src, openResult.view(), strengthView, openView, bounds, {}}, {});
  op.apply({src, protectedResult.view(), strengthView, protectedView, bounds, {}}, {});
  const auto openOut = static_cast<const pigment::OwnedYabPlanes&>(openResult).view();
  const auto protectedOut = static_cast<const pigment::OwnedYabPlanes&>(protectedResult).view();

  for (int y = bounds.y1; y < bounds.y2; ++y)
    for (int x = bounds.x1; x < split; ++x) {
      near(openOut.y.at(x, y), src.y.at(x, y), 0.0f,
           "zero strength preserves Y exactly");
      near(openOut.a.at(x, y), src.a.at(x, y), 0.0f,
           "zero strength preserves chroma exactly");
    }
  check(std::abs(openOut.y.at(split, 5) - protectedOut.y.at(split, 5)) > 0.05f,
        "boundary field changes permeability without changing strength");

  pigment::OwnedYabPlanes constantResult(bounds);
  op.apply({src, constantResult.view(), pigment::ScalarFieldView(1.0f),
            pigment::ScalarFieldView(1.0f), bounds, {}}, {});
  check(std::isfinite(constantResult.view().y.at(split, 5)),
        "constant scalar fields require no allocated plane");
}

void testMassScaleIsIndependentAndUnclipped() {
  const pigment::RectI bounds{0, 0, 23, 11};
  pigment::OwnedYabPlanes source(bounds), small(bounds), large(bounds);
  fillFields(source);
  auto smallOptions = referenceOptions();
  smallOptions.massScale = 1.0f;
  auto largeOptions = referenceOptions();
  largeOptions.massScale = 3.5f;
  pigment::detail::RollingYabMassOperator smallOp(smallOptions), largeOp(largeOptions);
  const auto src = static_cast<const pigment::OwnedYabPlanes&>(source).view();
  smallOp.apply({src, small.view(), 1.0f, 1.0f, bounds, {}}, {});
  largeOp.apply({src, large.view(), 1.0f, 1.0f, bounds, {}}, {});
  const auto a = static_cast<const pigment::OwnedYabPlanes&>(small).view();
  const auto b = static_cast<const pigment::OwnedYabPlanes&>(large).view();
  check(std::abs(a.a.at(5, 5) - b.a.at(5, 5)) > 1e-3f,
        "Mass Scale changes consolidation independently of a fixed boundary field");
  for (int y = bounds.y1; y < bounds.y2; ++y)
    for (int x = bounds.x1; x < bounds.x2; ++x) {
      check(std::isfinite(b.y.at(x, y)) && b.y.at(x, y) >= -0.5f && b.y.at(x, y) <= 3.0f,
            "negative and HDR luminance remains finite and unclipped");
      check(b.a.at(x, y) >= -1.25f && b.a.at(x, y) <= 1.75f,
            "opponent values remain within source convex bounds");
    }
}

void testDomainCancellationScratchAndDeterminism() {
  const pigment::RectI bounds{0, 0, 19, 9};
  pigment::OwnedYabPlanes source(bounds), serial(bounds), threaded(bounds);
  fillFields(source);
  pigment::detail::RollingYabMassOperator op(referenceOptions());
  check(op.requiredInputDomain({}).kind == pigment::InputDomainKind::FullRegionOfDefinition,
        "Rolling YAB reference requests the full RoD");

  pigment::ScratchArena arena;
  pigment::ExecutionContext serialExecution;
  serialExecution.scratch = &arena;
  const auto src = static_cast<const pigment::OwnedYabPlanes&>(source).view();
  op.apply({src, serial.view(), 1.0f, 1.0f, bounds, {}}, serialExecution);
  check(arena.planeCount() == 12, "render-local scratch arena owns iterative planes");

  pigment::ExecutionContext parallelExecution;
  parallelExecution.parallelRows = [](int begin, int end, const pigment::RowFunction& fn) {
    const int middle = begin + (end - begin) / 2;
    std::thread first([&] { fn(begin, middle); });
    std::thread second([&] { fn(middle, end); });
    first.join();
    second.join();
  };
  op.apply({src, threaded.view(), 1.0f, 1.0f, bounds, {}}, parallelExecution);
  const auto serialOut = static_cast<const pigment::OwnedYabPlanes&>(serial).view();
  const auto threadedOut = static_cast<const pigment::OwnedYabPlanes&>(threaded).view();
  for (int y = bounds.y1; y < bounds.y2; ++y) {
    check(std::memcmp(&serialOut.y.at(bounds.x1, y), &threadedOut.y.at(bounds.x1, y),
                      static_cast<std::size_t>(bounds.width()) * sizeof(float)) == 0,
          "row scheduling is bit deterministic");
  }

  int cancellationChecks = 0;
  pigment::ExecutionContext cancelled;
  cancelled.cancelled = [&] { return ++cancellationChecks > 1; };
  pigment::OwnedYabPlanes cancelledOutput(bounds);
  op.apply({src, cancelledOutput.view(), 1.0f, 1.0f, bounds, {}}, cancelled);
  check(cancellationChecks > 1, "Rolling YAB polls cancellation");

  bool rejectedPartial = false;
  try {
    op.apply({src, threaded.view(), 1.0f, 1.0f, {0, 0, 10, 9}, {}}, {});
  } catch (const std::invalid_argument&) {
    rejectedPartial = true;
  }
  check(rejectedPartial, "reference operator rejects partial-RoD application");
}

}  // namespace

int main() {
  testReusableFilteringPrimitives();
  testStructureScaleRejectsSmallStrongEdges();
  testIndependentStrengthAndBoundaryFields();
  testMassScaleIsIndependentAndUnclipped();
  testDomainCancellationScratchAndDeterminism();
  if (failures) {
    std::cerr << failures << " Mass Formation test(s) failed\n";
    return 1;
  }
  std::cout << "All Mass Formation core tests passed\n";
  return 0;
}
