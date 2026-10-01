#include "core/PlateSpill.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pigment {
namespace {

std::vector<float> transport(const SparseAffinityGraph &graph,
                             const std::vector<float> &seed, float reach,
                             float structureRespect,const ExecutionContext &execution) {
  const int count = graph.nodeCount();
  // Max-product envelope: T(p)=max_q seed(q)*exp(-D_F(q,p)/reach).
  // Reach is an e-fold graph-distance scale, not an amplitude-dependent
  // cutoff. Weak seeds have exactly the same relative travel as strong seeds.
  std::vector<float> result=seed;
  using Item = std::pair<float, int>;
  std::priority_queue<Item> queue;
  if (reach <= 0.0f)
    return seed;
  for (int node = 0; node < count; ++node)
    if (seed[size_t(node)] > 0.0f) {
      queue.push({seed[size_t(node)], node});
    }
  while (!queue.empty()) {
    if(execution.cancelled())throw std::runtime_error("Spill transport cancelled");
    auto [current, node] = queue.top();
    queue.pop();
    if (current != result[size_t(node)])
      continue;
    for (int edgeIndex = graph.rowOffsets[node];
         edgeIndex < graph.rowOffsets[node + 1]; ++edgeIndex) {
      const auto &edge = graph.edges[size_t(edgeIndex)];
      if(edge.weight<=0 || (structureRespect>=1 && edge.boundary>=1))continue;
      // Zero was excluded above; log is finite for every positive float F.
      // Do not promote very weak capacities to an arbitrary transport floor.
      float capacity = edge.weight;
      float edgeCost =
          edge.physicalDistance *
          (1.0f - std::log(capacity) + 6.0f * structureRespect * edge.boundary);
      float candidate = current * std::exp(-edgeCost/reach);
      if (candidate > result[size_t(edge.target)]) {
        result[size_t(edge.target)] = candidate;
        queue.push({candidate, edge.target});
      }
    }
  }
  return result;
}

} // namespace
Phase4SpillTransport preparePhase4SpillTransport(const PublicPlateSet &plates,
    const SparseAffinityGraph &graph,const Phase4Params &params,const ExecutionContext &execution){
  Phase4SpillTransport result;auto bounds=plates.bounds();int width=bounds.width(),height=bounds.height();
  result.reach=params.spillReach;result.structureRespect=params.structureRespect;
  result.y.resize(plates.count());result.ab.resize(plates.count());
  execution.parallelRows(0,plates.count(),[&](int begin,int end){
  for(int plate=begin;plate<end;++plate){std::vector<float> sy(size_t(graph.nodeCount())),sc(size_t(graph.nodeCount()));
    for(int gy=0;gy<graph.height;++gy)for(int gx=0;gx<graph.width;++gx){int x=bounds.x1+std::min(width-1,gx*width/graph.width),y=bounds.y1+std::min(height-1,gy*height/graph.height),node=gy*graph.width+gx;
      sy[size_t(node)]=plates.supportY(plate).at(x,y);sc[size_t(node)]=plates.supportAB(plate).at(x,y);}
    result.y[size_t(plate)]=transport(graph,sy,params.spillReach,params.structureRespect,execution);
    result.ab[size_t(plate)]=transport(graph,sc,params.spillReach,params.structureRespect,execution);
  }});return result;
}

