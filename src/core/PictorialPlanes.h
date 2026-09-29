#pragma once

#include "core/Execution.h"
#include "core/Types.h"

#include <array>

namespace pigment {

constexpr int kPictorialPlaneCount = 4;
constexpr int kQuadraticBasisSize = 6;

struct PlaneMembership {
  std::array<float, kPictorialPlaneCount> plane{};
  float base = 1.0f;
};

struct PlaneControl {
  bool enabled = true;
  float amount = 1.0f;
  float sourceMix = 1.0f;
  YabPixel manualTarget{};
  float yOffset = 0.0f;
  float toneInfluence = 1.0f;
  float aBias = 0.0f;
  float bBias = 0.0f;
  float chromaInfluence = 1.0f;
};

struct PictorialPlanesParams {
  std::array<PlaneControl, kPictorialPlaneCount> planes{};
  float fineExtinction = 0.9f;
  float mediumExtinction = 0.8f;
  float broadRetention = 0.2f;
  float detailStructurePreserve = 0.7f;
  float yTransitionWidth = 12.0f;
  float abTransitionWidth = 48.0f;
  float transitionStructureRespect = 0.7f;
  float localSoftness = 0.0f;
};

struct PlaneMembershipPlanes {
  std::array<FloatPlaneView, kPictorialPlaneCount> plane{};
  FloatPlaneView base{};
};

struct ConstPlaneMembershipPlanes {
  std::array<ConstFloatPlaneView, kPictorialPlaneCount> plane{};
  ConstFloatPlaneView base{};
};

class OwnedPlaneMemberships {
 public:
  explicit OwnedPlaneMemberships(RectI bounds);
  PlaneMembershipPlanes view() noexcept;
  ConstPlaneMembershipPlanes view() const noexcept;

 private:
  std::array<OwnedPlane, kPictorialPlaneCount> planes_;
  OwnedPlane base_;
};

struct QuadraticPlaneModel {
  std::array<std::array<float, kQuadraticBasisSize>, 3> coefficients{};
  float centerX = 0.0f;
  float centerY = 0.0f;
  float scaleX = 1.0f;
  float scaleY = 1.0f;
  float fitError = 0.0f;
  int order = 0;  // 0 constant, 1 affine, 2 quadratic.
  bool valid = false;
};

PlaneMembership normalizePlaneMembership(
    const std::array<float, kPictorialPlaneCount>& channels,
    const std::array<bool, kPictorialPlaneCount>& enabled,
    const std::array<float, kPictorialPlaneCount>& amounts) noexcept;

void normalizePlaneMembershipFields(
    const std::array<ConstFloatPlaneView, kPictorialPlaneCount>& channels,
    const std::array<bool, kPictorialPlaneCount>& enabled,
    const std::array<float, kPictorialPlaneCount>& amounts,
    PlaneMembershipPlanes destination,
    const ExecutionContext& execution = {});

// Width is the full-resolution 10-90% step-response distance. The returned
// lambda is in the supplied pixel coordinate system.
float screenedTransitionLambda(float width10To90Pixels) noexcept;

void propagatePlaneMemberships(ConstPlaneMembershipPlanes source,
                               ScalarFieldView structureProtection,
                               float width10To90,
                               float structureRespect,
                               const ImageGeometry& geometry,
                               PlaneMembershipPlanes destination,
                               const ExecutionContext& execution = {});

QuadraticPlaneModel fitQuadraticPlane(ConstYabPlanes source,
                                      ConstFloatPlaneView membership,
                                      const ConstFloatPlaneView* alpha = nullptr,
                                      const ExecutionContext& execution = {});

YabPixel evaluateQuadraticPlane(const QuadraticPlaneModel& model,
                                float x, float y) noexcept;

float residualSurvival(float gate, float occupancy, float extinction,
                       float structureProtection,
                       float detailStructurePreserve) noexcept;

}  // namespace pigment
