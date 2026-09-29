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
  uint comparisonMode, debugView, veilSeed, massEstimator;
  float amount, massScale, massStrength, toneSimilarity, chromaSimilarity;
  float lumaAttraction, chromaAttraction;
  float structurePreserve, boundaryPreserve, boundaryExtinction, boundarySoftness;
  float veilAmount, veilScale, veilIrregularity, veilContrast;
  float detailCleanup, fineDetail, mediumDetail, internalVariation;
  float chromaMigration, chromaScale, chromaEdgeRespect;
  float regionSoftness, modeSelectivity, boundaryScale, veilTonalBias, chromaLumaCoupling, mix;
  float renderScaleX, renderScaleY, pixelAspect, originX, originY;
  float whiteX, whiteZ;
  float rgbToXyz[9];
  float xyzToRgb[9];
  uint hasPlaneMap, debugPlane;
  uint planeEnabled[4];
  float planeAmount[4], planeSourceMix[4];
  float planeManualY[4], planeManualA[4], planeManualB[4];
  float planeYOffset[4], planeToneInfluence[4];
  float planeABias[4], planeBBias[4], planeChromaInfluence[4];
  float fineExtinction, mediumExtinction, broadRetention, detailStructurePreserve;
  float yTransitionWidth, abTransitionWidth, transitionStructureRespect, localSoftness;
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

