#include "core/PigmentPhase4.h"

#include "core/ColorSpace.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace pigment {
namespace {
float clamp01(float v) noexcept { return std::max(0.0f, std::min(1.0f, v)); }
float maskValue(const ConstImageView *mask, int x, int y, bool invert) {
  float v = mask ? clamp01(mask->pixel(x, y)[0]) : 1.0f;
  return invert ? 1.0f - v : v;
}
YabPixel value(ConstYabPlanes p, int x, int y) {
  return {p.y.at(x, y), p.a.at(x, y), p.b.at(x, y)};
}
YabPixel plateValue(const PublicPlateSet &p, int i, int x, int y) {
  auto a = p.appearance(i);
  return {a.y.at(x, y), a.a.at(x, y), a.b.at(x, y)};
}
YabPixel add(YabPixel a, YabPixel b) {
  return {a.y + b.y, a.a + b.a, a.b + b.b};
}
YabPixel mul(YabPixel a, float s) { return {a.y * s, a.a * s, a.b * s}; }
YabPixel mix(YabPixel a, YabPixel b, float t) {
  return {a.y + (b.y - a.y) * t, a.a + (b.a - a.a) * t, a.b + (b.b - a.b) * t};
}
} // namespace

Phase4RenderDiagnostics
processPigmentPhase4(const Phase4RenderInputs &in,
                     const ExecutionContext &execution) {
  const RectI b = in.source.bounds;
  const auto &p = in.params;
  MatrixOpponentTransform transform(p.gamut);
  if (p.debugView == PigmentDebugView::Final &&
      (p.amount == 0.0f || p.mix == 0.0f)) {
    execution.parallelRows(
        in.renderWindow.y1, in.renderWindow.y2, [&](int y0, int y1) {
          for (int y = y0; y < y1; ++y)
            for (int x = in.renderWindow.x1; x < in.renderWindow.x2; ++x) {
              const float *s = in.source.pixel(x, y);
              float *d = in.destination.pixel(x, y);
              for (int c = 0; c < in.destination.components; ++c)
                d[c] = s[c];
            }
        });
    return {};
  }
  OwnedYabPlanes original(b);
  OwnedPlane alpha(b, 1.0f);
  auto ov = original.view();
  auto av = alpha.view();
  for (int y = b.y1; y < b.y2; ++y)
    for (int x = b.x1; x < b.x2; ++x) {
      const float *s = in.source.pixel(x, y);
      float a = in.source.components == 4 ? s[3] : 1.0f;
      av.at(x, y) = a;
      std::array<float, 3> rgb{s[0], s[1], s[2]};
      if (p.premultiplied && std::abs(a) > 1e-6f)
        for (float &v : rgb)
          v /= a;
      YabPixel q = transform.toYab(rgb);
      ov.y.at(x, y) = q.y;
      ov.a.at(x, y) = q.a;
      ov.b.at(x, y) = q.b;
    }
  auto automatic = buildPhase4AutomaticPlates(
      static_cast<const OwnedYabPlanes &>(original).view(), p.phase4,
      in.geometry, execution);
  const int plateCount = automatic.plates.count(),
            latentCount = automatic.latent.count();
  execution.parallelRows(
      in.renderWindow.y1, in.renderWindow.y2, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y)
          for (int x = in.renderWindow.x1; x < in.renderWindow.x2; ++x) {
            const float *sp = in.source.pixel(x, y);
            float *dp = in.destination.pixel(x, y);
            const float sourceAlpha = av.at(x, y);
            YabPixel source = value(
                         static_cast<const OwnedYabPlanes &>(original).view(),
                         x, y),
                     out{};
            float weightSum = 0;
            for (int i = 0; i < plateCount; ++i) {
              const auto &control = p.phase4.plates[i];
              if (!control.enabled)
                continue;
              const float w = std::max(0.0f, control.weight) *
                              automatic.plates.alpha(i).at(x, y);
              YabPixel q = plateValue(automatic.plates, i, x, y);
              q.y += control.tone;
              q.a += control.biasA;
              q.b += control.biasB;
              out = add(out, mul(q, w));
              weightSum += w;
            }
            if (weightSum > 1e-8f)
              out = mul(out, 1.0f / weightSum);
            else
              out = source;
            bool gray = false, composite = false;
            switch (p.debugView) {
            case PigmentDebugView::Phase4LatentComponent: {
              int i =
                  std::max(0, std::min(latentCount - 1, p.phase4.debugLatent));
              float v = automatic.latent.alpha(i).at(x, y);
              out = {v, 0, 0};
              gray = true;
              break;
            }
            case PigmentDebugView::Phase4LatentComposite: {
              float r = 0, g = 0, bb = 0;
              for (int i = 0; i < latentCount; ++i) {
                float v = automatic.latent.alpha(i).at(x, y);
                r += v * float((i * 97 + 29) % 255) / 255.0f;
                g += v * float((i * 57 + 83) % 255) / 255.0f;
                bb += v * float((i * 131 + 17) % 255) / 255.0f;
              }
              out = {r, g, bb};
              composite = true;
              break;
            }
            case PigmentDebugView::Phase4LatentReconstructionError: {
              float v = automatic.latent.reconstructionError().at(x, y);
              out = {v, 0, 0};
              gray = true;
              break;
            }
            case PigmentDebugView::Phase4SpectralResidual: {
              int i =
                  std::max(0, std::min(automatic.latent.spectralModeCount() - 1,
                                       p.phase4.debugLatent));
              float v = .5f + .5f * automatic.latent.spectralMode(i).at(x, y);
              out = {v, 0, 0};
              gray = true;
              break;
            }
            case PigmentDebugView::Phase4ComponentRecoveryError: {
              float v = automatic.latent.recoveryError().at(x, y);
              out = {v, 0, 0};
              gray = true;
              break;
            }
            case PigmentDebugView::Phase4AppearanceUnmixingError: {
              float v = automatic.latent.reconstructionError().at(x, y);
              out = {v, 0, 0};
              gray = true;
              break;
            }
            case PigmentDebugView::Phase4PlateAlpha: {
              int i =
                  std::max(0, std::min(plateCount - 1, p.phase4.debugPlate));
              float v = automatic.plates.alpha(i).at(x, y);
              out = {v, 0, 0};
              gray = true;
              break;
            }
            case PigmentDebugView::Phase4PlateYSupport: {
              int i =
                  std::max(0, std::min(plateCount - 1, p.phase4.debugPlate));
              float v = automatic.plates.supportY(i).at(x, y);
              out = {v, 0, 0};
              gray = true;
              break;
            }
            case PigmentDebugView::Phase4PlateABSupport: {
              int i =
                  std::max(0, std::min(plateCount - 1, p.phase4.debugPlate));
              float v = automatic.plates.supportAB(i).at(x, y);
              out = {v, 0, 0};
              gray = true;
              break;
            }
            case PigmentDebugView::Phase4PlateYAppearance: {
              int i =
                  std::max(0, std::min(plateCount - 1, p.phase4.debugPlate));
              out = plateValue(automatic.plates, i, x, y);
              out.a = source.a;
              out.b = source.b;
              break;
            }
            case PigmentDebugView::Phase4PlateABAppearance: {
              int i =
                  std::max(0, std::min(plateCount - 1, p.phase4.debugPlate));
              out = plateValue(automatic.plates, i, x, y);
              out.y = source.y;
              break;
            }
            case PigmentDebugView::Phase4PlateOverlapComposite: {
              float r = 0, g = 0, bb = 0;
              for (int i = 0; i < plateCount; ++i) {
                float v = automatic.plates.alpha(i).at(x, y);
                r += v * float((i * 97 + 29) % 255) / 255.0f;
                g += v * float((i * 57 + 83) % 255) / 255.0f;
                bb += v * float((i * 131 + 17) % 255) / 255.0f;
              }
              out = {r, g, bb};
              composite = true;
              break;
            }
            case PigmentDebugView::Phase4DifferenceFromSource:
              out = {.5f + .25f * (out.y - source.y), .25f * (out.a - source.a),
                     .25f * (out.b - source.b)};
              break;
            default:
              break;
            }
            std::array<float, 3> rgb;
            if (gray)
              rgb = {out.y, out.y, out.y};
            else if (composite)
              rgb = {out.y, out.a, out.b};
            else
              rgb = transform.toRgb(out);
            if (p.debugView == PigmentDebugView::Final) {
              float gate =
                  clamp01(p.amount) * maskValue(in.mask, x, y, p.invertMask);
              rgb = transform.toRgb(mix(source, out, gate));
            }
            const float finalMix =
                p.debugView == PigmentDebugView::Final ? clamp01(p.mix) : 1.0f;
            for (int c = 0; c < 3; ++c) {
              float originalStraight =
                  (p.premultiplied && std::abs(sourceAlpha) > 1e-6f)
                      ? sp[c] / sourceAlpha
                      : sp[c];
              float v =
                  originalStraight + (rgb[c] - originalStraight) * finalMix;
              if (p.premultiplied && std::abs(sourceAlpha) > 1e-6f)
                v *= sourceAlpha;
              if (p.premultiplied && std::abs(sourceAlpha) <= 1e-6f)
                v = sp[c];
              dp[c] = v;
            }
            if (in.destination.components == 4)
              dp[3] = sourceAlpha;
          }
      });
  return {automatic.diagnostics};
}

} // namespace pigment
