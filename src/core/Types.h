#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace pigment {

struct RectI {
  int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
  int width() const noexcept { return std::max(0, x2 - x1); }
  int height() const noexcept { return std::max(0, y2 - y1); }
  bool empty() const noexcept { return x2 <= x1 || y2 <= y1; }
  bool contains(int x, int y) const noexcept {
    return x >= x1 && x < x2 && y >= y1 && y < y2;
  }
};

inline RectI intersect(RectI a, RectI b) noexcept {
  return {std::max(a.x1, b.x1), std::max(a.y1, b.y1),
          std::min(a.x2, b.x2), std::min(a.y2, b.y2)};
}

inline RectI expand(RectI r, int x, int y) noexcept {
  return {r.x1 - x, r.y1 - y, r.x2 + x, r.y2 + y};
}

template <class T> struct PlaneView {
  T* data = nullptr;
  std::ptrdiff_t rowStride = 0;
  RectI bounds{};

  T& at(int x, int y) const noexcept {
    return data[(y - bounds.y1) * rowStride + (x - bounds.x1)];
  }
  explicit operator bool() const noexcept { return data != nullptr; }
};

using FloatPlaneView = PlaneView<float>;
using ConstFloatPlaneView = PlaneView<const float>;

class OwnedPlane {
 public:
  OwnedPlane() = default;
  explicit OwnedPlane(RectI bounds, float value = 0.0f)
      : bounds_(bounds), pixels_(static_cast<std::size_t>(bounds.width()) *
                                static_cast<std::size_t>(bounds.height()), value) {}
  FloatPlaneView view() noexcept { return {pixels_.data(), bounds_.width(), bounds_}; }
  ConstFloatPlaneView view() const noexcept { return {pixels_.data(), bounds_.width(), bounds_}; }
  RectI bounds() const noexcept { return bounds_; }

 private:
  RectI bounds_{};
  std::vector<float> pixels_;
};

struct YabPixel { float y = 0.0f, a = 0.0f, b = 0.0f; };

struct YabPlanes {
  FloatPlaneView y, a, b;
};
struct ConstYabPlanes {
  ConstFloatPlaneView y, a, b;
};

inline ConstYabPlanes asConst(const YabPlanes& p) noexcept {
  return {{p.y.data, p.y.rowStride, p.y.bounds},
          {p.a.data, p.a.rowStride, p.a.bounds},
          {p.b.data, p.b.rowStride, p.b.bounds}};
}

class OwnedYabPlanes {
 public:
  explicit OwnedYabPlanes(RectI bounds) : y_(bounds), a_(bounds), b_(bounds) {}
  YabPlanes view() noexcept { return {y_.view(), a_.view(), b_.view()}; }
  ConstYabPlanes view() const noexcept { return {y_.view(), a_.view(), b_.view()}; }
 private:
  OwnedPlane y_, a_, b_;
};

struct ConstImageView {
  const float* data = nullptr;
  std::ptrdiff_t rowStride = 0;
  RectI bounds{};
  int components = 4;
  const float* pixel(int x, int y) const noexcept {
    return data + (y - bounds.y1) * rowStride +
           (x - bounds.x1) * components;
  }
};

struct ImageView {
  float* data = nullptr;
  std::ptrdiff_t rowStride = 0;
  RectI bounds{};
  int components = 4;
  float* pixel(int x, int y) const noexcept {
    return data + (y - bounds.y1) * rowStride +
           (x - bounds.x1) * components;
  }
};

struct ImageGeometry {
  double pixelAspect = 1.0;
  double renderScaleX = 1.0;
  double renderScaleY = 1.0;
};

}  // namespace pigment