Phase4SpillResult applyPhase4Spill(ConstYabPlanes original,
                                   const PublicPlateSet &plates,
                                   const Phase4ChunkSynthesis &synthesis,
                                   const SparseAffinityGraph &graph,
                                   const Phase4Params &params,
                                   const ExecutionContext &execution,const Phase4SpillTransport *prepared,WorkingGamut gamut) {
  Phase4SpillResult result(plates.bounds());
  const RectI bounds = plates.bounds();
  const int width = bounds.width(), height = bounds.height();
  const int plateCount = plates.count();
  std::optional<ColorInteraction> color;
  if(params.colorInteraction!=ColorInteractionLaw::LinearYAB)color.emplace(gamut);
  result.plateAppearance.reserve(plateCount);
  result.influence.reserve(plateCount);
  for (int plate = 0; plate < plateCount; ++plate) {
    result.plateAppearance.emplace_back(bounds);
    result.influence.emplace_back(bounds);
    result.influenceY.emplace_back(bounds);result.transportY.emplace_back(bounds);result.transportAB.emplace_back(bounds);
  }

  Phase4SpillTransport owned;
  if(!prepared){owned=preparePhase4SpillTransport(plates,graph,params,execution);prepared=&owned;}
  const auto &transportY=prepared->y,&transportAB=prepared->ab;
  if(prepared->reach!=params.spillReach || prepared->structureRespect!=params.structureRespect)throw std::runtime_error("Spill prepared transport settings mismatch");
  if(int(transportY.size())!=plateCount || int(transportAB.size())!=plateCount)throw std::runtime_error("Spill prepared transport plate mismatch");
  for(int i=0;i<plateCount;++i)if(int(transportY[size_t(i)].size())!=graph.nodeCount() || int(transportAB[size_t(i)].size())!=graph.nodeCount())throw std::runtime_error("Spill prepared transport geometry mismatch");

  auto graphValue = [&](const std::vector<float> &field, int x, int y) {
    int gx = std::min(graph.width - 1, (x - bounds.x1) * graph.width / width);
    int gy =
        std::min(graph.height - 1, (y - bounds.y1) * graph.height / height);
    return field[size_t(gy) * graph.width + gx];
  };
  // Lift only transported gain from the analysis graph. Intrinsic full-res
  // support must not be resampled into graph cells, especially at Reach=0.
  auto fullTransport=[&](int plate,bool ab,int x,int y){
    auto support=ab?plates.supportAB(plate):plates.supportY(plate);
    const float intrinsic=support.at(x,y);if(params.spillReach<=0)return intrinsic;
    int gx=std::min(graph.width-1,(x-bounds.x1)*graph.width/width),gy=std::min(graph.height-1,(y-bounds.y1)*graph.height/height);
    int sx=bounds.x1+std::min(width-1,gx*width/graph.width),sy=bounds.y1+std::min(height-1,gy*height/graph.height);
    float gain=graphValue(ab?transportAB[size_t(plate)]:transportY[size_t(plate)],x,y)-support.at(sx,sy);
    return std::min(1.f,intrinsic+std::max(0.f,gain));
  };
  for (int y = bounds.y1; y < bounds.y2; ++y) {
    if(execution.cancelled())throw std::runtime_error("Spill reconstruction cancelled");
    for (int x = bounds.x1; x < bounds.x2; ++x) {
      std::array<InteractionMaterial,kPhase4PlateCapacity> materials;
      if(color && params.spillAmount>0 && (params.lumaSpill>0 || params.chromaSpill>0))
        for(int i=0;i<plateCount;++i){auto v=synthesis.plateAppearance[size_t(i)].view();const auto &c=params.plates[size_t(i)];
          materials[size_t(i)]=color->encode({v.y.at(x,y)+c.tone,v.a.at(x,y)+c.biasA,v.b.at(x,y)+c.biasB},params.colorInteraction);}
      std::vector<float> artisticAlpha(static_cast<size_t>(plateCount));
      float alphaSum = 0.0f;
      for (int plate = 0; plate < plateCount; ++plate) {
        const auto &control = params.plates[size_t(plate)];
        artisticAlpha[size_t(plate)] =
            control.enabled
                ? std::max(0.0f, control.weight) * plates.alpha(plate).at(x, y)
                : 0.0f;
        alphaSum += artisticAlpha[size_t(plate)];
      }
      if (alphaSum > 1.0e-8f)
        for (float &value : artisticAlpha)
          value /= alphaSum;

      for (int receiver = 0; receiver < plateCount; ++receiver) {
        const auto &receiverControl = params.plates[size_t(receiver)];
        auto source = synthesis.plateAppearance[size_t(receiver)].view();
        float baseY = source.y.at(x, y) + receiverControl.tone;
        float baseA = source.a.at(x, y) + receiverControl.biasA;
        float baseB = source.b.at(x, y) + receiverControl.biasB;
        float sumY = baseY, sumA = baseA, sumB = baseB;
        float weightY = 1.0f, weightAB = 1.0f, influence = 0.0f;
        float influenceY=0;
        std::array<float,kPhase4PlateCapacity> weightsY{},weightsAB{};
        weightsY[size_t(receiver)]=weightsAB[size_t(receiver)]=1;
        result.transportY[size_t(receiver)].view().at(x,y)=fullTransport(receiver,false,x,y);
        result.transportAB[size_t(receiver)].view().at(x,y)=fullTransport(receiver,true,x,y);
        if (receiverControl.enabled && params.spillAmount > 0.0f) {
          for (int donor = 0; donor < plateCount; ++donor) {
            if (donor == receiver)
              continue;
            const auto &donorControl = params.plates[size_t(donor)];
            if (!donorControl.enabled)
              continue;
            float directedY = fullTransport(donor,false,x,y);
            float directedAB = fullTransport(donor,true,x,y);
            float reverseY = fullTransport(receiver,false,x,y);
            float reverseAB = fullTransport(receiver,true,x,y);
            float asymmetry =
                std::max(0.0f, std::min(1.0f, params.spillAsymmetry));
            directedY = asymmetry * directedY +
                        (1 - asymmetry) * .5f * (directedY + reverseY);
            directedAB = asymmetry * directedAB +
                         (1 - asymmetry) * .5f * (directedAB + reverseAB);
            float controls =
                donorControl.spillOut * receiverControl.receiveSpill;
            // Supports are independent participation confidences rather than
            // probabilities.  Their raw product made two legitimate soft
            // supports vanish quadratically (for example .2 * .2 = .04), so
            // even a full-strength spill remained visually inert.  The
            // geometric mean preserves the required two-sided overlap gate
            // while keeping Spill Amount calibrated as an artistic blend.
            // It does not expand either support or alter reconstruction alpha.
            float overlapY = std::sqrt(std::max(
                0.0f, plates.supportY(receiver).at(x, y) *
                          plates.supportY(donor).at(x, y)));
            float overlapAB = std::sqrt(std::max(
                0.0f, plates.supportAB(receiver).at(x, y) *
                          plates.supportAB(donor).at(x, y)));
            float interactionY = directedY * overlapY * controls;
            float interactionAB = directedAB * overlapAB * controls;
            auto donorAppearance =
                synthesis.plateAppearance[size_t(donor)].view();
            float donorY = donorAppearance.y.at(x, y) + donorControl.tone;
            float donorA = donorAppearance.a.at(x, y) + donorControl.biasA;
            float donorB = donorAppearance.b.at(x, y) + donorControl.biasB;
            float ky = params.spillAmount * params.lumaSpill * interactionY;
            float kab = params.spillAmount * params.chromaSpill * interactionAB;
            weightsY[size_t(donor)]=ky;weightsAB[size_t(donor)]=kab;
            sumY += ky * donorY;
            weightY += ky;
            sumA += kab * donorA;
            sumB += kab * donorB;
            weightAB += kab;
            influence += kab;
            influenceY += ky;
          }
        }
        auto output = result.plateAppearance[size_t(receiver)].view();
        output.y.at(x, y) = sumY / weightY;
        output.a.at(x, y) = sumA / weightAB;
        output.b.at(x, y) = sumB / weightAB;
        if(color){
          // Spatial interaction weights and all diagnostics above are exactly
          // the historical path. Only this appearance decode is law-specific.
          if(weightY>1){auto v=color->mix(materials.data(),weightsY.data(),plateCount,
              {sumY/weightY,0,0},params.colorInteraction,params.pigmentDensity);output.y.at(x,y)=v.y;}
          if(weightAB>1){float ly=baseY;for(int i=0;i<plateCount;++i)if(i!=receiver){auto d=synthesis.plateAppearance[size_t(i)].view();ly+=weightsAB[size_t(i)]*(d.y.at(x,y)+params.plates[size_t(i)].tone);}
            auto v=color->mix(materials.data(),weightsAB.data(),plateCount,
              {ly/weightAB,sumA/weightAB,sumB/weightAB},params.colorInteraction,params.pigmentDensity);
            output.a.at(x,y)=v.a;output.b.at(x,y)=v.b;}
        }
        result.influence[size_t(receiver)].view().at(x, y) = influence;
        result.influenceY[size_t(receiver)].view().at(x,y)=influenceY;
      }

      float yy = 0, aa = 0, bb = 0;
      if (alphaSum > 1.0e-8f)
        for (int plate = 0; plate < plateCount; ++plate) {
          auto appearance = result.plateAppearance[size_t(plate)].view();
          float alpha = artisticAlpha[size_t(plate)];
          yy += alpha * appearance.y.at(x, y);
          aa += alpha * appearance.a.at(x, y);
          bb += alpha * appearance.b.at(x, y);
        }
      else {
        // Disabling or zero-weighting the only locally occupied plate must
        // not manufacture black/zero YAB.  The specified fallback is the
        // original straight source at that pixel.
        yy = original.y.at(x, y);
        aa = original.a.at(x, y);
        bb = original.b.at(x, y);
      }
      auto composite = result.composite.view();
      composite.y.at(x, y) = yy;
      composite.a.at(x, y) = aa;
      composite.b.at(x, y) = bb;
    }
  }
  return result;
}

} // namespace pigment
