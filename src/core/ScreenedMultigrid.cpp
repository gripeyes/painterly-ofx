#include "core/ScreenedMultigrid.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace pigment {
namespace {

struct Level {
  int width = 0, height = 0;
  float lambdaX = 1.0f, lambdaY = 1.0f;
  std::vector<float> source, confidence, conductance, rhs, value, temporary, residual;

  explicit Level(int w = 0, int h = 0) : width(w), height(h) {
    const auto n = static_cast<std::size_t>(w) * h;
    source.resize(n); confidence.resize(n); conductance.resize(n);
    rhs.resize(n); value.resize(n); temporary.resize(n); residual.resize(n);
  }
  int index(int x, int y) const noexcept { return y * width + x; }
};

float edge(const Level& l, int a, int b) noexcept {
  return std::max(1.0e-5f, std::min(l.conductance[a], l.conductance[b]));
}

float applyAt(const Level& l, const std::vector<float>& x, int px, int py) noexcept {
  const int i = l.index(px, py);
  const int xl = std::max(0, px - 1), xr = std::min(l.width - 1, px + 1);
  const int yd = std::max(0, py - 1), yu = std::min(l.height - 1, py + 1);
  const int il = l.index(xl, py), ir = l.index(xr, py);
  const int id = l.index(px, yd), iu = l.index(px, yu);
  const float gl = edge(l, i, il), gr = edge(l, i, ir);
  const float gd = edge(l, i, id), gu = edge(l, i, iu);
  const float diagonal = l.confidence[i] + l.lambdaX * (gl + gr) +
                         l.lambdaY * (gd + gu);
  return diagonal * x[i] - l.lambdaX * (gl * x[il] + gr * x[ir]) -
         l.lambdaY * (gd * x[id] + gu * x[iu]);
}

void relax(Level& l, int iterations, float omega, const ExecutionContext& execution) {
  for (int iteration = 0; iteration < iterations; ++iteration) {
    if (execution.cancelled()) return;
    for (int y = 0; y < l.height; ++y) for (int x = 0; x < l.width; ++x) {
      const int i = l.index(x, y);
      const int xl = std::max(0, x - 1), xr = std::min(l.width - 1, x + 1);
      const int yd = std::max(0, y - 1), yu = std::min(l.height - 1, y + 1);
      const int il = l.index(xl, y), ir = l.index(xr, y);
      const int id = l.index(x, yd), iu = l.index(x, yu);
      const float gl = edge(l, i, il), gr = edge(l, i, ir);
      const float gd = edge(l, i, id), gu = edge(l, i, iu);
      const float diagonal = std::max(1.0e-8f, l.confidence[i] +
          l.lambdaX * (gl + gr) + l.lambdaY * (gd + gu));
      const float jacobi = (l.rhs[i] + l.lambdaX * (gl * l.value[il] + gr * l.value[ir]) +
                            l.lambdaY * (gd * l.value[id] + gu * l.value[iu])) / diagonal;
      l.temporary[i] = l.value[i] + omega * (jacobi - l.value[i]);
    }
    l.value.swap(l.temporary);
  }
}

void residual(Level& l) {
  for (int y = 0; y < l.height; ++y) for (int x = 0; x < l.width; ++x) {
    const int i = l.index(x, y);
    l.residual[i] = l.rhs[i] - applyAt(l, l.value, x, y);
  }
}

float sampleClamped(const std::vector<float>& values, int w, int h, int x, int y) {
  x = std::max(0, std::min(w - 1, x)); y = std::max(0, std::min(h - 1, y));
  return values[static_cast<std::size_t>(y) * w + x];
}

void restrictLevel(Level& fine, Level& coarse) {
  residual(fine);
  static constexpr float weights[3] = {1.0f, 2.0f, 1.0f};
  for (int y = 0; y < coarse.height; ++y) for (int x = 0; x < coarse.width; ++x) {
    float r = 0.0f, c = 0.0f, g = 0.0f, f = 0.0f, sum = 0.0f;
    for (int oy = -1; oy <= 1; ++oy) for (int ox = -1; ox <= 1; ++ox) {
      const float w = weights[ox + 1] * weights[oy + 1];
      const int fx = 2 * x + ox, fy = 2 * y + oy;
      r += w * sampleClamped(fine.residual, fine.width, fine.height, fx, fy);
      c += w * sampleClamped(fine.confidence, fine.width, fine.height, fx, fy);
      g += w * sampleClamped(fine.conductance, fine.width, fine.height, fx, fy);
      f += w * sampleClamped(fine.source, fine.width, fine.height, fx, fy);
      sum += w;
    }
    const int i = coarse.index(x, y);
    coarse.rhs[i] = r / sum;
    coarse.confidence[i] = std::max(1.0e-6f, c / sum);
    coarse.conductance[i] = std::max(1.0e-5f, g / sum);
    coarse.source[i] = f / sum;
    coarse.value[i] = 0.0f;
  }
}

void prolongateAdd(const Level& coarse, Level& fine) {
  for (int y = 0; y < fine.height; ++y) for (int x = 0; x < fine.width; ++x) {
    const float cx = 0.5f * x, cy = 0.5f * y;
    const int x0 = static_cast<int>(std::floor(cx)), y0 = static_cast<int>(std::floor(cy));
    const float tx = cx - x0, ty = cy - y0;
    const float a = sampleClamped(coarse.value, coarse.width, coarse.height, x0, y0);
    const float b = sampleClamped(coarse.value, coarse.width, coarse.height, x0 + 1, y0);
    const float c = sampleClamped(coarse.value, coarse.width, coarse.height, x0, y0 + 1);
    const float d = sampleClamped(coarse.value, coarse.width, coarse.height, x0 + 1, y0 + 1);
    fine.value[fine.index(x, y)] +=
        (1.0f - ty) * ((1.0f - tx) * a + tx * b) + ty * ((1.0f - tx) * c + tx * d);
  }
}

void vCycle(std::vector<Level>& levels, int levelIndex,
            const ScreenedMultigridParams& p, const ExecutionContext& execution) {
  Level& level = levels[levelIndex];
  if (levelIndex + 1 == static_cast<int>(levels.size())) {
    relax(level, p.coarseRelaxations, p.relaxation, execution);
    return;
  }
  relax(level, p.preRelaxations, p.relaxation, execution);
  Level& coarse = levels[levelIndex + 1];
  restrictLevel(level, coarse);
  vCycle(levels, levelIndex + 1, p, execution);
  prolongateAdd(coarse, level);
  relax(level, p.postRelaxations, p.relaxation, execution);
}

}  // namespace

