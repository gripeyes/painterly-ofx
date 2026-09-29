#include "core/PictorialPlanes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace pigment {
namespace {

constexpr double kTenNinety = 3.2188758248682006;
constexpr double kEpsilon = 1.0e-10;

float clamp01(float v) noexcept { return std::max(0.0f, std::min(1.0f, v)); }

std::array<double, 6> basis(double x, double y,
                            const QuadraticPlaneModel& model) {
  const double u = (x - model.centerX) / std::max(1.0e-6f, model.scaleX);
  const double v = (y - model.centerY) / std::max(1.0e-6f, model.scaleY);
  return {1.0, u, v, u * u, u * v, v * v};
}

bool solveSystem(std::array<std::array<double, 6>, 6> matrix,
                 std::array<double, 6> rhs, int dimensions,
                 std::array<float, 6>& result) {
  for (int column = 0; column < dimensions; ++column) {
    int pivot = column;
    for (int row = column + 1; row < dimensions; ++row)
      if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column])) pivot = row;
    if (std::abs(matrix[pivot][column]) < 1.0e-12) return false;
    std::swap(matrix[pivot], matrix[column]);
    std::swap(rhs[pivot], rhs[column]);
    const double inverse = 1.0 / matrix[column][column];
    for (int c = column; c < dimensions; ++c) matrix[column][c] *= inverse;
    rhs[column] *= inverse;
    for (int row = 0; row < dimensions; ++row) {
      if (row == column) continue;
      const double factor = matrix[row][column];
      for (int c = column; c < dimensions; ++c)
        matrix[row][c] -= factor * matrix[column][c];
      rhs[row] -= factor * rhs[column];
    }
  }
  result.fill(0.0f);
  for (int i = 0; i < dimensions; ++i) result[i] = static_cast<float>(rhs[i]);
  return true;
}

float membershipAt(ConstFloatPlaneView p, int x, int y) noexcept {
  return p.at(x, y);
}

template <class Function>
void rows(const ExecutionContext& execution, int begin, int end, Function&& function) {
  execution.parallelRows(begin, end, [&](int first, int last) {
    for (int y = first; y < last; ++y) function(y);
  });
}

}  // namespace

OwnedPlaneMemberships::OwnedPlaneMemberships(RectI bounds)
    : planes_{OwnedPlane(bounds), OwnedPlane(bounds), OwnedPlane(bounds), OwnedPlane(bounds)},
      base_(bounds, 1.0f) {}

PlaneMembershipPlanes OwnedPlaneMemberships::view() noexcept {
  return {{planes_[0].view(), planes_[1].view(), planes_[2].view(), planes_[3].view()},
          base_.view()};
}

ConstPlaneMembershipPlanes OwnedPlaneMemberships::view() const noexcept {
  return {{planes_[0].view(), planes_[1].view(), planes_[2].view(), planes_[3].view()},
          base_.view()};
}

PlaneMembership normalizePlaneMembership(
    const std::array<float, 4>& channels, const std::array<bool, 4>& enabled,
    const std::array<float, 4>& amounts) noexcept {
  PlaneMembership result;
  float sum = 0.0f;
  for (int i = 0; i < 4; ++i) {
    const float value = std::isfinite(channels[i]) ? clamp01(channels[i]) : 0.0f;
    result.plane[i] = enabled[i] ? value * clamp01(amounts[i]) : 0.0f;
    sum += result.plane[i];
  }
  const float occupancy = clamp01(sum);
  if (sum > 1.0e-8f) {
    const float scale = occupancy / sum;
    for (float& value : result.plane) value *= scale;
  } else {
    result.plane.fill(0.0f);
  }
  result.base = 1.0f - occupancy;
  return result;
}

void normalizePlaneMembershipFields(
    const std::array<ConstFloatPlaneView, 4>& channels,
    const std::array<bool, 4>& enabled, const std::array<float, 4>& amounts,
    PlaneMembershipPlanes destination, const ExecutionContext& execution) {
  const RectI bounds = destination.base.bounds;
  rows(execution, bounds.y1, bounds.y2, [&](int y) {
    for (int x = bounds.x1; x < bounds.x2; ++x) {
      std::array<float, 4> sample{};
      for (int i = 0; i < 4; ++i) sample[i] = channels[i].at(x, y);
      const auto membership = normalizePlaneMembership(sample, enabled, amounts);
      for (int i = 0; i < 4; ++i) destination.plane[i].at(x, y) = membership.plane[i];
      destination.base.at(x, y) = membership.base;
    }
  });
}

float screenedTransitionLambda(float width) noexcept {
  if (!(width > 0.0f) || !std::isfinite(width)) return 0.0f;
  const double value = static_cast<double>(width) / kTenNinety;
  return static_cast<float>(value * value);
}

