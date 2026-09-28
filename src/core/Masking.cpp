#include "core/Masking.h"
#include <algorithm>
#include <cmath>

namespace pigment {
namespace {
float clamp01(float x) noexcept { return std::max(0.0f, std::min(1.0f, x)); }
float smoothstep(float a, float b, float x) noexcept {
  if (a == b) return x >= b ? 1.0f : 0.0f;
  const float t = clamp01((x - a) / (b - a));
  return t * t * (3.0f - 2.0f * t);
}
float sample(ConstFloatPlaneView p, int x, int y) noexcept {
  x = std::max(p.bounds.x1, std::min(p.bounds.x2 - 1, x));
  y = std::max(p.bounds.y1, std::min(p.bounds.y2 - 1, y));
  return p.at(x, y);
}
}  // namespace

void buildBoundaryField(ConstYabPlanes src, FloatPlaneView dst,
                        const BoundaryOptions& o,
                        const ExecutionContext& exec) {
  exec.parallelRows(dst.bounds.y1, dst.bounds.y2, [&](int y1, int y2) {
    for (int y = y1; y < y2 && !exec.cancelled(); ++y) {
      for (int x = dst.bounds.x1; x < dst.bounds.x2; ++x) {
        const float gx = 0.5f * (sample(src.y, x + 1, y) - sample(src.y, x - 1, y));
        const float gy = 0.5f * (sample(src.y, x, y + 1) - sample(src.y, x, y - 1));
        const float local = std::max(std::abs(sample(src.y, x, y)), 1e-4f);
        const float relativeGradient = std::sqrt(gx * gx + gy * gy) / local;
        const float edge = smoothstep(0.0f, std::max(o.softness, 1e-5f), relativeGradient);
        dst.at(x, y) = 1.0f - clamp01(o.protection) * edge;
      }
    }
  });
}

float tonalWeight(float y, const TonalMaskOptions& o) noexcept {
  float range = 1.0f;
  if (o.rangeEnabled) {
    const float s = std::max(0.0f, o.rangeSoftness);
    range = smoothstep(o.rangeMinimum - s, o.rangeMinimum + s, y) *
            (1.0f - smoothstep(o.rangeMaximum - s, o.rangeMaximum + s, y));
  }
  const float shadow = 1.0f - smoothstep(0.05f, 0.25f, y);
  const float highlight = smoothstep(0.5f, 2.0f, y);
  const float midtone = clamp01(1.0f - std::max(shadow, highlight));
  const float biased = 1.0f + o.shadowBias * shadow + o.midtoneBias * midtone +
                       o.highlightBias * highlight;
  return range * clamp01(biased);
}

void composeControlField(ConstFloatPlaneView yPlane, ConstFloatPlaneView boundary,
                         const ConstFloatPlaneView* external, bool invert,
                         const TonalMaskOptions& options, FloatPlaneView dst,
                         const ExecutionContext& exec) {
  exec.parallelRows(dst.bounds.y1, dst.bounds.y2, [&](int y1, int y2) {
    for (int y = y1; y < y2 && !exec.cancelled(); ++y) {
      for (int x = dst.bounds.x1; x < dst.bounds.x2; ++x) {
        float mask = 1.0f;
        if (external && external->bounds.contains(x, y)) {
          mask = clamp01(external->at(x, y));
          if (invert) mask = 1.0f - mask;
        } else if (external) {
          mask = invert ? 1.0f : 0.0f;
        }
        dst.at(x, y) = tonalWeight(yPlane.at(x, y), options) *
                       clamp01(boundary.at(x, y)) * mask;
      }
    }
  });
}

}  // namespace pigment

