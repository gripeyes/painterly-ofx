#pragma once

#include "core/Types.h"

#include <memory>
#include <vector>

namespace pigment {

// Render-local storage for multi-pass operators. An arena is intentionally not
// shared between renders; callers may reset and reuse it for subsequent passes.
class ScratchArena {
 public:
  FloatPlaneView acquirePlane(RectI bounds, float value = 0.0f) {
    planes_.push_back(std::make_unique<OwnedPlane>(bounds, value));
    return planes_.back()->view();
  }

  void reset() noexcept { planes_.clear(); }
  std::size_t planeCount() const noexcept { return planes_.size(); }

 private:
  std::vector<std::unique_ptr<OwnedPlane>> planes_;
};

}  // namespace pigment
