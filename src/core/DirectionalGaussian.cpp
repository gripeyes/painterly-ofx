#include "core/DirectionalGaussian.h"

#include <algorithm>
#include <cmath>

namespace pigment {
namespace {
constexpr double kPi = 3.14159265358979323846;

float bilinear(ConstFloatPlaneView p, double x, double y) noexcept {
  x = std::max<double>(p.bounds.x1, std::min<double>(p.bounds.x2 - 1, x));
  y = std::max<double>(p.bounds.y1, std::min<double>(p.bounds.y2 - 1, y));
  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const int x1 = std::min(p.bounds.x2 - 1, x0 + 1);
  const int y1 = std::min(p.bounds.y2 - 1, y0 + 1);
  const float fx = static_cast<float>(x - x0);
  const float fy = static_cast<float>(y - y0);
  const float a = p.at(x0, y0) * (1.0f - fx) + p.at(x1, y0) * fx;
  const float b = p.at(x0, y1) * (1.0f - fx) + p.at(x1, y1) * fx;
  return a * (1.0f - fy) + b * fy;
}

void copyPlane(ConstFloatPlaneView src, FloatPlaneView dst, RectI r,
               const ExecutionContext& exec) {
  exec.parallelRows(r.y1, r.y2, [&](int y1, int y2) {
    for (int y = y1; y < y2 && !exec.cancelled(); ++y)
      for (int x = r.x1; x < r.x2; ++x) dst.at(x, y) = src.at(x, y);
  });
}

void filterPass(ConstFloatPlaneView src, ConstFloatPlaneView guide,
                ConstFloatPlaneView boundary, FloatPlaneView dst, RectI r,
                double dx, double dy, double sigma, float protection,
                float softness, const ExecutionContext& exec) {
  if (sigma <= 1e-4) { copyPlane(src, dst, r, exec); return; }
  const int support = std::max(1, static_cast<int>(std::ceil(3.0 * sigma)));
  const double invTwoSigma2 = 0.5 / (sigma * sigma);
  exec.parallelRows(r.y1, r.y2, [&](int y1, int y2) {
    for (int y = y1; y < y2 && !exec.cancelled(); ++y) {
      for (int x = r.x1; x < r.x2; ++x) {
        const float centerGuide = guide.at(x, y);
        const float centerBoundary = boundary.at(x, y);
        double sum = 0.0, weightSum = 0.0;
        for (int i = -support; i <= support; ++i) {
          const double sx = x + dx * i;
          const double sy = y + dy * i;
          const double spatial = std::exp(-(i * i) * invTwoSigma2);
          const float sampleGuide = bilinear(guide, sx, sy);
          const float scale = std::max({std::abs(centerGuide), std::abs(sampleGuide), 1e-4f});
          const float delta = std::abs(sampleGuide - centerGuide) /
                              (std::max(softness, 1e-5f) * scale);
          const float bilateral = std::exp(-0.5f * delta * delta);
          const float edgeWeight = (1.0f - protection) + protection * bilateral;
          const float permeability = std::min(centerBoundary, bilinear(boundary, sx, sy));
          const double w = spatial * edgeWeight * permeability;
          sum += w * bilinear(src, sx, sy);
          weightSum += w;
        }
        dst.at(x, y) = weightSum > 1e-15 ? static_cast<float>(sum / weightSum) : src.at(x, y);
      }
    }
  });
}
}  // namespace

InputDomainRequest DirectionalGaussianOperator::requiredInputDomain(const ImageGeometry& g) const noexcept {
  const double angle = options_.angleDegrees * kPi / 180.0;
  const double rx = std::max(0.0, static_cast<double>(options_.radius * options_.xScale));
  const double ry = std::max(0.0, static_cast<double>(options_.radius * options_.yScale));
  const double pixelX = g.renderScaleX / std::max(g.pixelAspect, 1e-6);
  const double haloX = 3.0 * pixelX *
      (std::abs(rx * std::cos(angle)) + std::abs(ry * std::sin(angle)));
  const double haloY = 3.0 * g.renderScaleY *
      (std::abs(rx * std::sin(angle)) + std::abs(ry * std::cos(angle)));
  return {InputDomainKind::LocalHalo,
          static_cast<int>(std::ceil(haloX)) + 2,
          static_cast<int>(std::ceil(haloY)) + 2};
}

void DirectionalGaussianOperator::apply(const SpatialOperation& op,
                                        const ExecutionContext& exec) const {
  const RectI r = intersect(op.outputRegion, op.source.y.bounds);
  if (r.empty()) return;
  const double angle = options_.angleDegrees * kPi / 180.0;
  const double pixelX = op.geometry.renderScaleX / std::max(op.geometry.pixelAspect, 1e-6);
  const double ux1 = std::cos(angle) * pixelX;
  const double uy1 = std::sin(angle) * op.geometry.renderScaleY;
  const double ux2 = -std::sin(angle) * pixelX;
  const double uy2 = std::cos(angle) * op.geometry.renderScaleY;
  const double length1 = std::max(std::hypot(ux1, uy1), 1e-12);
  const double length2 = std::max(std::hypot(ux2, uy2), 1e-12);
  const double dx1 = ux1 / length1;
  const double dy1 = uy1 / length1;
  const double dx2 = ux2 / length2;
  const double dy2 = uy2 / length2;
  const double sigma1 = std::max(0.0, static_cast<double>(options_.radius * options_.xScale)) * length1;
  const double sigma2 = std::max(0.0, static_cast<double>(options_.radius * options_.yScale)) * length2;

  OwnedYabPlanes intermediate(r);
  const YabPlanes mid = intermediate.view();
  filterPass(op.source.y, op.source.y, op.boundary, mid.y, r, dx1, dy1, sigma1,
             options_.edgeProtection, options_.edgeSoftness, exec);
  filterPass(op.source.a, op.source.y, op.boundary, mid.a, r, dx1, dy1, sigma1,
             options_.edgeProtection, options_.edgeSoftness, exec);
  filterPass(op.source.b, op.source.y, op.boundary, mid.b, r, dx1, dy1, sigma1,
             options_.edgeProtection, options_.edgeSoftness, exec);
  filterPass({mid.y.data, mid.y.rowStride, mid.y.bounds}, op.source.y, op.boundary,
             op.destination.y, r, dx2, dy2, sigma2, options_.edgeProtection,
             options_.edgeSoftness, exec);
  filterPass({mid.a.data, mid.a.rowStride, mid.a.bounds}, op.source.y, op.boundary,
             op.destination.a, r, dx2, dy2, sigma2, options_.edgeProtection,
             options_.edgeSoftness, exec);
  filterPass({mid.b.data, mid.b.rowStride, mid.b.bounds}, op.source.y, op.boundary,
             op.destination.b, r, dx2, dy2, sigma2, options_.edgeProtection,
             options_.edgeSoftness, exec);
}

}  // namespace pigment
