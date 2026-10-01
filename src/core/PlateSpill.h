#pragma once

#include "core/ChunkGradientSynthesis.h"

namespace pigment {
struct Phase4SpillTransport {
  std::vector<std::vector<float>> y,ab;
  float reach=0,structureRespect=0;
};
Phase4SpillTransport preparePhase4SpillTransport(const PublicPlateSet &plates,
    const SparseAffinityGraph &graph,const Phase4Params &params,const ExecutionContext &execution={});

struct Phase4SpillResult {
  RectI bounds{};
  std::vector<OwnedYabPlanes> plateAppearance;
  std::vector<OwnedPlane> influence;
  std::vector<OwnedPlane> influenceY, transportY, transportAB;
  OwnedYabPlanes composite;
  explicit Phase4SpillResult(RectI boundsIn)
      : bounds(boundsIn), composite(boundsIn) {}
};
struct SpectralSpillSample {
  int x=0,y=0,receiver=0;
  InteractionMaterial material;
  SpectralEncodeTrace encode;
  SpectralMixTrace yMix,abMix;
  std::array<float,kPhase4PlateCapacity> weightsY{},weightsAB{};
  YabPixel output{};
  float alpha=0;
};
using SpectralSpillObserver=std::function<void(const SpectralSpillSample&)>;

Phase4SpillResult applyPhase4Spill(ConstYabPlanes original,
                                   const PublicPlateSet &plates,
                                   const Phase4ChunkSynthesis &synthesis,
                                   const SparseAffinityGraph &graph,
                                   const Phase4Params &params,
                                   const ExecutionContext &execution = {},
                                   const Phase4SpillTransport *prepared=nullptr,
                                   WorkingGamut gamut=WorkingGamut::ACEScg,
                                   const SpectralSpillObserver &observer={},
                                   bool spectralSceneMass=false);

} // namespace pigment
