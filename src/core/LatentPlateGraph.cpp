#include "core/LatentPlateGraph.h"
#include "core/SpectralMattingBasis.h"

#include <Eigen/Dense>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pigment {
namespace {

constexpr float kEpsilon = 1.0e-7f;

float clamp01(float v) noexcept { return std::max(0.0f, std::min(1.0f, v)); }

struct Sample {
  float y = 0.0f, a = 0.0f, b = 0.0f;
};

Sample add(Sample x, Sample y) noexcept {
  return {x.y + y.y, x.a + y.a, x.b + y.b};
}
Sample mul(Sample x, float s) noexcept { return {x.y * s, x.a * s, x.b * s}; }
float distance2(Sample x, Sample y, Sample scale) noexcept {
  const float dy = (x.y - y.y) / scale.y, da = (x.a - y.a) / scale.a,
              db = (x.b - y.b) / scale.b;
  return dy * dy + da * da + db * db;
}

float median(std::vector<float> values) {
  if (values.empty())
    return 0.0f;
  const auto middle =
      values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
  std::nth_element(values.begin(), middle, values.end());
  return *middle;
}

Sample robustScale(const std::vector<Sample> &samples) {
  std::vector<float> y, a, b;
  y.reserve(samples.size());
  a.reserve(samples.size());
  b.reserve(samples.size());
  for (const auto &s : samples) {
    y.push_back(s.y);
    a.push_back(s.a);
    b.push_back(s.b);
  }
  const float my = median(y), ma = median(a), mb = median(b);
  for (auto &v : y)
    v = std::abs(v - my);
  for (auto &v : a)
    v = std::abs(v - ma);
  for (auto &v : b)
    v = std::abs(v - mb);
  return {std::max(1e-4f, 1.4826f * median(y)),
          std::max(1e-4f, 1.4826f * median(a)),
          std::max(1e-4f, 1.4826f * median(b))};
}

struct AnalysisImage {
  int width = 0, height = 0;
  RectI sourceBounds{};
  std::vector<Sample> pixels;
  int index(int x, int y) const noexcept { return y * width + x; }
};

AnalysisImage downsample(ConstYabPlanes source) {
  const RectI b = source.y.bounds;
  // Gate-A crops are deliberately solved at full resolution.  This is the
  // approved Spectral-Matting escalation for separating recovery artifacts
  // from reduced-grid artifacts.  Large frames retain the bounded analysis
  // grid used by the research renderer.
  const int sourceLong = std::max(b.width(), b.height());
  const int targetLong = sourceLong <= 512
                             ? sourceLong
                             : std::min(1024, std::max(1, sourceLong / 2));
  const float factor =
      float(targetLong) / float(std::max(1, std::max(b.width(), b.height())));
  AnalysisImage out;
  out.width = std::max(1, int(std::round(b.width() * factor)));
  out.height = std::max(1, int(std::round(b.height() * factor)));
  out.sourceBounds = b;
  out.pixels.resize(size_t(out.width) * out.height);
  for (int ay = 0; ay < out.height; ++ay)
    for (int ax = 0; ax < out.width; ++ax) {
      const int x0 = b.x1 + int((int64_t(ax) * b.width()) / out.width);
      const int x1 = b.x1 + int((int64_t(ax + 1) * b.width()) / out.width);
      const int y0 = b.y1 + int((int64_t(ay) * b.height()) / out.height);
      const int y1 = b.y1 + int((int64_t(ay + 1) * b.height()) / out.height);
      Sample sum{};
      int count = 0;
      for (int y = y0; y < std::max(y0 + 1, y1); ++y)
        for (int x = x0; x < std::max(x0 + 1, x1); ++x) {
          const int sx = std::min(b.x2 - 1, x), sy = std::min(b.y2 - 1, y);
          sum = add(sum, {source.y.at(sx, sy), source.a.at(sx, sy),
                          source.b.at(sx, sy)});
          ++count;
        }
      out.pixels[out.index(ax, ay)] = mul(sum, 1.0f / std::max(1, count));
    }
  return out;
}

SparseAffinityGraph buildGraph(const AnalysisImage &image,
                               const Phase4Params &params,
                               const ImageGeometry &geometry) {
  (void)params;
  SparseAffinityGraph graph;
  graph.width = image.width;
  graph.height = image.height;
  const int n = graph.nodeCount();
  graph.rowOffsets.resize(size_t(n) + 1);
  const Sample scale = robustScale(image.pixels);
  const float sourcePerAnalysisX =
      float(image.sourceBounds.width()) / image.width;
  const float sourcePerAnalysisY =
      float(image.sourceBounds.height()) / image.height;
  const float par = float(std::max(1e-6, geometry.pixelAspect));
  Sample minimum = image.pixels.front(), maximum = image.pixels.front();
  for (const auto &value : image.pixels) {
    minimum.y = std::min(minimum.y, value.y);
    minimum.a = std::min(minimum.a, value.a);
    minimum.b = std::min(minimum.b, value.b);
    maximum.y = std::max(maximum.y, value.y);
    maximum.a = std::max(maximum.a, value.a);
    maximum.b = std::max(maximum.b, value.b);
  }
  auto bucketKey = [&](Sample value) {
    auto bin = [](float value, float low, float high) {
      return std::max(
          0, std::min(7, int(8 * (value - low) / std::max(1e-8f, high - low))));
    };
    return (bin(value.y, minimum.y, maximum.y) << 6) |
           (bin(value.a, minimum.a, maximum.a) << 3) |
           bin(value.b, minimum.b, maximum.b);
  };
  std::array<std::vector<int>, 512> buckets;
  for (int p = 0; p < n; ++p)
    buckets[size_t(bucketKey(image.pixels[p]))].push_back(p);
  constexpr std::array<std::pair<int, int>, 8> local{
      {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}}};
  for (int y = 0; y < image.height; ++y)
    for (int x = 0; x < image.width; ++x) {
      const int p = image.index(x, y);
      graph.rowOffsets[p] = int(graph.edges.size());
      std::vector<int> candidates;
      auto collect = [&](int qx, int qy) {
        if (qx < 0 || qx >= image.width || qy < 0 || qy >= image.height)
          return;
        int q = image.index(qx, qy);
        if (q != p && std::find(candidates.begin(), candidates.end(), q) ==
                          candidates.end())
          candidates.push_back(q);
      };
      for (auto [dx, dy] : local)
        collect(x + dx, y + dy);
      const int mixtureCount = int(candidates.size());
      const auto &bucket = buckets[size_t(bucketKey(image.pixels[p]))];
      std::vector<std::pair<float, int>> nonlocal;
      for (int sample = 0; sample < 32 && !bucket.empty(); ++sample) {
        uint32_t state =
            uint32_t(p + 1) * 0x9e3779b9u + uint32_t(sample + 1) * 0x85ebca6bu;
        state ^= state >> 16;
        state *= 0x7feb352du;
        state ^= state >> 15;
        int q = bucket[size_t(state) % bucket.size()];
        int qx = q % image.width, qy = q / image.width;
        float physical = std::hypot((qx - x) * sourcePerAnalysisX / par,
                                    (qy - y) * sourcePerAnalysisY);
        if (q != p &&
            physical > 4 * std::max(sourcePerAnalysisX, sourcePerAnalysisY))
          nonlocal.push_back(
              {distance2(image.pixels[p], image.pixels[q], scale) +
                   1e-5f * physical,
               q});
      }
      std::sort(nonlocal.begin(), nonlocal.end());
      for (size_t i = 0; i < std::min<size_t>(4, nonlocal.size()); ++i) {
        int q = nonlocal[i].second;
        collect(q % image.width, q / image.width);
      }
      const int count = int(candidates.size());
      Eigen::MatrixXd system =
          Eigen::MatrixXd::Zero(mixtureCount + 1, mixtureCount + 1);
      Eigen::VectorXd target = Eigen::VectorXd::Zero(mixtureCount + 1);
      for (int i = 0; i < mixtureCount; ++i) {
        Sample ci = image.pixels[candidates[size_t(i)]];
        for (int j = 0; j < mixtureCount; ++j) {
          Sample cj = image.pixels[candidates[size_t(j)]];
          system(i, j) = (ci.y * cj.y) / (scale.y * scale.y) +
                         (ci.a * cj.a) / (scale.a * scale.a) +
                         (ci.b * cj.b) / (scale.b * scale.b);
        }
        system(i, i) += 1e-1;
        system(i, mixtureCount) = system(mixtureCount, i) = 1;
        target[i] = (ci.y * image.pixels[p].y) / (scale.y * scale.y) +
                    (ci.a * image.pixels[p].a) / (scale.a * scale.a) +
                    (ci.b * image.pixels[p].b) / (scale.b * scale.b);
      }
      target[mixtureCount] = 1;
      Eigen::VectorXd solution = system.fullPivLu().solve(target);
      double positiveSum = 0;
      for (int i = 0; i < mixtureCount; ++i)
        positiveSum += std::max(0.0, solution[i]);
      for (int i = 0; i < count; ++i) {
        int q = candidates[size_t(i)], qx = q % image.width,
            qy = q / image.width;
        float d2 = distance2(image.pixels[p], image.pixels[q], scale);
        float physical = std::hypot((qx - x) * sourcePerAnalysisX / par,
                                    (qy - y) * sourcePerAnalysisY);
        float boundary = 1 - std::exp(-.5f * d2);
        float signedWeight = i < mixtureCount ? float(solution[i]) : 0;
        float capacity =
            i < mixtureCount
                ? float(std::max(0.0, solution[i]) /
                        std::max(1e-12, positiveSum)) *
                      std::exp(-.5f * d2)
                : .25f * std::exp(-.5f * d2) / (1 + .05f * physical);
        graph.edges.push_back(
            {q, signedWeight, clamp01(capacity), physical, boundary});
      }
    }
  graph.rowOffsets[n] = int(graph.edges.size());
  return graph;
}