void solveScreenedMultigrid(ScalarFieldView source, ScalarFieldView confidence,
                            ScalarFieldView conductance, FloatPlaneView destination,
                            const ScreenedMultigridParams& params,
                            const ExecutionContext& execution) {
  const RectI bounds = destination.bounds;
  if (bounds.empty()) return;
  std::vector<Level> levels;
  int width = bounds.width(), height = bounds.height();
  for (int level = 0; level < std::max(1, params.maximumLevels); ++level) {
    levels.emplace_back(width, height);
    if (std::min(width, height) <= params.minimumCoarseExtent) break;
    width = std::max(1, (width + 1) / 2); height = std::max(1, (height + 1) / 2);
  }
  levels[0].lambdaX = std::max(0.0f, params.lambdaX);
  levels[0].lambdaY = std::max(0.0f, params.lambdaY);
  for (std::size_t i = 1; i < levels.size(); ++i) {
    levels[i].lambdaX = levels[i - 1].lambdaX * 0.25f;
    levels[i].lambdaY = levels[i - 1].lambdaY * 0.25f;
  }
  Level& finest = levels[0];
  for (int y = 0; y < finest.height; ++y) for (int x = 0; x < finest.width; ++x) {
    const int px = bounds.x1 + x, py = bounds.y1 + y, i = finest.index(x, y);
    finest.source[i] = source.at(px, py);
    finest.confidence[i] = std::max(1.0e-6f, confidence.at(px, py));
    finest.conductance[i] = std::max(1.0e-5f, conductance.at(px, py));
    finest.rhs[i] = finest.confidence[i] * finest.source[i];
    finest.value[i] = finest.source[i];
  }
  // Initialize coarse coefficients before the first residual-correction cycle.
  for (std::size_t li = 1; li < levels.size(); ++li) {
    Level& f = levels[li - 1]; Level& c = levels[li];
    for (int y = 0; y < c.height; ++y) for (int x = 0; x < c.width; ++x) {
      const int i = c.index(x, y);
      c.confidence[i] = sampleClamped(f.confidence, f.width, f.height, 2 * x, 2 * y);
      c.conductance[i] = sampleClamped(f.conductance, f.width, f.height, 2 * x, 2 * y);
    }
  }
  for (int cycle = 0; cycle < params.vCycles && !execution.cancelled(); ++cycle)
    vCycle(levels, 0, params, execution);
  for (int y = 0; y < finest.height; ++y) for (int x = 0; x < finest.width; ++x)
    destination.at(bounds.x1 + x, bounds.y1 + y) = finest.value[finest.index(x, y)];
}

float screenedResidualRms(ConstFloatPlaneView solution, ScalarFieldView source,
                          ScalarFieldView confidence, ScalarFieldView conductance,
                          float lambdaX, float lambdaY) noexcept {
  const RectI b = solution.bounds;
  double sum = 0.0; std::size_t count = 0;
  for (int y = b.y1; y < b.y2; ++y) for (int x = b.x1; x < b.x2; ++x) {
    const int xl = std::max(b.x1, x - 1), xr = std::min(b.x2 - 1, x + 1);
    const int yd = std::max(b.y1, y - 1), yu = std::min(b.y2 - 1, y + 1);
    const float center = solution.at(x, y), c = std::max(1.0e-6f, confidence.at(x, y));
    auto g = [&](int xx, int yy) { return std::max(1.0e-5f,
        std::min(conductance.at(x, y), conductance.at(xx, yy))); };
    const float residual = c * (source.at(x, y) - center) +
        lambdaX * (g(xl, y) * (solution.at(xl, y) - center) +
                   g(xr, y) * (solution.at(xr, y) - center)) +
        lambdaY * (g(x, yd) * (solution.at(x, yd) - center) +
                   g(x, yu) * (solution.at(x, yu) - center));
    sum += static_cast<double>(residual) * residual; ++count;
  }
  return count ? static_cast<float>(std::sqrt(sum / count)) : 0.0f;
}

}  // namespace pigment
