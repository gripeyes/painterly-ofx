#pragma once
#include "core/ColorSpace.h"
#include "core/DirectionalGaussian.h"
#include "core/Masking.h"

namespace pigment {

struct ChromaDiffusionParams {
  float amount = 0.0f;
  DirectionalGaussianOptions diffusion{};
  float luminancePreservation = 1.0f;
  float saturationCompensationStops = 0.0f;
  TonalMaskOptions tonalMask{};
  float mix = 1.0f;
  bool invertMask = false;
  bool premultiplied = false;
  WorkingGamut gamut = WorkingGamut::ACEScg;
};

InputDomainRequest chromaDiffusionInputDomain(const ChromaDiffusionParams& params,
                                               const ImageGeometry& geometry) noexcept;

void processChromaDiffusion(ConstImageView source, ImageView destination,
                            RectI outputRegion, const ChromaDiffusionParams& params,
                            const ImageGeometry& geometry,
                            const ConstFloatPlaneView* externalMask,
                            const ExecutionContext& execution = {});

}  // namespace pigment