struct SpectralBasis {
  int count = 0;
  std::vector<float> values, eigenvalues, eigenResidual;
};

Eigen::VectorXd projectSimplex(const Eigen::VectorXd &value) {
  std::vector<double> sorted(static_cast<size_t>(value.size()));
  for (Eigen::Index i = 0; i < value.size(); ++i)
    sorted[size_t(i)] = value[i];
  std::sort(sorted.begin(), sorted.end(), std::greater<double>());
  double sum = 0, theta = 0;
  for (size_t i = 0; i < sorted.size(); ++i) {
    sum += sorted[i];
    double candidate = (sum - 1.0) / double(i + 1);
    if (i + 1 == sorted.size() || sorted[i + 1] <= candidate) {
      theta = candidate;
      break;
    }
  }
  Eigen::VectorXd output(value.size());
  for (Eigen::Index i = 0; i < value.size(); ++i)
    output[i] = std::max(0.0, value[i] - theta);
  return output;
}

std::vector<int> deterministicKMeans(const Eigen::MatrixXd &features, int count,
                                     int start) {
  const int n = int(features.rows()), dimensions = int(features.cols());
  std::vector<int> seeds;
  seeds.reserve(count);
  int first = 0;
  if (start == 0) {
    double firstScore = -1;
    for (int p = 0; p < n; ++p) {
      double score = features.row(p).squaredNorm();
      if (score > firstScore) {
        firstScore = score;
        first = p;
      }
    }
  } else {
    uint32_t state = 0x9e3779b9u * uint32_t(start + 1);
    state ^= state >> 16;
    state *= 0x7feb352du;
    state ^= state >> 15;
    first = int(state % uint32_t(std::max(1, n)));
  }
  seeds.push_back(first);
  Eigen::VectorXd nearest =
      Eigen::VectorXd::Constant(n, std::numeric_limits<double>::infinity());
  while (int(seeds.size()) < count) {
    int selected = 0;
    double far = -1;
    int latest = seeds.back();
    for (int p = 0; p < n; ++p) {
      nearest[p] = std::min(
          nearest[p], (features.row(p) - features.row(latest)).squaredNorm());
      double candidate =
          nearest[p] * (1.0 + 1e-12 * ((p + start * 3571) % 991));
      if (candidate > far) {
        far = candidate;
        selected = p;
      }
    }
    seeds.push_back(selected);
  }
  Eigen::MatrixXd centers(count, dimensions);
  for (int c = 0; c < count; ++c)
    centers.row(c) = features.row(seeds[c]);
  std::vector<int> labels(static_cast<size_t>(n));
  for (int iteration = 0; iteration < 40; ++iteration) {
    bool changed = false;
    for (int p = 0; p < n; ++p) {
      int best = 0;
      double distance = std::numeric_limits<double>::infinity();
      for (int c = 0; c < count; ++c) {
        double d = (features.row(p) - centers.row(c)).squaredNorm();
        if (d < distance) {
          distance = d;
          best = c;
        }
      }
      changed |= labels[size_t(p)] != best;
      labels[size_t(p)] = best;
    }
    Eigen::MatrixXd next = Eigen::MatrixXd::Zero(count, dimensions);
    std::vector<int> mass(static_cast<size_t>(count));
    for (int p = 0; p < n; ++p) {
      next.row(labels[size_t(p)]) += features.row(p);
      ++mass[size_t(labels[size_t(p)])];
    }
    for (int c = 0; c < count; ++c) {
      if (mass[size_t(c)])
        next.row(c) /= mass[size_t(c)];
      else
        next.row(c) = features.row(seeds[c]);
    }
    centers.swap(next);
    if (!changed && iteration > 0)
      break;
  }
  return labels;
}

