#include "core/DetailCollapseResearch.h"

#include "core/RollingYabMass.h"
#include "core/AcceleratedYabMass.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>

namespace pigment {
namespace {

float clamp01(float value) noexcept {
  return std::max(0.0f, std::min(1.0f, value));
}

void copyImageRegion(ConstImageView src, ImageView dst, RectI region) {
  const std::size_t bytes = static_cast<std::size_t>(region.width()) *
                            static_cast<std::size_t>(src.components) * sizeof(float);
  for (int y = region.y1; y < region.y2; ++y)
    std::memcpy(dst.pixel(region.x1, y), src.pixel(region.x1, y), bytes);
}

bool isIterationView(DetailCollapseDebugView view, int& index) noexcept {
  const int first = static_cast<int>(DetailCollapseDebugView::RollingIteration1);
  const int value = static_cast<int>(view);
  if (value >= first && value < first + 4) {
    index = value - first;
    return true;
  }
  return false;
}

std::array<float, 3> straightRgb(ConstImageView src, int x, int y,
                                 bool premultiplied) noexcept {
  const float* pixel = src.pixel(x, y);
  const float alpha = src.components == 4 ? pixel[3] : 1.0f;
  std::array<float, 3> rgb{pixel[0], pixel[1], pixel[2]};
  if (premultiplied) {
    if (std::abs(alpha) > 1e-6f) {
      for (float& channel : rgb) channel /= alpha;
    } else {
      rgb = {0.0f, 0.0f, 0.0f};
    }
  }
  return rgb;
}

float fieldMinimum(ConstFloatPlaneView field) {
  float result = std::numeric_limits<float>::infinity();
  for (int y = field.bounds.y1; y < field.bounds.y2; ++y)
    for (int x = field.bounds.x1; x < field.bounds.x2; ++x)
      if (std::isfinite(field.at(x, y))) result = std::min(result, field.at(x, y));
  return std::isfinite(result) ? result : 0.0f;
}

float fieldMaximum(ConstFloatPlaneView field) {
  float result = -std::numeric_limits<float>::infinity();
  for (int y = field.bounds.y1; y < field.bounds.y2; ++y)
    for (int x = field.bounds.x1; x < field.bounds.x2; ++x)
      if (std::isfinite(field.at(x, y))) result = std::max(result, field.at(x, y));
  return std::isfinite(result) ? result : 1.0f;
}

}  // namespace

InputDomainRequest detailCollapseResearchInputDomain() noexcept {
  return {InputDomainKind::FullRegionOfDefinition, 0, 0};
}

void processDetailCollapseResearch(
    ConstImageView src, ImageView dst, RectI output,
    const DetailCollapseResearchParams& p, const ImageGeometry& geometry,
    const ConstFloatPlaneView* externalMask, const ExecutionContext& exec) {
  if (!src.data || !dst.data || (src.components != 3 && src.components != 4) ||
      src.components != dst.components)
    throw std::invalid_argument(
        "DetailCollapse research requires matching float RGB or RGBA views");
  output = intersect(output, intersect(src.bounds, dst.bounds));
  if (output.empty()) return;
  if (p.debugView == DetailCollapseDebugView::Original ||
      (p.debugView == DetailCollapseDebugView::Final &&
       (p.amount == 0.0f || p.mix == 0.0f))) {
    copyImageRegion(src, dst, output);
    return;
  }

  MatrixOpponentTransform transform(p.gamut);
  OwnedYabPlanes original(src.bounds), result(src.bounds), structureGuide(src.bounds);
  OwnedPlane processingStrength(src.bounds, 1.0f);
  OwnedPlane boundaryProtection(src.bounds, 1.0f);
  auto originalView = original.view();

  exec.parallelRows(src.bounds.y1, src.bounds.y2, [&](int y1, int y2) {
    for (int y = y1; y < y2 && !exec.cancelled(); ++y) {
      for (int x = src.bounds.x1; x < src.bounds.x2; ++x) {
        const YabPixel value = transform.toYab(straightRgb(src, x, y, p.premultiplied));
        originalView.y.at(x, y) = value.y;
        originalView.a.at(x, y) = value.a;
        originalView.b.at(x, y) = value.b;
      }
    }
  });
  const ConstYabPlanes originalConst = asConst(originalView);

  composeProcessingStrengthField(originalConst.y, externalMask, p.invertMask,
                                 p.tonalMask, processingStrength.view(), exec);
  const float amount = clamp01(p.amount);
  exec.parallelRows(src.bounds.y1, src.bounds.y2, [&](int y1, int y2) {
    for (int y = y1; y < y2 && !exec.cancelled(); ++y)
      for (int x = src.bounds.x1; x < src.bounds.x2; ++x)
        processingStrength.view().at(x, y) *= amount;
  });

  const float structureSigmaX = std::max(0.0f, p.structureScale) *
      static_cast<float>(geometry.renderScaleX /
                         std::max(geometry.pixelAspect, 1e-6));
  const float structureSigmaY = std::max(0.0f, p.structureScale) *
      static_cast<float>(geometry.renderScaleY);
  buildStructureGuide(originalConst, structureGuide.view(), structureSigmaX,
                      structureSigmaY, p.structurePreserve, exec);
  StructureBoundaryOptions boundaryOptions;
  boundaryOptions.protection = p.boundaryPreserve;
  boundaryOptions.softness = p.boundarySoftness;
  boundaryOptions.luminanceWeight = 1.0f;
  boundaryOptions.axisAWeight = 0.25f;
  boundaryOptions.axisBWeight = 0.25f;
  buildBoundaryProtectionFromGuide(
      static_cast<const OwnedYabPlanes&>(structureGuide).view(),
      boundaryProtection.view(), boundaryOptions, exec);

  detail::RollingYabMassOptions rollingOptions;
  rollingOptions.massScale = p.massScale;
  rollingOptions.massStrength = p.massStrength;
  rollingOptions.internalVariation = p.internalVariation;
  rollingOptions.lumaMassing = p.lumaMassing;
  rollingOptions.chromaMassing = p.chromaMassing;
  rollingOptions.similarity.luminanceScale = std::max(std::abs(p.toneSimilarity), 1e-6f);
  rollingOptions.similarity.chromaScale = std::max(std::abs(p.chromaSimilarity), 1e-6f);

  std::unique_ptr<OwnedYabPlanes> debugYab;
  detail::RollingYabMassDebugOutputs debug;
  int iterationIndex = -1;
  const bool iterationView = isIterationView(p.debugView, iterationIndex);
  const bool needsCapturedYab =
      p.debugView == DetailCollapseDebugView::ConsolidationSeed || iterationView ||
      p.debugView == DetailCollapseDebugView::InternalVariationResidual ||
      p.debugView == DetailCollapseDebugView::PreReintegrationMass;
  if (needsCapturedYab) debugYab = std::make_unique<OwnedYabPlanes>(src.bounds);
  YabPlanes capturedView{};
  if (debugYab) {
    capturedView = debugYab->view();
    debug = {};
    if (p.debugView == DetailCollapseDebugView::ConsolidationSeed)
      debug.consolidationSeed = &capturedView;
    if (iterationView)
      debug.rollingIterations[static_cast<std::size_t>(iterationIndex)] = &capturedView;
    if (p.debugView == DetailCollapseDebugView::InternalVariationResidual)
      debug.internalVariationResidual = &capturedView;
    if (p.debugView == DetailCollapseDebugView::PreReintegrationMass)
      debug.preReintegrationMass = &capturedView;
  }

  const bool accelerated = p.backend == DetailCollapseBackend::GuidedCpu ||
      p.backend == DetailCollapseBackend::GuidedMetal ||
      p.backend == DetailCollapseBackend::DomainTransformCpu ||
      p.backend == DetailCollapseBackend::DomainTransformMetal;
  const auto strengthConst = static_cast<const OwnedPlane&>(processingStrength).view();
  const auto boundaryConst = static_cast<const OwnedPlane&>(boundaryProtection).view();
  const SpatialOperation operation{originalConst, result.view(), strengthConst,
                                   boundaryConst, src.bounds, geometry};
  if (accelerated) {
    const auto algorithm = (p.backend == DetailCollapseBackend::GuidedCpu ||
                            p.backend == DetailCollapseBackend::GuidedMetal)
        ? detail::AcceleratedMassAlgorithm::Guided
        : detail::AcceleratedMassAlgorithm::DomainTransform;
    detail::AcceleratedYabMassOperator mass(algorithm, rollingOptions);
    mass.applyWithDebug(operation, exec, debug);
  } else {
    detail::RollingYabMassOperator mass(rollingOptions);
    mass.applyWithDebug(operation, exec, debug);
  }

  const ConstYabPlanes finalYab = static_cast<const OwnedYabPlanes&>(result).view();
  ConstYabPlanes displayYab = finalYab;
  if (debugYab) displayYab = static_cast<const OwnedYabPlanes&>(*debugYab).view();
  const ConstYabPlanes structureConst =
      static_cast<const OwnedYabPlanes&>(structureGuide).view();

  float scalarMinimum = 0.0f, scalarMaximum = 1.0f;
  if (p.debugView == DetailCollapseDebugView::StructureGuide) {
    scalarMinimum = fieldMinimum(structureConst.y);
    scalarMaximum = fieldMaximum(structureConst.y);
  }

  float signedMaximum = 0.0f;
  if (p.debugView == DetailCollapseDebugView::InternalVariationResidual ||
      p.debugView == DetailCollapseDebugView::DifferenceFromOriginal) {
    for (int y = src.bounds.y1; y < src.bounds.y2; ++y) {
      for (int x = src.bounds.x1; x < src.bounds.x2; ++x) {
        YabPixel delta;
        if (p.debugView == DetailCollapseDebugView::InternalVariationResidual) {
          delta = {displayYab.y.at(x, y), displayYab.a.at(x, y), displayYab.b.at(x, y)};
        } else {
          delta = {finalYab.y.at(x, y) - originalConst.y.at(x, y),
                   finalYab.a.at(x, y) - originalConst.a.at(x, y),
                   finalYab.b.at(x, y) - originalConst.b.at(x, y)};
        }
        const auto rgb = transform.toRgb(delta);
        signedMaximum = std::max({signedMaximum, std::abs(rgb[0]),
                                  std::abs(rgb[1]), std::abs(rgb[2])});
      }
    }
    signedMaximum = std::max(signedMaximum, 1e-6f);
  }

  const float mix = clamp01(p.mix);
  exec.parallelRows(output.y1, output.y2, [&](int y1, int y2) {
    for (int y = y1; y < y2 && !exec.cancelled(); ++y) {
      for (int x = output.x1; x < output.x2; ++x) {
        const float* input = src.pixel(x, y);
        float* outputPixel = dst.pixel(x, y);
        const float alpha = src.components == 4 ? input[3] : 1.0f;
        std::array<float, 3> rgb{};

        if (p.debugView == DetailCollapseDebugView::ProcessingStrength) {
          rgb.fill(clamp01(strengthConst.at(x, y)));
        } else if (p.debugView == DetailCollapseDebugView::BoundaryProtection) {
          rgb.fill(clamp01(boundaryConst.at(x, y)));
        } else if (p.debugView == DetailCollapseDebugView::StructureGuide) {
          const float range = std::max(scalarMaximum - scalarMinimum, 1e-6f);
          rgb.fill(clamp01((structureConst.y.at(x, y) - scalarMinimum) / range));
        } else if (p.debugView == DetailCollapseDebugView::InternalVariationResidual ||
                   p.debugView == DetailCollapseDebugView::DifferenceFromOriginal) {
          YabPixel delta;
          if (p.debugView == DetailCollapseDebugView::InternalVariationResidual) {
            delta = {displayYab.y.at(x, y), displayYab.a.at(x, y), displayYab.b.at(x, y)};
          } else {
            delta = {finalYab.y.at(x, y) - originalConst.y.at(x, y),
                     finalYab.a.at(x, y) - originalConst.a.at(x, y),
                     finalYab.b.at(x, y) - originalConst.b.at(x, y)};
          }
          const auto signedRgb = transform.toRgb(delta);
          for (int channel = 0; channel < 3; ++channel)
            rgb[channel] = 0.5f + 0.45f * signedRgb[channel] / signedMaximum;
        } else {
          YabPixel value{displayYab.y.at(x, y), displayYab.a.at(x, y),
                         displayYab.b.at(x, y)};
          if (p.debugView == DetailCollapseDebugView::YMassResult)
            value = {finalYab.y.at(x, y), originalConst.a.at(x, y), originalConst.b.at(x, y)};
          else if (p.debugView == DetailCollapseDebugView::ABMassResult)
            value = {originalConst.y.at(x, y), finalYab.a.at(x, y), finalYab.b.at(x, y)};
          rgb = transform.toRgb(value);
          if (p.debugView == DetailCollapseDebugView::Final) {
            const auto originalRgb = straightRgb(src, x, y, p.premultiplied);
            for (int channel = 0; channel < 3; ++channel)
              rgb[channel] = originalRgb[channel] + mix * (rgb[channel] - originalRgb[channel]);
          }
        }

        if (p.debugView == DetailCollapseDebugView::Final && p.premultiplied &&
            std::abs(alpha) <= 1e-6f) {
          std::copy(input, input + src.components, outputPixel);
          continue;
        }
        for (int channel = 0; channel < 3; ++channel)
          outputPixel[channel] = p.premultiplied ? rgb[channel] * alpha : rgb[channel];
        if (src.components == 4) outputPixel[3] = input[3];
      }
    }
  });
}

}  // namespace pigment