void propagatePlaneMemberships(ConstPlaneMembershipPlanes source,
                               ScalarFieldView structureProtection, float width,
                               float structureRespect, const ImageGeometry& geometry,
                               PlaneMembershipPlanes destination,
                               const ExecutionContext& execution) {
  const RectI bounds = source.base.bounds;
  if (width <= 1.0e-5f) {
    rows(execution, bounds.y1, bounds.y2, [&](int y) {
      for (int x = bounds.x1; x < bounds.x2; ++x) {
        for (int i = 0; i < 4; ++i)
          destination.plane[i].at(x, y) = source.plane[i].at(x, y);
        destination.base.at(x, y) = source.base.at(x, y);
      }
    });
    return;
  }
  const float widthX = width * static_cast<float>(geometry.renderScaleX /
      std::max(geometry.pixelAspect, 1.0e-9));
  const float widthY = width * static_cast<float>(geometry.renderScaleY);
  const float lambdaX = screenedTransitionLambda(widthX);
  const float lambdaY = screenedTransitionLambda(widthY);
  const float respect = clamp01(structureRespect);

  std::array<OwnedPlane, 5> a{OwnedPlane(bounds), OwnedPlane(bounds), OwnedPlane(bounds),
                              OwnedPlane(bounds), OwnedPlane(bounds)};
  std::array<OwnedPlane, 5> b{OwnedPlane(bounds), OwnedPlane(bounds), OwnedPlane(bounds),
                              OwnedPlane(bounds), OwnedPlane(bounds)};
  auto sourceChannel = [&](int i, int x, int y) {
    return i < 4 ? membershipAt(source.plane[i], x, y) : source.base.at(x, y);
  };
  for (int i = 0; i < 5; ++i)
    for (int y = bounds.y1; y < bounds.y2; ++y)
      for (int x = bounds.x1; x < bounds.x2; ++x) a[i].view().at(x, y) = sourceChannel(i, x, y);

  // The CPU reference favors calibration and determinism over speed. The Metal
  // path uses the fixed multiresolution schedule.
  constexpr int iterations = 320;
  for (int iteration = 0; iteration < iterations; ++iteration) {
    if (execution.cancelled()) return;
    rows(execution, bounds.y1, bounds.y2, [&](int y) {
      for (int x = bounds.x1; x < bounds.x2; ++x) {
        const float p = clamp01(structureProtection.at(x, y));
        const auto conductance = [&](int xx, int yy) {
          return std::max(1.0e-4f, 1.0f - respect * std::max(p, clamp01(structureProtection.at(xx, yy))));
        };
        const int xl = std::max(bounds.x1, x - 1), xr = std::min(bounds.x2 - 1, x + 1);
        const int yd = std::max(bounds.y1, y - 1), yu = std::min(bounds.y2 - 1, y + 1);
        const float gl = conductance(xl, y), gr = conductance(xr, y);
        const float gd = conductance(x, yd), gu = conductance(x, yu);
        const float denom = 1.0f + lambdaX * (gl + gr) + lambdaY * (gd + gu);
        for (int i = 0; i < 5; ++i) {
          const auto av = static_cast<const OwnedPlane&>(a[i]).view();
          const float rhs = sourceChannel(i, x, y) + lambdaX *
              (gl * av.at(xl, y) + gr * av.at(xr, y)) + lambdaY *
              (gd * av.at(x, yd) + gu * av.at(x, yu));
          b[i].view().at(x, y) = rhs / denom;
        }
      }
    });
    std::swap(a, b);
  }
  rows(execution, bounds.y1, bounds.y2, [&](int y) {
    for (int x = bounds.x1; x < bounds.x2; ++x) {
      float sum = 0.0f;
      for (int i = 0; i < 5; ++i)
        sum += std::max(0.0f, static_cast<const OwnedPlane&>(a[i]).view().at(x, y));
      const float inv = 1.0f / std::max(sum, 1.0e-8f);
      for (int i = 0; i < 4; ++i)
        destination.plane[i].at(x, y) = std::max(0.0f, static_cast<const OwnedPlane&>(a[i]).view().at(x, y)) * inv;
      destination.base.at(x, y) = std::max(0.0f, static_cast<const OwnedPlane&>(a[4]).view().at(x, y)) * inv;
    }
  });
}