std::vector<int> progressiveKMeans(const Eigen::MatrixXd &features, int count,
                                   int start, int &achieved) {
  const int n = int(features.rows()), dimensions = int(features.cols());
  int active = std::min(5, count);
  auto labels = deterministicKMeans(features, active, start);
  while (active < count) {
    double bestGain = -1;
    int bestCluster = -1;
    std::vector<int> bestMembers, bestSide;
    for (int cluster = 0; cluster < active; ++cluster) {
      std::vector<int> members;
      for (int p = 0; p < n; ++p)
        if (labels[size_t(p)] == cluster)
          members.push_back(p);
      if (members.size() < 4)
        continue;
      Eigen::RowVectorXd mean = Eigen::RowVectorXd::Zero(dimensions);
      for (int p : members)
        mean += features.row(p);
      mean /= double(members.size());
      double oldError = 0;
      int seed0 = members.front();
      double far = -1;
      for (int p : members) {
        double d = (features.row(p) - mean).squaredNorm();
        oldError += d;
        if (d > far) {
          far = d;
          seed0 = p;
        }
      }
      int seed1 = seed0;
      far = -1;
      for (int p : members) {
        double d = (features.row(p) - features.row(seed0)).squaredNorm();
        if (d > far) {
          far = d;
          seed1 = p;
        }
      }
      Eigen::MatrixXd centers(2, dimensions);
      centers.row(0) = features.row(seed0);
      centers.row(1) = features.row(seed1);
      std::vector<int> side(members.size());
      for (int iteration = 0; iteration < 30; ++iteration) {
        Eigen::MatrixXd next = Eigen::MatrixXd::Zero(2, dimensions);
        int mass[2]{};
        for (size_t j = 0; j < members.size(); ++j) {
          int p = members[j];
          double d0 = (features.row(p) - centers.row(0)).squaredNorm(),
                 d1 = (features.row(p) - centers.row(1)).squaredNorm();
          side[j] = d1 < d0 ? 1 : 0;
          next.row(side[j]) += features.row(p);
          ++mass[side[j]];
        }
        if (!mass[0] || !mass[1])
          break;
        next.row(0) /= mass[0];
        next.row(1) /= mass[1];
        if ((next - centers).squaredNorm() < 1e-14) {
          centers = next;
          break;
        }
        centers = next;
      }
      double error = 0;
      int sideMass[2]{};
      for (size_t j = 0; j < members.size(); ++j) {
        error +=
            (features.row(members[j]) - centers.row(side[j])).squaredNorm();
        ++sideMass[side[j]];
      }
      double gain = oldError - error;
      if (sideMass[0] && sideMass[1] && gain > bestGain) {
        bestGain = gain;
        bestCluster = cluster;
        bestMembers = std::move(members);
        bestSide = std::move(side);
      }
    }
    if (bestCluster < 0)
      break;
    for (size_t j = 0; j < bestMembers.size(); ++j)
      if (bestSide[j])
        labels[size_t(bestMembers[j])] = active;
    ++active;
  }
  achieved = active;
  return labels;
}

struct ComponentRecovery {
  int count = 0;
  std::vector<float> values;
};

std::vector<std::vector<Sample>> buildSpatialAppearances(
    const AnalysisImage &image, const SparseAffinityGraph &graph,
    const std::vector<float> &alpha, const std::vector<Sample> &globalMean,
    int componentCount, Phase4GateDiagnostics &diagnostics,
    Phase4AppearanceDistributionGrid &retained,
    const ExecutionContext &execution) {
  const int n = graph.nodeCount();
  struct Distribution {
    Eigen::Vector3d mean = Eigen::Vector3d::Zero();
    Eigen::Matrix3d covariance = Eigen::Matrix3d::Identity();
  };
  auto color = [&](int p) {
    const auto &v = image.pixels[size_t(p)];
    return Eigen::Vector3d(v.y,v.a,v.b);
  };
  // Overlapping local color-distribution cells, not pixel/alpha smoothing.
  // The prior carries a full YAB covariance rather than one scalar variance.
  // Alphas and their recovery are fixed throughout this conditional solve.
  constexpr int spacing = 32, radius = 32;
  const int cols = (image.width-1)/spacing+2;
  const int rows = (image.height-1)/spacing+2;
  std::vector<std::vector<Distribution>> cells(
      size_t(componentCount), std::vector<Distribution>(size_t(cols)*rows));
  auto scale = robustScale(image.pixels);
  Eigen::Vector3d floor(1e-5*scale.y*scale.y+1e-8,
                        1e-5*scale.a*scale.a+1e-8,
                        1e-5*scale.b*scale.b+1e-8);
  for(int c=0;c<componentCount;++c) {
    if(execution.cancelled()) throw std::runtime_error("Phase 4 appearance cancelled");
    double globalMass=0;
    Eigen::Matrix3d globalMoment=Eigen::Matrix3d::Zero();
    Eigen::Vector3d center(globalMean[c].y,globalMean[c].a,globalMean[c].b);
    for(int p=0;p<n;++p) {
      double w=alpha[size_t(c)*n+p];Eigen::Vector3d d=color(p)-center;
      globalMass+=w;globalMoment+=w*d*d.transpose();
    }
    Eigen::Matrix3d prior=globalMoment/std::max(1e-12,globalMass);
    prior.diagonal()+=floor;
    for(int cy=0;cy<rows;++cy) for(int cx=0;cx<cols;++cx) {
      const int x=std::min(image.width-1,cx*spacing);
      const int y=std::min(image.height-1,cy*spacing);
      double mass=0;
      Eigen::Vector3d sum=Eigen::Vector3d::Zero();
      Eigen::Matrix3d second=Eigen::Matrix3d::Zero();
      for(int yy=std::max(0,y-radius);yy<std::min(image.height,y+radius+1);++yy)
        for(int xx=std::max(0,x-radius);xx<std::min(image.width,x+radius+1);++xx) {
          int p=yy*image.width+xx;double w=alpha[size_t(c)*n+p];
          auto v=color(p);mass+=w;sum+=w*v;second+=w*v*v.transpose();
        }
      auto &cell=cells[size_t(c)][size_t(cy)*cols+cx];
      if(mass<1e-6) {cell.mean=center;cell.covariance=prior;continue;}
      cell.mean=sum/mass;
      Eigen::Matrix3d covariance=second/mass-cell.mean*cell.mean.transpose();
      // Roundoff must not turn a distribution covariance indefinite.
      Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> eig(covariance);
      cell.covariance=eig.eigenvectors()*
          eig.eigenvalues().cwiseMax(0.0).asDiagonal()*eig.eigenvectors().transpose();
      cell.covariance.diagonal()+=floor;
    }
  }
  retained.width=cols;retained.height=rows;retained.spacing=spacing;
  retained.components.resize(size_t(componentCount));
  for(int c=0;c<componentCount;++c) {
    auto &out=retained.components[size_t(c)];out.resize(size_t(cols)*rows);
    for(size_t p=0;p<out.size();++p) {
      const auto &d=cells[size_t(c)][p];
      for(int k=0;k<3;++k) out[p].mean[size_t(k)]=d.mean[k];
      for(int r=0;r<3;++r) for(int k=0;k<3;++k)
        out[p].covariance[size_t(r)*3+k]=d.covariance(r,k);
    }
  }
  std::vector<std::vector<Sample>> appearance(
      static_cast<size_t>(componentCount),std::vector<Sample>(static_cast<size_t>(n)));
  double reconstructionError=0,spatialVariation=0;
  std::vector<Distribution> local{size_t(componentCount)};
  for(int p=0;p<n;++p) {
    if((p&4095)==0 && execution.cancelled())
      throw std::runtime_error("Phase 4 appearance cancelled");
    int x=p%image.width,y=p/image.width,cx=x/spacing,cy=y/spacing;
    double tx=double(x%spacing)/spacing,ty=double(y%spacing)/spacing;
    Eigen::Vector3d mixture=Eigen::Vector3d::Zero();
    Eigen::Matrix3d mixtureCovariance=Eigen::Matrix3d::Zero();
    for(int c=0;c<componentCount;++c) {
      auto &d=local[size_t(c)];
      d.mean.setZero();d.covariance.setZero();
      for(int oy=0;oy<2;++oy) for(int ox=0;ox<2;++ox) {
        double w=(ox?tx:1-tx)*(oy?ty:1-ty);
        const auto &cell=cells[size_t(c)][size_t(cy+oy)*cols+cx+ox];
        d.mean+=w*cell.mean;d.covariance+=w*cell.covariance;
      }
      double a=alpha[size_t(c)*n+p];
      mixture+=a*d.mean;mixtureCovariance+=a*d.covariance;
    }
    // Fixed-alpha Gaussian color refinement:
    // min sum_i alpha_i (C_i-mu_i)^T Sigma_i^-1 (C_i-mu_i)
    // subject to sum_i alpha_i C_i = source.
    // C_i = mu_i + Sigma_i (sum_j alpha_j Sigma_j)^-1 residual.
    // This is the conditional color-unmixing energy, not source copies or
    // confidence fallback. It preserves HDR and negative values unclipped.
    Eigen::Vector3d multiplier=mixtureCovariance.ldlt().solve(color(p)-mixture);
    Eigen::Vector3d check=Eigen::Vector3d::Zero();
    for(int c=0;c<componentCount;++c) {
      Eigen::Vector3d v=local[size_t(c)].mean+local[size_t(c)].covariance*multiplier;
      appearance[size_t(c)][size_t(p)]={float(v[0]),float(v[1]),float(v[2])};
      double a=alpha[size_t(c)*n+p];check+=a*v;
      Eigen::Vector3d mean(globalMean[c].y,globalMean[c].a,globalMean[c].b);
      spatialVariation+=a*(v-mean).squaredNorm();
    }
    reconstructionError+=(check-color(p)).squaredNorm();
  }
  diagnostics.appearanceUnmixingError=float(std::sqrt(reconstructionError/std::max(1,n)));
  diagnostics.appearanceSpatialVariation=float(std::sqrt(spatialVariation/std::max(1,n)));
  return appearance;
}

