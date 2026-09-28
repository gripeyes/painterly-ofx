#include "core/IntegratedPigment.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace pigment {
namespace {

float clamp01(float value) noexcept {
  return std::max(0.0f, std::min(1.0f, value));
}

float smooth01(float value) noexcept {
  value = clamp01(value);
  return value * value * (3.0f - 2.0f * value);
}

std::uint32_t hash(std::uint32_t x) noexcept {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  return x ^ (x >> 16);
}

float lattice(int x, int y, std::int32_t seed) noexcept {
  const auto h = hash(static_cast<std::uint32_t>(x) * 0x9e3779b9u ^
                      static_cast<std::uint32_t>(y) * 0x85ebca6bu ^
                      static_cast<std::uint32_t>(seed));
  return static_cast<float>(h & 0x00ffffffu) / 8388607.5f - 1.0f;
}

float valueNoise(float x, float y, std::int32_t seed) noexcept {
  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const float tx = smooth01(x - static_cast<float>(x0));
  const float ty = smooth01(y - static_cast<float>(y0));
  const float a = lattice(x0, y0, seed);
  const float b = lattice(x0 + 1, y0, seed);
  const float c = lattice(x0, y0 + 1, seed);
  const float d = lattice(x0 + 1, y0 + 1, seed);
  return (a + (b - a) * tx) * (1.0f - ty) +
         (c + (d - c) * tx) * ty;
}

}  // namespace

float painterlyVeilValue(float x, float y, float luminance,
                         const IntegratedPigmentParams& p) noexcept {
  const float scale = std::max(8.0f, p.veilScale);
  const float irregularity = clamp01(p.veilIrregularity);
  const float warp = valueNoise(x / (scale * 1.7f), y / (scale * 1.7f),
                                p.veilSeed + 97) * irregularity * 0.35f;
  const float nx = x / scale + warp;
  const float ny = y / scale - 0.7f * warp;
  float value = valueNoise(nx, ny, p.veilSeed);
  value += irregularity * 0.48f * valueNoise(nx * 2.03f, ny * 2.03f,
                                             p.veilSeed + 17);
  value += irregularity * irregularity * 0.22f *
      valueNoise(nx * 4.07f, ny * 4.07f, p.veilSeed + 41);
  value /= 1.0f + irregularity * 0.48f + irregularity * irregularity * 0.22f;
  value += p.veilTonalBias * (luminance / (1.0f + std::abs(luminance)));
  const float contrast = std::exp2(std::max(-2.0f, std::min(2.0f, p.veilContrast)));
  return clamp01(0.5f + 0.5f * std::tanh(value * contrast));
}

PigmentFieldValues derivePigmentFields(float veil, float mask,
                                       const IntegratedPigmentParams& p) noexcept {
  const float v = clamp01(veil);
  const float gate = clamp01(mask) * clamp01(p.amount);
  const float amount = clamp01(p.veilAmount);
  PigmentFieldValues result;
  result.massStrength = gate * ((1.0f - amount) + amount * smooth01(0.15f + 0.85f * v));
  result.boundaryExtinction = gate * clamp01(p.boundaryExtinction) *
      ((1.0f - amount) + amount * smooth01((v - 0.25f) / 0.75f));
  result.chromaMigration = gate * clamp01(p.chromaMigration) *
      ((1.0f - amount) + amount * smooth01(1.0f - std::abs(2.0f * v - 1.0f)));
  result.detailRetention = clamp01(1.0f - gate * amount * smooth01(v));
  return result;
}

