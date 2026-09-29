#pragma once

#include "core/Execution.h"
#include "core/PictorialPlanes.h"
#include "core/Types.h"

#include <array>
#include <vector>

namespace pigment {

enum class PlaneSourceMode { Manual = 0, AutoConvexPalette, AutoSpatialLayers, Hybrid };

struct SoftPlateConstructionParams {
  PlaneSourceMode source = PlaneSourceMode::AutoConvexPalette;
  float automaticOccupancy = 1.0f;
  float automaticPlaneStrength = 1.0f;
  float spatialCoherence = 0.5f;
  float colorCoherence = 0.5f;
  float hybridGuidance = 0.75f;
};

struct SoftPlateSetView {
  std::array<FloatPlaneView, kPictorialPlaneCount> raw{};
  std::array<FloatPlaneView, kPictorialPlaneCount> normalized{};
  FloatPlaneView base{}, confidence{}, reconstructionError{};
  int planeCount = kPictorialPlaneCount;
};
struct ConstSoftPlateSetView {
  std::array<ConstFloatPlaneView, kPictorialPlaneCount> raw{};
  std::array<ConstFloatPlaneView, kPictorialPlaneCount> normalized{};
  ConstFloatPlaneView base{}, confidence{}, reconstructionError{};
  int planeCount = kPictorialPlaneCount;
};
struct SpatialControlVertex {
  float x = 0.0f, y = 0.0f;
  YabPixel color{};
  std::array<float, kPictorialPlaneCount> weights{};
};

class OwnedSoftPlateSet {
 public:
  explicit OwnedSoftPlateSet(RectI bounds);
  SoftPlateSetView view() noexcept;
  ConstSoftPlateSetView view() const noexcept;
  std::array<YabPixel, kPictorialPlaneCount>& palette() noexcept { return palette_; }
  const std::array<YabPixel, kPictorialPlaneCount>& palette() const noexcept { return palette_; }
  std::vector<SpatialControlVertex>& controlVertices() noexcept { return vertices_; }
  const std::vector<SpatialControlVertex>& controlVertices() const noexcept { return vertices_; }
 private:
  std::array<OwnedPlane, kPictorialPlaneCount> raw_;
  std::array<OwnedPlane, kPictorialPlaneCount> normalized_;
  OwnedPlane base_, confidence_, reconstructionError_;
  std::array<YabPixel, kPictorialPlaneCount> palette_{};
  std::vector<SpatialControlVertex> vertices_;
};

// Simplified deterministic convex palette model; not Aksoy/Disney SCU or PCU.
void buildConvexPalettePlates(ConstYabPlanes source, ScalarFieldView structureProtection,
                              const SoftPlateConstructionParams& params,
                              OwnedSoftPlateSet& result,
                              const ExecutionContext& execution = {});
// RGBXY-inspired two-stage model retaining a rich triangular control mesh.
void buildSpatialLayerPlates(ConstYabPlanes source, ScalarFieldView structureProtection,
                             const SoftPlateConstructionParams& params,
                             OwnedSoftPlateSet& result,
                             const ExecutionContext& execution = {});
void applyHybridPlateGuidance(ConstSoftPlateSetView automatic,
                              ConstPlaneMembershipPlanes manual, float guidance,
                              SoftPlateSetView destination,
                              const ExecutionContext& execution = {});

}  // namespace pigment
