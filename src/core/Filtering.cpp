#include "core/Filtering.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace pigment {
namespace {

float sample(ConstFloatPlaneView p, int x, int y) noexcept {
  x = std::max(p.bounds.x1, std::min(p.bounds.x2 - 1, x));
  y = std::max(p.bounds.y1, std::min(p.bounds.y2 - 1, y));
  return p.at(x, y);
}

std::vector<float> gaussianKernel(float sigma) {
  if (sigma <= 1e-4f) return {1.0f};
  const int radius = std::max(1, static_cast<int>(std::ceil(3.0f * sigma)));
  std::vector<float> kernel(static_cast<std::size_t>(radius * 2 + 1));
  double total = 0.0;
  for (int i = -radius; i <= radius; ++i) {
    const double value = std::exp(-0.5 * (i / static_cast<double>(sigma)) *
                                 (i / static_cast<double>(sigma)));
    kernel[static_cast<std::size_t>(i + radius)] = static_cast<float>(value);
    total += value;
  }
  for (float& value : kernel) value = static_cast<float>(value / total);
  return kernel;
}

void convolveHorizontal(ConstFloatPlaneView src, FloatPlaneView dst,
                        const std::vector<float>& kernel,
                        const ExecutionContext& exec) {
  const int radius = static_cast<int>(kernel.size() / 2);
  exec.parallelRows(dst.bounds.y1, dst.bounds.y2, [&](int y1, int y2) {
    for (int y = y1; y < y2 && !exec.cancelled(); ++y) {
      for (int x = dst.bounds.x1; x < dst.bounds.x2; ++x) {
        double sum = 0.0;
        for (int i = -radius; i <= radius; ++i)
          sum += kernel[static_cast<std::size_t>(i + radius)] * sample(src, x + i, y);
        dst.at(x, y) = static_cast<float>(sum);
      }
    }
  });
}

void convolveVertical(ConstFloatPlaneView src, FloatPlaneView dst,
                      const std::vector<float>& kernel,
                      const ExecutionContext& exec) {
  const int radius = static_cast<int>(kernel.size() / 2);
  exec.parallelRows(dst.bounds.y1, dst.bounds.y2, [&](int y1, int y2) {
    for (int y = y1; y < y2 && !exec.cancelled(); ++y) {
      for (int x = dst.bounds.x1; x < dst.bounds.x2; ++x) {
        double sum = 0.0;
        for (int i = -radius; i <= radius; ++i)
          sum += kernel[static_cast<std::size_t>(i + radius)] * sample(src, x, y + i);
        dst.at(x, y) = static_cast<float>(sum);
      }
    }
  });
}

}  // namespace

void copyPlane(ConstFloatPlaneView src, FloatPlaneView dst, RectI region,
               const ExecutionContext& exec) {
  region = intersect(region, intersect(src.bounds, dst.bounds));
  exec.parallelRows(region.y1, region.y2, [&](int y1, int y2) {
    for (int y = y1; y < y2 && !exec.cancelled(); ++y)
      for (int x = region.x1; x < region.x2; ++x) dst.at(x, y) = src.at(x, y);
  });
}

void gaussianBlurPlane(ConstFloatPlaneView src, FloatPlaneView dst,
                       float sigmaX, float sigmaY,
                       const ExecutionContext& exec) {
  const RectI bounds = intersect(src.bounds, dst.bounds);
  if (bounds.empty()) return;
  const auto horizontal = gaussianKernel(std::max(0.0f, sigmaX));
  const auto vertical = gaussianKernel(std::max(0.0f, sigmaY));
  OwnedPlane intermediate(bounds);
  convolveHorizontal(src, intermediate.view(), horizontal, exec);
  convolveVertical(static_cast<const OwnedPlane&>(intermediate).view(), dst, vertical, exec);
}

void gaussianBlurYab(ConstYabPlanes src, YabPlanes dst,
                     float sigmaX, float sigmaY,
                     const ExecutionContext& exec) {
  gaussianBlurPlane(src.y, dst.y, sigmaX, sigmaY, exec);
  gaussianBlurPlane(src.a, dst.a, sigmaX, sigmaY, exec);
  gaussianBlurPlane(src.b, dst.b, sigmaX, sigmaY, exec);
}

void boxBlurPlane(ConstFloatPlaneView src, FloatPlaneView dst,
                  int radiusX, int radiusY, const ExecutionContext& exec) {
  radiusX = std::max(0, radiusX);
  radiusY = std::max(0, radiusY);
  const RectI bounds = intersect(src.bounds, dst.bounds);
  if (bounds.empty()) return;
  OwnedPlane horizontal(bounds);
  auto temp = horizontal.view();
  exec.parallelRows(bounds.y1, bounds.y2, [&](int y1, int y2) {
    for (int y = y1; y < y2 && !exec.cancelled(); ++y) {
      double sum = 0.0;
      for (int i = -radiusX; i <= radiusX; ++i) sum += sample(src, bounds.x1 + i, y);
      for (int x = bounds.x1; x < bounds.x2; ++x) {
        temp.at(x, y) = static_cast<float>(sum / (2 * radiusX + 1));
        sum += sample(src, x + radiusX + 1, y) - sample(src, x - radiusX, y);
      }
    }
  });
  const auto tempConst = static_cast<const OwnedPlane&>(horizontal).view();
  exec.parallelRows(bounds.x1, bounds.x2, [&](int x1, int x2) {
    for (int x = x1; x < x2 && !exec.cancelled(); ++x) {
      double sum = 0.0;
      for (int i = -radiusY; i <= radiusY; ++i) sum += sample(tempConst, x, bounds.y1 + i);
      for (int y = bounds.y1; y < bounds.y2; ++y) {
        dst.at(x, y) = static_cast<float>(sum / (2 * radiusY + 1));
        sum += sample(tempConst, x, y + radiusY + 1) - sample(tempConst, x, y - radiusY);
      }
    }
  });
}

}  // namespace pigment
