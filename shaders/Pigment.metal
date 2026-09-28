#include <metal_stdlib>
using namespace metal;

struct ImageLayout {
  uint width;
  uint height;
  uint rowFloats;
  uint components;
  uint startFloat;
};

struct Params {
  uint width;
  uint height;
  uint sourceComponents;
  uint destinationComponents;
  uint hasMask;
  uint maskComponents;
  uint premultiplied;
  uint invertMask;
  uint rangeEnabled;
  uint debugView;
  float amount;
  float massStrength;
  float toneSimilarity;
  float chromaSimilarity;
  float boundaryPreserve;
  float boundarySoftness;
  float structurePreserve;
  float internalVariation;
  float lumaMassing;
  float chromaMassing;
  float mix;
  float shadowBias;
  float midtoneBias;
  float highlightBias;
  float rangeMinimum;
  float rangeMaximum;
  float rangeSoftness;
  float whiteX;
  float whiteZ;
  float rgbToXyz[9];
  float xyzToRgb[9];
};

float clamp01(float v) { return clamp(v, 0.0f, 1.0f); }
float smoother(float a, float b, float x) {
  if (a == b) return x >= b ? 1.0f : 0.0f;
  float t = clamp01((x - a) / (b - a));
  return t * t * (3.0f - 2.0f * t);
}

float3 rgbToYab(float3 rgb, constant Params& p) {
  float x = p.rgbToXyz[0] * rgb.x + p.rgbToXyz[1] * rgb.y + p.rgbToXyz[2] * rgb.z;
  float y = p.rgbToXyz[3] * rgb.x + p.rgbToXyz[4] * rgb.y + p.rgbToXyz[5] * rgb.z;
  float z = p.rgbToXyz[6] * rgb.x + p.rgbToXyz[7] * rgb.y + p.rgbToXyz[8] * rgb.z;
  return float3(y, x / p.whiteX - y, z / p.whiteZ - y);
}

float3 yabToRgb(float3 yab, constant Params& p) {
  float3 xyz = float3((yab.y + yab.x) * p.whiteX, yab.x,
                      (yab.z + yab.x) * p.whiteZ);
  return float3(p.xyzToRgb[0] * xyz.x + p.xyzToRgb[1] * xyz.y + p.xyzToRgb[2] * xyz.z,
                p.xyzToRgb[3] * xyz.x + p.xyzToRgb[4] * xyz.y + p.xyzToRgb[5] * xyz.z,
                p.xyzToRgb[6] * xyz.x + p.xyzToRgb[7] * xyz.y + p.xyzToRgb[8] * xyz.z);
}

float readComponent(const device float* buffer, constant ImageLayout& layout,
                    uint2 gid, uint component) {
  return buffer[layout.startFloat + gid.y * layout.rowFloats +
                gid.x * layout.components + component];
}

kernel void pigment_rgb_to_yab(const device float* source [[buffer(0)]],
                               constant ImageLayout& layout [[buffer(1)]],
                               constant Params& p [[buffer(2)]],
                               texture2d<float, access::write> output [[texture(0)]],
                               uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  float alpha = layout.components == 4 ? readComponent(source, layout, gid, 3) : 1.0f;
  float3 rgb = float3(readComponent(source, layout, gid, 0),
                      readComponent(source, layout, gid, 1),
                      readComponent(source, layout, gid, 2));
  if (p.premultiplied != 0) rgb = abs(alpha) > 1.0e-6f ? rgb / alpha : float3(0.0f);
  output.write(float4(rgbToYab(rgb, p), alpha), gid);
}