ComponentRecovery recoverComponents(const SpectralBasis &basis,
                                    const SparseAffinityGraph &g, int requested,
                                    float, Phase4GateDiagnostics &diag) {
  const int n = g.nodeCount(), m = basis.count;
  Eigen::MatrixXd vectors(n, m);
  for (int k = 0; k < m; ++k)
    for (int p = 0; p < n; ++p)
      vectors(p, k) = basis.values[size_t(k) * n + p];
  const int featureCount = std::min(20, m - 1);
  Eigen::MatrixXd features(n, featureCount);
  for (int k = 0; k < featureCount; ++k) {
    double scale =
        1.0 / std::sqrt(std::max(
                  1e-12, std::abs(double(basis.eigenvalues[size_t(k + 1)]))));
    features.col(k) = vectors.col(k + 1) * scale;
  }
  std::vector<int> labels;
  int count = std::min(5, requested);
  double bestObjective = std::numeric_limits<double>::infinity();
  for (int start = 0; start < 7; ++start) {
    int candidateCount = 0;
    auto candidate =
        progressiveKMeans(features, requested, start, candidateCount);
    Eigen::MatrixXd centers =
        Eigen::MatrixXd::Zero(candidateCount, featureCount);
    std::vector<int> mass(static_cast<size_t>(candidateCount));
    for (int p = 0; p < n; ++p) {
      centers.row(candidate[size_t(p)]) += features.row(p);
      ++mass[size_t(candidate[size_t(p)])];
    }
    for (int c = 0; c < candidateCount; ++c)
      if (mass[size_t(c)])
        centers.row(c) /= mass[size_t(c)];
    double objective = 0;
    for (int p = 0; p < n; ++p)
      objective +=
          (features.row(p) - centers.row(candidate[size_t(p)])).squaredNorm();
    objective /= std::max(1, n);
    if (objective < bestObjective) {
      bestObjective = objective;
      labels = std::move(candidate);
      count = candidateCount;
    }
  }
  Eigen::MatrixXd segments = Eigen::MatrixXd::Zero(n, count);
  for (int p = 0; p < n; ++p)
    segments(p, labels[size_t(p)]) = 1.0;
  Eigen::VectorXd eigenvalues(m);
  for (int k = 0; k < m; ++k)
    eigenvalues[k] = std::max(0.0, double(basis.eigenvalues[size_t(k)]));
  const Eigen::MatrixXd diagonal = eigenvalues.asDiagonal();
  // Frozen visual-basis recovery.  This is the simple constrained transform
  // used by the first successful Eigen/Spectra photographic contact sheets.
  // It deliberately does not optimize sparsity or component occupancy.
  constexpr double sparsity = 1.0, w0 = .3, w1 = .3, threshold = 1e-10;
  for (int iteration = 0; iteration < 12; ++iteration) {
    Eigen::MatrixXd e0(n, count), e1(n, count);
    for (int p = 0; p < n; ++p)
      for (int c = 0; c < count; ++c) {
        e0(p, c) = std::pow(w0, sparsity) *
                   std::pow(std::max(threshold, std::abs(segments(p, c))),
                            sparsity - 2);
        e1(p, c) = std::pow(w1, sparsity) *
                   std::pow(std::max(threshold, std::abs(segments(p, c) - 1.0)),
                            sparsity - 2);
        e0(p, c) = std::min(e0(p, c), 1e10);
        e1(p, c) = std::min(e1(p, c), 1e10);
      }
    std::vector<Eigen::MatrixXd> blocks(static_cast<size_t>(count));
    std::vector<Eigen::VectorXd> rhs(static_cast<size_t>(count));
    for (int c = 0; c < count; ++c) {
      Eigen::VectorXd weights = e0.col(c) + e1.col(c);
      blocks[size_t(c)] =
          vectors.transpose() * weights.asDiagonal() * vectors + diagonal;
      rhs[size_t(c)] =
          vectors.transpose() * (c == count - 1 ? e0.col(c) : e1.col(c));
    }
    const int dimension = (count - 1) * m;
    Eigen::MatrixXd system = Eigen::MatrixXd::Zero(dimension, dimension);
    Eigen::VectorXd target(dimension);
    for (int row = 0; row < count - 1; ++row) {
      target.segment(row * m, m) = rhs[size_t(row)] + rhs.back();
      for (int column = 0; column < count - 1; ++column)
        system.block(row * m, column * m, m, m) = blocks.back();
      system.block(row * m, row * m, m, m) += blocks[size_t(row)];
    }
    system.diagonal().array() += 1e-10;
    Eigen::VectorXd solution = system.ldlt().solve(target);
    Eigen::MatrixXd coefficients = Eigen::Map<
        Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor>>(
        solution.data(), m, count - 1);
    segments.leftCols(count - 1) = vectors * coefficients;
    segments.col(count - 1) =
        Eigen::VectorXd::Ones(n) - segments.leftCols(count - 1).rowwise().sum();
  }
  Eigen::MatrixXd projected(n, count);
  for (int p = 0; p < n; ++p)
    projected.row(p) = projectSimplex(segments.row(p).transpose()).transpose();
  // Keep weak vocabulary entries.  The requested count is a capacity, but
  // harmless weak functions are preferable to changing the validated basis.
  // Downstream public grouping decides whether they carry useful plate mass.
  Eigen::MatrixXd compact = std::move(projected);
  double projection = 0, effective = 0, entropy = 0;
  int hard = 0;
  diag.componentOccupancy.assign(count, 0);
  diag.componentCorrelation.assign(size_t(count) * count, 0);
  diag.componentMattingEnergy.assign(count, 0);
  for (int c = 0; c < count; ++c) {
    Eigen::VectorXd approximation =
        vectors * (vectors.transpose() * compact.col(c));
    projection += (approximation - compact.col(c)).squaredNorm();
    diag.componentOccupancy[c] = float(compact.col(c).sum() / n);
    double energy = 0;
    for (int p = 0; p < n; ++p)
      for (int edge = g.rowOffsets[p]; edge < g.rowOffsets[p + 1]; ++edge) {
        double delta = compact(p, c) - compact(g.edges[edge].target, c);
        energy += g.edges[edge].weight * delta * delta;
      }
    diag.componentMattingEnergy[c] = float(.5 * energy / n);
  }
  Eigen::MatrixXd gram = compact.transpose() * compact;
  Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> rankSolver(gram);
  double eigenSum = rankSolver.eigenvalues().sum(), rankEntropy = 0;
  for (double value : rankSolver.eigenvalues())
    if (value > 1e-15 && eigenSum > 0) {
      double probability = value / eigenSum;
      rankEntropy -= probability * std::log(probability);
    }
  diag.componentEffectiveRank = float(std::exp(rankEntropy));
  for (int i = 0; i < count; ++i)
    for (int j = 0; j < count; ++j) {
      double denominator = std::sqrt(std::max(1e-20, gram(i, i) * gram(j, j)));
      diag.componentCorrelation[size_t(i) * count + j] =
          float(gram(i, j) / denominator);
    }
  for (int p = 0; p < n; ++p) {
    double squares = compact.row(p).squaredNorm();
    effective += 1.0 / std::max(1e-12, squares);
    double pixelEntropy = 0;
    for (int c = 0; c < count; ++c)
      if (compact(p, c) > 1e-12)
        pixelEntropy -= compact(p, c) * std::log(compact(p, c));
    entropy += pixelEntropy;
    if (compact.row(p).maxCoeff() > .98)
      ++hard;
  }
  diag.requestedLatentCount = requested;
  diag.activeLatentCount = count;
  diag.componentProjectionError =
      float(std::sqrt(projection / std::max(1, n * count)));
  diag.meanEffectiveComponents = float(effective / n);
  diag.meanComponentEntropy = float(entropy / n);
  diag.hardPixelFraction = float(hard) / n;
  ComponentRecovery output;
  output.count = count;
  output.values.resize(size_t(n) * count);
  for (int c = 0; c < count; ++c)
    for (int p = 0; p < n; ++p)
      output.values[size_t(c) * n + p] = float(compact(p, c));
  return output;
}

