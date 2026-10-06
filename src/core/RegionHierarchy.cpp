#include "core/RegionHierarchy.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace pigment {
namespace {

struct DisjointSet {
  std::vector<int> parent, size;
  explicit DisjointSet(int count) : parent(count), size(count, 1) {
    std::iota(parent.begin(), parent.end(), 0);
  }
  int find(int value) {
    while (parent[value] != value) {
      parent[value] = parent[parent[value]];
      value = parent[value];
    }
    return value;
  }
  bool unite(int a, int b) {
    a = find(a);
    b = find(b);
    if (a == b)
      return false;
    if (size[a] < size[b])
      std::swap(a, b);
    parent[b] = a;
    size[a] += size[b];
    return true;
  }
};

struct PixelEdge {
  int first = 0, second = 0;
  float strength = 0.0f;
};

struct RegionEdge {
  int first = 0, second = 0;
  float boundarySum = 0.0f;
  int length = 0;
};

struct RegionStats {
  double support = 0.0, alpha = 0.0, mass = 0.0;
  double y = 0.0, a = 0.0, b = 0.0;
  double y2 = 0.0, a2 = 0.0, b2 = 0.0;
  std::array<double, 6> feature{}, feature2{};
  double area = 0.0, perimeter = 0.0;
};

struct Sample {
  float y = 0.0f, a = 0.0f, b = 0.0f;
};

std::vector<Sample> analysisGaussian(const std::vector<Sample> &input, int width,
                                     int height, float sigma) {
  const int radius = int(std::ceil(3*sigma));
  std::vector<float> kernel(size_t(2*radius+1));
  float total = 0;
  for (int k=-radius;k<=radius;++k) {
    kernel[size_t(k+radius)] = std::exp(-.5f*k*k/(sigma*sigma));
    total += kernel[size_t(k+radius)];
  }
  for (float &w:kernel) w/=total;
  std::vector<Sample> horizontal(input.size()), output(input.size());
  auto at = [&](int x, int y) -> const Sample & {
    return input[size_t(y) * width + x];
  };
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      Sample value{};
      for(int k=-radius;k<=radius;++k) {
        const auto &p=at(std::clamp(x+k,0,width-1),y);
        float w=kernel[size_t(k+radius)];
        value.y+=w*p.y;value.a+=w*p.a;value.b+=w*p.b;
      }
      horizontal[size_t(y) * width + x] = value;
    }
  auto hat = [&](int x, int y) -> const Sample & {
    return horizontal[size_t(y) * width + x];
  };
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      Sample value{};
      for(int k=-radius;k<=radius;++k) {
        const auto &p=hat(x,std::clamp(y+k,0,height-1));
        float w=kernel[size_t(k+radius)];
        value.y+=w*p.y;value.a+=w*p.a;value.b+=w*p.b;
      }
      output[size_t(y) * width + x] = value;
    }
  return output;
}

float median(std::vector<float> values) {
  if (values.empty())
    return 1.0f;
  auto middle = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
  std::nth_element(values.begin(), middle, values.end());
  return std::max(1.0e-5f, *middle);
}

float hashedLabel(int label) {
  unsigned value = unsigned(label + 1) * 0x9e3779b9u;
  value ^= value >> 16;
  return float(value & 0xffffu) / 65535.0f;
}

std::vector<int> compactLabels(DisjointSet &set, int count, int &labelCount) {
  std::vector<int> rootToLabel(count, -1), labels(count);
  labelCount = 0;
  for (int index = 0; index < count; ++index) {
    int root = set.find(index);
    if (rootToLabel[root] < 0)
      rootToLabel[root] = labelCount++;
    labels[index] = rootToLabel[root];
  }
  return labels;
}

struct TreeResult {
  std::vector<Phase4MergeNode> nodes;
  std::vector<int> labels, parent, depth;
  std::vector<std::vector<int>> ancestor;
  int count = 0;
  float firstLevel = 0.0f;
  float edgeLevel(int a, int b) const {
    if (a == b) return 0.0f;
    if (depth[a] < depth[b]) std::swap(a, b);
    int difference = depth[a] - depth[b];
    for (size_t k = 0; k < ancestor.size(); ++k)
      if ((difference >> k) & 1) a = ancestor[k][a];
    if (a == b) return nodes[size_t(a)].level;
    for (int k = int(ancestor.size()) - 1; k >= 0; --k)
      if (ancestor[size_t(k)][a] != ancestor[size_t(k)][b]) {
        a = ancestor[size_t(k)][a]; b = ancestor[size_t(k)][b];
      }
    int lca = parent[a];
    return lca < 0 ? std::numeric_limits<float>::infinity()
                   : nodes[size_t(lca)].level;
  }
};