kernel void pigment_processing_strength(
    texture2d<float, access::read> original [[texture(0)]],
    const device float* mask [[buffer(0)]],
    constant ImageLayout& maskLayout [[buffer(1)]],
    constant Params& p [[buffer(2)]],
    texture2d<float, access::write> strength [[texture(1)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  float y = original.read(gid).x;
  float range = 1.0f;
  if (p.rangeEnabled != 0) {
    float s = max(0.0f, p.rangeSoftness);
    range = smoother(p.rangeMinimum - s, p.rangeMinimum + s, y) *
            (1.0f - smoother(p.rangeMaximum - s, p.rangeMaximum + s, y));
  }
  float shadow = 1.0f - smoother(0.05f, 0.25f, y);
  float highlight = smoother(0.5f, 2.0f, y);
  float midtone = clamp01(1.0f - max(shadow, highlight));
  float tonal = range * clamp01(1.0f + p.shadowBias * shadow +
      p.midtoneBias * midtone + p.highlightBias * highlight);
  float m = 1.0f;
  if (p.hasMask != 0) {
    m = clamp01(readComponent(mask, maskLayout, gid, 0));
    if (p.invertMask != 0) m = 1.0f - m;
  }
  strength.write(float4(tonal * m * clamp01(p.amount)), gid);
}

kernel void pigment_structure_combine(
    texture2d<float, access::read> original [[texture(0)]],
    texture2d<float, access::read> blurred [[texture(1)]],
    constant Params& p [[buffer(0)]],
    texture2d<float, access::write> guide [[texture(2)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  float4 o = original.read(gid);
  guide.write(mix(o, blurred.read(gid), clamp01(p.structurePreserve)), gid);
}

kernel void pigment_boundary(
    texture2d<float, access::read> guide [[texture(0)]],
    constant Params& p [[buffer(0)]],
    texture2d<float, access::write> boundary [[texture(1)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  uint2 l = uint2(gid.x > 0 ? gid.x - 1 : 0, gid.y);
  uint2 r = uint2(min(gid.x + 1, p.width - 1), gid.y);
  uint2 d = uint2(gid.x, gid.y > 0 ? gid.y - 1 : 0);
  uint2 u = uint2(gid.x, min(gid.y + 1, p.height - 1));
  float3 gx = 0.5f * (guide.read(r).xyz - guide.read(l).xyz);
  float3 gy = 0.5f * (guide.read(u).xyz - guide.read(d).xyz);
  float localY = max(abs(guide.read(gid).x), 1.0e-3f);
  float mag2 = (gx.x * gx.x + gy.x * gy.x) / (localY * localY) +
               0.25f * (gx.y * gx.y + gy.y * gy.y + gx.z * gx.z + gy.z * gy.z);
  float edge = smoother(0.0f, max(p.boundarySoftness, 1.0e-5f), sqrt(max(0.0f, mag2)));
  boundary.write(float4(1.0f - clamp01(p.boundaryPreserve) * edge), gid);
}

kernel void pigment_prepare_guidance(
    texture2d<float, access::read> current [[texture(0)]],
    constant Params& p [[buffer(0)]],
    texture2d<float, access::write> guidance [[texture(1)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  float4 v = current.read(gid);
  guidance.write(float4(v.x / max(abs(p.toneSimilarity), 1.0e-6f),
                        v.y / max(abs(p.chromaSimilarity), 1.0e-6f),
                        v.z / max(abs(p.chromaSimilarity), 1.0e-6f), v.w), gid);
}

kernel void pigment_guided_statistics(
    texture2d<float, access::read> original [[texture(0)]],
    texture2d<float, access::read> guidance [[texture(1)]],
    texture2d<float, access::read> boundary [[texture(2)]],
    constant Params& p [[buffer(0)]],
    texture2d<float, access::write> wp [[texture(3)]],
    texture2d<float, access::write> wg [[texture(4)]],
    texture2d<float, access::write> wgg [[texture(5)]],
    texture2d<float, access::write> wgp [[texture(6)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  float w = max(1.0e-4f, clamp01(boundary.read(gid).x));
  float4 g = guidance.read(gid);
  float4 source = original.read(gid);
  wp.write(w * source, gid);
  wg.write(w * g, gid);
  wgg.write(w * g * g, gid);
  wgp.write(w * g * source, gid);
}

kernel void pigment_guided_solve(
    texture2d<float, access::read> meanWeight [[texture(0)]],
    texture2d<float, access::read> meanWp [[texture(1)]],
    texture2d<float, access::read> meanWg [[texture(2)]],
    texture2d<float, access::read> meanWgg [[texture(3)]],
    texture2d<float, access::read> meanWgp [[texture(4)]],
    constant Params& p [[buffer(0)]],
    texture2d<float, access::write> coefficientA [[texture(5)]],
    texture2d<float, access::write> coefficientB [[texture(6)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  float invW = 1.0f / max(meanWeight.read(gid).x, 1.0e-6f);
  float4 meanG = meanWg.read(gid) * invW;
  float4 meanP = meanWp.read(gid) * invW;
  float4 variance = max(float4(0.0f), meanWgg.read(gid) * invW - meanG * meanG);
  float4 covariance = meanWgp.read(gid) * invW - meanG * meanP;
  float4 a = covariance / (variance + 0.04f);
  coefficientA.write(a, gid);
  coefficientB.write(meanP - a * meanG, gid);
}

kernel void pigment_guided_reconstruct(
    texture2d<float, access::read> guidance [[texture(0)]],
    texture2d<float, access::read> meanA [[texture(1)]],
    texture2d<float, access::read> meanB [[texture(2)]],
    constant Params& p [[buffer(0)]],
    texture2d<float, access::write> output [[texture(3)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  output.write(meanA.read(gid) * guidance.read(gid) + meanB.read(gid), gid);
}

kernel void pigment_update(
    texture2d<float, access::read> previous [[texture(0)]],
    texture2d<float, access::read> filtered [[texture(1)]],
    texture2d<float, access::read> strength [[texture(2)]],
    constant Params& p [[buffer(0)]],
    texture2d<float, access::write> output [[texture(3)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  float4 a = previous.read(gid);
  float gain = clamp01(p.massStrength) * clamp01(strength.read(gid).x);
  float4 result = mix(a, filtered.read(gid), gain);
  result.w = a.w;
  output.write(result, gid);
}

kernel void pigment_copy_texture(
    texture2d<float, access::read> source [[texture(0)]],
    constant Params& p [[buffer(0)]],
    texture2d<float, access::write> destination [[texture(1)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x < p.width && gid.y < p.height) destination.write(source.read(gid), gid);
}

kernel void pigment_final(
    const device float* sourceBuffer [[buffer(0)]],
    device float* destinationBuffer [[buffer(1)]],
    constant ImageLayout& sourceLayout [[buffer(2)]],
    constant ImageLayout& destinationLayout [[buffer(3)]],
    constant Params& p [[buffer(4)]],
    texture2d<float, access::read> original [[texture(0)]],
    texture2d<float, access::read> mass [[texture(1)]],
    texture2d<float, access::read> variation [[texture(2)]],
    texture2d<float, access::read> structureGuide [[texture(3)]],
    texture2d<float, access::read> strength [[texture(4)]],
    texture2d<float, access::read> boundary [[texture(5)]],
    texture2d<float, access::read> captured [[texture(6)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  float4 o = original.read(gid);
  float4 m = mass.read(gid);
  if (p.debugView == 0 && (p.amount == 0.0f || p.mix == 0.0f)) {
    uint sourceBase = sourceLayout.startFloat + gid.y * sourceLayout.rowFloats +
                      gid.x * sourceLayout.components;
    uint destinationBase = destinationLayout.startFloat + gid.y * destinationLayout.rowFloats +
                           gid.x * destinationLayout.components;
    for (uint component = 0; component < destinationLayout.components; ++component)
      destinationBuffer[destinationBase + component] = sourceBuffer[sourceBase + component];
    return;
  }
  float effected = clamp01(p.massStrength) * clamp01(strength.read(gid).x);
  float3 candidate = m.xyz + clamp01(p.internalVariation) * effected *
      (variation.read(gid).xyz - m.xyz);
  float3 resultYab = float3(o.x + clamp01(p.lumaMassing) * (candidate.x - o.x),
                           o.y + clamp01(p.chromaMassing) * (candidate.y - o.y),
                           o.z + clamp01(p.chromaMassing) * (candidate.z - o.z));
  float3 rgb;
  if (p.debugView == 2 || (p.debugView >= 6 && p.debugView <= 9) || p.debugView == 13) {
    rgb = yabToRgb(captured.read(gid).xyz, p);
  } else if (p.debugView == 3) {
    float v = structureGuide.read(gid).x;
    v = 0.5f + 0.5f * v / (1.0f + abs(v));
    rgb = float3(v);
  } else if (p.debugView == 4) {
    rgb = float3(clamp01(strength.read(gid).x));
  } else if (p.debugView == 5) {
    rgb = float3(clamp01(boundary.read(gid).x));
  } else if (p.debugView == 10) {
    rgb = yabToRgb(float3(resultYab.x, o.y, o.z), p);
  } else if (p.debugView == 11) {
    rgb = yabToRgb(float3(o.x, resultYab.y, resultYab.z), p);
  } else if (p.debugView == 12) {
    float3 delta = variation.read(gid).xyz - m.xyz;
    rgb = clamp(float3(0.5f) + 0.45f * yabToRgb(delta, p), 0.0f, 1.0f);
  } else if (p.debugView == 14) {
    rgb = clamp(float3(0.5f) + 0.45f * yabToRgb(resultYab - o.xyz, p), 0.0f, 1.0f);
  } else {
    float3 originalRgb = yabToRgb(o.xyz, p);
    rgb = mix(originalRgb, yabToRgb(resultYab, p), clamp01(p.mix));
  }
  float alpha = o.w;
  if (p.debugView == 0 && p.premultiplied != 0 && abs(alpha) <= 1.0e-6f) {
    rgb = float3(readComponent(sourceBuffer, sourceLayout, gid, 0),
                 readComponent(sourceBuffer, sourceLayout, gid, 1),
                 readComponent(sourceBuffer, sourceLayout, gid, 2));
  } else if (p.premultiplied != 0) {
    rgb *= alpha;
  }
  uint base = destinationLayout.startFloat + gid.y * destinationLayout.rowFloats +
              gid.x * destinationLayout.components;
  destinationBuffer[base] = rgb.x;
  destinationBuffer[base + 1] = rgb.y;
  destinationBuffer[base + 2] = rgb.z;
  if (destinationLayout.components == 4) destinationBuffer[base + 3] = alpha;
}

// Integrated Pigment graph ---------------------------------------------------

struct IntegratedParams {
  uint width, height, sourceComponents, destinationComponents;
  uint hasMask, maskComponents, premultiplied, invertMask;
  uint comparisonMode, debugView, veilSeed, reserved;
  float amount, massScale, massStrength, toneSimilarity, chromaSimilarity;
  float lumaAttraction, chromaAttraction;
  float structurePreserve, boundaryPreserve, boundaryExtinction, boundarySoftness;
  float veilAmount, veilScale, veilIrregularity, veilContrast;
  float detailCleanup, fineDetail, mediumDetail, internalVariation;
  float chromaMigration, chromaScale, chromaEdgeRespect;
  float regionSoftness, boundaryScale, veilTonalBias, chromaLumaCoupling, mix;
  float renderScaleX, renderScaleY, pixelAspect, originX, originY;
  float whiteX, whiteZ;
  float rgbToXyz[9];
  float xyzToRgb[9];
};

struct RegionLevelParams {
  uint width, height;
  float levelScale, radiusX, radiusY, quarterBlend;
};

float3 integratedRgbToYab(float3 rgb, constant IntegratedParams& p) {
  float x = p.rgbToXyz[0] * rgb.x + p.rgbToXyz[1] * rgb.y + p.rgbToXyz[2] * rgb.z;
  float y = p.rgbToXyz[3] * rgb.x + p.rgbToXyz[4] * rgb.y + p.rgbToXyz[5] * rgb.z;
  float z = p.rgbToXyz[6] * rgb.x + p.rgbToXyz[7] * rgb.y + p.rgbToXyz[8] * rgb.z;
  return float3(y, x / p.whiteX - y, z / p.whiteZ - y);
}

float3 integratedYabToRgb(float3 yab, constant IntegratedParams& p) {
  float3 xyz = float3((yab.y + yab.x) * p.whiteX, yab.x,
                      (yab.z + yab.x) * p.whiteZ);
  return float3(p.xyzToRgb[0] * xyz.x + p.xyzToRgb[1] * xyz.y + p.xyzToRgb[2] * xyz.z,
                p.xyzToRgb[3] * xyz.x + p.xyzToRgb[4] * xyz.y + p.xyzToRgb[5] * xyz.z,
                p.xyzToRgb[6] * xyz.x + p.xyzToRgb[7] * xyz.y + p.xyzToRgb[8] * xyz.z);
}

uint integratedHash(uint value) {
  value ^= value >> 16; value *= 0x7feb352du;
  value ^= value >> 15; value *= 0x846ca68bu;
  return value ^ (value >> 16);
}

float integratedLattice(int2 p, uint seed) {
  uint h = integratedHash(as_type<uint>(p.x) * 0x9e3779b9u ^
                          as_type<uint>(p.y) * 0x85ebca6bu ^ seed);
  return float(h & 0x00ffffffu) / 8388607.5f - 1.0f;
}

float integratedNoise(float2 p, uint seed) {
  int2 i = int2(floor(p));
  float2 t = fract(p);
  t = t * t * (3.0f - 2.0f * t);
  float a = integratedLattice(i, seed);
  float b = integratedLattice(i + int2(1, 0), seed);
  float c = integratedLattice(i + int2(0, 1), seed);
  float d = integratedLattice(i + int2(1, 1), seed);
  return mix(mix(a, b, t.x), mix(c, d, t.x), t.y);
}

kernel void pigment_integrated_rgb_to_yab(
    const device float* source [[buffer(0)]],
    constant ImageLayout& layout [[buffer(1)]],
    constant IntegratedParams& p [[buffer(2)]],
    texture2d<float, access::write> output [[texture(0)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  float alpha = layout.components == 4 ? readComponent(source, layout, gid, 3) : 1.0f;
  float3 rgb = float3(readComponent(source, layout, gid, 0),
                      readComponent(source, layout, gid, 1),
                      readComponent(source, layout, gid, 2));
  if (p.premultiplied != 0) rgb = abs(alpha) > 1.0e-6f ? rgb / alpha : float3(0.0f);
  output.write(float4(integratedRgbToYab(rgb, p), alpha), gid);
}

kernel void pigment_integrated_structure(
    texture2d<float, access::read> blurNear [[texture(0)]],
    texture2d<float, access::read> blurFar [[texture(1)]],
    constant IntegratedParams& p [[buffer(0)]],
    texture2d<float, access::write> guide [[texture(2)]],
    texture2d<float, access::write> protection [[texture(3)]],
    texture2d<float, access::write> permeability [[texture(4)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  uint2 l = uint2(gid.x > 0 ? gid.x - 1 : 0, gid.y);
  uint2 r = uint2(min(gid.x + 1, p.width - 1), gid.y);
  uint2 d = uint2(gid.x, gid.y > 0 ? gid.y - 1 : 0);
  uint2 u = uint2(gid.x, min(gid.y + 1, p.height - 1));
  float3 nx = 0.5f * (blurNear.read(r).xyz - blurNear.read(l).xyz);
  float3 ny = 0.5f * (blurNear.read(u).xyz - blurNear.read(d).xyz);
  float3 fx = 0.5f * (blurFar.read(r).xyz - blurFar.read(l).xyz);
  float3 fy = 0.5f * (blurFar.read(u).xyz - blurFar.read(d).xyz);
  float local = max(0.04f, abs(blurFar.read(gid).x) * 0.2f);
  float nearMag = length(float2(nx.x, ny.x)) / local + 0.25f *
      sqrt(dot(nx.yz, nx.yz) + dot(ny.yz, ny.yz));
  float farMag = length(float2(fx.x, fy.x)) / local + 0.25f *
      sqrt(dot(fx.yz, fx.yz) + dot(fy.yz, fy.yz));
  float alignment = abs(dot(float2(nx.x, ny.x), float2(fx.x, fy.x))) /
      max(1.0e-6f, length(float2(nx.x, ny.x)) * length(float2(fx.x, fy.x)));
  float persistent = min(nearMag, farMag * 1.75f) * mix(0.5f, 1.0f, alignment);
  float edge = smoother(0.0f, max(1.0e-5f, p.boundarySoftness), persistent);
  float protect = clamp01(p.structurePreserve) * clamp01(p.boundaryPreserve) * edge;
  guide.write(blurNear.read(gid), gid);
  protection.write(float4(protect), gid);
  permeability.write(float4(max(1.0e-4f, 1.0f - protect)), gid);
}

kernel void pigment_integrated_fields(
    texture2d<float, access::read> original [[texture(0)]],
    const device float* mask [[buffer(0)]],
    constant ImageLayout& maskLayout [[buffer(1)]],
    constant IntegratedParams& p [[buffer(2)]],
    texture2d<float, access::write> veilOut [[texture(1)]],
    texture2d<float, access::write> massOut [[texture(2)]],
    texture2d<float, access::write> extinctionOut [[texture(3)]],
    texture2d<float, access::write> chromaOut [[texture(4)]],
    texture2d<float, access::write> detailOut [[texture(5)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  float2 canonical = float2(p.originX + float(gid.x) / max(p.renderScaleX, 1.0e-6f),
                            p.originY + float(gid.y) / max(p.renderScaleY, 1.0e-6f));
  float scale = max(8.0f, p.veilScale);
  float irregularity = clamp01(p.veilIrregularity);
  float warp = integratedNoise(canonical / (scale * 1.7f), p.veilSeed + 97u) *
      irregularity * 0.35f;
  float2 q = canonical / scale + float2(warp, -0.7f * warp);
  float n = integratedNoise(q, p.veilSeed);
  n += irregularity * 0.48f * integratedNoise(q * 2.03f, p.veilSeed + 17u);
  n += irregularity * irregularity * 0.22f *
      integratedNoise(q * 4.07f, p.veilSeed + 41u);
  n /= 1.0f + irregularity * 0.48f + irregularity * irregularity * 0.22f;
  float y = original.read(gid).x;
  n += p.veilTonalBias * y / (1.0f + abs(y));
  float v = clamp01(0.5f + 0.5f * tanh(n * exp2(clamp(p.veilContrast, -2.0f, 2.0f))));
  float maskValue = 1.0f;
  if (p.hasMask != 0) {
    maskValue = clamp01(readComponent(mask, maskLayout, gid, 0));
    if (p.invertMask != 0) maskValue = 1.0f - maskValue;
  }
  float gate = clamp01(p.amount) * maskValue;
  float veilAmount = clamp01(p.veilAmount);
  float mass = gate * ((1.0f - veilAmount) + veilAmount *
      smoother(0.0f, 1.0f, 0.15f + 0.85f * v));
  float extinction = gate * clamp01(p.boundaryExtinction) *
      ((1.0f - veilAmount) + veilAmount * smoother(0.25f, 1.0f, v));
  float chroma = gate * clamp01(p.chromaMigration) *
      ((1.0f - veilAmount) + veilAmount * smoother(0.0f, 1.0f, 1.0f - abs(2.0f * v - 1.0f)));
  float detail = clamp01(1.0f - gate * veilAmount * smoother(0.0f, 1.0f, v));
  veilOut.write(float4(v), gid); massOut.write(float4(mass), gid);
  extinctionOut.write(float4(extinction), gid); chromaOut.write(float4(chroma), gid);
  detailOut.write(float4(detail), gid);
}

constexpr sampler integratedLinear(coord::pixel, address::clamp_to_edge, filter::linear);

kernel void pigment_integrated_downsample(
    texture2d<float, access::sample> source [[texture(0)]],
    constant RegionLevelParams& level [[buffer(0)]],
    texture2d<float, access::write> yab [[texture(1)]],
    texture2d<float, access::write> position [[texture(2)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= level.width || gid.y >= level.height) return;
  float2 p = (float2(gid) + 0.5f) / level.levelScale;
  yab.write(source.sample(integratedLinear, p), gid);
  position.write(float4(float2(gid) + 0.5f, 0.0f, 1.0f), gid);
}

kernel void pigment_integrated_region_iteration(
    texture2d<float, access::sample> previousYab [[texture(0)]],
    texture2d<float, access::sample> previousPosition [[texture(1)]],
    texture2d<float, access::sample> protection [[texture(2)]],
    texture2d<float, access::sample> massField [[texture(3)]],
    constant IntegratedParams& p [[buffer(0)]],
    constant RegionLevelParams& level [[buffer(1)]],
    texture2d<float, access::write> nextYab [[texture(4)]],
    texture2d<float, access::write> nextPosition [[texture(5)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= level.width || gid.y >= level.height) return;
  float2 here = float2(gid) + 0.5f;
  float2 center = previousPosition.sample(integratedLinear, here).xy;
  float4 centerYab = previousYab.sample(integratedLinear, center);
  float2 fullCenter = center / level.levelScale;
  float centerProtection = protection.sample(integratedLinear, fullCenter).x;
  float localYScale = max(0.05f, abs(centerYab.x) * 0.2f);
  float sumWeight = 0.0f; float2 sumPosition = float2(0.0f); float4 sumYab = float4(0.0f);
  for (int gy = -4; gy <= 4; ++gy) {
    for (int gx = -4; gx <= 4; ++gx) {
      float2 candidate = center + float2(level.radiusX * float(gx) / 4.0f,
                                         level.radiusY * float(gy) / 4.0f);
      float2 samplePosition = previousPosition.sample(integratedLinear, candidate).xy;
      float4 sampleYab = previousYab.sample(integratedLinear, candidate);
      float2 delta = float2((samplePosition.x - center.x) / max(level.radiusX, 0.25f),
                            (samplePosition.y - center.y) / max(level.radiusY, 0.25f));
      float dy = (sampleYab.x - centerYab.x) /
          max(1.0e-5f, p.toneSimilarity * localYScale);
      float2 dc = (sampleYab.yz - centerYab.yz) / max(1.0e-5f, p.chromaSimilarity);
      float feature = dy * dy + dot(dc, dc);
      float spatial = dot(delta, delta) / max(0.1f, p.regionSoftness);
      float2 fullSample = candidate / level.levelScale;
      float sampleProtection = protection.sample(integratedLinear, fullSample).x;
      float permeability = max(1.0e-4f, 1.0f - max(centerProtection, sampleProtection));
      float weight = exp(-0.5f * min(80.0f, feature + spatial)) * permeability;
      sumWeight += weight; sumPosition += weight * samplePosition; sumYab += weight * sampleYab;
    }
  }
  float invWeight = 1.0f / max(sumWeight, 1.0e-8f);
  float strength = massField.sample(integratedLinear, fullCenter).x;
  float gain = clamp01(p.massStrength) * clamp01(strength);
  float2 movedPosition = mix(center, sumPosition * invWeight, gain);
  float4 movedYab = mix(centerYab, sumYab * invWeight, gain);
  movedYab.w = centerYab.w;
  nextPosition.write(float4(movedPosition, 0.0f, 1.0f), gid);
  nextYab.write(movedYab, gid);
}

kernel void pigment_integrated_reconstruct_mass(
    texture2d<float, access::read> original [[texture(0)]],
    texture2d<float, access::sample> halfYab [[texture(1)]],
    texture2d<float, access::sample> halfPosition [[texture(2)]],
    texture2d<float, access::sample> quarterYab [[texture(3)]],
    texture2d<float, access::sample> quarterPosition [[texture(4)]],
    constant IntegratedParams& p [[buffer(0)]],
    constant RegionLevelParams& halfLevel [[buffer(1)]],
    constant RegionLevelParams& quarterLevel [[buffer(2)]],
    texture2d<float, access::write> mass [[texture(5)]],
    texture2d<float, access::write> attraction [[texture(6)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  float2 halfCoord = (float2(gid) + 0.5f) * halfLevel.levelScale;
  float2 quarterCoord = (float2(gid) + 0.5f) * quarterLevel.levelScale;
  float2 halfMode = halfPosition.sample(integratedLinear, halfCoord).xy;
  float2 quarterMode = quarterPosition.sample(integratedLinear, quarterCoord).xy;
  float4 h = halfYab.sample(integratedLinear, halfMode);
  float4 q = quarterYab.sample(integratedLinear, quarterMode);
  float4 mode = mix(h, q, quarterLevel.quarterBlend);
  float4 o = original.read(gid);
  float3 result = float3(mix(o.x, mode.x, clamp01(p.lumaAttraction)),
                         mix(o.y, mode.y, clamp01(p.chromaAttraction)),
                         mix(o.z, mode.z, clamp01(p.chromaAttraction)));
  mass.write(float4(result, o.w), gid);
  float magnitude = length(float3((result.x - o.x) / max(0.05f, abs(o.x) * 0.2f),
                                  result.y - o.y, result.z - o.z));
  attraction.write(float4(magnitude), gid);
}

kernel void pigment_integrated_boundary_extinction(
    texture2d<float, access::read> mass [[texture(0)]],
    texture2d<float, access::read> blurredMass [[texture(1)]],
    texture2d<float, access::read> protection [[texture(2)]],
    texture2d<float, access::read> extinction [[texture(3)]],
    constant IntegratedParams& p [[buffer(0)]],
    texture2d<float, access::write> output [[texture(4)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  float4 m = mass.read(gid);
  float dissolve = clamp01(extinction.read(gid).x) * (1.0f - clamp01(protection.read(gid).x));
  float4 result = mix(m, blurredMass.read(gid), dissolve);
  result.w = m.w;
  output.write(result, gid);
}

kernel void pigment_integrated_chroma(
    texture2d<float, access::read> source [[texture(0)]],
    texture2d<float, access::read> blurred [[texture(1)]],
    texture2d<float, access::read> protection [[texture(2)]],
    texture2d<float, access::read> chromaField [[texture(3)]],
    constant IntegratedParams& p [[buffer(0)]],
    texture2d<float, access::write> output [[texture(4)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  float4 value = source.read(gid);
  float4 migrated = blurred.read(gid);
  float edgeGate = 1.0f - clamp01(p.chromaEdgeRespect) * clamp01(protection.read(gid).x) *
      clamp01(p.chromaLumaCoupling);
  float gain = clamp01(chromaField.read(gid).x) * edgeGate;
  value.yz = mix(value.yz, migrated.yz, gain);
  output.write(value, gid);
}

kernel void pigment_integrated_reintegrate(
    texture2d<float, access::read> original [[texture(0)]],
    texture2d<float, access::read> base [[texture(1)]],
    texture2d<float, access::read> fineBlur [[texture(2)]],
    texture2d<float, access::read> mediumBlur [[texture(3)]],
    texture2d<float, access::read> broadBlur [[texture(4)]],
    texture2d<float, access::read> detailField [[texture(5)]],
    constant IntegratedParams& p [[buffer(0)]],
    texture2d<float, access::write> result [[texture(6)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  float4 o = original.read(gid); float4 f = fineBlur.read(gid);
  float4 m = mediumBlur.read(gid); float4 b = broadBlur.read(gid);
  float retention = clamp01(detailField.read(gid).x);
  float3 fine = o.xyz - f.xyz;
  float3 medium = f.xyz - m.xyz;
  float3 internal = m.xyz - b.xyz;
  float4 value = base.read(gid);
  value.xyz += retention * (clamp01(p.fineDetail) * fine +
      clamp01(p.mediumDetail) * medium + clamp01(p.internalVariation) * internal);
  value.w = o.w;
  result.write(value, gid);
}

kernel void pigment_integrated_final(
    const device float* sourceBuffer [[buffer(0)]],
    device float* destinationBuffer [[buffer(1)]],
    constant ImageLayout& sourceLayout [[buffer(2)]],
    constant ImageLayout& destinationLayout [[buffer(3)]],
    constant IntegratedParams& p [[buffer(4)]],
    texture2d<float, access::read> original [[texture(0)]],
    texture2d<float, access::read> structure [[texture(1)]],
    texture2d<float, access::read> veil [[texture(2)]],
    texture2d<float, access::read> massField [[texture(3)]],
    texture2d<float, access::read> extinctionField [[texture(4)]],
    texture2d<float, access::read> chromaField [[texture(5)]],
    texture2d<float, access::read> detailField [[texture(6)]],
    texture2d<float, access::read> mass [[texture(7)]],
    texture2d<float, access::read> attraction [[texture(8)]],
    texture2d<float, access::read> preBoundary [[texture(9)]],
    texture2d<float, access::read> protection [[texture(10)]],
    texture2d<float, access::read> boundaryResult [[texture(11)]],
    texture2d<float, access::read> chromaResult [[texture(12)]],
    texture2d<float, access::read> fineBlur [[texture(13)]],
    texture2d<float, access::read> mediumBlur [[texture(14)]],
    texture2d<float, access::read> broadBlur [[texture(15)]],
    texture2d<float, access::read> finalYab [[texture(16)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  float4 o = original.read(gid);
  if (p.comparisonMode == 0 || (p.debugView == 0 && (p.amount == 0.0f || p.mix == 0.0f))) {
    uint sb = sourceLayout.startFloat + gid.y * sourceLayout.rowFloats + gid.x * sourceLayout.components;
    uint db = destinationLayout.startFloat + gid.y * destinationLayout.rowFloats + gid.x * destinationLayout.components;
    for (uint c = 0; c < destinationLayout.components; ++c) destinationBuffer[db + c] = sourceBuffer[sb + c];
    return;
  }
  float3 rgb;
  uint view = p.debugView;
  if (view == 1) rgb = integratedYabToRgb(structure.read(gid).xyz, p);
  else if (view >= 2 && view <= 6) {
    float v = view == 2 ? veil.read(gid).x : view == 3 ? massField.read(gid).x :
        view == 4 ? extinctionField.read(gid).x : view == 5 ? chromaField.read(gid).x :
        detailField.read(gid).x;
    rgb = float3(clamp01(v));
  } else if (view == 7) rgb = integratedYabToRgb(mass.read(gid).xyz, p);
  else if (view == 8) rgb = float3(clamp01(attraction.read(gid).x));
  else if (view == 9) rgb = integratedYabToRgb(float3(mass.read(gid).x, o.y, o.z), p);
  else if (view == 10) rgb = integratedYabToRgb(float3(o.x, mass.read(gid).y, mass.read(gid).z), p);
  else if (view == 11) rgb = integratedYabToRgb(preBoundary.read(gid).xyz, p);
  else if (view == 12) rgb = float3(clamp01(protection.read(gid).x));
  else if (view == 13) rgb = float3(clamp01(extinctionField.read(gid).x));
  else if (view == 14) rgb = integratedYabToRgb(chromaResult.read(gid).xyz, p);
  else if (view == 15) rgb = clamp(float3(0.5f) + 0.45f *
      integratedYabToRgb(o.xyz - fineBlur.read(gid).xyz, p), 0.0f, 1.0f);
  else if (view == 16) rgb = clamp(float3(0.5f) + 0.45f *
      integratedYabToRgb(fineBlur.read(gid).xyz - mediumBlur.read(gid).xyz, p), 0.0f, 1.0f);
  else if (view == 17) rgb = clamp(float3(0.5f) + 0.45f *
      integratedYabToRgb(mediumBlur.read(gid).xyz - broadBlur.read(gid).xyz, p), 0.0f, 1.0f);
  else if (view == 18) rgb = integratedYabToRgb(chromaResult.read(gid).xyz, p);
  else if (view == 19) rgb = clamp(float3(0.5f) + 0.45f *
      integratedYabToRgb(finalYab.read(gid).xyz - o.xyz, p), 0.0f, 1.0f);
  else rgb = mix(integratedYabToRgb(o.xyz, p), integratedYabToRgb(finalYab.read(gid).xyz, p),
                 clamp01(p.mix));
  float alpha = o.w;
  if (view == 0 && p.premultiplied != 0 && abs(alpha) <= 1.0e-6f) {
    rgb = float3(readComponent(sourceBuffer, sourceLayout, gid, 0),
                 readComponent(sourceBuffer, sourceLayout, gid, 1),
                 readComponent(sourceBuffer, sourceLayout, gid, 2));
  } else if (p.premultiplied != 0) rgb *= alpha;
  uint base = destinationLayout.startFloat + gid.y * destinationLayout.rowFloats +
      gid.x * destinationLayout.components;
  destinationBuffer[base] = rgb.x; destinationBuffer[base + 1] = rgb.y;
  destinationBuffer[base + 2] = rgb.z;
  if (destinationLayout.components == 4) destinationBuffer[base + 3] = alpha;
}
