#pragma once

#include "core/Execution.h"
#include "core/Phase4Types.h"
#include "core/Types.h"

#include <vector>

namespace pigment {

struct SpectralMattingBasis {
  int width = 0;
  int height = 0;
  int count = 0;
  std::vector<float> values;       // column-major: mode * (width*height) + pixel
  std::vector<float> eigenvalues;
  std::vector<float> residuals;
  float maximumAbsoluteCorrelation = 0.0f;
};

// Construct the exact 3x3-window closed-form matting Laplacian and solve for
// its smallest eigenvectors.  The input is an unclipped linear YAB image; a
// robust invertible channel normalization is used only to condition the local
// covariance calculations.
SpectralMattingBasis buildSpectralMattingBasis(
    const std::vector<YabPixel>& image, int width, int height, int modeCount,
    const ExecutionContext& execution = {},
    const SparseAffinityGraph* informationFlow = nullptr,
    float informationFlowWeight = 0.0f);

}  // namespace pigment