TreeResult buildTree(std::vector<RegionStats> stats,
                     const std::vector<RegionEdge> &edges, bool chroma,
                     const Phase4Params &params, float scale,
                     const ExecutionContext &execution) {
  const int leaves = int(stats.size()), capacity = std::max(1, 2 * leaves);
  stats.resize(size_t(capacity));
  TreeResult result;
  result.nodes.resize(size_t(leaves));
  result.parent.assign(size_t(capacity), -1);
  struct Boundary { double sum = 0.0; int length = 0; };
  std::vector<std::map<int, Boundary>> adjacency{size_t(capacity)};
  std::vector<int> version(size_t(capacity), 0);
  std::vector<bool> active(size_t(capacity), false);
  for (int i = 0; i < leaves; ++i) active[size_t(i)] = true;
  for (const auto &e : edges) {
    adjacency[size_t(e.first)][e.second] = {e.boundarySum, e.length};
    adjacency[size_t(e.second)][e.first] = {e.boundarySum, e.length};
  }
  auto cost = [&](int a, int b, Boundary boundary) {
    const auto &s = stats[size_t(a)], &t = stats[size_t(b)];
    if (s.mass < 1e-8 || t.mass < 1e-8 ||
        std::min(s.support / std::max(1.0, s.area),
                 t.support / std::max(1.0, t.area)) < .02)
      return std::numeric_limits<float>::infinity();
    double delta = 0.0;
    for (int k = 0; k < (chroma ? 6 : 3); ++k) {
      double difference = s.feature[size_t(k)] / s.mass -
                          t.feature[size_t(k)] / t.mass;
      delta += s.mass * t.mass / (s.mass + t.mass) *
               difference * difference;
    }
    if (chroma) {
      double difference = s.y / s.mass - t.y / t.mass;
      double yEvidence = s.mass * t.mass / (s.mass + t.mass) *
                         difference * difference;
      delta = (1 - params.lumaChromaCoupling) * delta +
              params.lumaChromaCoupling * yEvidence;
    }
    // Most of the initial linear tolerance range zeroed the Ward increment
    // on photographs, collapsing the tree at the first nonzero scale. Keep
    // the same approved merge energy, but give its tolerance control a
    // conservative onset. The endpoint still permits deliberate high
    // within-region variation; ordinary settings no longer dominate Scale.
    double variation=std::clamp(double(params.internalVariation),0.0,1.0);
    double tolerance = .15 * variation*variation*variation*variation;
    delta = std::max(0.0, delta - tolerance * tolerance * (s.mass + t.mass));
    double perimeter = s.perimeter + t.perimeter - 2 * boundary.length;
    double gain = s.perimeter / std::max(1.0, s.area) +
                  t.perimeter / std::max(1.0, t.area) -
                  perimeter / std::max(1.0, s.area + t.area);
    double barrier = boundary.sum / std::max(1, boundary.length);
    // The variance/perimeter-gain scale is recomputed from the growing
    // union, rather than treating the original edge as a fixed link.
    float raw = float(std::sqrt(delta / std::max(1e-9, gain)) /
                      std::max(.02, 1 - params.boundaryLock * barrier));
    return std::max({raw, result.nodes[size_t(a)].level,
                     result.nodes[size_t(b)].level});
  };
  using Item = std::tuple<float, int, int, int, int>;
  std::priority_queue<Item, std::vector<Item>, std::greater<Item>> queue;
  auto enqueue = [&](int a, int b, Boundary boundary) {
    if (a > b) std::swap(a, b);
    queue.emplace(cost(a, b, boundary), a, b, version[size_t(a)],
                  version[size_t(b)]);
  };
  for (const auto &e : edges)
    enqueue(e.first, e.second, {e.boundarySum, e.length});
  int merges = 0;
  while (!queue.empty()) {
    if ((merges & 255) == 0 && execution.cancelled())
      throw std::runtime_error("Phase 4 hierarchy cancelled");
    auto [level, a, b, va, vb] = queue.top(); queue.pop();
    if (!active[size_t(a)] || !active[size_t(b)] ||
        va != version[size_t(a)] || vb != version[size_t(b)]) continue;
    if (!std::isfinite(level)) break;
    auto ab = adjacency[size_t(a)].find(b);
    if (ab == adjacency[size_t(a)].end()) continue;
    int node = int(result.nodes.size());
    if (merges++ == 0) result.firstLevel = level;
    result.nodes.push_back({a, b, level});
    result.parent[size_t(a)] = result.parent[size_t(b)] = node;
    active[size_t(a)] = active[size_t(b)] = false;
    active[size_t(node)] = true;
    RegionStats merged;
    const auto &sa = stats[size_t(a)], &sb = stats[size_t(b)];
    merged.area = sa.area + sb.area;
    merged.perimeter = sa.perimeter + sb.perimeter - 2 * ab->second.length;
    merged.mass = sa.mass + sb.mass; merged.support = sa.support + sb.support;
    merged.alpha = sa.alpha + sb.alpha;
    merged.y = sa.y + sb.y; merged.a = sa.a + sb.a; merged.b = sa.b + sb.b;
    for (int k = 0; k < 6; ++k) {
      merged.feature[size_t(k)] = sa.feature[size_t(k)] + sb.feature[size_t(k)];
      merged.feature2[size_t(k)] = sa.feature2[size_t(k)] + sb.feature2[size_t(k)];
    }
    stats[size_t(node)] = merged;
    for (int child : {a, b})
      for (const auto &[neighbor, boundary] : adjacency[size_t(child)]) {
        if (neighbor == a || neighbor == b || !active[size_t(neighbor)]) continue;
        auto &target = adjacency[size_t(node)][neighbor];
        target.sum += boundary.sum; target.length += boundary.length;
        adjacency[size_t(neighbor)].erase(child);
      }
    adjacency[size_t(a)].clear(); adjacency[size_t(b)].clear();
    for (const auto &[neighbor, boundary] : adjacency[size_t(node)]) {
      adjacency[size_t(neighbor)][node] = boundary;
      enqueue(node, neighbor, boundary);
    }
  }
  // Join disconnected/unsupported roots only at infinity. They never merge
  // at a finite artist scale, but all atomic grid edges then have an LCA.
  int root = -1;
  std::vector<int> roots;
  for (int i = 0; i < int(result.nodes.size()); ++i)
    if (result.parent[size_t(i)] < 0) roots.push_back(i);
  for (int i : roots) {
      if (root < 0) root = i;
      else {
        int node = int(result.nodes.size());
        result.nodes.push_back({root, i, std::numeric_limits<float>::infinity()});
        result.parent[size_t(root)] = result.parent[size_t(i)] = node;
        root = node;
      }
  }
  result.parent.resize(result.nodes.size());
  result.depth.assign(result.nodes.size(), 0);
  std::vector<int> stack;
  if (root >= 0) stack.push_back(root);
  while (!stack.empty()) {
    int n = stack.back(); stack.pop_back();
    for (int child : {result.nodes[size_t(n)].left, result.nodes[size_t(n)].right})
      if (child >= 0) { result.depth[size_t(child)] = result.depth[size_t(n)] + 1;
                        stack.push_back(child); }
  }
  int levels = 1;
  while ((1ull << levels) <= result.nodes.size()) ++levels;
  result.ancestor.resize(size_t(levels), std::vector<int>(result.nodes.size()));
  for (size_t n = 0; n < result.nodes.size(); ++n)
    result.ancestor[0][n] = result.parent[n] < 0 ? int(n) : result.parent[n];
  for (int k = 1; k < levels; ++k)
    for (size_t n = 0; n < result.nodes.size(); ++n)
      result.ancestor[size_t(k)][n] =
          result.ancestor[size_t(k-1)][size_t(result.ancestor[size_t(k-1)][n])];
  std::vector<int> nodeLabel(result.nodes.size(), -1);
  result.labels.resize(size_t(leaves));
  for (int leaf = 0; leaf < leaves; ++leaf) {
    int n = leaf;
    if (scale > 0)
      for (int k = levels - 1; k >= 0; --k) {
        int candidate = result.ancestor[size_t(k)][size_t(n)];
        if (result.nodes[size_t(candidate)].level <= scale) n = candidate;
      }
    if (nodeLabel[size_t(n)] < 0) nodeLabel[size_t(n)] = result.count++;
    result.labels[size_t(leaf)] = nodeLabel[size_t(n)];
  }
  return result;
}

} // namespace

