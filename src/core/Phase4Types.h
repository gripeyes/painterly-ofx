#pragma once

#include "core/Types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace pigment {

constexpr int kPhase4LatentCapacity = 24;
constexpr int kPhase4PlateCapacity = 8;
// Research selectors only; C2 remains a standalone preserved baseline.
enum class Phase4Representation { Poisson = 0, A3Passthrough, RegionalEigen, SparseCurve };

enum class Phase4DebugView {
  Final = 0,
  LatentComponent,
  LatentComposite,
  LatentReconstructionError,
  SpectralResidual,
  ComponentRecoveryError,
  AppearanceUnmixingError,
  PlateAlpha,
  PlateYSupport,
  PlateABSupport,
  PlateYAppearance,
  PlateABAppearance,
  PlateOverlapComposite
};

struct Phase4PlateControl {
  bool enabled = true;
  float weight = 1.0f;
  float tone = 0.0f;
  float biasA = 0.0f;
  float biasB = 0.0f;
  float spillOut = 1.0f;
  float receiveSpill = 1.0f;
};

struct Phase4Params {
  Phase4Representation representation = Phase4Representation::Poisson;
  // Participation-amplitude overrides, not blur/extent radii. The existing
  // Plate Scale/Overlap/Chroma Support Ratio determine intrinsic graph extent.
  float ySupport = 1.0f, abSupport = 1.0f;
  int latentCount = 16;
  int plateCount = 6;
  float plateScale = 48.0f;
  float plateOverlap = 0.55f;
  float chromaSupportRatio = 2.0f;
  float boundaryLock = 0.75f;
  float lumaChromaCoupling = 0.35f;
  float lumaChunkScale = 24.0f;
  float chromaChunkScale = 64.0f;
  float mergeSelectivity = 0.6f;
  float internalVariation = 0.45f;
  float gradientComplexity = 0.35f;
  float spillAmount = 0.25f;
  float spillReach = 48.0f;
  float spillAsymmetry = 0.5f;
  float chromaSpill = 0.75f;
  float lumaSpill = 0.15f;
  float structureRespect = 0.8f;
  int debugLatent = 0;
  int debugPlate = 0;
  Phase4DebugView debugView = Phase4DebugView::Final;
  std::array<Phase4PlateControl, kPhase4PlateCapacity> plates{};
};

struct SparseAffinityEdge {
  int target = 0;
  // Signed affine reconstruction coefficient used only by W_CMF.
  float signedMixtureWeight = 0.0f;
  // Nonnegative transport capacity F in [0,1].
  float weight = 0.0f;
  float physicalDistance = 1.0f;
  float boundary = 0.0f;
};

struct SparseAffinityGraph {
  int width = 0, height = 0;
  std::vector<int> rowOffsets;
  std::vector<SparseAffinityEdge> edges;
  int nodeCount() const noexcept { return width * height; }
};

struct Phase4GateDiagnostics {
  int requestedLatentCount = 0;
  int activeLatentCount = 0;
  float maximumEigenResidual = 0.0f;
  float meanEigenResidual = 0.0f;
  float maximumEigenmodeCorrelation = 0.0f;
  float componentProjectionError = 0.0f;
  float appearanceUnmixingError = 0.0f;
  float meanEffectiveComponents = 0.0f;
  float meanComponentEntropy = 0.0f;
  float componentEffectiveRank = 0.0f;
  float hardPixelFraction = 0.0f;
  float appearanceSpatialVariation = 0.0f;
  float fullResolutionReconstructionError = 0.0f;
  float publicReconstructionError = 0.0f;
  float publicPlateEffectiveRank = 0.0f;
  float maximumPublicPlateCorrelation = 0.0f;
  bool eigenspaceFinite = true;
  bool componentsFinite = true;
  bool appearanceFinite = true;
  std::vector<float> componentOccupancy;
  std::vector<float> componentCorrelation;
  std::vector<float> componentMattingEnergy;
};

struct Phase4AppearanceDistribution {
  std::array<double, 3> mean{};
  std::array<double, 9> covariance{}; // row-major YAB covariance
};

