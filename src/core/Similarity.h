#pragma once
#include "core/Types.h"

namespace pigment {

struct SimilarityWeights {
  float luminance = 1.0f;
  float axisA = 1.0f;
  float axisB = 1.0f;
  float luminanceScale = 1.0f;
  float chromaScale = 1.0f;
};

float yabDistanceSquared(const YabPixel& lhs, const YabPixel& rhs,
                         const SimilarityWeights& weights) noexcept;
float gaussianSimilarity(const YabPixel& lhs, const YabPixel& rhs,
                         const SimilarityWeights& weights) noexcept;

}  // namespace pigment