Phase4PlateChunkHierarchy::Phase4PlateChunkHierarchy(RectI bounds)
    : yRemovedBoundaries(bounds), yRetainedBoundaries(bounds),
      abRemovedBoundaries(bounds), abRetainedBoundaries(bounds) {}

Phase4RegionHierarchy
buildPhase4RegionHierarchy(ConstYabPlanes source, const PublicPlateSet &plates,
                           const Phase4Params &params,
                           const ExecutionContext &execution) {
  const RectI bounds = source.y.bounds;
  const int width = bounds.width(), height = bounds.height();
  const int pixelCount = width * height;
  auto index = [&](int x, int y) {
    return (y - bounds.y1) * width + (x - bounds.x1);
  };

  std::array<std::vector<Sample>, 4> levels;
  levels[0].resize(pixelCount);
  for (int y = bounds.y1; y < bounds.y2; ++y)
    for (int x = bounds.x1; x < bounds.x2; ++x)
      levels[0][index(x, y)] = {source.y.at(x, y), source.a.at(x, y),
                                source.b.at(x, y)};
  // These representations are contour analysis only, never output shading.
  // The previous four tiny binomial passes did not span the approved 1/2/4/8
  // scales and incorrectly called descriptive texture persistent structure.
  const auto original = levels[0];
  for(int level=0;level<4;++level)
    levels[level] = analysisGaussian(original,width,height,float(1<<level));
  std::array<float, 4> yScales{}, abScales{};
  for (int level = 0; level < 4; ++level) {
    std::vector<float> yDifferences, abDifferences;
    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x) {
        if (x + 1 < width) {
          const auto &a = levels[level][size_t(y) * width + x];
          const auto &b = levels[level][size_t(y) * width + x + 1];
          yDifferences.push_back(std::abs(b.y - a.y));
          abDifferences.push_back(std::hypot(b.a - a.a, b.b - a.b));
        }
        if (y + 1 < height) {
          const auto &a = levels[level][size_t(y) * width + x];
          const auto &b = levels[level][size_t(y + 1) * width + x];
          yDifferences.push_back(std::abs(b.y - a.y));
          abDifferences.push_back(std::hypot(b.a - a.a, b.b - a.b));
        }
      }
    yScales[level] = 2.5f * median(std::move(yDifferences));
    abScales[level] = 2.5f * median(std::move(abDifferences));
  }
  const float yScale = yScales[0], abScale = abScales[0];
  double sy = 0, sy2 = 0, sa = 0, sb = 0, sc2 = 0;
  for (const auto &p : levels[0]) {
    sy += p.y; sy2 += p.y*p.y; sa += p.a; sb += p.b;
    sc2 += p.a*p.a + p.b*p.b;
  }
  double n = std::max(1, pixelCount);
  double selectivityScale = 2.0 - 1.5 * params.mergeSelectivity;
  double toneScale = std::max(1e-5, std::sqrt(std::max(0.0, sy2/n - sy*sy/(n*n)))) * selectivityScale;
  double colorScale = std::max(1e-5, std::sqrt(std::max(0.0, sc2/n - (sa*sa+sb*sb)/(n*n)))) * selectivityScale;

  std::vector<PixelEdge> pixelEdges;
  pixelEdges.reserve(size_t(pixelCount) * 2);
  auto appendEdge = [&](int x0, int y0, int x1, int y1) {
    constexpr float weights[4]{.15f, .25f, .30f, .30f};
    float strength = 0.0f;
    for (int level = 0; level < 4; ++level) {
      const auto &first = levels[level][index(x0, y0)];
      const auto &second = levels[level][index(x1, y1)];
      float dy = (second.y - first.y) / yScales[level];
      float dc =
          std::hypot(second.a - first.a, second.b - first.b) / abScales[level];
      strength += weights[level] *
                  (1.0f - std::exp(-0.5f * (dy * dy + 0.6f * dc * dc)));
    }
    pixelEdges.push_back({index(x0, y0), index(x1, y1), strength});
  };
  for (int y = bounds.y1; y < bounds.y2; ++y)
    for (int x = bounds.x1; x < bounds.x2; ++x) {
      if (x + 1 < bounds.x2)
        appendEdge(x, y, x + 1, y);
      if (y + 1 < bounds.y2)
        appendEdge(x, y, x, y + 1);
    }
  std::sort(pixelEdges.begin(), pixelEdges.end(),
            [](const auto &a, const auto &b) {
              return std::tie(a.strength, a.first, a.second) <
                     std::tie(b.strength, b.first, b.second);
            });
  // Marker-controlled watershed as a minimum spanning forest. Regional
  // minima of the source contour topography seed the flood; distinct marker
  // trees never join during flooding. This replaces the provisional global
  // low-edge threshold, which was not a watershed and discarded the nesting
  // relationship between local contour basins.
  std::vector<float> topography(size_t(pixelCount),0);
  for(const auto &edge:pixelEdges) {
    topography[size_t(edge.first)]=std::max(topography[size_t(edge.first)],edge.strength);
    topography[size_t(edge.second)]=std::max(topography[size_t(edge.second)],edge.strength);
  }
  DisjointSet plateau(pixelCount);
  for(const auto &edge:pixelEdges)
    if(topography[size_t(edge.first)]==topography[size_t(edge.second)])
      plateau.unite(edge.first,edge.second);
  std::vector<bool> minimum(size_t(pixelCount),true);
  for(const auto &edge:pixelEdges) {
    if(topography[size_t(edge.first)]>topography[size_t(edge.second)])
      minimum[size_t(plateau.find(edge.first))]=false;
    if(topography[size_t(edge.second)]>topography[size_t(edge.first)])
      minimum[size_t(plateau.find(edge.second))]=false;
  }
  DisjointSet atomic(pixelCount);
  std::vector<int> marker(size_t(pixelCount),-1);
  for(int p=0;p<pixelCount;++p) {
    int root=plateau.find(p);
    if(minimum[size_t(root)]) marker[size_t(p)]=root;
  }
  for(const auto &edge:pixelEdges) {
    int first=atomic.find(edge.first),second=atomic.find(edge.second);
    if(first==second) continue;
    int a=marker[size_t(first)],b=marker[size_t(second)];
    if(a>=0 && b>=0 && a!=b) continue;
    atomic.unite(first,second);
    marker[size_t(atomic.find(first))]=a>=0?a:b;
  }
  // Absorb tiny islands only through their weakest incident source edge.
  for (const auto &edge : pixelEdges) {
    int first = atomic.find(edge.first), second = atomic.find(edge.second);
    if (first != second && (atomic.size[first] < 4 || atomic.size[second] < 4))
      atomic.unite(first, second);
  }

  Phase4RegionHierarchy result;
  result.bounds = bounds;
  result.boundaryStrength = OwnedPlane(bounds);
  result.atomicRegionDisplay = OwnedPlane(bounds);
  result.atomicRegion =
      compactLabels(atomic, pixelCount, result.atomicRegionCount);
  for (int y = bounds.y1; y < bounds.y2; ++y)
    for (int x = bounds.x1; x < bounds.x2; ++x)
      result.atomicRegionDisplay.view().at(x, y) =
          hashedLabel(result.atomicRegion[index(x, y)]);

  std::map<std::pair<int, int>, RegionEdge> adjacency;
  for (const auto &edge : pixelEdges) {
    int first = result.atomicRegion[edge.first];
    int second = result.atomicRegion[edge.second];
    if (first == second)
      continue;
    if (first > second)
      std::swap(first, second);
    auto &regionEdge = adjacency[{first, second}];
    regionEdge.first = first;
    regionEdge.second = second;
    regionEdge.boundarySum += edge.strength;
    ++regionEdge.length;
  }
  std::vector<RegionEdge> regionEdges;
  regionEdges.reserve(adjacency.size());
  for (const auto &[key, edge] : adjacency)
    regionEdges.push_back(edge);

  auto boundaryDisplay = result.boundaryStrength.view();
  for (const auto &edge : pixelEdges) {
    int x0 = bounds.x1 + edge.first % width;
    int y0 = bounds.y1 + edge.first / width;
    int x1 = bounds.x1 + edge.second % width;
    int y1 = bounds.y1 + edge.second / width;
    boundaryDisplay.at(x0, y0) =
        std::max(boundaryDisplay.at(x0, y0), edge.strength);
    boundaryDisplay.at(x1, y1) =
        std::max(boundaryDisplay.at(x1, y1), edge.strength);
  }

  result.plates.reserve(plates.count());
  for(int plate=0;plate<plates.count();++plate)result.plates.emplace_back(bounds);
  execution.parallelRows(0,plates.count(),[&](int first,int last){
  for (int plate = first; plate < last; ++plate) {
    if (execution.cancelled()) throw std::runtime_error("Phase 4 hierarchy cancelled");
    auto &hierarchy = result.plates[size_t(plate)];
    std::vector<RegionStats> yStats(result.atomicRegionCount),
        abStats(result.atomicRegionCount);
    auto appearance = plates.appearance(plate);
    for (int y = bounds.y1; y < bounds.y2; ++y)
      for (int x = bounds.x1; x < bounds.x2; ++x) {
        int region = result.atomicRegion[index(x, y)];
        float alpha = plates.alpha(plate).at(x, y);
        auto accumulate = [&](RegionStats &stats, float support, bool chroma) {
          float weight = support >= .02f ? support : 0.0f;
          stats.support += support;
          stats.alpha += alpha;
          stats.mass += weight;
          stats.area += 1;
          for (auto [dx,dy] : std::array<std::pair<int,int>,4>{{{1,0},{-1,0},{0,1},{0,-1}}}) {
            int qx=x+dx,qy=y+dy;
            if (qx<bounds.x1 || qx>=bounds.x2 || qy<bounds.y1 || qy>=bounds.y2 ||
                result.atomicRegion[index(qx,qy)] != region) stats.perimeter += 1;
          }
          double yy = appearance.y.at(x, y), aa = appearance.a.at(x, y),
                 bb = appearance.b.at(x, y);
          stats.y += weight * yy / toneScale;
          stats.a += weight * aa;
          stats.b += weight * bb;
          stats.y2 += weight * yy * yy;
          stats.a2 += weight * aa * aa;
          stats.b2 += weight * bb * bb;
          int xp=std::min(bounds.x2-1,x+1), yp=std::min(bounds.y2-1,y+1);
          auto domain=chroma?plates.supportAB(plate):plates.supportY(plate);
          // A weak/absent neighbor cannot supply a strong conditional layer
          // gradient. Common source contours still govern geometry there.
          bool validX=domain.at(xp,y)>=.02f,validY=domain.at(x,yp)>=.02f;
          std::array<double,6> f{};
          if (chroma) {
            f = {aa/colorScale, bb/colorScale,
                 validX?.15*(appearance.a.at(xp,y)-aa)/abScale:0,
                 validY?.15*(appearance.a.at(x,yp)-aa)/abScale:0,
                 validX?.15*(appearance.b.at(xp,y)-bb)/abScale:0,
                 validY?.15*(appearance.b.at(x,yp)-bb)/abScale:0};
          } else {
            f = {yy/toneScale,
                 validX?.15*(appearance.y.at(xp,y)-yy)/yScale:0,
                 validY?.15*(appearance.y.at(x,yp)-yy)/yScale:0,0,0,0};
          }
          for (int k=0;k<6;++k) {
            stats.feature[size_t(k)] += weight*f[size_t(k)];
            stats.feature2[size_t(k)] += weight*f[size_t(k)]*f[size_t(k)];
          }
        };
        accumulate(yStats[region], plates.supportY(plate).at(x, y), false);
        accumulate(abStats[region], plates.supportAB(plate).at(x, y), true);
      }

    auto yTree = buildTree(std::move(yStats), regionEdges, false, params,
                           params.lumaChunkScale, execution);
    auto abTree = buildTree(std::move(abStats), regionEdges, true, params,
                            params.chromaChunkScale, execution);
    hierarchy.yMinimumDisappearance = yTree.firstLevel;
    hierarchy.abMinimumDisappearance = abTree.firstLevel;
    hierarchy.yChunkCount = yTree.count;
    hierarchy.abChunkCount = abTree.count;
    hierarchy.yTree = yTree.nodes;
    hierarchy.abTree = abTree.nodes;
    hierarchy.yChunk.resize(pixelCount);
    hierarchy.abChunk.resize(pixelCount);
    for (int p = 0; p < pixelCount; ++p) {
      hierarchy.yChunk[p] = yTree.labels[size_t(result.atomicRegion[p])];
      hierarchy.abChunk[p] = abTree.labels[size_t(result.atomicRegion[p])];
    }
    const float infinity = std::numeric_limits<float>::infinity();
    hierarchy.yEdgeX.assign(size_t(pixelCount), infinity);
    hierarchy.yEdgeY.assign(size_t(pixelCount), infinity);
    hierarchy.abEdgeX.assign(size_t(pixelCount), infinity);
    hierarchy.abEdgeY.assign(size_t(pixelCount), infinity);
    for (const auto &e : pixelEdges) {
      int a = result.atomicRegion[size_t(e.first)], b = result.atomicRegion[size_t(e.second)];
      bool horizontal = e.second == e.first + 1;
      (horizontal ? hierarchy.yEdgeX : hierarchy.yEdgeY)[size_t(e.first)] = yTree.edgeLevel(a,b);
      (horizontal ? hierarchy.abEdgeX : hierarchy.abEdgeY)[size_t(e.first)] = abTree.edgeLevel(a,b);
    }

    auto markBoundaries = [&](const std::vector<int> &chunks,
                              FloatPlaneView removed, FloatPlaneView retained) {
      for (const auto &edge : pixelEdges) {
        if (result.atomicRegion[edge.first] == result.atomicRegion[edge.second])
          continue;
        int x0 = bounds.x1 + edge.first % width;
        int y0 = bounds.y1 + edge.first / width;
        int x1 = bounds.x1 + edge.second % width;
        int y1 = bounds.y1 + edge.second / width;
        auto field =
            chunks[edge.first] == chunks[edge.second] ? removed : retained;
        field.at(x0, y0) = field.at(x1, y1) = 1.0f;
      }
    };
    markBoundaries(hierarchy.yChunk, hierarchy.yRemovedBoundaries.view(),
                   hierarchy.yRetainedBoundaries.view());
    markBoundaries(hierarchy.abChunk, hierarchy.abRemovedBoundaries.view(),
                   hierarchy.abRetainedBoundaries.view());
  }});
  return result;
}

