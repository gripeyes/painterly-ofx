#pragma once

#include "core/DetailCollapseResearch.h"
#include "core/IntegratedPigment.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace pigment::metal {

struct MetalImageView {
  void* storage = nullptr;
  std::size_t storageBytes = 0;
  std::ptrdiff_t rowBytes = 0;
  RectI bounds{};
  int components = 4;
};

enum class MetalPath { None, NativeHostBuffers, CpuNoCopy, CpuStaging };
enum class MetalFailure { None, Unsupported, InvalidLayout, Initialization,
                          Allocation, Encoding, Execution };

struct MetalDiagnostics {
  MetalPath path = MetalPath::None;
  MetalFailure failure = MetalFailure::None;
  double wrapOrUploadMs = 0.0;
  double rgbToYabEncodeMs = 0.0;
  double strengthEncodeMs = 0.0;
  double structureEncodeMs = 0.0;
  double boundaryEncodeMs = 0.0;
  double iterationsEncodeMs = 0.0;
  double reconstructionEncodeMs = 0.0;
  double yabToRgbEncodeMs = 0.0;
  double commandEncodingMs = 0.0;
  double gpuMs = 0.0;
  double readbackMs = 0.0;
  double totalMs = 0.0;
  std::uint64_t uploadedBytes = 0;
  std::uint64_t downloadedBytes = 0;
  std::uint64_t scratchBytes = 0;
  std::uint64_t deviceAllocatedBytes = 0;
  std::uint32_t scratchAllocations = 0;
  bool sourceNoCopy = false;
  bool destinationNoCopy = false;
  std::string deviceName;
  std::string message;
};

struct MetalExecutionRequest {
  MetalImageView source;
  MetalImageView destination;
  MetalImageView mask;
  bool hasMask = false;
  bool nativeHostBuffers = false;
  void* hostCommandQueue = nullptr;
  RectI renderWindow{};
  DetailCollapseResearchParams params{};
  ImageGeometry geometry{};
};

struct IntegratedMetalExecutionRequest {
  MetalImageView source;
  MetalImageView destination;
  MetalImageView mask;
  bool hasMask = false;
  bool nativeHostBuffers = false;
  void* hostCommandQueue = nullptr;
  RectI renderWindow{};
  IntegratedPigmentParams params{};
  ImageGeometry geometry{};
};

class MetalInstance {
 public:
  MetalInstance();
  ~MetalInstance();
  MetalInstance(const MetalInstance&) = delete;
  MetalInstance& operator=(const MetalInstance&) = delete;

  bool render(const MetalExecutionRequest& request);
  bool renderIntegrated(const IntegratedMetalExecutionRequest& request);
  const MetalDiagnostics& diagnostics() const noexcept;
  void releaseTransientResources();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace pigment::metal