std::vector<float> groupLatents(const AnalysisImage &image,
                                const std::vector<float> &alpha,
                                const std::vector<Sample> &means,
                                const std::vector<float> &variances,
                                const SpectralBasis &basis,
                                const Phase4Params &params) {
  const int latent = params.latentCount, plates = params.plateCount,
            n = image.width * image.height;
  constexpr int dimensions = 11;
  Eigen::MatrixXd descriptors = Eigen::MatrixXd::Zero(latent, dimensions);
  std::vector<double> mass(static_cast<size_t>(latent));
  for (int c = 0; c < latent; ++c) {
    double cx = 0, cy = 0, vx = 0, vy = 0;
    for (int p = 0; p < n; ++p) {
      double w = alpha[size_t(c) * n + p];
      int x = p % image.width, y = p / image.width;
      mass[size_t(c)] += w;
      cx += w * x;
      cy += w * y;
    }
    cx /= std::max(1e-12, mass[size_t(c)]);
    cy /= std::max(1e-12, mass[size_t(c)]);
    for (int p = 0; p < n; ++p) {
      double w = alpha[size_t(c) * n + p];
      double dx = (p % image.width) - cx, dy = (p / image.width) - cy;
      vx += w * dx * dx;
      vy += w * dy * dy;
    }
    descriptors(c, 0) = means[c].y;
    descriptors(c, 1) = means[c].a;
    descriptors(c, 2) = means[c].b;
    descriptors(c, 3) = cx / std::max(1, image.width - 1);
    descriptors(c, 4) = cy / std::max(1, image.height - 1);
    descriptors(c, 5) = std::sqrt(vx / std::max(1e-12, mass[size_t(c)])) /
                        std::max(1, image.width);
    descriptors(c, 6) = std::sqrt(vy / std::max(1e-12, mass[size_t(c)])) /
                        std::max(1, image.height);
    descriptors(c, 7) = std::sqrt(std::max(0.0f, variances[c]));
    for (int k = 0; k < 3 && k + 1 < basis.count; ++k) {
      double coefficient = 0;
      for (int p = 0; p < n; ++p)
        coefficient +=
            alpha[size_t(c) * n + p] * basis.values[size_t(k + 1) * n + p];
      descriptors(c, 8 + k) = coefficient / std::max(1e-12, mass[size_t(c)]);
    }
  }
  for (int d = 0; d < dimensions; ++d) {
    double mean = descriptors.col(d).mean();
    double scale =
        std::sqrt((descriptors.col(d).array() - mean).square().mean());
    descriptors.col(d) =
        (descriptors.col(d).array() - mean) / std::max(1e-5, scale);
  }
  Eigen::MatrixXd adjacency = Eigen::MatrixXd::Zero(latent, latent);
  for (int i = 0; i < latent; ++i)
    for (int j = i; j < latent; ++j) {
      double overlap = 0;
      for (int p = 0; p < n; ++p)
        overlap += alpha[size_t(i) * n + p] * alpha[size_t(j) * n + p];
      overlap /= std::sqrt(std::max(1e-12, mass[size_t(i)] * mass[size_t(j)]));
      adjacency(i, j) = adjacency(j, i) = overlap;
    }
  std::vector<int> anchors;
  anchors.reserve(plates);
  auto addBest = [&](auto score) {
    int best = 0;
    double value = -std::numeric_limits<double>::infinity();
    for (int c = 0; c < latent; ++c)
      if (std::find(anchors.begin(), anchors.end(), c) == anchors.end() &&
          score(c) > value) {
        value = score(c);
        best = c;
      }
    anchors.push_back(best);
  };
  addBest([&](int c) { return -double(means[c].y); });
  addBest([&](int c) { return double(means[c].y); });
  if (plates > 2)
    addBest([&](int c) {
      double best = std::numeric_limits<double>::infinity();
      for (int a : anchors)
        best = std::min(
            best, (descriptors.row(c) - descriptors.row(a)).squaredNorm());
      return best + 2.0 * (descriptors(c, 1) * descriptors(c, 1) +
                           descriptors(c, 2) * descriptors(c, 2));
    });
  while (int(anchors.size()) < plates)
    addBest([&](int c) {
      double best = std::numeric_limits<double>::infinity();
      for (int a : anchors)
        best = std::min(
            best, (descriptors.row(c) - descriptors.row(a)).squaredNorm());
      return best;
    });
  std::vector<double> nearest;
  for (int c = 0; c < latent; ++c) {
    double d = std::numeric_limits<double>::infinity();
    for (int a : anchors)
      d = std::min(d, (descriptors.row(c) - descriptors.row(a)).squaredNorm());
    if (d > 1e-8)
      nearest.push_back(d);
  }
  double base = 1.0;
  if (!nearest.empty()) {
    auto middle =
        nearest.begin() + static_cast<std::ptrdiff_t>(nearest.size() / 2);
    std::nth_element(nearest.begin(), middle, nearest.end());
    base = *middle;
  }
  double temperature = std::max(
      1e-4,
      base * (.02 + .08 * phase4PlateEntropyCoefficient(params.plateOverlap)));
  Eigen::MatrixXd assignment = Eigen::MatrixXd::Zero(latent, plates),
                  centers(plates, dimensions);
  for (int i = 0; i < plates; ++i)
    centers.row(i) = descriptors.row(anchors[size_t(i)]);
  for (int iteration = 0; iteration < 12; ++iteration) {
    for (int c = 0; c < latent; ++c) {
      double maximum = -std::numeric_limits<double>::infinity();
      for (int i = 0; i < plates; ++i) {
        double neighbor = 0, norm = 0;
        for (int l = 0; l < latent; ++l) {
          neighbor += adjacency(c, l) * assignment(l, i);
          norm += adjacency(c, l);
        }
        double value =
            -(descriptors.row(c) - centers.row(i)).squaredNorm() / temperature +
            (iteration ? params.plateOverlap * neighbor / std::max(1e-9, norm)
                       : 0);
        assignment(c, i) = value;
        maximum = std::max(maximum, value);
      }
      double sum = 0;
      for (int i = 0; i < plates; ++i) {
        assignment(c, i) = std::exp(assignment(c, i) - maximum);
        sum += assignment(c, i);
      }
      assignment.row(c) /= std::max(1e-12, sum);
    }
    // Preserve the canonical public identities.  The public alpha remains
    // soft because each latent alpha is fractional; this constraint merely
    // prevents all latent-to-plate assignments from drifting to equal rows.
    for (int i = 0; i < plates; ++i) {
      assignment.row(anchors[size_t(i)]).setZero();
      assignment(anchors[size_t(i)], i) = 1.0;
    }
    centers.setZero();
    Eigen::VectorXd centerMass = Eigen::VectorXd::Zero(plates);
    for (int c = 0; c < latent; ++c)
      for (int i = 0; i < plates; ++i) {
        double w = mass[size_t(c)] * assignment(c, i);
        centers.row(i) += w * descriptors.row(c);
        centerMass[i] += w;
      }
    for (int i = 0; i < plates; ++i)
      if (centerMass[i] > 1e-12)
        centers.row(i) /= centerMass[i];
  }
  std::vector<float> output(size_t(latent) * plates);
  for (int c = 0; c < latent; ++c)
    for (int i = 0; i < plates; ++i)
      output[size_t(c) * plates + i] = float(assignment(c, i));
  return output;
}

