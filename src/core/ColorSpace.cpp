#include "core/ColorSpace.h"

#include <cmath>
#include <stdexcept>

namespace pigment {
namespace {
using M = std::array<double, 9>;

M inverse(const M& m) {
  const double d = m[0] * (m[4] * m[8] - m[5] * m[7]) -
                   m[1] * (m[3] * m[8] - m[5] * m[6]) +
                   m[2] * (m[3] * m[7] - m[4] * m[6]);
  if (std::abs(d) < 1e-15) throw std::runtime_error("singular gamut matrix");
  return {(m[4]*m[8]-m[5]*m[7])/d, (m[2]*m[7]-m[1]*m[8])/d, (m[1]*m[5]-m[2]*m[4])/d,
          (m[5]*m[6]-m[3]*m[8])/d, (m[0]*m[8]-m[2]*m[6])/d, (m[2]*m[3]-m[0]*m[5])/d,
          (m[3]*m[7]-m[4]*m[6])/d, (m[1]*m[6]-m[0]*m[7])/d, (m[0]*m[4]-m[1]*m[3])/d};
}

M matrixFor(WorkingGamut gamut) {
  switch (gamut) {
    case WorkingGamut::ACEScg:
      return {0.6624541811, 0.1340042065, 0.1561876870,
              0.2722287168, 0.6740817658, 0.0536895174,
             -0.0055746495, 0.0040607335, 1.0103391003};
    case WorkingGamut::LinearRec709:
      return {0.4123907993, 0.3575843394, 0.1804807884,
              0.2126390059, 0.7151686788, 0.0721923154,
              0.0193308187, 0.1191947798, 0.9505321522};
    case WorkingGamut::LinearRec2020:
      return {0.6369580483, 0.1446169036, 0.1688809752,
              0.2627002120, 0.6779980715, 0.0593017165,
              0.0000000000, 0.0280726930, 1.0609850577};
    case WorkingGamut::DisplayP3D65:
      return {0.4865709486, 0.2656676932, 0.1982172852,
              0.2289745641, 0.6917385218, 0.0792869141,
              0.0000000000, 0.0451133819, 1.0439443689};
  }
  throw std::invalid_argument("unknown working gamut");
}
}  // namespace

MatrixOpponentTransform::MatrixOpponentTransform(WorkingGamut gamut)
    : rgbToXyz_(matrixFor(gamut)), xyzToRgb_(inverse(rgbToXyz_)) {
  whiteX_ = rgbToXyz_[0] + rgbToXyz_[1] + rgbToXyz_[2];
  const double whiteY = rgbToXyz_[3] + rgbToXyz_[4] + rgbToXyz_[5];
  whiteZ_ = rgbToXyz_[6] + rgbToXyz_[7] + rgbToXyz_[8];
  whiteX_ /= whiteY;
  whiteZ_ /= whiteY;
}

OpponentMatrixData opponentMatrixData(WorkingGamut gamut) {
  const M rgbToXyz = matrixFor(gamut);
  const M xyzToRgb = inverse(rgbToXyz);
  const double whiteY = rgbToXyz[3] + rgbToXyz[4] + rgbToXyz[5];
  OpponentMatrixData result;
  for (std::size_t i = 0; i < 9; ++i) {
    result.rgbToXyz[i] = static_cast<float>(rgbToXyz[i]);
    result.xyzToRgb[i] = static_cast<float>(xyzToRgb[i]);
  }
  result.whiteX = static_cast<float>((rgbToXyz[0] + rgbToXyz[1] + rgbToXyz[2]) / whiteY);
  result.whiteZ = static_cast<float>((rgbToXyz[6] + rgbToXyz[7] + rgbToXyz[8]) / whiteY);
  return result;
}

YabPixel MatrixOpponentTransform::toYab(const std::array<float, 3>& rgb) const noexcept {
  const double x = rgbToXyz_[0]*rgb[0] + rgbToXyz_[1]*rgb[1] + rgbToXyz_[2]*rgb[2];
  const double y = rgbToXyz_[3]*rgb[0] + rgbToXyz_[4]*rgb[1] + rgbToXyz_[5]*rgb[2];
  const double z = rgbToXyz_[6]*rgb[0] + rgbToXyz_[7]*rgb[1] + rgbToXyz_[8]*rgb[2];
  return {static_cast<float>(y), static_cast<float>(x / whiteX_ - y),
          static_cast<float>(z / whiteZ_ - y)};
}

std::array<float, 3> MatrixOpponentTransform::toRgb(const YabPixel& p) const noexcept {
  const double x = (p.a + p.y) * whiteX_;
  const double y = p.y;
  const double z = (p.b + p.y) * whiteZ_;
  return {static_cast<float>(xyzToRgb_[0]*x + xyzToRgb_[1]*y + xyzToRgb_[2]*z),
          static_cast<float>(xyzToRgb_[3]*x + xyzToRgb_[4]*y + xyzToRgb_[5]*z),
          static_cast<float>(xyzToRgb_[6]*x + xyzToRgb_[7]*y + xyzToRgb_[8]*z)};
}

}  // namespace pigment