struct Phase4AppearanceDistributionGrid {
  int width = 0, height = 0, spacing = 32;
  std::vector<std::vector<Phase4AppearanceDistribution>> components;
};

class LatentComponentSet {
public:
  LatentComponentSet() = default;
  LatentComponentSet(RectI bounds, int count);
  RectI bounds() const noexcept { return bounds_; }
  int count() const noexcept { return count_; }
  Phase4AppearanceDistributionGrid &distributions() noexcept { return distributions_; }
  const Phase4AppearanceDistributionGrid &distributions() const noexcept { return distributions_; }
  FloatPlaneView alpha(int index) noexcept { return alpha_[index].view(); }
  ConstFloatPlaneView alpha(int index) const noexcept {
    return alpha_[index].view();
  }
  YabPlanes appearance(int index) noexcept { return appearance_[index].view(); }
  ConstYabPlanes appearance(int index) const noexcept {
    return appearance_[index].view();
  }
  FloatPlaneView confidence() noexcept { return confidence_.view(); }
  ConstFloatPlaneView confidence() const noexcept { return confidence_.view(); }
  FloatPlaneView reconstructionError() noexcept {
    return reconstructionError_.view();
  }
  ConstFloatPlaneView reconstructionError() const noexcept {
    return reconstructionError_.view();
  }
  FloatPlaneView spectralResidual() noexcept {
    return spectralResidual_.view();
  }
  ConstFloatPlaneView spectralResidual() const noexcept {
    return spectralResidual_.view();
  }
  FloatPlaneView recoveryError() noexcept { return recoveryError_.view(); }
  ConstFloatPlaneView recoveryError() const noexcept {
    return recoveryError_.view();
  }
  void allocateSpectralModes(int count);
  int spectralModeCount() const noexcept {
    return static_cast<int>(spectralModes_.size());
  }
  FloatPlaneView spectralMode(int index) noexcept {
    return spectralModes_[index].view();
  }
  ConstFloatPlaneView spectralMode(int index) const noexcept {
    return spectralModes_[index].view();
  }

private:
  RectI bounds_{};
  int count_ = 0;
  Phase4AppearanceDistributionGrid distributions_;
  std::vector<OwnedPlane> alpha_;
  std::vector<OwnedYabPlanes> appearance_;
  std::vector<OwnedPlane> spectralModes_;
  OwnedPlane confidence_, reconstructionError_, spectralResidual_,
      recoveryError_;
};

class PublicPlateSet {
public:
  PublicPlateSet() = default;
  PublicPlateSet(RectI bounds, int count);
  RectI bounds() const noexcept { return bounds_; }
  int count() const noexcept { return count_; }
  FloatPlaneView alpha(int index) noexcept { return alpha_[index].view(); }
  ConstFloatPlaneView alpha(int index) const noexcept {
    return alpha_[index].view();
  }
  FloatPlaneView supportY(int index) noexcept {
    return supportY_[index].view();
  }
  ConstFloatPlaneView supportY(int index) const noexcept {
    return supportY_[index].view();
  }
  FloatPlaneView supportAB(int index) noexcept {
    return supportAB_[index].view();
  }
  ConstFloatPlaneView supportAB(int index) const noexcept {
    return supportAB_[index].view();
  }
  YabPlanes appearance(int index) noexcept { return appearance_[index].view(); }
  ConstYabPlanes appearance(int index) const noexcept {
    return appearance_[index].view();
  }
  std::vector<float> &latentAssignments() noexcept {
    return latentAssignments_;
  }
  const std::vector<float> &latentAssignments() const noexcept {
    return latentAssignments_;
  }

private:
  RectI bounds_{};
  int count_ = 0;
  std::vector<OwnedPlane> alpha_, supportY_, supportAB_;
  std::vector<OwnedYabPlanes> appearance_;
  std::vector<float> latentAssignments_;
};

float phase4PlateEntropyCoefficient(float overlap) noexcept;
float phase4SupportRadiusY(const Phase4Params &params) noexcept;
float phase4SupportRadiusAB(const Phase4Params &params) noexcept;

} // namespace pigment
