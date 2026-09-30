#include "core/PlateSpill.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

namespace pigment {
namespace {

std::vector<float> transport(const SparseAffinityGraph &graph,
                             const std::vector<float> &seed, float reach,
                             float structureRespect) {
  const int count = graph.nodeCount();
  std::vector<float> distance(size_t(count),
                              std::numeric_limits<float>::infinity());
  using Item = std::pair<float, int>;
  std::priority_queue<Item, std::vector<Item>, std::greater<Item>> queue;
  if (reach <= 0.0f)
    return seed;
  for (int node = 0; node < count; ++node)
    if (seed[size_t(node)] > 1.0e-5f) {
      distance[size_t(node)] =
          -reach * std::log(std::max(1.0e-5f, seed[size_t(node)]));
      queue.push({distance[size_t(node)], node});
    }
  while (!queue.empty()) {
    auto [current, node] = queue.top();
    queue.pop();
    if (current != distance[size_t(node)] || current > reach)
      continue;
    for (int edgeIndex = graph.rowOffsets[node];
         edgeIndex < graph.rowOffsets[node + 1]; ++edgeIndex) {
      const auto &edge = graph.edges[size_t(edgeIndex)];
      float capacity = std::max(1.0e-6f, edge.weight);
      float edgeCost =
          edge.physicalDistance *
          (1.0f - std::log(capacity) + 6.0f * structureRespect * edge.boundary);
      float candidate = current + edgeCost;
      if (candidate < distance[size_t(edge.target)] && candidate <= reach) {
        distance[size_t(edge.target)] = candidate;
        queue.push({candidate, edge.target});
      }
    }
  }
  std::vector<float> result(static_cast<size_t>(count));
  for (int node = 0; node < count; ++node)
    if (std::isfinite(distance[size_t(node)]))
      result[size_t(node)] = std::exp(-distance[size_t(node)] / reach);
  return result;
}

} // namespace

Phase4SpillResult applyPhase4Spill(ConstYabPlanes original,
                                   const PublicPlateSet &plates,
                                   const Phase4ChunkSynthesis &synthesis,
                                   const SparseAffinityGraph &graph,
                                   const Phase4Params &params,
                                   const ExecutionContext &execution) {
  (void)execution;
  Phase4SpillResult result(plates.bounds());
  const RectI bounds = plates.bounds();
  const int width = bounds.width(), height = bounds.height();
  const int plateCount = plates.count();
  result.plateAppearance.reserve(plateCount);
  result.influence.reserve(plateCount);
  for (int plate = 0; plate < plateCount; ++plate) {
    result.plateAppearance.emplace_back(bounds);
    result.influence.emplace_back(bounds);
  }

  std::vector<std::vector<float>> transportY(plateCount),
      transportAB(plateCount);
  for (int plate = 0; plate < plateCount; ++plate) {
    std::vector<float> seedY(size_t(graph.nodeCount())),
        seedAB(size_t(graph.nodeCount()));
    for (int gy = 0; gy < graph.height; ++gy)
      for (int gx = 0; gx < graph.width; ++gx) {
        int x = bounds.x1 + std::min(width - 1, gx * width / graph.width);
        int y = bounds.y1 + std::min(height - 1, gy * height / graph.height);
        int node = gy * graph.width + gx;
        seedY[size_t(node)] = plates.supportY(plate).at(x, y);
        seedAB[size_t(node)] = plates.supportAB(plate).at(x, y);
      }
    transportY[plate] =
        transport(graph, seedY, params.spillReach, params.structureRespect);
    transportAB[plate] =
        transport(graph, seedAB, params.spillReach, params.structureRespect);
  }

  auto graphValue = [&](const std::vector<float> &field, int x, int y) {
    int gx = std::min(graph.width - 1, (x - bounds.x1) * graph.width / width);
    int gy =
        std::min(graph.height - 1, (y - bounds.y1) * graph.height / height);
    return field[size_t(gy) * graph.width + gx];
  };
  for (int y = bounds.y1; y < bounds.y2; ++y)
    for (int x = bounds.x1; x < bounds.x2; ++x) {
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
        if (receiverControl.enabled && params.spillAmount > 0.0f) {
          for (int donor = 0; donor < plateCount; ++donor) {
            if (donor == receiver)
              continue;
            const auto &donorControl = params.plates[size_t(donor)];
            if (!donorControl.enabled)
              continue;
            float directedY = graphValue(transportY[donor], x, y);
            float directedAB = graphValue(transportAB[donor], x, y);
            float reverseY = graphValue(transportY[receiver], x, y);
            float reverseAB = graphValue(transportAB[receiver], x, y);
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
            sumY += ky * donorY;
            weightY += ky;
            sumA += kab * donorA;
            sumB += kab * donorB;
            weightAB += kab;
            influence += kab;
          }
        }
        auto output = result.plateAppearance[size_t(receiver)].view();
        output.y.at(x, y) = sumY / weightY;
        output.a.at(x, y) = sumA / weightAB;
        output.b.at(x, y) = sumB / weightAB;
        result.influence[size_t(receiver)].view().at(x, y) = influence;
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
  return result;
}

} // namespace pigment