float graphEdgeCost(const SparseAffinityEdge &e, float structureRespect) {
  return e.physicalDistance * (1.0f - std::log(std::max(1e-6f, e.weight)) +
                               structureRespect * 4.0f * e.boundary);
}

std::vector<float> maxProductSupport(const SparseAffinityGraph &g,
                                     const float *seed, float radius,
                                     float structureRespect) {
  const int n = g.nodeCount();
  std::vector<float> distance(n, std::numeric_limits<float>::infinity());
  using Item = std::pair<float, int>;
  std::priority_queue<Item, std::vector<Item>, std::greater<Item>> queue;
  if (radius <= 0) {
    return std::vector<float>(seed, seed + n);
  }
  for (int p = 0; p < n; ++p)
    if (seed[p] > 1e-6f) {
      distance[p] = -radius * std::log(std::max(1e-6f, seed[p]));
      queue.push({distance[p], p});
    }
  const float limit = 3 * radius;
  while (!queue.empty()) {
    auto [d, p] = queue.top();
    queue.pop();
    if (d != distance[p] || d > limit)
      continue;
    for (int ei = g.rowOffsets[p]; ei < g.rowOffsets[p + 1]; ++ei) {
      const auto &edge = g.edges[ei];
      float nd = d + graphEdgeCost(edge, structureRespect);
      if (nd < distance[edge.target] && nd <= limit) {
        distance[edge.target] = nd;
        queue.push({nd, edge.target});
      }
    }
  }
  std::vector<float> out(n);
  for (int p = 0; p < n; ++p)
    out[p] = std::isfinite(distance[p]) ? std::exp(-distance[p] / radius) : 0;
  return out;
}

Sample sampleSamples(const std::vector<Sample> &values, const AnalysisImage &i,
                     float x, float y) {
  x = std::max(0.0f, std::min(float(i.width - 1), x));
  y = std::max(0.0f, std::min(float(i.height - 1), y));
  int x0 = int(std::floor(x)), y0 = int(std::floor(y)),
      x1 = std::min(i.width - 1, x0 + 1), y1 = std::min(i.height - 1, y0 + 1);
  float fx = x - x0, fy = y - y0;
  auto p00 = values[i.index(x0, y0)], p10 = values[i.index(x1, y0)],
       p01 = values[i.index(x0, y1)], p11 = values[i.index(x1, y1)];
  return add(add(mul(p00, (1 - fx) * (1 - fy)), mul(p10, fx * (1 - fy))),
             add(mul(p01, (1 - fx) * fy), mul(p11, fx * fy)));
}

float sampleField(const std::vector<float> &field, const AnalysisImage &i,
                  float x, float y) {
  x = std::max(0.0f, std::min(float(i.width - 1), x));
  y = std::max(0.0f, std::min(float(i.height - 1), y));
  int x0 = int(x), y0 = int(y), x1 = std::min(i.width - 1, x0 + 1),
      y1 = std::min(i.height - 1, y0 + 1);
  float fx = x - x0, fy = y - y0;
  return field[i.index(x0, y0)] * (1 - fx) * (1 - fy) +
         field[i.index(x1, y0)] * fx * (1 - fy) +
         field[i.index(x0, y1)] * (1 - fx) * fy +
         field[i.index(x1, y1)] * fx * fy;
}

float sampleField(const float *field, const AnalysisImage &i, float x,
                  float y) {
  x = std::max(0.0f, std::min(float(i.width - 1), x));
  y = std::max(0.0f, std::min(float(i.height - 1), y));
  int x0 = int(x), y0 = int(y), x1 = std::min(i.width - 1, x0 + 1),
      y1 = std::min(i.height - 1, y0 + 1);
  float fx = x - x0, fy = y - y0;
  return field[i.index(x0, y0)] * (1 - fx) * (1 - fy) +
         field[i.index(x1, y0)] * fx * (1 - fy) +
         field[i.index(x0, y1)] * (1 - fx) * fy +
         field[i.index(x1, y1)] * fx * fy;
}

} // namespace

