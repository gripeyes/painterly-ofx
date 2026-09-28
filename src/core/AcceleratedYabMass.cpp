#include "core/AcceleratedYabMass.h"

#include "core/Filtering.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace pigment::detail {
namespace {

constexpr int kRollingPasses = 4;

float clamp01(float v) noexcept { return std::max(0.0f, std::min(1.0f, v)); }

ConstYabPlanes constantView(const OwnedYabPlanes& p) noexcept { return p.view(); }

void copyYab(ConstYabPlanes src, YabPlanes dst, RectI region,
             const ExecutionContext& exec) {
  copyPlane(src.y, dst.y, region, exec);
  copyPlane(src.a, dst.a, region, exec);
  copyPlane(src.b, dst.b, region, exec);
}

void boxBlurYab(ConstYabPlanes src, YabPlanes dst, int rx, int ry,
                const ExecutionContext& exec) {
  boxBlurPlane(src.y, dst.y, rx, ry, exec);
  boxBlurPlane(src.a, dst.a, rx, ry, exec);
  boxBlurPlane(src.b, dst.b, rx, ry, exec);
}

void guidedChannel(ConstFloatPlaneView source, ConstFloatPlaneView guide,
                   ScalarFieldView boundary, FloatPlaneView destination,
                   int rx, int ry, float guideScale,
                   const ExecutionContext& exec) {
  const RectI r = destination.bounds;
  OwnedPlane weight(r), g(r), wp(r), wg(r), wgg(r), wgp(r);
  auto weightV = weight.view();
  auto gV = g.view();
  exec.parallelRows(r.y1, r.y2, [&](int y1, int y2) {
    for (int y = y1; y < y2 && !exec.cancelled(); ++y) {
      for (int x = r.x1; x < r.x2; ++x) {
        const float w = std::max(1.0e-4f, clamp01(boundary.at(x, y)));
        const float gv = guide.at(x, y) / std::max(std::abs(guideScale), 1.0e-6f);
        weightV.at(x, y) = w;
        gV.at(x, y) = gv;
        wp.view().at(x, y) = w * source.at(x, y);
        wg.view().at(x, y) = w * gv;
        wgg.view().at(x, y) = w * gv * gv;
        wgp.view().at(x, y) = w * gv * source.at(x, y);
      }
    }
  });

  OwnedPlane mw(r), mp(r), mg(r), mgg(r), mgp(r);
  boxBlurPlane(static_cast<const OwnedPlane&>(weight).view(), mw.view(), rx, ry, exec);
  boxBlurPlane(static_cast<const OwnedPlane&>(wp).view(), mp.view(), rx, ry, exec);
  boxBlurPlane(static_cast<const OwnedPlane&>(wg).view(), mg.view(), rx, ry, exec);
  boxBlurPlane(static_cast<const OwnedPlane&>(wgg).view(), mgg.view(), rx, ry, exec);
  boxBlurPlane(static_cast<const OwnedPlane&>(wgp).view(), mgp.view(), rx, ry, exec);

  OwnedPlane coeffA(r), coeffB(r), meanA(r), meanB(r);
  exec.parallelRows(r.y1, r.y2, [&](int y1, int y2) {
    for (int y = y1; y < y2 && !exec.cancelled(); ++y) {
      for (int x = r.x1; x < r.x2; ++x) {
        const float invW = 1.0f / std::max(mw.view().at(x, y), 1.0e-6f);
        const float meanG = mg.view().at(x, y) * invW;
        const float meanP = mp.view().at(x, y) * invW;
        const float variance = std::max(0.0f, mgg.view().at(x, y) * invW - meanG * meanG);
        const float covariance = mgp.view().at(x, y) * invW - meanG * meanP;
        const float a = covariance / (variance + 0.04f);
        coeffA.view().at(x, y) = a;
        coeffB.view().at(x, y) = meanP - a * meanG;
      }
    }
  });
  boxBlurPlane(static_cast<const OwnedPlane&>(coeffA).view(), meanA.view(), rx, ry, exec);
  boxBlurPlane(static_cast<const OwnedPlane&>(coeffB).view(), meanB.view(), rx, ry, exec);
  exec.parallelRows(r.y1, r.y2, [&](int y1, int y2) {
    for (int y = y1; y < y2 && !exec.cancelled(); ++y)
      for (int x = r.x1; x < r.x2; ++x)
        destination.at(x, y) = meanA.view().at(x, y) * gV.at(x, y) +
                               meanB.view().at(x, y);
  });
}

float domainCoefficient(float leftGuide, float rightGuide, float scale,
                        float permeability, float spatialSigma) noexcept {
  const float feature = std::abs(rightGuide - leftGuide) /
                        std::max(std::abs(scale), 1.0e-6f);
  const float barrier = -std::log(std::max(1.0e-4f, clamp01(permeability)));
  const float distance = 1.0f + spatialSigma * (feature + barrier);
  return std::exp(-std::sqrt(2.0f) * distance / std::max(spatialSigma, 0.25f));
}

void recursiveHorizontal(FloatPlaneView signal, ConstFloatPlaneView guide,
                         ScalarFieldView boundary, float scale, float sigma,
                         bool reverse, const ExecutionContext& exec) {
  exec.parallelRows(signal.bounds.y1, signal.bounds.y2, [&](int y1, int y2) {
    for (int y = y1; y < y2 && !exec.cancelled(); ++y) {
      if (!reverse) {
        for (int x = signal.bounds.x1 + 1; x < signal.bounds.x2; ++x) {
          const float a = domainCoefficient(guide.at(x - 1, y), guide.at(x, y),
                                            scale, std::min(boundary.at(x - 1, y), boundary.at(x, y)), sigma);
          signal.at(x, y) += a * (signal.at(x - 1, y) - signal.at(x, y));
        }
      } else {
        for (int x = signal.bounds.x2 - 2; x >= signal.bounds.x1; --x) {
          const float a = domainCoefficient(guide.at(x, y), guide.at(x + 1, y),
                                            scale, std::min(boundary.at(x, y), boundary.at(x + 1, y)), sigma);
          signal.at(x, y) += a * (signal.at(x + 1, y) - signal.at(x, y));
        }
      }
    }
  });
}

void recursiveVertical(FloatPlaneView signal, ConstFloatPlaneView guide,
                       ScalarFieldView boundary, float scale, float sigma,
                       bool reverse, const ExecutionContext& exec) {
  // Columns are independent; use the supplied row executor as a generic range executor.
  exec.parallelRows(signal.bounds.x1, signal.bounds.x2, [&](int x1, int x2) {
    for (int x = x1; x < x2 && !exec.cancelled(); ++x) {
      if (!reverse) {
        for (int y = signal.bounds.y1 + 1; y < signal.bounds.y2; ++y) {
          const float a = domainCoefficient(guide.at(x, y - 1), guide.at(x, y),
                                            scale, std::min(boundary.at(x, y - 1), boundary.at(x, y)), sigma);
          signal.at(x, y) += a * (signal.at(x, y - 1) - signal.at(x, y));
        }
      } else {
        for (int y = signal.bounds.y2 - 2; y >= signal.bounds.y1; --y) {
          const float a = domainCoefficient(guide.at(x, y), guide.at(x, y + 1),
                                            scale, std::min(boundary.at(x, y), boundary.at(x, y + 1)), sigma);
          signal.at(x, y) += a * (signal.at(x, y + 1) - signal.at(x, y));
        }
      }
    }
  });
}

void domainChannel(ConstFloatPlaneView source, ConstFloatPlaneView guide,
                   ScalarFieldView boundary, FloatPlaneView destination,
                   float scale, float sigma, bool verticalFirst,
                   const ExecutionContext& exec) {
  copyPlane(source, destination, destination.bounds, exec);
  if (verticalFirst) {
    recursiveVertical(destination, guide, boundary, scale, sigma, false, exec);
    recursiveVertical(destination, guide, boundary, scale, sigma, true, exec);
    recursiveHorizontal(destination, guide, boundary, scale, sigma, false, exec);
    recursiveHorizontal(destination, guide, boundary, scale, sigma, true, exec);
  } else {
    recursiveHorizontal(destination, guide, boundary, scale, sigma, false, exec);
    recursiveHorizontal(destination, guide, boundary, scale, sigma, true, exec);
    recursiveVertical(destination, guide, boundary, scale, sigma, false, exec);
    recursiveVertical(destination, guide, boundary, scale, sigma, true, exec);
  }
}

void blendUpdate(ConstYabPlanes previous, ConstYabPlanes filtered, YabPlanes output,
                 ScalarFieldView strength, float massStrength,
                 const ExecutionContext& exec) {
  const RectI r = output.y.bounds;
  exec.parallelRows(r.y1, r.y2, [&](int y1, int y2) {
    for (int y = y1; y < y2 && !exec.cancelled(); ++y) {
      for (int x = r.x1; x < r.x2; ++x) {
        const float gain = clamp01(massStrength) * clamp01(strength.at(x, y));
        output.y.at(x, y) = previous.y.at(x, y) + gain * (filtered.y.at(x, y) - previous.y.at(x, y));
        output.a.at(x, y) = previous.a.at(x, y) + gain * (filtered.a.at(x, y) - previous.a.at(x, y));
        output.b.at(x, y) = previous.b.at(x, y) + gain * (filtered.b.at(x, y) - previous.b.at(x, y));
      }
    }
  });
}

}  // namespace

