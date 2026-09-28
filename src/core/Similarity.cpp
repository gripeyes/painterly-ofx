#include "core/Similarity.h"
#include <algorithm>
#include <cmath>

namespace pigment {

float yabDistanceSquared(const YabPixel& x, const YabPixel& y,
                         const SimilarityWeights& w) noexcept {
  const float ls = std::max(std::abs(w.luminanceScale), 1e-12f);
  const float cs = std::max(std::abs(w.chromaScale), 1e-12f);
  const float dy = (x.y - y.y) / ls;
  const float da = (x.a - y.a) / cs;
  const float db = (x.b - y.b) / cs;
  return std::max(0.0f, w.luminance) * dy * dy +
         std::max(0.0f, w.axisA) * da * da +
         std::max(0.0f, w.axisB) * db * db;
}

float gaussianSimilarity(const YabPixel& x, const YabPixel& y,
                         const SimilarityWeights& w) noexcept {
  return std::exp(-0.5f * yabDistanceSquared(x, y, w));
}

}  // namespace pigment