Phase4AutomaticResult buildPhase4AutomaticPlates(
    ConstYabPlanes source, const Phase4Params &inputParams,
    const ImageGeometry &geometry, const ExecutionContext &execution) {
  Phase4Params params = inputParams;
  params.latentCount =
      std::max(12, std::min(kPhase4LatentCapacity, params.latentCount));
  params.plateCount =
      std::max(4, std::min(kPhase4PlateCapacity, params.plateCount));
  Phase4AutomaticResult result(source.y.bounds, params.latentCount,
                               params.plateCount);
  // Frozen A1/A2 checkpoint: use the raw deterministic analysis image for the
  // matting eigenspace.  The analysis graph still stores distinct signed
  // W_CMF and non-negative F semantics for downstream support and spill.
  AnalysisImage analysis = downsample(source);
  result.analysisGraph = buildGraph(analysis, params, geometry);
  const int n = result.analysisGraph.nodeCount();
  std::vector<YabPixel> spectralInput;
  spectralInput.reserve(analysis.pixels.size());
  for (const auto &value : analysis.pixels)
    spectralInput.push_back({value.y, value.a, value.b});
  auto exactBasis = buildSpectralMattingBasis(
      spectralInput, analysis.width, analysis.height,
      std::min(32, params.latentCount + 8), execution, nullptr, 0.0f);
  if (exactBasis.count < std::min(32, params.latentCount + 8))
    throw std::runtime_error("Phase 4 spectral eigensolver did not converge");
  SpectralBasis basis;
  basis.count = exactBasis.count;
  basis.values = exactBasis.values;
  basis.eigenvalues = exactBasis.eigenvalues;
  basis.eigenResidual = exactBasis.residuals;
  result.diagnostics.maximumEigenmodeCorrelation =
      exactBasis.maximumAbsoluteCorrelation;
  for (float v : basis.eigenResidual) {
    result.diagnostics.maximumEigenResidual =
        std::max(result.diagnostics.maximumEigenResidual, v);
    result.diagnostics.meanEigenResidual += v;
    result.diagnostics.eigenspaceFinite &= std::isfinite(v);
  }
  result.diagnostics.meanEigenResidual /=
      std::max(1, int(basis.eigenResidual.size()));
  auto recovery =
      recoverComponents(basis, result.analysisGraph, params.latentCount,
                        params.plateOverlap, result.diagnostics);
  params.latentCount = recovery.count;
  auto &alpha = recovery.values;
  result.latent = LatentComponentSet(source.y.bounds, params.latentCount);
  result.latent.allocateSpectralModes(exactBasis.count);
  std::vector<Sample> latentMean(params.latentCount);
  std::vector<float> latentMass(params.latentCount),
      latentVariance(params.latentCount);
  for (int c = 0; c < params.latentCount; ++c)
    for (int p = 0; p < n; ++p) {
      float w = alpha[size_t(c) * n + p];
      latentMean[c] = add(latentMean[c], mul(analysis.pixels[p], w));
      latentMass[c] += w;
    }
  for (int c = 0; c < params.latentCount; ++c)
    latentMean[c] = mul(latentMean[c], 1 / std::max(kEpsilon, latentMass[c]));
  for (int c = 0; c < params.latentCount; ++c)
    for (int p = 0; p < n; ++p)
      latentVariance[c] +=
          alpha[size_t(c) * n + p] *
          distance2(analysis.pixels[p], latentMean[c], {1, 1, 1});
  for (int c = 0; c < params.latentCount; ++c)
    latentVariance[c] =
        std::max(1e-5f, latentVariance[c] / std::max(kEpsilon, latentMass[c]));
  auto appearance = buildSpatialAppearances(
      analysis, result.analysisGraph, alpha, latentMean,
      params.latentCount, result.diagnostics, result.latent.distributions(), execution);
  auto &assign = result.plates.latentAssignments();
  assign =
      groupLatents(analysis, alpha, latentMean, latentVariance, basis, params);
  std::vector<std::vector<float>> plateAlpha(params.plateCount,
                                             std::vector<float>(n));
  std::vector<std::vector<Sample>> plateAppearance(params.plateCount,
                                                   std::vector<Sample>(n));
  for (int p = 0; p < n; ++p)
    for (int i = 0; i < params.plateCount; ++i) {
      float pa = 0;
      Sample value{};
      for (int c = 0; c < params.latentCount; ++c) {
        float w = alpha[size_t(c) * n + p] *
                  assign[size_t(c) * params.plateCount + i];
        pa += w;
        value = add(value, mul(appearance[c][p], w));
      }
      plateAlpha[i][p] = pa;
      // Appearance has no reliable conditional estimate when a plate is
      // numerically absent. Keep a finite contextual source value there;
      // alpha stays unchanged and the correction is bounded by epsilon.
      // Dividing a tiny contribution by a floored denominator instead made
      // absent plates approach black and exposed holes under Weight edits.
      plateAppearance[i][p] = pa > kEpsilon
                                  ? mul(value, 1 / pa)
                                  : analysis.pixels[p];
    }

  Eigen::MatrixXd publicGram =
      Eigen::MatrixXd::Zero(params.plateCount, params.plateCount);
  double publicError = 0.0;
  for (int p = 0; p < n; ++p) {
    Sample reconstruction{};
    for (int i = 0; i < params.plateCount; ++i) {
      reconstruction =
          add(reconstruction, mul(plateAppearance[i][p], plateAlpha[i][p]));
      for (int j = 0; j < params.plateCount; ++j)
        publicGram(i, j) += plateAlpha[i][p] * plateAlpha[j][p];
    }
    publicError += distance2(reconstruction, analysis.pixels[p], {1, 1, 1});
  }
  result.diagnostics.publicReconstructionError =
      std::sqrt(float(publicError / std::max(1, n)));
  Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> publicRank(publicGram);
  double publicEigenSum = publicRank.eigenvalues().sum(), publicEntropy = 0.0;
  for (double eigenvalue : publicRank.eigenvalues())
    if (eigenvalue > 1.0e-15 && publicEigenSum > 0.0) {
      const double probability = eigenvalue / publicEigenSum;
      publicEntropy -= probability * std::log(probability);
    }
  result.diagnostics.publicPlateEffectiveRank = float(std::exp(publicEntropy));
  for (int i = 0; i < params.plateCount; ++i)
    for (int j = i + 1; j < params.plateCount; ++j) {
      double denominator =
          std::sqrt(std::max(1.0e-20, publicGram(i, i) * publicGram(j, j)));
      result.diagnostics.maximumPublicPlateCorrelation =
          std::max(result.diagnostics.maximumPublicPlateCorrelation,
                   float(publicGram(i, j) / denominator));
    }
  std::vector<std::vector<float>> supportY(params.plateCount),
      supportAB(params.plateCount);
  float ry = phase4SupportRadiusY(params), rab = phase4SupportRadiusAB(params);
  const float sourcePerAnalysis =
      std::max(float(source.y.bounds.width()) / analysis.width,
               float(source.y.bounds.height()) / analysis.height);
  for (int i = 0; i < params.plateCount; ++i) {
    supportY[i] =
        maxProductSupport(result.analysisGraph, plateAlpha[i].data(),
                          ry / sourcePerAnalysis, params.structureRespect);
    supportAB[i] =
        maxProductSupport(result.analysisGraph, plateAlpha[i].data(),
                          rab / sourcePerAnalysis, params.structureRespect);
  }
  std::vector<float> spectralScale(exactBasis.count, 1.0f);
  for (int mode = 0; mode < exactBasis.count; ++mode) {
    float maximum = 0;
    for (int p = 0; p < n; ++p)
      maximum =
          std::max(maximum, std::abs(exactBasis.values[size_t(mode) * n + p]));
    spectralScale[mode] = std::max(1e-8f, maximum);
  }
  RectI b = source.y.bounds;
  for (int y = b.y1; y < b.y2; ++y)
    for (int x = b.x1; x < b.x2; ++x) {
      float ax = (float(x - b.x1) + .5f) * analysis.width / b.width() - .5f,
            ay = (float(y - b.y1) + .5f) * analysis.height / b.height() - .5f;
      Sample original{source.y.at(x, y), source.a.at(x, y), source.b.at(x, y)};
      Sample latentReconstruction{};
      float entropy = 0;
      for (int mode = 0; mode < exactBasis.count; ++mode)
        result.latent.spectralMode(mode).at(x, y) =
            sampleField(exactBasis.values.data() + size_t(mode) * n, analysis,
                        ax, ay) /
            spectralScale[mode];
      for (int c = 0; c < params.latentCount; ++c) {
        float av = std::max(
            0.0f, sampleField(alpha.data() + size_t(c) * n, analysis, ax, ay));
        result.latent.alpha(c).at(x, y) = av;
        Sample app = sampleSamples(appearance[c], analysis, ax, ay);
        auto out = result.latent.appearance(c);
        out.y.at(x, y) = app.y;
        out.a.at(x, y) = app.a;
        out.b.at(x, y) = app.b;
        latentReconstruction = add(latentReconstruction, mul(app, av));
        if (av > 1e-8f)
          entropy -= av * std::log(av);
      }
      float error =
          std::sqrt(distance2(latentReconstruction, original, {1, 1, 1}));
      result.latent.reconstructionError().at(x, y) = error;
      result.latent.confidence().at(x, y) = clamp01(1 - error) / (1 + entropy);
      result.latent.spectralResidual().at(x, y) =
          result.diagnostics.maximumEigenResidual;
      result.latent.recoveryError().at(x, y) = 0;
      for (int i = 0; i < params.plateCount; ++i) {
        float pa = sampleField(plateAlpha[i], analysis, ax, ay);
        result.plates.alpha(i).at(x, y) = pa;
        result.plates.supportY(i).at(x, y) =
            sampleField(supportY[i], analysis, ax, ay);
        result.plates.supportAB(i).at(x, y) =
            sampleField(supportAB[i], analysis, ax, ay);
        Sample app = sampleSamples(plateAppearance[i], analysis, ax, ay);
        auto po = result.plates.appearance(i);
        po.y.at(x, y) = app.y;
        po.a.at(x, y) = app.a;
        po.b.at(x, y) = app.b;
      }
    }
  // Lift colors under the original full-resolution source constraint.
  // Independent interpolation of alpha and color does not preserve their
  // product and formerly returned an analysis-resolution photograph.
  // This is fixed-alpha color refinement, never occupancy/confidence fallback.
  double fullError = 0;
  const auto &grid=result.latent.distributions();
  std::vector<Eigen::Matrix3d> covariances{size_t(params.latentCount)};
  for(int y=b.y1;y<b.y2;++y) for(int x=b.x1;x<b.x2;++x) {
    if(x==b.x1 && execution.cancelled())
      throw std::runtime_error("Phase 4 full-resolution appearance cancelled");
    double ax=std::clamp((double(x-b.x1)+.5)*analysis.width/b.width()-.5,
                         0.0,double(analysis.width-1));
    double ay=std::clamp((double(y-b.y1)+.5)*analysis.height/b.height()-.5,
                         0.0,double(analysis.height-1));
    int cx=int(ax)/grid.spacing,cy=int(ay)/grid.spacing;
    double tx=(ax-cx*grid.spacing)/grid.spacing,ty=(ay-cy*grid.spacing)/grid.spacing;
    Eigen::Vector3d mixture=Eigen::Vector3d::Zero();
    Eigen::Matrix3d combined=Eigen::Matrix3d::Zero();
    for(int c=0;c<params.latentCount;++c) {
      auto v=result.latent.appearance(c);
      double a=result.latent.alpha(c).at(x,y);
      mixture+=a*Eigen::Vector3d(v.y.at(x,y),v.a.at(x,y),v.b.at(x,y));
      auto &cov=covariances[size_t(c)];cov.setZero();
      for(int oy=0;oy<2;++oy) for(int ox=0;ox<2;++ox) {
        double w=(ox?tx:1-tx)*(oy?ty:1-ty);
        const auto &d=grid.components[size_t(c)][size_t(cy+oy)*grid.width+cx+ox];
        for(int r=0;r<3;++r) for(int k=0;k<3;++k)
          cov(r,k)+=w*d.covariance[size_t(r)*3+k];
      }
      combined+=a*cov;
    }
    Eigen::Vector3d original(source.y.at(x,y),source.a.at(x,y),source.b.at(x,y));
    Eigen::Vector3d correction=combined.ldlt().solve(original-mixture);
    for(int c=0;c<params.latentCount;++c) {
      auto v=result.latent.appearance(c);Eigen::Vector3d d=covariances[size_t(c)]*correction;
      v.y.at(x,y)+=float(d[0]);v.a.at(x,y)+=float(d[1]);v.b.at(x,y)+=float(d[2]);
    }
    Eigen::Vector3d check=Eigen::Vector3d::Zero();
    for(int i=0;i<params.plateCount;++i) {
      Eigen::Vector3d value=Eigen::Vector3d::Zero();double pa=0;
      for(int c=0;c<params.latentCount;++c) {
        double w=result.latent.alpha(c).at(x,y)*assign[size_t(c)*params.plateCount+i];
        auto v=result.latent.appearance(c);pa+=w;
        value+=w*Eigen::Vector3d(v.y.at(x,y),v.a.at(x,y),v.b.at(x,y));
      }
      auto out=result.plates.appearance(i);
      Eigen::Vector3d v=pa>kEpsilon?Eigen::Vector3d(value/pa):original;
      out.y.at(x,y)=float(v[0]);out.a.at(x,y)=float(v[1]);out.b.at(x,y)=float(v[2]);
      check+=result.plates.alpha(i).at(x,y)*v;
    }
    double error=(check-original).squaredNorm();fullError+=error;
    result.latent.reconstructionError().at(x,y)=float(std::sqrt(error));
  }
  result.diagnostics.fullResolutionReconstructionError=
      float(std::sqrt(fullError/std::max(1,b.width()*b.height())));
  result.diagnostics.componentsFinite =
      std::isfinite(result.diagnostics.meanEffectiveComponents);
  result.diagnostics.appearanceFinite =
      std::isfinite(result.diagnostics.appearanceUnmixingError);
  return result;
}

} // namespace pigment
