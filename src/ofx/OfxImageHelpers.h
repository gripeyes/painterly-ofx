#pragma once
#include "core/Types.h"
#include "ofxsImageEffect.h"

namespace pigment::ofx {

ConstImageView makeConstImageView(const OFX::Image& image);
ImageView makeImageView(OFX::Image& image);
ConstFloatPlaneView makeMaskView(const OFX::Image& image);
RectI toRect(const OfxRectI& rect) noexcept;
OfxRectD expandedCanonical(const OfxRectD& rect, double haloX, double haloY) noexcept;

}  // namespace pigment::ofx