QuadraticPlaneModel fitQuadraticPlane(ConstYabPlanes source,
                                      ConstFloatPlaneView membership,
                                      const ConstFloatPlaneView* alpha,
                                      const ExecutionContext&) {
  QuadraticPlaneModel model;
  const RectI bounds = source.y.bounds;
  double weightSum = 0.0, meanX = 0.0, meanY = 0.0;
  for (int y = bounds.y1; y < bounds.y2; ++y) for (int x = bounds.x1; x < bounds.x2; ++x) {
    const double coverage = alpha ? clamp01(std::abs(alpha->at(x, y))) : 1.0;
    const double w = clamp01(membership.at(x, y)) * coverage;
    weightSum += w; meanX += w * x; meanY += w * y;
  }
  if (weightSum < 1.0e-5) return model;
  model.centerX = static_cast<float>(meanX / weightSum);
  model.centerY = static_cast<float>(meanY / weightSum);
  double varianceX = 0.0, varianceY = 0.0;
  for (int y = bounds.y1; y < bounds.y2; ++y) for (int x = bounds.x1; x < bounds.x2; ++x) {
    const double coverage = alpha ? clamp01(std::abs(alpha->at(x, y))) : 1.0;
    const double w = clamp01(membership.at(x, y)) * coverage;
    varianceX += w * (x - model.centerX) * (x - model.centerX);
    varianceY += w * (y - model.centerY) * (y - model.centerY);
  }
  model.scaleX = static_cast<float>(std::max(1.0, std::sqrt(varianceX / weightSum)));
  model.scaleY = static_cast<float>(std::max(1.0, std::sqrt(varianceY / weightSum)));

  std::vector<float> robust(static_cast<std::size_t>(bounds.width()) * bounds.height(), 1.0f);
  auto fitPass = [&](int dimensions) {
    std::array<std::array<double, 6>, 6> normal{};
    std::array<std::array<double, 6>, 3> rhs{};
    for (int y = bounds.y1; y < bounds.y2; ++y) for (int x = bounds.x1; x < bounds.x2; ++x) {
      const double coverage = alpha ? clamp01(std::abs(alpha->at(x, y))) : 1.0;
      const auto index = static_cast<std::size_t>(y - bounds.y1) * bounds.width() + (x - bounds.x1);
      const double w = clamp01(membership.at(x, y)) * coverage * robust[index];
      const auto f = basis(x, y, model);
      for (int r = 0; r < dimensions; ++r) {
        for (int c = 0; c < dimensions; ++c) normal[r][c] += w * f[r] * f[c];
        rhs[0][r] += w * f[r] * source.y.at(x, y);
        rhs[1][r] += w * f[r] * source.a.at(x, y);
        rhs[2][r] += w * f[r] * source.b.at(x, y);
      }
    }
    const double trace = std::max(kEpsilon, normal[0][0]);
    for (int i = 1; i < dimensions; ++i)
      normal[i][i] += trace * (i < 3 ? 1.0e-7 : 1.0e-4);
    bool solved = true;
    for (int channel = 0; channel < 3; ++channel)
      solved &= solveSystem(normal, rhs[channel], dimensions, model.coefficients[channel]);
    return solved;
  };

  int dimensions = weightSum >= 32.0 ? 6 : weightSum >= 8.0 ? 3 : 1;
  while (dimensions > 1 && !fitPass(dimensions)) dimensions = dimensions == 6 ? 3 : 1;
  if (!fitPass(dimensions)) return model;
  model.order = dimensions == 6 ? 2 : dimensions == 3 ? 1 : 0;

  double residual2 = 0.0;
  for (int y = bounds.y1; y < bounds.y2; ++y) for (int x = bounds.x1; x < bounds.x2; ++x) {
    const auto predicted = evaluateQuadraticPlane(model, x, y);
    const double dy = source.y.at(x, y) - predicted.y;
    const double da = source.a.at(x, y) - predicted.a;
    const double db = source.b.at(x, y) - predicted.b;
    const double r2 = dy * dy + da * da + db * db;
    const double coverage = alpha ? clamp01(std::abs(alpha->at(x, y))) : 1.0;
    const double w = clamp01(membership.at(x, y)) * coverage;
    residual2 += w * r2;
  }
  const double scale2 = std::max(1.0e-10, residual2 / weightSum);
  for (int y = bounds.y1; y < bounds.y2; ++y) for (int x = bounds.x1; x < bounds.x2; ++x) {
    const auto predicted = evaluateQuadraticPlane(model, x, y);
    const double dy = source.y.at(x, y) - predicted.y;
    const double da = source.a.at(x, y) - predicted.a;
    const double db = source.b.at(x, y) - predicted.b;
    const double r2 = dy * dy + da * da + db * db;
    const auto index = static_cast<std::size_t>(y - bounds.y1) * bounds.width() + (x - bounds.x1);
    robust[index] = static_cast<float>(1.0 / (1.0 + r2 / scale2));
  }
  fitPass(dimensions);
  model.fitError = static_cast<float>(std::sqrt(scale2));
  model.valid = true;
  return model;
}

YabPixel evaluateQuadraticPlane(const QuadraticPlaneModel& model, float x, float y) noexcept {
  if (!model.valid && model.order == 0 && model.coefficients[0][0] == 0.0f &&
      model.coefficients[1][0] == 0.0f && model.coefficients[2][0] == 0.0f) return {};
  const auto f = basis(x, y, model);
  YabPixel result;
  float* channels[3] = {&result.y, &result.a, &result.b};
  for (int channel = 0; channel < 3; ++channel)
    for (int i = 0; i < 6; ++i) *channels[channel] += model.coefficients[channel][i] * static_cast<float>(f[i]);
  return result;
}

float residualSurvival(float gate, float occupancy, float extinction,
                       float protection, float preserve) noexcept {
  return 1.0f - clamp01(gate) * clamp01(occupancy) * clamp01(extinction) *
      (1.0f - clamp01(protection) * clamp01(preserve));
}

}  // namespace pigment
