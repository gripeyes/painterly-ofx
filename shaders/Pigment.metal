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