void cutPhase4RegionHierarchy(Phase4RegionHierarchy &h,float yScale,float abScale){
  const auto b=h.bounds;const int width=b.width(),n=width*b.height();
  for(auto& plate:h.plates){
    auto cut=[&](const std::vector<Phase4MergeNode>& tree,float scale,std::vector<int>& chunks,int& count,OwnedPlane& removed,OwnedPlane& retained){
      std::vector<int> roots(tree.size()),labels(tree.size(),-1);for(size_t i=0;i<roots.size();++i)roots[i]=int(i);
      for(int i=int(tree.size())-1;i>=0;--i)for(int child:{tree[i].left,tree[i].right})if(child>=0)roots[child]=(scale>0 && tree[i].level<=scale)?roots[i]:child;
      count=0;for(int i=0;i<h.atomicRegionCount;++i)if(labels[roots[i]]<0)labels[roots[i]]=count++;
      chunks.resize(n);for(int p=0;p<n;++p)chunks[p]=labels[roots[h.atomicRegion[p]]];
      auto r=removed.view(),t=retained.view();for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x)r.at(x,y)=t.at(x,y)=0;
      for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){int p=(y-b.y1)*width+x-b.x1;for(auto [dx,dy]:std::array<std::pair<int,int>,2>{{{1,0},{0,1}}}){int xx=x+dx,yy=y+dy;if(xx>=b.x2 || yy>=b.y2)continue;int q=p+dx+dy*width;if(h.atomicRegion[p]==h.atomicRegion[q])continue;auto f=chunks[p]==chunks[q]?r:t;f.at(x,y)=f.at(xx,yy)=1;}}
    };
    cut(plate.yTree,yScale,plate.yChunk,plate.yChunkCount,plate.yRemovedBoundaries,plate.yRetainedBoundaries);
    cut(plate.abTree,abScale,plate.abChunk,plate.abChunkCount,plate.abRemovedBoundaries,plate.abRetainedBoundaries);
  }
}

} // namespace pigment
