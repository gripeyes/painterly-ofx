#pragma once

#include "core/Execution.h"
#include "core/Types.h"

namespace pigment {

void copyPlane(ConstFloatPlaneView source, FloatPlaneView destination,
               RectI region, const ExecutionContext& execution = {});

// Sigma is expressed in pixels of the supplied plane. Values at and beyond the
// plane bounds are extended by clamping to the closest valid sample.
void gaussianBlurPlane(ConstFloatPlaneView source, FloatPlaneView destination,
                       float sigmaX, float sigmaY,
                       const ExecutionContext& execution = {});

void gaussianBlurYab(ConstYabPlanes source, YabPlanes destination,
                     float sigmaX, float sigmaY,
                     const ExecutionContext& execution = {});

// Linear-time box statistics primitive reserved for guided-filter and
// multiscale work. Radius zero is an exact copy.
void boxBlurPlane(ConstFloatPlaneView source, FloatPlaneView destination,
                  int radiusX, int radiusY,
                  const ExecutionContext& execution = {});

}  // namespace pigment