InputDomainRequest AcceleratedYabMassOperator::requiredInputDomain(
    const ImageGeometry&) const noexcept {
  return {InputDomainKind::FullRegionOfDefinition, 0, 0};
}

void AcceleratedYabMassOperator::apply(const SpatialOperation& op,
                                       const ExecutionContext& exec) const {
  applyWithDebug(op, exec, {});
}

void AcceleratedYabMassOperator::applyWithDebug(
    const SpatialOperation& op, const ExecutionContext& exec,
    const RollingYabMassDebugOutputs& debug) const {
  const RectI r = op.source.y.bounds;
  if (r.empty()) return;
  if (op.outputRegion.x1 != r.x1 || op.outputRegion.y1 != r.y1 ||
      op.outputRegion.x2 != r.x2 || op.outputRegion.y2 != r.y2)
    throw std::invalid_argument("accelerated mass backends require the full source RoD");

  const float sigmaX = std::max(0.25f, options_.massScale *
      static_cast<float>(op.geometry.renderScaleX / std::max(op.geometry.pixelAspect, 1.0e-6)));
  const float sigmaY = std::max(0.25f, options_.massScale *
      static_cast<float>(op.geometry.renderScaleY));
  const int rx = std::max(1, static_cast<int>(std::ceil(sigmaX)));
  const int ry = std::max(1, static_cast<int>(std::ceil(sigmaY)));

  OwnedYabPlanes seed(r), seedTemp(r), variation(r), current(r), filtered(r), next(r);
  copyYab(op.source, seed.view(), r, exec);
  for (int i = 0; i < 3; ++i) {
    boxBlurYab(constantView(seed), seedTemp.view(), rx, ry, exec);
    std::swap(seed, seedTemp);
  }
  boxBlurYab(op.source, variation.view(), std::max(1, rx / 4),
             std::max(1, ry / 4), exec);
  if (debug.consolidationSeed) copyYab(constantView(seed), *debug.consolidationSeed, r, exec);
  blendUpdate(op.source, constantView(seed), current.view(), op.processingStrength,
              options_.massStrength, exec);

  for (int pass = 0; pass < kRollingPasses && !exec.cancelled(); ++pass) {
    const ConstYabPlanes guide = constantView(current);
    if (algorithm_ == AcceleratedMassAlgorithm::Guided) {
      guidedChannel(op.source.y, guide.y, op.boundaryProtection, filtered.view().y,
                    rx, ry, options_.similarity.luminanceScale, exec);
      guidedChannel(op.source.a, guide.a, op.boundaryProtection, filtered.view().a,
                    rx, ry, options_.similarity.chromaScale, exec);
      guidedChannel(op.source.b, guide.b, op.boundaryProtection, filtered.view().b,
                    rx, ry, options_.similarity.chromaScale, exec);
    } else {
      const float schedule = std::max(sigmaX, sigmaY) *
          std::sqrt(3.0f) * static_cast<float>(1 << (kRollingPasses - pass - 1)) /
          std::sqrt(static_cast<float>((1 << (2 * kRollingPasses)) - 1));
      const bool verticalFirst = (pass & 1) != 0;
      domainChannel(op.source.y, guide.y, op.boundaryProtection, filtered.view().y,
                    options_.similarity.luminanceScale, schedule, verticalFirst, exec);
      domainChannel(op.source.a, guide.a, op.boundaryProtection, filtered.view().a,
                    options_.similarity.chromaScale, schedule, verticalFirst, exec);
      domainChannel(op.source.b, guide.b, op.boundaryProtection, filtered.view().b,
                    options_.similarity.chromaScale, schedule, verticalFirst, exec);
    }
    blendUpdate(guide, constantView(filtered), next.view(), op.processingStrength,
                options_.massStrength, exec);
    std::swap(current, next);
    if (debug.rollingIterations[static_cast<std::size_t>(pass)])
      copyYab(constantView(current), *debug.rollingIterations[static_cast<std::size_t>(pass)], r, exec);
  }

  if (debug.preReintegrationMass)
    copyYab(constantView(current), *debug.preReintegrationMass, r, exec);
  const float internal = clamp01(options_.internalVariation);
  const float luma = clamp01(options_.lumaMassing);
  const float chroma = clamp01(options_.chromaMassing);
  const ConstYabPlanes mass = constantView(current);
  const ConstYabPlanes varied = constantView(variation);
  exec.parallelRows(r.y1, r.y2, [&](int y1, int y2) {
    for (int y = y1; y < y2 && !exec.cancelled(); ++y) {
      for (int x = r.x1; x < r.x2; ++x) {
        const float effected = clamp01(options_.massStrength) *
                               clamp01(op.processingStrength.at(x, y));
        const float cy = mass.y.at(x, y) + internal * effected * (varied.y.at(x, y) - mass.y.at(x, y));
        const float ca = mass.a.at(x, y) + internal * effected * (varied.a.at(x, y) - mass.a.at(x, y));
        const float cb = mass.b.at(x, y) + internal * effected * (varied.b.at(x, y) - mass.b.at(x, y));
        if (debug.internalVariationResidual) {
          debug.internalVariationResidual->y.at(x, y) = varied.y.at(x, y) - mass.y.at(x, y);
          debug.internalVariationResidual->a.at(x, y) = varied.a.at(x, y) - mass.a.at(x, y);
          debug.internalVariationResidual->b.at(x, y) = varied.b.at(x, y) - mass.b.at(x, y);
        }
        op.destination.y.at(x, y) = op.source.y.at(x, y) + luma * (cy - op.source.y.at(x, y));
        op.destination.a.at(x, y) = op.source.a.at(x, y) + chroma * (ca - op.source.a.at(x, y));
        op.destination.b.at(x, y) = op.source.b.at(x, y) + chroma * (cb - op.source.b.at(x, y));
      }
    }
  });
}

}  // namespace pigment::detail