kernel void pigment_integrated_local_density(
    texture2d<float, access::sample> previousYab [[texture(0)]],
    texture2d<float, access::sample> previousPosition [[texture(1)]],
    texture2d<float, access::sample> protection [[texture(2)]],
    texture2d<float, access::sample> extinction [[texture(3)]],
    constant IntegratedParams& p [[buffer(0)]],
    constant RegionLevelParams& level [[buffer(1)]],
    texture2d<float, access::write> densityOut [[texture(4)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= level.width || gid.y >= level.height) return;
  float2 here = float2(gid) + 0.5f;
  float2 center = previousPosition.sample(integratedLinear, here).xy;
  float4 centerYab = previousYab.sample(integratedLinear, center);
  float2 fullCenter = center / level.levelScale;
  float centerProtection = protection.sample(integratedLinear, fullCenter).x *
      (1.0f - extinction.sample(integratedLinear, fullCenter).x);
  float localYScale = max(0.05f, abs(centerYab.x) * 0.2f);
  float density = 0.0f;
  for (int gy = -4; gy <= 4; ++gy) for (int gx = -4; gx <= 4; ++gx) {
    float2 candidate = center + float2(level.radiusX * float(gx) / 4.0f,
                                       level.radiusY * float(gy) / 4.0f);
    float2 samplePosition = previousPosition.sample(integratedLinear, candidate).xy;
    float4 sampleYab = previousYab.sample(integratedLinear, candidate);
    float2 delta = float2((samplePosition.x - center.x) / max(level.radiusX, 0.25f),
                          (samplePosition.y - center.y) / max(level.radiusY, 0.25f));
    float dy = (sampleYab.x - centerYab.x) /
        max(1.0e-5f, p.toneSimilarity * localYScale);
    float2 dc = (sampleYab.yz - centerYab.yz) / max(1.0e-5f, p.chromaSimilarity);
    float2 fullSample = candidate / level.levelScale;
    float sampleProtection = protection.sample(integratedLinear, fullSample).x *
        (1.0f - extinction.sample(integratedLinear, fullSample).x);
    float permeability = max(1.0e-4f, 1.0f - max(centerProtection, sampleProtection));
    float energy = dot(delta, delta) / max(0.1f, p.regionSoftness) +
        dy * dy + dot(dc, dc);
    density += exp(-0.5f * min(80.0f, energy)) * permeability;
  }
  densityOut.write(float4(density / 81.0f), gid);
}

kernel void pigment_integrated_representative_iteration(
    texture2d<float, access::sample> previousYab [[texture(0)]],
    texture2d<float, access::sample> previousPosition [[texture(1)]],
    texture2d<float, access::sample> density [[texture(2)]],
    texture2d<float, access::sample> protection [[texture(3)]],
    texture2d<float, access::sample> extinction [[texture(4)]],
    texture2d<float, access::sample> massField [[texture(5)]],
    constant IntegratedParams& p [[buffer(0)]],
    constant RegionLevelParams& level [[buffer(1)]],
    texture2d<float, access::write> nextYab [[texture(6)]],
    texture2d<float, access::write> nextPosition [[texture(7)]],
    texture2d<float, access::write> dominantOut [[texture(8)]],
    texture2d<float, access::write> diagnosticOut [[texture(9)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= level.width || gid.y >= level.height) return;
  float2 here = float2(gid) + 0.5f;
  float2 center = previousPosition.sample(integratedLinear, here).xy;
  float4 centerYab = previousYab.sample(integratedLinear, center);
  float2 fullCenter = center / level.levelScale;
  float centerProtection = protection.sample(integratedLinear, fullCenter).x *
      (1.0f - extinction.sample(integratedLinear, fullCenter).x);
  float localYScale = max(0.05f, abs(centerYab.x) * 0.2f);
  float topScore[4] = {-1.0f, -1.0f, -1.0f, -1.0f};
  float4 topYab[4] = {centerYab, centerYab, centerYab, centerYab};
  float2 topPosition[4] = {center, center, center, center};
  for (int gy = -4; gy <= 4; ++gy) for (int gx = -4; gx <= 4; ++gx) {
    float2 candidate = center + float2(level.radiusX * float(gx) / 4.0f,
                                       level.radiusY * float(gy) / 4.0f);
    float2 samplePosition = previousPosition.sample(integratedLinear, candidate).xy;
    float4 sampleYab = previousYab.sample(integratedLinear, candidate);
    float2 delta = float2((samplePosition.x - center.x) / max(level.radiusX, 0.25f),
                          (samplePosition.y - center.y) / max(level.radiusY, 0.25f));
    float dy = (sampleYab.x - centerYab.x) /
        max(1.0e-5f, p.toneSimilarity * localYScale);
    float2 dc = (sampleYab.yz - centerYab.yz) / max(1.0e-5f, p.chromaSimilarity);
    float2 fullSample = candidate / level.levelScale;
    float sampleProtection = protection.sample(integratedLinear, fullSample).x *
        (1.0f - extinction.sample(integratedLinear, fullSample).x);
    float permeability = max(1.0e-4f, 1.0f - max(centerProtection, sampleProtection));
    float eligibility = exp(-0.5f * min(80.0f,
        dot(delta, delta) / max(0.1f, p.regionSoftness) + dy * dy + dot(dc, dc))) *
        permeability;
    float score = density.sample(integratedLinear, candidate).x * eligibility;
    for (int rank = 0; rank < 4; ++rank) {
      if (score > topScore[rank]) {
        for (int move = 3; move > rank; --move) {
          topScore[move] = topScore[move - 1]; topYab[move] = topYab[move - 1];
          topPosition[move] = topPosition[move - 1];
        }
        topScore[rank] = score; topYab[rank] = sampleYab;
        topPosition[rank] = samplePosition; break;
      }
    }
  }
  float temperature = 0.6f * exp2(-4.0f * clamp01(p.modeSelectivity)) + 0.025f;
  float weightSum = 0.0f; float4 representative = float4(0.0f);
  float2 representativePosition = float2(0.0f);
  for (int rank = 0; rank < 4; ++rank) {
    float weight = exp((topScore[rank] - topScore[0]) / temperature);
    weightSum += weight; representative += weight * topYab[rank];
    representativePosition += weight * topPosition[rank];
  }
  representative /= max(weightSum, 1.0e-8f);
  representativePosition /= max(weightSum, 1.0e-8f);
  float gain = clamp01(p.massStrength) *
      clamp01(massField.sample(integratedLinear, fullCenter).x);
  float4 movedYab = mix(centerYab, representative, gain); movedYab.w = centerYab.w;
  float2 movedPosition = mix(center, representativePosition, gain);
  float confidence = clamp01(topScore[0] /
      max(1.0e-6f, topScore[0] + topScore[1] + topScore[2] + topScore[3]));
  float competition = clamp01(topScore[1] / max(1.0e-6f, topScore[0]));
  float distance = length(float3((representative.x - centerYab.x) / localYScale,
                                  representative.yz - centerYab.yz));
  nextPosition.write(float4(movedPosition, 0.0f, 1.0f), gid);
  nextYab.write(movedYab, gid); dominantOut.write(representative, gid);
  diagnosticOut.write(float4(density.sample(integratedLinear, center).x,
      confidence, 1.0f - exp(-distance), competition), gid);
}

kernel void pigment_integrated_reconstruct_mass(
    texture2d<float, access::read> original [[texture(0)]],
    texture2d<float, access::sample> halfYab [[texture(1)]],
    texture2d<float, access::sample> halfPosition [[texture(2)]],
    texture2d<float, access::sample> quarterYab [[texture(3)]],
    texture2d<float, access::sample> quarterPosition [[texture(4)]],
    texture2d<float, access::sample> halfDominant [[texture(5)]],
    texture2d<float, access::sample> quarterDominant [[texture(6)]],
    texture2d<float, access::sample> halfDiagnostic [[texture(7)]],
    texture2d<float, access::sample> quarterDiagnostic [[texture(8)]],
    constant IntegratedParams& p [[buffer(0)]],
    constant RegionLevelParams& halfLevel [[buffer(1)]],
    constant RegionLevelParams& quarterLevel [[buffer(2)]],
    texture2d<float, access::write> mass [[texture(9)]],
    texture2d<float, access::write> attraction [[texture(10)]],
    texture2d<float, access::write> dominantFull [[texture(11)]],
    texture2d<float, access::write> diagnosticFull [[texture(12)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  float2 halfCoord = (float2(gid) + 0.5f) * halfLevel.levelScale;
  float2 quarterCoord = (float2(gid) + 0.5f) * quarterLevel.levelScale;
  float2 halfMode = halfPosition.sample(integratedLinear, halfCoord).xy;
  float2 quarterMode = quarterPosition.sample(integratedLinear, quarterCoord).xy;
  float4 h = halfYab.sample(integratedLinear, halfMode);
  float4 q = quarterYab.sample(integratedLinear, quarterMode);
  float4 mode = mix(h, q, quarterLevel.quarterBlend);
  float4 dominant = mode;
  float4 diagnostic = float4(0.0f);
  if (p.massEstimator != 0) {
    dominant = mix(halfDominant.sample(integratedLinear, halfMode),
                   quarterDominant.sample(integratedLinear, quarterMode),
                   quarterLevel.quarterBlend);
    diagnostic = mix(halfDiagnostic.sample(integratedLinear, halfMode),
                     quarterDiagnostic.sample(integratedLinear, quarterMode),
                     quarterLevel.quarterBlend);
  }
  float4 o = original.read(gid);
  float3 result = float3(mix(o.x, mode.x, clamp01(p.lumaAttraction)),
                         mix(o.y, mode.y, clamp01(p.chromaAttraction)),
                         mix(o.z, mode.z, clamp01(p.chromaAttraction)));
  mass.write(float4(result, o.w), gid);
  float magnitude = length(float3((result.x - o.x) / max(0.05f, abs(o.x) * 0.2f),
                                  result.y - o.y, result.z - o.z));
  attraction.write(float4(magnitude), gid);
  dominantFull.write(float4(dominant.xyz, o.w), gid);
  diagnosticFull.write(diagnostic, gid);
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
    texture2d<float, access::read> dominantMode [[texture(17)]],
    texture2d<float, access::read> modeDiagnostic [[texture(18)]],
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
  else if (view == 20) rgb = float3(clamp01(modeDiagnostic.read(gid).x));
  else if (view == 21) rgb = integratedYabToRgb(dominantMode.read(gid).xyz, p);
  else if (view == 22) rgb = float3(clamp01(modeDiagnostic.read(gid).y));
  else if (view == 23) rgb = float3(clamp01(modeDiagnostic.read(gid).z));
  else if (view == 24) rgb = float3(clamp01(modeDiagnostic.read(gid).w));
  else if (view == 25 || view == 26) rgb = integratedYabToRgb(finalYab.read(gid).xyz, p);
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

// Phase 3.2 manual pictorial planes -----------------------------------------

struct PlaneLevelParams {
  uint width, height;
  float levelScale;
  float lambdaX, lambdaY;
};

struct PlaneFitModel {
  float centerX, centerY, scaleX, scaleY;
  float coefficient[18];
  float fitError;
  uint order, valid, padding;
};

void rawPlaneMembership(const device float* planeMap,
                        constant ImageLayout& layout,
                        uint2 pixel, constant IntegratedParams& p,
                        thread float4& planes, thread float& base) {
  if (p.hasPlaneMap == 0) { planes = float4(0.0f); base = 1.0f; return; }
  float4 raw = float4(readComponent(planeMap, layout, pixel, 0),
                      readComponent(planeMap, layout, pixel, 1),
                      readComponent(planeMap, layout, pixel, 2),
                      readComponent(planeMap, layout, pixel, 3));
  raw = clamp(raw, 0.0f, 1.0f);
  planes = float4(p.planeEnabled[0] ? raw.x * clamp01(p.planeAmount[0]) : 0.0f,
                  p.planeEnabled[1] ? raw.y * clamp01(p.planeAmount[1]) : 0.0f,
                  p.planeEnabled[2] ? raw.z * clamp01(p.planeAmount[2]) : 0.0f,
                  p.planeEnabled[3] ? raw.w * clamp01(p.planeAmount[3]) : 0.0f);
  float sum = planes.x + planes.y + planes.z + planes.w;
  float occupancy = clamp01(sum);
  planes *= occupancy / max(sum, 1.0e-8f);
  base = 1.0f - occupancy;
}

kernel void pigment_plane_membership_init(
    const device float* planeMap [[buffer(0)]],
    constant ImageLayout& layout [[buffer(1)]],
    constant IntegratedParams& p [[buffer(2)]],
    constant PlaneLevelParams& level [[buffer(3)]],
    texture2d<float, access::write> planesOut [[texture(0)]],
    texture2d<float, access::write> baseOut [[texture(1)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= level.width || gid.y >= level.height) return;
  uint2 sourcePixel = min(uint2((float2(gid) + 0.5f) / level.levelScale),
                          uint2(p.width - 1, p.height - 1));
  float4 planes; float base;
  rawPlaneMembership(planeMap, layout, sourcePixel, p, planes, base);
  planesOut.write(planes, gid); baseOut.write(float4(base), gid);
}

kernel void pigment_plane_structure(
    texture2d<float, access::read> blurNear [[texture(0)]],
    texture2d<float, access::read> blurFar [[texture(1)]],
    constant IntegratedParams& p [[buffer(0)]],
    texture2d<float, access::write> protection [[texture(2)]],
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
  protection.write(float4(smoother(0.0f, max(1.0e-5f, p.boundarySoftness), persistent)), gid);
}

kernel void pigment_plane_transition_relax(
    texture2d<float, access::read> anchorPlanes [[texture(0)]],
    texture2d<float, access::read> anchorBase [[texture(1)]],
    texture2d<float, access::read> previousPlanes [[texture(2)]],
    texture2d<float, access::read> previousBase [[texture(3)]],
    texture2d<float, access::sample> protection [[texture(4)]],
    constant IntegratedParams& p [[buffer(0)]],
    constant PlaneLevelParams& level [[buffer(1)]],
    texture2d<float, access::write> nextPlanes [[texture(5)]],
    texture2d<float, access::write> nextBase [[texture(6)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= level.width || gid.y >= level.height) return;
  uint2 l = uint2(gid.x > 0 ? gid.x - 1 : 0, gid.y);
  uint2 r = uint2(min(gid.x + 1, level.width - 1), gid.y);
  uint2 d = uint2(gid.x, gid.y > 0 ? gid.y - 1 : 0);
  uint2 u = uint2(gid.x, min(gid.y + 1, level.height - 1));
  float2 fullPosition = (float2(gid) + 0.5f) / level.levelScale;
  float g = max(1.0e-4f, 1.0f - clamp01(p.transitionStructureRespect) *
      clamp01(protection.sample(integratedLinear, fullPosition).x));
  float denom = 1.0f + 2.0f * g * (level.lambdaX + level.lambdaY);
  float4 value = (anchorPlanes.read(gid) + g * level.lambdaX *
      (previousPlanes.read(l) + previousPlanes.read(r)) + g * level.lambdaY *
      (previousPlanes.read(d) + previousPlanes.read(u))) / denom;
  float base = (anchorBase.read(gid).x + g * level.lambdaX *
      (previousBase.read(l).x + previousBase.read(r).x) + g * level.lambdaY *
      (previousBase.read(d).x + previousBase.read(u).x)) / denom;
  float total = max(1.0e-8f, base + value.x + value.y + value.z + value.w);
  nextPlanes.write(max(value, 0.0f) / total, gid);
  nextBase.write(float4(max(base, 0.0f) / total), gid);
}

float2 planePhysicalPosition(uint2 pixel, constant IntegratedParams& p) {
  return float2((p.originX + float(pixel.x) / max(p.renderScaleX, 1.0e-6f)) * p.pixelAspect,
                p.originY + float(pixel.y) / max(p.renderScaleY, 1.0e-6f));
}

void planeBasis(float2 position, thread PlaneFitModel& model, thread float values[6]) {
  float u = (position.x - model.centerX) / max(model.scaleX, 1.0e-6f);
  float v = (position.y - model.centerY) / max(model.scaleY, 1.0e-6f);
  values[0] = 1.0f; values[1] = u; values[2] = v;
  values[3] = u * u; values[4] = u * v; values[5] = v * v;
}

bool planeSolve6(thread float inputMatrix[36], thread float inputRhs[6],
                 thread float output[6], uint dimensions) {
  float matrix[36]; float rhs[6];
  for (uint i = 0; i < 36; ++i) matrix[i] = inputMatrix[i];
  for (uint i = 0; i < 6; ++i) { rhs[i] = inputRhs[i]; output[i] = 0.0f; }
  for (uint column = 0; column < dimensions; ++column) {
    uint pivot = column;
    for (uint row = column + 1; row < dimensions; ++row)
      if (abs(matrix[row * 6 + column]) > abs(matrix[pivot * 6 + column])) pivot = row;
    if (abs(matrix[pivot * 6 + column]) < 1.0e-10f) return false;
    for (uint c = 0; c < 6; ++c) {
      float temp = matrix[column * 6 + c]; matrix[column * 6 + c] = matrix[pivot * 6 + c];
      matrix[pivot * 6 + c] = temp;
    }
    float tempRhs = rhs[column]; rhs[column] = rhs[pivot]; rhs[pivot] = tempRhs;
    float inverse = 1.0f / matrix[column * 6 + column];
    for (uint c = column; c < dimensions; ++c) matrix[column * 6 + c] *= inverse;
    rhs[column] *= inverse;
    for (uint row = 0; row < dimensions; ++row) if (row != column) {
      float factor = matrix[row * 6 + column];
      for (uint c = column; c < dimensions; ++c)
        matrix[row * 6 + c] -= factor * matrix[column * 6 + c];
      rhs[row] -= factor * rhs[column];
    }
  }
  for (uint i = 0; i < dimensions; ++i) output[i] = rhs[i];
  return true;
}

float3 planeEvaluate(thread PlaneFitModel& model, float2 position) {
  float f[6]; planeBasis(position, model, f); float3 result = float3(0.0f);
  for (uint channel = 0; channel < 3; ++channel)
    for (uint i = 0; i < 6; ++i) result[channel] += model.coefficient[channel * 6 + i] * f[i];
  return result;
}

kernel void pigment_plane_fit_solve(
    texture2d<float, access::read> original [[texture(0)]],
    const device float* planeMap [[buffer(0)]],
    constant ImageLayout& planeLayout [[buffer(1)]],
    constant IntegratedParams& p [[buffer(2)]],
    device PlaneFitModel* models [[buffer(3)]],
    uint planeIndex [[thread_position_in_grid]]) {
  if (planeIndex >= 4) return;
  PlaneFitModel model{};
  // A low-order global model does not benefit from visiting every source pixel.
  // A deterministic 4x4 stratified lattice removes serial fit cost while still
  // fitting the original (never preblurred) YAB samples.
  constexpr uint fitStride = 4u;
  float sumW = 0.0f; float2 sumPosition = float2(0.0f);
  for (uint y = planeIndex & 1u; y < p.height; y += fitStride)
    for (uint x = (planeIndex >> 1u) & 1u; x < p.width; x += fitStride) {
    float4 membership; float base;
    rawPlaneMembership(planeMap, planeLayout, uint2(x, y), p, membership, base);
    float w = membership[planeIndex] * clamp(abs(original.read(uint2(x, y)).w), 0.0f, 1.0f);
    sumW += w; sumPosition += w * planePhysicalPosition(uint2(x, y), p);
  }
  if (sumW < 1.0e-5f) { models[planeIndex] = model; return; }
  float2 center = sumPosition / sumW; float2 variance = float2(0.0f);
  for (uint y = planeIndex & 1u; y < p.height; y += fitStride)
    for (uint x = (planeIndex >> 1u) & 1u; x < p.width; x += fitStride) {
    float4 membership; float base;
    rawPlaneMembership(planeMap, planeLayout, uint2(x, y), p, membership, base);
    float w = membership[planeIndex] * clamp(abs(original.read(uint2(x, y)).w), 0.0f, 1.0f);
    float2 delta = planePhysicalPosition(uint2(x, y), p) - center;
    variance += w * delta * delta;
  }
  model.centerX = center.x; model.centerY = center.y;
  model.scaleX = max(1.0f, sqrt(variance.x / sumW));
  model.scaleY = max(1.0f, sqrt(variance.y / sumW));
  uint dimensions = sumW >= 32.0f ? 6u : sumW >= 8.0f ? 3u : 1u;
  float normal[36]; float rhs[18];
  for (uint i = 0; i < 36; ++i) normal[i] = 0.0f;
  for (uint i = 0; i < 18; ++i) rhs[i] = 0.0f;
  for (uint y = planeIndex & 1u; y < p.height; y += fitStride)
    for (uint x = (planeIndex >> 1u) & 1u; x < p.width; x += fitStride) {
    float4 membership; float base;
    rawPlaneMembership(planeMap, planeLayout, uint2(x, y), p, membership, base);
    float4 sample = original.read(uint2(x, y));
    float w = membership[planeIndex] * clamp(abs(sample.w), 0.0f, 1.0f);
    float f[6]; planeBasis(planePhysicalPosition(uint2(x, y), p), model, f);
    for (uint r = 0; r < dimensions; ++r) {
      for (uint c = 0; c < dimensions; ++c) normal[r * 6 + c] += w * f[r] * f[c];
      rhs[r] += w * f[r] * sample.x;
      rhs[6 + r] += w * f[r] * sample.y;
      rhs[12 + r] += w * f[r] * sample.z;
    }
  }
  float trace = max(normal[0], 1.0e-8f);
  for (uint i = 1; i < dimensions; ++i) normal[i * 6 + i] += trace * (i < 3 ? 1.0e-7f : 1.0e-4f);
  bool solved = false;
  uint trialDimensions = dimensions;
  while (!solved) {
    solved = true;
    for (uint channel = 0; channel < 3; ++channel) {
      float channelRhs[6]; float output[6];
      for (uint i = 0; i < 6; ++i) channelRhs[i] = rhs[channel * 6 + i];
      solved = planeSolve6(normal, channelRhs, output, trialDimensions) && solved;
      for (uint i = 0; i < 6; ++i) model.coefficient[channel * 6 + i] = output[i];
    }
    if (solved) { dimensions = trialDimensions; break; }
    if (trialDimensions == 1u) break;
    trialDimensions = trialDimensions == 6u ? 3u : 1u;
  }
  if (!solved) { models[planeIndex] = PlaneFitModel{}; return; }
  float residual = 0.0f;
  for (uint y = planeIndex & 1u; y < p.height; y += fitStride)
    for (uint x = (planeIndex >> 1u) & 1u; x < p.width; x += fitStride) {
    float4 membership; float base;
    rawPlaneMembership(planeMap, planeLayout, uint2(x, y), p, membership, base);
    float4 sample = original.read(uint2(x, y));
    float w = membership[planeIndex] * clamp(abs(sample.w), 0.0f, 1.0f);
    float3 delta = sample.xyz - planeEvaluate(model, planePhysicalPosition(uint2(x, y), p));
    residual += w * dot(delta, delta);
  }
  float residualScale2 = max(1.0e-10f, residual / sumW);
  for (uint i = 0; i < 36; ++i) normal[i] = 0.0f;
  for (uint i = 0; i < 18; ++i) rhs[i] = 0.0f;
  for (uint y = planeIndex & 1u; y < p.height; y += fitStride)
    for (uint x = (planeIndex >> 1u) & 1u; x < p.width; x += fitStride) {
    float4 membership; float base;
    rawPlaneMembership(planeMap, planeLayout, uint2(x, y), p, membership, base);
    float4 sample = original.read(uint2(x, y));
    float3 delta = sample.xyz - planeEvaluate(model, planePhysicalPosition(uint2(x, y), p));
    float robust = 1.0f / (1.0f + dot(delta, delta) / residualScale2);
    float w = membership[planeIndex] * clamp(abs(sample.w), 0.0f, 1.0f) * robust;
    float f[6]; planeBasis(planePhysicalPosition(uint2(x, y), p), model, f);
    for (uint r = 0; r < dimensions; ++r) {
      for (uint c = 0; c < dimensions; ++c) normal[r * 6 + c] += w * f[r] * f[c];
      rhs[r] += w * f[r] * sample.x;
      rhs[6 + r] += w * f[r] * sample.y;
      rhs[12 + r] += w * f[r] * sample.z;
    }
  }
  trace = max(normal[0], 1.0e-8f);
  for (uint i = 1; i < dimensions; ++i) normal[i * 6 + i] += trace * (i < 3 ? 1.0e-7f : 1.0e-4f);
  for (uint channel = 0; channel < 3; ++channel) {
    float channelRhs[6]; float output[6];
    for (uint i = 0; i < 6; ++i) channelRhs[i] = rhs[channel * 6 + i];
    if (planeSolve6(normal, channelRhs, output, dimensions))
      for (uint i = 0; i < 6; ++i) model.coefficient[channel * 6 + i] = output[i];
  }
  model.fitError = sqrt(residualScale2);
  model.order = dimensions == 6 ? 2 : dimensions == 3 ? 1 : 0;
  model.valid = 1; models[planeIndex] = model;
}

kernel void pigment_plane_evaluate_combine(
    texture2d<float, access::read> original [[texture(0)]],
    texture2d<float, access::read> fineBlur [[texture(1)]],
    texture2d<float, access::read> broad [[texture(2)]],
    texture2d<float, access::sample> yPlanes [[texture(3)]],
    texture2d<float, access::sample> yBase [[texture(4)]],
    texture2d<float, access::sample> abPlanes [[texture(5)]],
    texture2d<float, access::sample> abBase [[texture(6)]],
    texture2d<float, access::read> protection [[texture(7)]],
    const device float* mask [[buffer(0)]],
    constant ImageLayout& maskLayout [[buffer(1)]],
    constant IntegratedParams& p [[buffer(2)]],
    const device PlaneFitModel* models [[buffer(3)]],
    texture2d<float, access::write> combinedTarget [[texture(8)]],
    texture2d<float, access::write> result [[texture(9)]],
    texture2d<float, access::write> extinctionOut [[texture(10)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  float2 q = (float2(gid) + 0.5f) * 0.25f;
  float4 wy = yPlanes.sample(integratedLinear, q);
  float4 wab = abPlanes.sample(integratedLinear, q);
  float y0 = yBase.sample(integratedLinear, q).x;
  float ab0 = abBase.sample(integratedLinear, q).x;
  float4 source = original.read(gid); float4 broadValue = broad.read(gid);
  float targetY = y0 * broadValue.x; float2 targetAB = ab0 * broadValue.yz;
  float2 position = planePhysicalPosition(gid, p);
  for (uint i = 0; i < 4; ++i) {
    PlaneFitModel model = models[i];
    float3 fitted = model.valid ? planeEvaluate(model, position) : broadValue.xyz;
    float3 manual = float3(p.planeManualY[i], p.planeManualA[i], p.planeManualB[i]);
    float3 target = mix(manual, fitted, clamp01(p.planeSourceMix[i]));
    target.x += p.planeYOffset[i]; target.y += p.planeABias[i]; target.z += p.planeBBias[i];
    targetY += wy[i] * mix(broadValue.x, target.x, clamp01(p.planeToneInfluence[i]));
    targetAB += wab[i] * mix(broadValue.yz, target.yz, clamp01(p.planeChromaInfluence[i]));
  }
  float protect = clamp01(protection.read(gid).x);
  float yRetain = clamp01(p.broadRetention + (1.0f - p.broadRetention) *
      protect * clamp01(p.detailStructurePreserve));
  float3 planeBroad = float3(mix(targetY, broadValue.x, yRetain),
                             mix(targetAB, broadValue.yz, clamp01(p.broadRetention)));
  float maskValue = 1.0f;
  if (p.hasMask != 0) {
    maskValue = clamp01(readComponent(mask, maskLayout, gid, 0));
    if (p.invertMask != 0) maskValue = 1.0f - maskValue;
  }
  float gate = clamp01(p.amount) * maskValue;
  float4 fineValue = fineBlur.read(gid);
  float3 fine = source.xyz - fineValue.xyz;
  float3 medium = fineValue.xyz - broadValue.xyz;
  float occY = 1.0f - y0, occAB = 1.0f - ab0;
  float structureKeep = protect * clamp01(p.detailStructurePreserve);
  float fineY = 1.0f - gate * occY * clamp01(p.fineExtinction) * (1.0f - structureKeep);
  float fineAB = 1.0f - gate * occAB * clamp01(p.fineExtinction) * (1.0f - structureKeep);
  float mediumY = 1.0f - gate * occY * clamp01(p.mediumExtinction) * (1.0f - structureKeep);
  float mediumAB = 1.0f - gate * occAB * clamp01(p.mediumExtinction) * (1.0f - structureKeep);
  float3 base = mix(broadValue.xyz, planeBroad, gate);
  float3 reconstructed = base + float3(fineY * fine.x + mediumY * medium.x,
      fineAB * fine.y + mediumAB * medium.y, fineAB * fine.z + mediumAB * medium.z);
  combinedTarget.write(float4(planeBroad, source.w), gid);
  result.write(float4(reconstructed, source.w), gid);
  extinctionOut.write(float4(0.5f * (gate * occY * (p.fineExtinction + p.mediumExtinction))), gid);
}

kernel void pigment_plane_apply_veil(
    texture2d<float, access::read> original [[texture(0)]],
    texture2d<float, access::read> preVeil [[texture(1)]],
    constant IntegratedParams& p [[buffer(0)]],
    texture2d<float, access::write> preSoftness [[texture(2)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  float2 canonical = float2(p.originX + float(gid.x) / max(p.renderScaleX, 1.0e-6f),
                            p.originY + float(gid.y) / max(p.renderScaleY, 1.0e-6f));
  float scale = max(8.0f, p.veilScale); float irregularity = clamp01(p.veilIrregularity);
  float warp = integratedNoise(canonical / (scale * 1.7f), p.veilSeed + 97u) * irregularity * 0.35f;
  float2 q = canonical / scale + float2(warp, -0.7f * warp);
  float n = integratedNoise(q, p.veilSeed) + irregularity * 0.48f *
      integratedNoise(q * 2.03f, p.veilSeed + 17u);
  n /= 1.0f + irregularity * 0.48f;
  float v = 0.5f + 0.5f * tanh(n * exp2(clamp(p.veilContrast, -2.0f, 2.0f)));
  float modulation = 1.0f + clamp01(p.veilAmount) * 0.7f * (v - 0.5f);
  float4 o = original.read(gid), effect = preVeil.read(gid);
  preSoftness.write(float4(o.xyz + modulation * (effect.xyz - o.xyz), o.w), gid);
}

kernel void pigment_plane_local_softness_blend(
    texture2d<float, access::read> sharp [[texture(0)]],
    texture2d<float, access::read> soft [[texture(1)]],
    const device float* mask [[buffer(0)]],
    constant ImageLayout& maskLayout [[buffer(1)]],
    constant IntegratedParams& p [[buffer(2)]],
    texture2d<float, access::write> output [[texture(2)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  float gate = 1.0f;
  if (p.hasMask != 0) {
    gate = clamp01(readComponent(mask, maskLayout, gid, 0));
    if (p.invertMask != 0) gate = 1.0f - gate;
  }
  float4 value = mix(sharp.read(gid), soft.read(gid), clamp01(p.localSoftness) * gate);
  value.w = sharp.read(gid).w; output.write(value, gid);
}

kernel void pigment_plane_final(
    const device float* sourceBuffer [[buffer(0)]],
    device float* destinationBuffer [[buffer(1)]],
    constant ImageLayout& sourceLayout [[buffer(2)]],
    constant ImageLayout& destinationLayout [[buffer(3)]],
    const device float* planeMap [[buffer(4)]],
    constant ImageLayout& planeLayout [[buffer(5)]],
    constant IntegratedParams& p [[buffer(6)]],
    const device PlaneFitModel* models [[buffer(7)]],
    texture2d<float, access::read> original [[texture(0)]],
    texture2d<float, access::read> fineBlur [[texture(1)]],
    texture2d<float, access::read> broad [[texture(2)]],
    texture2d<float, access::sample> yPlanes [[texture(3)]],
    texture2d<float, access::sample> yBase [[texture(4)]],
    texture2d<float, access::sample> abPlanes [[texture(5)]],
    texture2d<float, access::sample> abBase [[texture(6)]],
    texture2d<float, access::read> combinedTarget [[texture(7)]],
    texture2d<float, access::read> protection [[texture(8)]],
    texture2d<float, access::read> extinction [[texture(9)]],
    texture2d<float, access::read> preVeil [[texture(10)]],
    texture2d<float, access::read> preSoftness [[texture(11)]],
    texture2d<float, access::read> finalYab [[texture(12)]],
    uint2 gid [[thread_position_in_grid]]) {
  if (gid.x >= p.width || gid.y >= p.height) return;
  float4 o = original.read(gid); float4 result = finalYab.read(gid);
  uint view = p.debugView; float3 rgb;
  if (view == 0 && (p.amount <= 0.0f || p.mix <= 0.0f || p.hasPlaneMap == 0)) {
    uint outputBase = destinationLayout.startFloat + gid.y * destinationLayout.rowFloats +
                      gid.x * destinationLayout.components;
    destinationBuffer[outputBase] = readComponent(sourceBuffer, sourceLayout, gid, 0);
    destinationBuffer[outputBase + 1] = readComponent(sourceBuffer, sourceLayout, gid, 1);
    destinationBuffer[outputBase + 2] = readComponent(sourceBuffer, sourceLayout, gid, 2);
    if (destinationLayout.components == 4) {
      destinationBuffer[outputBase + 3] = readComponent(sourceBuffer, sourceLayout, gid, 3);
    }
    return;
  }
  float2 q = (float2(gid) + 0.5f) * 0.25f;
  float4 rawPlanes; float rawBase;
  rawPlaneMembership(planeMap, planeLayout, gid, p, rawPlanes, rawBase);
  uint selected = min(p.debugPlane, 3u);
  if (view == 27 || view == 28) {
    float4 rawMap = p.hasPlaneMap != 0 ? clamp(float4(readComponent(planeMap, planeLayout, gid, 0),
        readComponent(planeMap, planeLayout, gid, 1), readComponent(planeMap, planeLayout, gid, 2),
        readComponent(planeMap, planeLayout, gid, 3)), 0.0f, 1.0f) : float4(0.0f);
    float4 values = view == 27 ? rawMap : rawPlanes;
    rgb = p.debugPlane == 4 ? clamp(values.xyz + values.w, 0.0f, 1.0f) : float3(values[selected]);
  } else if (view == 29) rgb = float3(rawBase);
  else if (view == 30 || view == 31) {
    float4 values = view == 30 ? yPlanes.sample(integratedLinear, q) :
                                abPlanes.sample(integratedLinear, q);
    rgb = p.debugPlane == 4 ? clamp(values.xyz + values.w, 0.0f, 1.0f) : float3(values[selected]);
  } else if (view == 32 || view == 33) {
    PlaneFitModel model = models[selected];
    float3 target = model.valid ? planeEvaluate(model, planePhysicalPosition(gid, p)) : o.xyz;
    if (view == 32) target = float3(target.x, o.yz); else target = float3(o.x, target.yz);
    rgb = integratedYabToRgb(target, p);
  } else if (view == 34) rgb = integratedYabToRgb(float3(combinedTarget.read(gid).x, o.yz), p);
  else if (view == 35) rgb = integratedYabToRgb(float3(o.x, combinedTarget.read(gid).yz), p);
  else if (view == 36) rgb = integratedYabToRgb(broad.read(gid).xyz, p);
  else if (view == 37) rgb = clamp(float3(0.5f) + 0.45f * integratedYabToRgb(o.xyz - fineBlur.read(gid).xyz, p), 0.0f, 1.0f);
  else if (view == 38) rgb = clamp(float3(0.5f) + 0.45f * integratedYabToRgb(fineBlur.read(gid).xyz - broad.read(gid).xyz, p), 0.0f, 1.0f);
  else if (view == 39) rgb = float3(clamp01(extinction.read(gid).x));
  else if (view == 40) rgb = float3(clamp01(protection.read(gid).x));
  else if (view == 41) rgb = integratedYabToRgb(preVeil.read(gid).xyz, p);
  else if (view == 42) rgb = integratedYabToRgb(preSoftness.read(gid).xyz, p);
  else if (view == 43) rgb = float3(clamp01(models[selected].fitError));
  else if (view == 44) rgb = clamp(float3(0.5f) + 0.45f * integratedYabToRgb(result.xyz - o.xyz, p), 0.0f, 1.0f);
  else rgb = mix(integratedYabToRgb(o.xyz, p), integratedYabToRgb(result.xyz, p), clamp01(p.mix));
  float alpha = o.w;
  if (view == 0 && p.premultiplied != 0 && abs(alpha) <= 1.0e-6f) {
    rgb = float3(readComponent(sourceBuffer, sourceLayout, gid, 0),
                 readComponent(sourceBuffer, sourceLayout, gid, 1),
                 readComponent(sourceBuffer, sourceLayout, gid, 2));
  } else if (p.premultiplied != 0) rgb *= alpha;
  uint outputBase = destinationLayout.startFloat + gid.y * destinationLayout.rowFloats +
                    gid.x * destinationLayout.components;
  destinationBuffer[outputBase] = rgb.x; destinationBuffer[outputBase + 1] = rgb.y;
  destinationBuffer[outputBase + 2] = rgb.z;
  if (destinationLayout.components == 4) destinationBuffer[outputBase + 3] = alpha;
}
