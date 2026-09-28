#pragma once

#include <array>
#include "core/Types.h"

namespace pigment {

enum class WorkingGamut { ACEScg = 0, LinearRec709, LinearRec2020, DisplayP3D65 };

class OpponentTransform {
 public:
  virtual ~OpponentTransform() = default;
  virtual YabPixel toYab(const std::array<float, 3>& rgb) const noexcept = 0;
  virtual std::array<float, 3> toRgb(const YabPixel& yab) const noexcept = 0;
};

class MatrixOpponentTransform final : public OpponentTransform {
 public:
  explicit MatrixOpponentTransform(WorkingGamut gamut);
  YabPixel toYab(const std::array<float, 3>& rgb) const noexcept override;
  std::array<float, 3> toRgb(const YabPixel& yab) const noexcept override;
 private:
  std::array<double, 9> rgbToXyz_{};
  std::array<double, 9> xyzToRgb_{};
  double whiteX_ = 1.0;
  double whiteZ_ = 1.0;
};

}  // namespace pigment

