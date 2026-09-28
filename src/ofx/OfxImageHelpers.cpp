#include "ofx/OfxImageHelpers.h"

#include <stdexcept>

namespace pigment::ofx {

RectI toRect(const OfxRectI& r) noexcept { return {r.x1, r.y1, r.x2, r.y2}; }

ConstImageView makeConstImageView(const OFX::Image& image) {
  const auto b = toRect(image.getBounds());
  const auto* base = static_cast<const float*>(image.getPixelAddress(b.x1, b.y1));
  if (!base || image.getRowBytes() % static_cast<int>(sizeof(float)) != 0)
    throw std::runtime_error("invalid OpenFX float image storage");
  return {base, image.getRowBytes() / static_cast<std::ptrdiff_t>(sizeof(float)),
          b, image.getPixelComponentCount()};
}

ImageView makeImageView(OFX::Image& image) {
  const auto b = toRect(image.getBounds());
  auto* base = static_cast<float*>(image.getPixelAddress(b.x1, b.y1));
  if (!base || image.getRowBytes() % static_cast<int>(sizeof(float)) != 0)
    throw std::runtime_error("invalid OpenFX float image storage");
  return {base, image.getRowBytes() / static_cast<std::ptrdiff_t>(sizeof(float)),
          b, image.getPixelComponentCount()};
}

ConstFloatPlaneView makeMaskView(const OFX::Image& image) {
  if (image.getPixelComponentCount() != 1)
    throw std::runtime_error("mask clip must be alpha-only");
  const auto b = toRect(image.getBounds());
  const auto* base = static_cast<const float*>(image.getPixelAddress(b.x1, b.y1));
  if (!base || image.getRowBytes() % static_cast<int>(sizeof(float)) != 0)
    throw std::runtime_error("invalid OpenFX mask storage");
  return {base, image.getRowBytes() / static_cast<std::ptrdiff_t>(sizeof(float)), b};
}

OfxRectD expandedCanonical(const OfxRectD& r, double x, double y) noexcept {
  return {r.x1 - x, r.y1 - y, r.x2 + x, r.y2 + y};
}

}  // namespace pigment::ofx