void softRegionMassReference(ConstYabPlanes source, YabPlanes destination,
                             ScalarFieldView processingStrength,
                             ScalarFieldView boundaryProtection,
                             const IntegratedPigmentParams& p,
                             const ExecutionContext& execution) {
  const RectI bounds = source.y.bounds;
  const int width = bounds.width();
  const int height = bounds.height();
  const std::size_t count = static_cast<std::size_t>(width) * height;
  std::vector<float> y(count), a(count), b(count), px(count), py(count);
  std::vector<float> ny(count), na(count), nb(count), npx(count), npy(count);
  auto index = [&](int x, int yy) {
    return static_cast<std::size_t>(yy - bounds.y1) * width + (x - bounds.x1);
  };
  for (int yy = bounds.y1; yy < bounds.y2; ++yy) {
    for (int x = bounds.x1; x < bounds.x2; ++x) {
      const auto i = index(x, yy);
      y[i] = source.y.at(x, yy); a[i] = source.a.at(x, yy); b[i] = source.b.at(x, yy);
      px[i] = static_cast<float>(x); py[i] = static_cast<float>(yy);
    }
  }
  const float radius = std::max(0.25f, p.massScale);
  const float tone = std::max(1.0e-5f, p.toneSimilarity);
  const float chroma = std::max(1.0e-5f, p.chromaSimilarity);
  const float softness = std::max(0.1f, p.regionSoftness);
  for (int iteration = 0; iteration < 3; ++iteration) {
    if (execution.cancelled && execution.cancelled()) return;
    auto rows = [&](int begin, int end) {
      for (int yy = begin; yy < end; ++yy) {
        for (int x = bounds.x1; x < bounds.x2; ++x) {
          const auto i = index(x, yy);
          double sumW = 0.0, sumY = 0.0, sumA = 0.0, sumB = 0.0;
          double sumX = 0.0, sumPy = 0.0;
          for (int gy = -4; gy <= 4; ++gy) {
            for (int gx = -4; gx <= 4; ++gx) {
              const float sx = px[i] + radius * static_cast<float>(gx) / 4.0f;
              const float sy = py[i] + radius * static_cast<float>(gy) / 4.0f;
              const int ix = std::max(bounds.x1, std::min(bounds.x2 - 1,
                  static_cast<int>(std::lround(sx))));
              const int iy = std::max(bounds.y1, std::min(bounds.y2 - 1,
                  static_cast<int>(std::lround(sy))));
              const auto j = index(ix, iy);
              const float dx = (px[j] - px[i]) / radius;
              const float dy = (py[j] - py[i]) / radius;
              const float localYScale = std::max({0.05f, std::abs(y[i]) * 0.2f,
                                                   std::abs(y[j]) * 0.2f});
              const float dyab = (y[j] - y[i]) / (tone * localYScale);
              const float da = (a[j] - a[i]) / chroma;
              const float db = (b[j] - b[i]) / chroma;
              const float spatial = (dx * dx + dy * dy) / softness;
              const float feature = dyab * dyab + da * da + db * db;
              const float protection = std::max(boundaryProtection.at(x, yy),
                                                boundaryProtection.at(ix, iy));
              const double w = std::exp(-0.5 * (spatial + feature)) *
                  std::max(0.001f, 1.0f - clamp01(p.boundaryPreserve) * protection);
              sumW += w; sumY += w * y[j]; sumA += w * a[j]; sumB += w * b[j];
              sumX += w * px[j]; sumPy += w * py[j];
            }
          }
          const float invW = static_cast<float>(1.0 / std::max(1.0e-12, sumW));
          const float gain = clamp01(p.massStrength) * clamp01(processingStrength.at(x, yy));
          ny[i] = y[i] + gain * (static_cast<float>(sumY) * invW - y[i]);
          na[i] = a[i] + gain * (static_cast<float>(sumA) * invW - a[i]);
          nb[i] = b[i] + gain * (static_cast<float>(sumB) * invW - b[i]);
          npx[i] = px[i] + gain * (static_cast<float>(sumX) * invW - px[i]);
          npy[i] = py[i] + gain * (static_cast<float>(sumPy) * invW - py[i]);
        }
      }
    };
    if (execution.parallelRows) execution.parallelRows(bounds.y1, bounds.y2, rows);
    else rows(bounds.y1, bounds.y2);
    y.swap(ny); a.swap(na); b.swap(nb); px.swap(npx); py.swap(npy);
  }
  for (int yy = bounds.y1; yy < bounds.y2; ++yy) {
    for (int x = bounds.x1; x < bounds.x2; ++x) {
      const auto i = index(x, yy);
      destination.y.at(x, yy) = source.y.at(x, yy) + clamp01(p.lumaAttraction) *
          (y[i] - source.y.at(x, yy));
      destination.a.at(x, yy) = source.a.at(x, yy) + clamp01(p.chromaAttraction) *
          (a[i] - source.a.at(x, yy));
      destination.b.at(x, yy) = source.b.at(x, yy) + clamp01(p.chromaAttraction) *
          (b[i] - source.b.at(x, yy));
    }
  }
}

