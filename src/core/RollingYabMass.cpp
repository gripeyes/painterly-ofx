#include "core/RollingYabMass.h"

#include "core/Filtering.h"
#include "core/ScratchArena.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace pigment::detail {
namespace {

float clamp01(float value) noexcept {
  return std::max(0.0f, std::min(1.0f, value));
}

YabPixel read(ConstYabPlanes planes, int x, int y) noexcept {
  return {planes.y.at(x, y), planes.a.at(x, y), planes.b.at(x, y)};
}

YabPixel read(YabPlanes planes, int x, int y) noexcept {
  return {planes.y.at(x, y), planes.a.at(x, y), planes.b.at(x, y)};
}

void write(YabPlanes planes, int x, int y, const YabPixel& value) noexcept {
  planes.y.at(x, y) = value.y;
  planes.a.at(x, y) = value.a;
  planes.b.at(x, y) = value.b;
}

ConstYabPlanes makeConst(YabPlanes p) noexcept {
  return {{p.y.data, p.y.rowStride, p.y.bounds},
          {p.a.data, p.a.rowStride, p.a.bounds},
          {p.b.data, p.b.rowStride, p.b.bounds}};
}

void copyYab(ConstYabPlanes src, YabPlanes dst, RectI region,
             const ExecutionContext& exec) {
  copyPlane(src.y, dst.y, region, exec);
  copyPlane(src.a, dst.a, region, exec);
  copyPlane(src.b, dst.b, region, exec);
}

void validateField(ScalarFieldView field, RectI region, const char* name) {
  if (!field.isConstant && intersect(field.plane.bounds, region).width() != region.width())
    throw std::invalid_argument(name);
  if (!field.isConstant && intersect(field.plane.bounds, region).height() != region.height())
    throw std::invalid_argument(name);
}

bool contains(RectI outer, RectI inner) noexcept {
  return outer.x1 <= inner.x1 && outer.y1 <= inner.y1 &&
         outer.x2 >= inner.x2 && outer.y2 >= inner.y2;
}

void validatePlanes(ConstYabPlanes source, YabPlanes destination, RectI region) {
  if (!source.y || !source.a || !source.b ||
      !destination.y || !destination.a || !destination.b)
    throw std::invalid_argument("RollingYabMass requires valid YAB planes");
  if (!contains(source.y.bounds, region) || !contains(source.a.bounds, region) ||
      !contains(source.b.bounds, region) || !contains(destination.y.bounds, region) ||
      !contains(destination.a.bounds, region) || !contains(destination.b.bounds, region))
    throw std::invalid_argument("YAB planes do not cover the full source RoD");
}

float pathPermeability(ScalarFieldView field, int x0, int y0,
                       int x1, int y1) noexcept {
  if (field.isConstant) return clamp01(field.constant);
  const int steps = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
  float permeability = 1.0f;
  for (int step = 0; step <= steps; ++step) {
    const float t = steps == 0 ? 0.0f : step / static_cast<float>(steps);
    const int x = static_cast<int>(std::lround(x0 + t * (x1 - x0)));
    const int y = static_cast<int>(std::lround(y0 + t * (y1 - y0)));
    permeability = std::min(permeability, clamp01(field.at(x, y)));
    if (permeability <= 0.0f) break;
  }
  return permeability;
}

}  // namespace

InputDomainRequest RollingYabMassOperator::requiredInputDomain(
    const ImageGeometry&) const noexcept {
  // The bilateral reference intentionally chooses correctness over tiled
  // recomputation. Later approximations may report a conservative local halo.
  return {InputDomainKind::FullRegionOfDefinition, 0, 0};
}

void RollingYabMassOperator::apply(const SpatialOperation& op,
                                   const ExecutionContext& exec) const {
  applyWithDebug(op, exec, {});
}

