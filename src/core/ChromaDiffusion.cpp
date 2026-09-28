#include "core/ChromaDiffusion.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace pigment {
namespace {
float clamp01(float x) { return std::max(0.0f, std::min(1.0f, x)); }

void copyRegion(ConstImageView src, ImageView dst, RectI region) {
  const std::size_t bytes = static_cast<std::size_t>(region.width()) *
                            static_cast<std::size_t>(src.components) * sizeof(float);
  for (int y = region.y1; y < region.y2; ++y)
    std::memcpy(dst.pixel(region.x1, y), src.pixel(region.x1, y), bytes);
}
}  // namespace

InputDomainRequest chromaDiffusionInputDomain(const ChromaDiffusionParams& p,
                                               const ImageGeometry& g) noexcept {
  return DirectionalGaussianOperator(p.diffusion).requiredInputDomain(g);
}

void processChromaDiffusion(ConstImageView src, ImageView dst, RectI output,
                            const ChromaDiffusionParams& p, const ImageGeometry& geometry,
                            const ConstFloatPlaneView* external,
                            const ExecutionContext& exec) {
  if (!src.data || !dst.data || (src.components != 3 && src.components != 4) ||
      src.components != dst.components)
    throw std::invalid_argument("ChromaDiffusion requires matching float RGB or RGBA views");
  output = intersect(output, intersect(src.bounds, dst.bounds));
  if (output.empty()) return;
  if (p.amount == 0.0f || p.mix == 0.0f) { copyRegion(src, dst, output); return; }

  MatrixOpponentTransform transform(p.gamut);
  OwnedYabPlanes original(src.bounds), filtered(src.bounds);
  OwnedPlane alpha(src.bounds, 1.0f), boundary(src.bounds, 1.0f), control(src.bounds, 1.0f);
  YabPlanes originalView = original.view();
  auto alphaView = alpha.view();

  exec.parallelRows(src.bounds.y1, src.bounds.y2, [&](int y1, int y2) {
    for (int y = y1; y < y2 && !exec.cancelled(); ++y) {
      for (int x = src.bounds.x1; x < src.bounds.x2; ++x) {
        const float* pixel = src.pixel(x, y);
        const float a = src.components == 4 ? pixel[3] : 1.0f;
        alphaView.at(x, y) = a;
        std::array<float, 3> rgb{pixel[0], pixel[1], pixel[2]};
        if (p.premultiplied) {
          if (std::abs(a) > 1e-6f) for (float& c : rgb) c /= a;
          else rgb = {0.0f, 0.0f, 0.0f};
        }
        const YabPixel yab = transform.toYab(rgb);
        originalView.y.at(x, y) = yab.y;
        originalView.a.at(x, y) = yab.a;
        originalView.b.at(x, y) = yab.b;
      }
    }
  });

  const ConstYabPlanes originalConst = asConst(originalView);
  buildBoundaryField(originalConst, boundary.view(),
                     {p.diffusion.edgeProtection, p.diffusion.edgeSoftness}, exec);
  const auto boundaryConst = static_cast<const OwnedPlane&>(boundary).view();
  composeControlField(originalConst.y, boundaryConst, external, p.invertMask,
                      p.tonalMask, control.view(), exec);

  DirectionalGaussianOperator op(p.diffusion);
  const auto controlConst = static_cast<const OwnedPlane&>(control).view();
  SpatialOperation operation{originalConst, filtered.view(), controlConst, boundaryConst,
                             src.bounds, geometry};
  op.apply(operation, exec);

  const ConstYabPlanes filteredView = static_cast<const OwnedYabPlanes&>(filtered).view();
  const ConstFloatPlaneView controlView = controlConst;
  const float chromaGain = std::exp2(p.saturationCompensationStops);
  const float amount = clamp01(p.amount);
  const float mix = clamp01(p.mix);
  const float preserve = clamp01(p.luminancePreservation);
  exec.parallelRows(output.y1, output.y2, [&](int y1, int y2) {
    for (int y = y1; y < y2 && !exec.cancelled(); ++y) {
      for (int x = output.x1; x < output.x2; ++x) {
        const float* in = src.pixel(x, y);
        float* out = dst.pixel(x, y);
        const float a = alphaView.at(x, y);
        if (p.premultiplied && std::abs(a) <= 1e-6f) {
          std::copy(in, in + src.components, out);
          continue;
        }
        const YabPixel base{originalConst.y.at(x,y), originalConst.a.at(x,y), originalConst.b.at(x,y)};
        const YabPixel candidate{
          preserve * base.y + (1.0f - preserve) * filteredView.y.at(x,y),
          filteredView.a.at(x,y) * chromaGain,
          filteredView.b.at(x,y) * chromaGain};
        const float strength = amount * controlView.at(x, y);
        const YabPixel effected{base.y + strength * (candidate.y - base.y),
                                base.a + strength * (candidate.a - base.a),
                                base.b + strength * (candidate.b - base.b)};
        auto rgb = transform.toRgb(effected);
        std::array<float,3> originalRgb{in[0], in[1], in[2]};
        if (p.premultiplied) {
          for (float& c : originalRgb) c /= a;
        }
        for (int c = 0; c < 3; ++c) {
          float value = originalRgb[c] + mix * (rgb[c] - originalRgb[c]);
          out[c] = p.premultiplied ? value * a : value;
        }
        if (src.components == 4) out[3] = in[3];
      }
    }
  });
}

}  // namespace pigment