void representativeRegionMassReference(ConstYabPlanes source, YabPlanes destination,
                                       ScalarFieldView processingStrength,
                                       ScalarFieldView boundaryProtection,
                                       const IntegratedPigmentParams& p,
                                       const ExecutionContext& execution) {
  const RectI bounds = source.y.bounds;
  const int width = bounds.width();
  const int height = bounds.height();
  const std::size_t count = static_cast<std::size_t>(width) * height;
  std::vector<float> y(count), a(count), b(count), px(count), py(count);
  std::vector<float> ny(count), na(count), nb(count), npx(count), npy(count);
  auto index = [&](int x, int yy) {
    return static_cast<std::size_t>(yy - bounds.y1) * width + (x - bounds.x1);
  };
  for (int yy = bounds.y1; yy < bounds.y2; ++yy) for (int x = bounds.x1; x < bounds.x2; ++x) {
    const auto i = index(x, yy);
    y[i] = source.y.at(x, yy); a[i] = source.a.at(x, yy); b[i] = source.b.at(x, yy);
    px[i] = static_cast<float>(x); py[i] = static_cast<float>(yy);
  }
  const float radius = std::max(0.25f, p.massScale);
  const float tone = std::max(1.0e-5f, p.toneSimilarity);
  const float chroma = std::max(1.0e-5f, p.chromaSimilarity);
  const float softness = std::max(0.1f, p.regionSoftness);
  const float temperature = 0.6f * std::exp2(-4.0f * clamp01(p.modeSelectivity)) + 0.025f;

  struct Candidate { std::size_t sample = 0; float score = -1.0f; };
  for (int iteration = 0; iteration < 3; ++iteration) {
    if (execution.cancelled && execution.cancelled()) return;
    auto rows = [&](int begin, int end) {
      for (int yy = begin; yy < end; ++yy) for (int x = bounds.x1; x < bounds.x2; ++x) {
        const auto i = index(x, yy);
        std::size_t samples[81];
        int sampleCount = 0;
        for (int gy = -4; gy <= 4; ++gy) for (int gx = -4; gx <= 4; ++gx) {
          const int sx = std::max(bounds.x1, std::min(bounds.x2 - 1,
              static_cast<int>(std::lround(px[i] + radius * gx / 4.0f))));
          const int sy = std::max(bounds.y1, std::min(bounds.y2 - 1,
              static_cast<int>(std::lround(py[i] + radius * gy / 4.0f))));
          samples[sampleCount++] = index(sx, sy);
        }
        Candidate top[4];
        const float localYScale = std::max(0.05f, std::abs(y[i]) * 0.2f);
        for (int candidateIndex = 0; candidateIndex < sampleCount; ++candidateIndex) {
          const auto c = samples[candidateIndex];
          double density = 0.0;
          for (int neighborIndex = 0; neighborIndex < sampleCount; ++neighborIndex) {
            const auto n = samples[neighborIndex];
            const float dx = (px[n] - px[c]) / radius;
            const float dy = (py[n] - py[c]) / radius;
            const float ys = std::max({localYScale, std::abs(y[c]) * 0.2f,
                                       std::abs(y[n]) * 0.2f});
            const float yd = (y[n] - y[c]) / (tone * ys);
            const float ad = (a[n] - a[c]) / chroma;
            const float bd = (b[n] - b[c]) / chroma;
            density += std::exp(-0.5f * std::min(80.0f,
                (dx * dx + dy * dy) / softness + yd * yd + ad * ad + bd * bd));
          }
          const float dx = (px[c] - px[i]) / radius;
          const float dy = (py[c] - py[i]) / radius;
          const float yd = (y[c] - y[i]) / (tone * localYScale);
          const float ad = (a[c] - a[i]) / chroma;
          const float bd = (b[c] - b[i]) / chroma;
          const int cx = std::max(bounds.x1, std::min(bounds.x2 - 1,
              static_cast<int>(std::lround(px[c]))));
          const int cy = std::max(bounds.y1, std::min(bounds.y2 - 1,
              static_cast<int>(std::lround(py[c]))));
          const float protection = std::max(boundaryProtection.at(x, yy),
                                             boundaryProtection.at(cx, cy));
          const float eligibility = std::exp(-0.5f * std::min(80.0f,
              (dx * dx + dy * dy) / softness + yd * yd + ad * ad + bd * bd)) *
              std::max(0.001f, 1.0f - clamp01(p.boundaryPreserve) * protection);
          Candidate value{c, static_cast<float>(density / sampleCount) * eligibility};
          for (auto& entry : top) {
            if (value.score > entry.score) { std::swap(value, entry); }
          }
        }
        const float maximum = top[0].score;
        double total = 0.0, targetY = 0.0, targetA = 0.0, targetB = 0.0;
        double targetX = 0.0, targetPy = 0.0;
        for (const auto& candidate : top) {
          const double weight = std::exp((candidate.score - maximum) / temperature);
          total += weight; targetY += weight * y[candidate.sample];
          targetA += weight * a[candidate.sample]; targetB += weight * b[candidate.sample];
          targetX += weight * px[candidate.sample]; targetPy += weight * py[candidate.sample];
        }
        const float inverse = static_cast<float>(1.0 / std::max(1.0e-12, total));
        const float gain = clamp01(p.massStrength) * clamp01(processingStrength.at(x, yy));
        ny[i] = y[i] + gain * (static_cast<float>(targetY) * inverse - y[i]);
        na[i] = a[i] + gain * (static_cast<float>(targetA) * inverse - a[i]);
        nb[i] = b[i] + gain * (static_cast<float>(targetB) * inverse - b[i]);
        npx[i] = px[i] + gain * (static_cast<float>(targetX) * inverse - px[i]);
        npy[i] = py[i] + gain * (static_cast<float>(targetPy) * inverse - py[i]);
      }
    };
    if (execution.parallelRows) execution.parallelRows(bounds.y1, bounds.y2, rows);
    else rows(bounds.y1, bounds.y2);
    y.swap(ny); a.swap(na); b.swap(nb); px.swap(npx); py.swap(npy);
  }
  for (int yy = bounds.y1; yy < bounds.y2; ++yy) for (int x = bounds.x1; x < bounds.x2; ++x) {
    const auto i = index(x, yy);
    destination.y.at(x, yy) = source.y.at(x, yy) + clamp01(p.lumaAttraction) *
        (y[i] - source.y.at(x, yy));
    destination.a.at(x, yy) = source.a.at(x, yy) + clamp01(p.chromaAttraction) *
        (a[i] - source.a.at(x, yy));
    destination.b.at(x, yy) = source.b.at(x, yy) + clamp01(p.chromaAttraction) *
        (b[i] - source.b.at(x, yy));
  }
}

}  // namespace pigment