void RollingYabMassOperator::applyWithDebug(
    const SpatialOperation& op, const ExecutionContext& exec,
    const RollingYabMassDebugOutputs& debug) const {
  const RectI region = intersect(op.outputRegion, op.source.y.bounds);
  if (region.empty()) return;
  if (region.x1 != op.source.y.bounds.x1 || region.y1 != op.source.y.bounds.y1 ||
      region.x2 != op.source.y.bounds.x2 || region.y2 != op.source.y.bounds.y2)
    throw std::invalid_argument("RollingYabMass reference requires the full source RoD");
  validatePlanes(op.source, op.destination, region);
  validateField(op.processingStrength, region,
                "processing-strength field does not cover the source RoD");
  validateField(op.boundaryProtection, region,
                "boundary-protection field does not cover the source RoD");

  const float strength = clamp01(options_.massStrength);
  if (strength <= 0.0f || options_.massScale <= 1e-4f) {
    copyYab(op.source, op.destination, region, exec);
    return;
  }

  const double sigmaX = std::max(0.25, static_cast<double>(options_.massScale) *
      op.geometry.renderScaleX / std::max(op.geometry.pixelAspect, 1e-6));
  const double sigmaY = std::max(0.25, static_cast<double>(options_.massScale) *
      op.geometry.renderScaleY);
  const int radiusX = std::max(1, static_cast<int>(std::ceil(2.0 * sigmaX)));
  const int radiusY = std::max(1, static_cast<int>(std::ceil(2.0 * sigmaY)));

  ScratchArena localScratch;
  ScratchArena& scratch = exec.scratch ? *exec.scratch : localScratch;
  scratch.reset();
  const auto makePlanes = [&]() {
    return YabPlanes{scratch.acquirePlane(region), scratch.acquirePlane(region),
                     scratch.acquirePlane(region)};
  };
  YabPlanes seed = makePlanes();
  YabPlanes current = makePlanes();
  YabPlanes next = makePlanes();
  YabPlanes variation = makePlanes();

  gaussianBlurYab(op.source, seed, static_cast<float>(sigmaX),
                  static_cast<float>(sigmaY), exec);
  gaussianBlurYab(op.source, variation, static_cast<float>(sigmaX * 0.25),
                  static_cast<float>(sigmaY * 0.25), exec);
  if (debug.consolidationSeed)
    copyYab(makeConst(seed), *debug.consolidationSeed, region, exec);

  exec.parallelRows(region.y1, region.y2, [&](int y1, int y2) {
    for (int y = y1; y < y2 && !exec.cancelled(); ++y) {
      for (int x = region.x1; x < region.x2; ++x) {
        const float gain = strength * clamp01(op.processingStrength.at(x, y));
        const YabPixel original = read(op.source, x, y);
        const YabPixel blurred = read(seed, x, y);
        write(current, x, y,
              {original.y + gain * (blurred.y - original.y),
               original.a + gain * (blurred.a - original.a),
               original.b + gain * (blurred.b - original.b)});
      }
    }
  });

  constexpr int kRollingPasses = 4;
  for (int pass = 0; pass < kRollingPasses && !exec.cancelled(); ++pass) {
    const ConstYabPlanes guide = makeConst(current);
    exec.parallelRows(region.y1, region.y2, [&](int y1, int y2) {
      for (int y = y1; y < y2 && !exec.cancelled(); ++y) {
        for (int x = region.x1; x < region.x2; ++x) {
          const YabPixel centerGuide = read(guide, x, y);
          double sumY = 0.0, sumA = 0.0, sumB = 0.0, sumWeight = 0.0;
          for (int oy = -radiusY; oy <= radiusY; ++oy) {
            const int sy = std::max(region.y1, std::min(region.y2 - 1, y + oy));
            const double ny = oy / sigmaY;
            for (int ox = -radiusX; ox <= radiusX; ++ox) {
              const int sx = std::max(region.x1, std::min(region.x2 - 1, x + ox));
              const double nx = ox / sigmaX;
              const double spatial = std::exp(-0.5 * (nx * nx + ny * ny));
              const float similarity = gaussianSimilarity(
                  centerGuide, read(guide, sx, sy), options_.similarity);
              const float permeability = pathPermeability(
                  op.boundaryProtection, x, y, sx, sy);
              const double weight = spatial * similarity * permeability;
              const YabPixel sample = read(op.source, sx, sy);
              sumY += weight * sample.y;
              sumA += weight * sample.a;
              sumB += weight * sample.b;
              sumWeight += weight;
            }
          }
          const YabPixel previous = read(guide, x, y);
          YabPixel filtered = previous;
          if (sumWeight > 1e-15) {
            filtered = {static_cast<float>(sumY / sumWeight),
                        static_cast<float>(sumA / sumWeight),
                        static_cast<float>(sumB / sumWeight)};
          }
          const float gain = strength * clamp01(op.processingStrength.at(x, y));
          write(next, x, y,
                {previous.y + gain * (filtered.y - previous.y),
                 previous.a + gain * (filtered.a - previous.a),
                 previous.b + gain * (filtered.b - previous.b)});
        }
      }
    });
    std::swap(current, next);
    if (debug.rollingIterations[static_cast<std::size_t>(pass)])
      copyYab(makeConst(current),
              *debug.rollingIterations[static_cast<std::size_t>(pass)], region, exec);
  }

  const float internal = clamp01(options_.internalVariation);
  const float luma = clamp01(options_.lumaMassing);
  const float chroma = clamp01(options_.chromaMassing);
  const ConstYabPlanes mass = makeConst(current);
  const ConstYabPlanes varied = makeConst(variation);
  if (debug.preReintegrationMass)
    copyYab(mass, *debug.preReintegrationMass, region, exec);
  exec.parallelRows(region.y1, region.y2, [&](int y1, int y2) {
    for (int y = y1; y < y2 && !exec.cancelled(); ++y) {
      for (int x = region.x1; x < region.x2; ++x) {
        const YabPixel original = read(op.source, x, y);
        const YabPixel consolidated = read(mass, x, y);
        const YabPixel lowVariation = read(varied, x, y);
        const float effected = strength * clamp01(op.processingStrength.at(x, y));
        const YabPixel candidate{
            consolidated.y + internal * effected * (lowVariation.y - consolidated.y),
            consolidated.a + internal * effected * (lowVariation.a - consolidated.a),
            consolidated.b + internal * effected * (lowVariation.b - consolidated.b)};
        if (debug.internalVariationResidual) {
          write(*debug.internalVariationResidual, x, y,
                {lowVariation.y - consolidated.y,
                 lowVariation.a - consolidated.a,
                 lowVariation.b - consolidated.b});
        }
        write(op.destination, x, y,
              {original.y + luma * (candidate.y - original.y),
               original.a + chroma * (candidate.a - original.a),
               original.b + chroma * (candidate.b - original.b)});
      }
    }
  });
}

}  // namespace pigment::detail
