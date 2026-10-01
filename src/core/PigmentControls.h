#pragma once
#include "core/Phase4Types.h"
#include <algorithm>
#include <cmath>

namespace pigment {
// Artist controls are an opt-in mapping, not a new appearance algorithm.
struct PigmentControls {
  float pictorialScale=48, structureLock=.75f;
  float lumaOrganization=.5f, chromaOrganization=2.f/3.f;
  float lumaComplexity=.5f, chromaComplexity=.25f;
  float chromaSpread=1.f/3.f, spill=.25f, spillReach=48, directionality=.5f;
};
inline Phase4Params mapPigmentControls(const PigmentControls &c,Phase4Params p={}) {
  auto unit=[](float v){return std::isfinite(v)?std::clamp(v,0.f,1.f):0.f;};
  float scale=std::isfinite(c.pictorialScale)?std::clamp(c.pictorialScale,4.f,256.f):48.f;
  float lock=unit(c.structureLock),y=unit(c.lumaOrganization),ab=unit(c.chromaOrganization),spread=unit(c.chromaSpread);
  // Geometry vocabulary and cuts use a common physical unit; organization
  // zero bypasses synthesis. Chroma can merge twice as broadly as luminance.
  p.representation=Phase4Representation::Poisson;
  p.plateScale=scale;p.plateOverlap=.55f;p.lumaChromaCoupling=.35f;
  p.lumaChunkScale=scale*y;p.chromaChunkScale=2*scale*ab;
  // Lock coordinates hierarchy discrimination and Spill permeability, not
  // source-detail reintegration or a hidden blur radius.
  p.boundaryLock=lock;p.mergeSelectivity=.3f+.4f*lock;
  p.internalVariation=.6f-.2f*lock;p.structureRespect=.2f+.8f*lock;
  p.yGradientComplexity=unit(c.lumaComplexity);
  p.abGradientComplexity=unit(c.chromaComplexity);
  // Support extent depends on scale/spread, never organization/chunk cuts.
  p.chromaSupportRatio=1+3*spread;p.ySupport=1;p.abSupport=.75f+.75f*spread;
  p.spillAmount=unit(c.spill);
  p.lumaSpill=.3f*y*(1-.5f*lock);
  p.chromaSpill=ab*(.5f+.5f*spread);
  p.spillReach=std::isfinite(c.spillReach)?std::clamp(c.spillReach,0.f,256.f):0;
  p.spillAsymmetry=unit(c.directionality);
  return p; // Preserve expert plate count/budget and per-plate overrides.
}
}
