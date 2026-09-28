#include "metal/PigmentMetal.h"

#include "core/ColorSpace.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <functional>
#include <map>
#include <mutex>
#include <sstream>
#include <vector>

namespace pigment::metal {
namespace {

using Clock = std::chrono::steady_clock;

double milliseconds(Clock::time_point a, Clock::time_point b) {
  return std::chrono::duration<double, std::milli>(b - a).count();
}

struct GpuImageLayout {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint32_t rowFloats = 0;
  std::uint32_t components = 0;
  std::uint32_t startFloat = 0;
};

struct GpuParams {
  std::uint32_t width, height, sourceComponents, destinationComponents;
  std::uint32_t hasMask, maskComponents, premultiplied, invertMask;
  std::uint32_t rangeEnabled, debugView;
  float amount, massStrength, toneSimilarity, chromaSimilarity;
  float boundaryPreserve, boundarySoftness, structurePreserve;
  float internalVariation, lumaMassing, chromaMassing, mix;
  float shadowBias, midtoneBias, highlightBias;
  float rangeMinimum, rangeMaximum, rangeSoftness;
  float whiteX, whiteZ;
  float rgbToXyz[9];
  float xyzToRgb[9];
};

struct GpuIntegratedParams {
  std::uint32_t width, height, sourceComponents, destinationComponents;
  std::uint32_t hasMask, maskComponents, premultiplied, invertMask;
  std::uint32_t comparisonMode, debugView, veilSeed, massEstimator;
  float amount, massScale, massStrength, toneSimilarity, chromaSimilarity;
  float lumaAttraction, chromaAttraction;
  float structurePreserve, boundaryPreserve, boundaryExtinction, boundarySoftness;
  float veilAmount, veilScale, veilIrregularity, veilContrast;
  float detailCleanup, fineDetail, mediumDetail, internalVariation;
  float chromaMigration, chromaScale, chromaEdgeRespect;
  float regionSoftness, modeSelectivity, boundaryScale, veilTonalBias, chromaLumaCoupling, mix;
  float renderScaleX, renderScaleY, pixelAspect, originX, originY;
  float whiteX, whiteZ;
  float rgbToXyz[9];
  float xyzToRgb[9];
};

struct GpuRegionLevelParams {
  std::uint32_t width = 0, height = 0;
  float levelScale = 1.0f, radiusX = 1.0f, radiusY = 1.0f, quarterBlend = 0.0f;
};

NSString* metallibPath() {
  if (const char* overridePath = std::getenv("PIGMENT_METAL_RESOURCE_DIR")) {
    return [[NSString stringWithUTF8String:overridePath]
        stringByAppendingPathComponent:@"Pigment.metallib"];
  }
  Dl_info info{};
  if (dladdr(reinterpret_cast<const void*>(&metallibPath), &info) == 0 ||
      !info.dli_fname)
    return nil;
  NSString* binary = [NSString stringWithUTF8String:info.dli_fname];
  NSString* contents = [[[binary stringByDeletingLastPathComponent]
      stringByDeletingLastPathComponent] stringByStandardizingPath];
  return [contents stringByAppendingPathComponent:@"Resources/Pigment.metallib"];
}

struct DeviceResources {
  id<MTLDevice> device = nil;
  id<MTLLibrary> library = nil;
  std::map<std::string, id<MTLComputePipelineState>> pipelines;
  std::string error;

  explicit DeviceResources(id<MTLDevice> d) : device(d) {
    NSError* nsError = nil;
    NSString* path = metallibPath();
    if (!path) {
      error = "Could not resolve Pigment bundle resource directory";
      return;
    }
    library = [device newLibraryWithURL:[NSURL fileURLWithPath:path] error:&nsError];
    if (!library) {
      error = "Could not load Pigment.metallib at " +
              std::string(path.UTF8String ? path.UTF8String : "") + ": " +
              std::string(nsError.localizedDescription.UTF8String
                              ? nsError.localizedDescription.UTF8String : "unknown error");
      return;
    }
    for (const char* name : {"pigment_rgb_to_yab", "pigment_processing_strength",
                             "pigment_structure_combine", "pigment_boundary",
                             "pigment_prepare_guidance", "pigment_guided_statistics",
                             "pigment_guided_solve", "pigment_guided_reconstruct", "pigment_update",
                             "pigment_copy_texture", "pigment_final",
                             "pigment_integrated_rgb_to_yab", "pigment_integrated_structure",
                             "pigment_integrated_fields", "pigment_integrated_downsample",
                             "pigment_integrated_region_iteration",
                             "pigment_integrated_local_density",
                             "pigment_integrated_representative_iteration",
                             "pigment_integrated_reconstruct_mass",
                             "pigment_integrated_boundary_extinction",
                             "pigment_integrated_chroma", "pigment_integrated_reintegrate",
                             "pigment_integrated_final"}) {
      id<MTLFunction> function = [library newFunctionWithName:
          [NSString stringWithUTF8String:name]];
      if (!function) {
        error = std::string("Missing Metal function: ") + name;
        pipelines.clear();
        return;
      }
      id<MTLComputePipelineState> pipeline =
          [device newComputePipelineStateWithFunction:function error:&nsError];
      if (!pipeline) {
        error = std::string("Could not create Metal pipeline ") + name + ": " +
                std::string(nsError.localizedDescription.UTF8String
                                ? nsError.localizedDescription.UTF8String : "unknown error");
        pipelines.clear();
        return;
      }
      pipelines.emplace(name, pipeline);
    }
  }

  bool valid() const { return library != nil && pipelines.size() == 23; }
};

std::mutex gRegistryMutex;
std::map<std::uint64_t, std::weak_ptr<DeviceResources>> gRegistry;

std::shared_ptr<DeviceResources> resourcesFor(id<MTLDevice> device) {
  const std::uint64_t key = device.registryID;
  std::lock_guard<std::mutex> lock(gRegistryMutex);
  if (auto cached = gRegistry[key].lock()) return cached;
  auto created = std::make_shared<DeviceResources>(device);
  gRegistry[key] = created;
  return created;
}

id<MTLTexture> texture(id<MTLDevice> device, MTLPixelFormat format,
                       NSUInteger width, NSUInteger height) {
  MTLTextureDescriptor* descriptor =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                         width:width height:height
                                                      mipmapped:NO];
  descriptor.storageMode = MTLStorageModePrivate;
  descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
  return [device newTextureWithDescriptor:descriptor];
}

void dispatch(id<MTLCommandBuffer> commandBuffer,
              id<MTLComputePipelineState> pipeline, NSUInteger width,
              NSUInteger height,
              const std::function<void(id<MTLComputeCommandEncoder>)>& bind) {
  id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];
  [encoder setComputePipelineState:pipeline];
  bind(encoder);
  const NSUInteger w = pipeline.threadExecutionWidth;
  const NSUInteger h = std::max<NSUInteger>(1, pipeline.maxTotalThreadsPerThreadgroup / w);
  [encoder dispatchThreads:MTLSizeMake(width, height, 1)
      threadsPerThreadgroup:MTLSizeMake(w, h, 1)];
  [encoder endEncoding];
}

bool validateView(const MetalImageView& image, RectI window, bool native,
                  std::string& reason) {
  if (!image.storage || image.components < 1 || image.components > 4 ||
      image.rowBytes <= 0 || image.bounds.empty()) {
    reason = "Null storage or invalid image layout";
    return false;
  }
  if (window.x1 < image.bounds.x1 || window.y1 < image.bounds.y1 ||
      window.x2 > image.bounds.x2 || window.y2 > image.bounds.y2) {
    reason = "Render window lies outside image bounds";
    return false;
  }
  const std::size_t last = static_cast<std::size_t>(window.y2 - image.bounds.y1 - 1) *
          static_cast<std::size_t>(image.rowBytes) +
      static_cast<std::size_t>(window.x2 - image.bounds.x1) * image.components * sizeof(float);
  if (!native && image.storageBytes != 0 && last > image.storageBytes) {
    reason = "CPU image layout exceeds supplied storage";
    return false;
  }
  return true;
}

struct BufferBinding {
  id<MTLBuffer> buffer = nil;
  GpuImageLayout layout{};
  bool noCopy = false;
  bool staged = false;
};

BufferBinding nativeBinding(const MetalImageView& image, RectI window,
                            std::string& reason) {
  BufferBinding result;
  result.buffer = (__bridge id<MTLBuffer>)image.storage;
  if (!result.buffer) {
    reason = "Host native image handle is not a Metal buffer";
    return {};
  }
  const std::size_t start = static_cast<std::size_t>(window.y1 - image.bounds.y1) *
      image.rowBytes + static_cast<std::size_t>(window.x1 - image.bounds.x1) *
      image.components * sizeof(float);
  const std::size_t required = start +
      static_cast<std::size_t>(window.height() - 1) * image.rowBytes +
      static_cast<std::size_t>(window.width()) * image.components * sizeof(float);
  if (required > result.buffer.length || image.rowBytes % sizeof(float) != 0) {
    reason = "Native Metal buffer is shorter than its declared image layout";
    return {};
  }
  result.layout = {static_cast<std::uint32_t>(window.width()),
                   static_cast<std::uint32_t>(window.height()),
                   static_cast<std::uint32_t>(image.rowBytes / sizeof(float)),
                   static_cast<std::uint32_t>(image.components),
                   static_cast<std::uint32_t>(start / sizeof(float))};
  result.noCopy = true;
  return result;
}

BufferBinding cpuInputBinding(id<MTLDevice> device, const MetalImageView& image,
                              RectI window, MetalDiagnostics& diagnostics) {
  BufferBinding result;
  const std::size_t packedRow = static_cast<std::size_t>(window.width()) *
      image.components * sizeof(float);
  const bool contiguous = image.rowBytes == static_cast<std::ptrdiff_t>(packedRow) &&
      window.x1 == image.bounds.x1 && window.y1 == image.bounds.y1;
  const std::size_t packedBytes = packedRow * static_cast<std::size_t>(window.height());
  if (contiguous) {
    result.buffer = [device newBufferWithBytesNoCopy:image.storage
                                              length:packedBytes
                                             options:MTLResourceStorageModeShared
                                         deallocator:nil];
    if (result.buffer) result.noCopy = true;
  }
  if (!result.buffer) {
    result.buffer = [device newBufferWithLength:packedBytes
                                         options:MTLResourceStorageModeShared];
    if (!result.buffer) return {};
    auto* destination = static_cast<std::byte*>(result.buffer.contents);
    auto* source = static_cast<const std::byte*>(image.storage);
    for (int row = 0; row < window.height(); ++row) {
      const std::ptrdiff_t sourceOffset =
          static_cast<std::ptrdiff_t>(window.y1 - image.bounds.y1 + row) * image.rowBytes +
          static_cast<std::ptrdiff_t>(window.x1 - image.bounds.x1) *
              image.components * sizeof(float);
      std::memcpy(destination + static_cast<std::size_t>(row) * packedRow,
                  source + sourceOffset, packedRow);
    }
    result.staged = true;
    diagnostics.uploadedBytes += packedBytes;
  }
  result.layout = {static_cast<std::uint32_t>(window.width()),
                   static_cast<std::uint32_t>(window.height()),
                   static_cast<std::uint32_t>(packedRow / sizeof(float)),
                   static_cast<std::uint32_t>(image.components), 0};
  return result;
}

BufferBinding cpuOutputBinding(id<MTLDevice> device, const MetalImageView& image,
                               RectI window) {
  BufferBinding result;
  const std::size_t packedRow = static_cast<std::size_t>(window.width()) *
      image.components * sizeof(float);
  const std::size_t packedBytes = packedRow * static_cast<std::size_t>(window.height());
  const bool contiguous = image.rowBytes == static_cast<std::ptrdiff_t>(packedRow) &&
      window.x1 == image.bounds.x1 && window.y1 == image.bounds.y1;
  if (contiguous) {
    result.buffer = [device newBufferWithBytesNoCopy:image.storage length:packedBytes
                                             options:MTLResourceStorageModeShared
                                         deallocator:nil];
    if (result.buffer) result.noCopy = true;
  }
  if (!result.buffer) {
    result.buffer = [device newBufferWithLength:packedBytes
                                         options:MTLResourceStorageModeShared];
    if (!result.buffer) return {};
    result.staged = true;
  }
  result.layout = {static_cast<std::uint32_t>(window.width()),
                   static_cast<std::uint32_t>(window.height()),
                   static_cast<std::uint32_t>(packedRow / sizeof(float)),
                   static_cast<std::uint32_t>(image.components), 0};
  return result;
}

void unpackOutput(const BufferBinding& binding, const MetalImageView& image,
                  RectI window, MetalDiagnostics& diagnostics) {
  if (!binding.staged) return;
  const std::size_t packedRow = static_cast<std::size_t>(window.width()) *
      image.components * sizeof(float);
  auto* source = static_cast<const std::byte*>(binding.buffer.contents);
  auto* destination = static_cast<std::byte*>(image.storage);
  for (int row = 0; row < window.height(); ++row) {
    const std::ptrdiff_t destinationOffset =
        static_cast<std::ptrdiff_t>(window.y1 - image.bounds.y1 + row) * image.rowBytes +
        static_cast<std::ptrdiff_t>(window.x1 - image.bounds.x1) *
            image.components * sizeof(float);
    std::memcpy(destination + destinationOffset,
                source + static_cast<std::size_t>(row) * packedRow, packedRow);
  }
  diagnostics.downloadedBytes += packedRow * static_cast<std::size_t>(window.height());
}

GpuParams makeParams(const MetalExecutionRequest& request) {
  const auto& p = request.params;
  GpuParams result{};
  result.width = request.renderWindow.width();
  result.height = request.renderWindow.height();
  result.sourceComponents = request.source.components;
  result.destinationComponents = request.destination.components;
  result.hasMask = request.hasMask;
  result.maskComponents = request.hasMask ? request.mask.components : 1;
  result.premultiplied = p.premultiplied;
  result.invertMask = p.invertMask;
  result.rangeEnabled = p.tonalMask.rangeEnabled;
  result.debugView = static_cast<std::uint32_t>(p.debugView);
  result.amount = p.amount;
  result.massStrength = p.massStrength;
  result.toneSimilarity = p.toneSimilarity;
  result.chromaSimilarity = p.chromaSimilarity;
  result.boundaryPreserve = p.boundaryPreserve;
  result.boundarySoftness = p.boundarySoftness;
  result.structurePreserve = p.structurePreserve;
  result.internalVariation = p.internalVariation;
  result.lumaMassing = p.lumaMassing;
  result.chromaMassing = p.chromaMassing;
  result.mix = p.mix;
  result.shadowBias = p.tonalMask.shadowBias;
  result.midtoneBias = p.tonalMask.midtoneBias;
  result.highlightBias = p.tonalMask.highlightBias;
  result.rangeMinimum = p.tonalMask.rangeMinimum;
  result.rangeMaximum = p.tonalMask.rangeMaximum;
  result.rangeSoftness = p.tonalMask.rangeSoftness;
  const auto matrix = opponentMatrixData(p.gamut);
  result.whiteX = matrix.whiteX;
  result.whiteZ = matrix.whiteZ;
  std::copy(matrix.rgbToXyz.begin(), matrix.rgbToXyz.end(), result.rgbToXyz);
  std::copy(matrix.xyzToRgb.begin(), matrix.xyzToRgb.end(), result.xyzToRgb);
  return result;
}

GpuIntegratedParams makeIntegratedParams(const IntegratedMetalExecutionRequest& request) {
  const auto& p = request.params;
  GpuIntegratedParams result{};
  result.width = request.renderWindow.width();
  result.height = request.renderWindow.height();
  result.sourceComponents = request.source.components;
  result.destinationComponents = request.destination.components;
  result.hasMask = request.hasMask;
  result.maskComponents = request.hasMask ? request.mask.components : 1;
  result.premultiplied = p.premultiplied;
  result.invertMask = p.invertMask;
  result.comparisonMode = static_cast<std::uint32_t>(p.comparison);
  result.debugView = static_cast<std::uint32_t>(p.debugView);
  result.veilSeed = static_cast<std::uint32_t>(p.veilSeed);
  result.massEstimator = (p.comparison == PigmentComparisonMode::RepresentativeModePigment ||
      p.debugView == PigmentDebugView::RepresentativeModeResult) ? 1u : 0u;
  if (p.debugView == PigmentDebugView::LegacyWeightedMean) result.massEstimator = 0u;
  result.amount = p.amount; result.massScale = p.massScale;
  result.massStrength = p.massStrength; result.toneSimilarity = p.toneSimilarity;
  result.chromaSimilarity = p.chromaSimilarity; result.lumaAttraction = p.lumaAttraction;
  result.chromaAttraction = p.chromaAttraction; result.structurePreserve = p.structurePreserve;
  result.boundaryPreserve = p.boundaryPreserve; result.boundaryExtinction = p.boundaryExtinction;
  result.boundarySoftness = p.boundarySoftness; result.veilAmount = p.veilAmount;
  result.veilScale = p.veilScale; result.veilIrregularity = p.veilIrregularity;
  result.veilContrast = p.veilContrast; result.detailCleanup = p.detailCleanup;
  result.fineDetail = p.fineDetail; result.mediumDetail = p.mediumDetail;
  result.internalVariation = p.internalVariation; result.chromaMigration = p.chromaMigration;
  result.chromaScale = p.chromaScale; result.chromaEdgeRespect = p.chromaEdgeRespect;
  result.regionSoftness = p.regionSoftness; result.modeSelectivity = p.modeSelectivity;
  result.boundaryScale = p.boundaryScale;
  result.veilTonalBias = p.veilTonalBias; result.chromaLumaCoupling = p.chromaLumaCoupling;
  result.mix = p.mix;
  result.renderScaleX = static_cast<float>(request.geometry.renderScaleX);
  result.renderScaleY = static_cast<float>(request.geometry.renderScaleY);
  result.pixelAspect = static_cast<float>(request.geometry.pixelAspect);
  result.originX = static_cast<float>(request.renderWindow.x1 /
      std::max(request.geometry.renderScaleX, 1.0e-9));
  result.originY = static_cast<float>(request.renderWindow.y1 /
      std::max(request.geometry.renderScaleY, 1.0e-9));
  const auto matrix = opponentMatrixData(p.gamut);
  result.whiteX = matrix.whiteX; result.whiteZ = matrix.whiteZ;
  std::copy(matrix.rgbToXyz.begin(), matrix.rgbToXyz.end(), result.rgbToXyz);
  std::copy(matrix.xyzToRgb.begin(), matrix.xyzToRgb.end(), result.xyzToRgb);
  return result;
}

}  // namespace

struct MetalInstance::Impl {
  mutable std::mutex mutex;
  MetalDiagnostics diagnostics;
  id<MTLCommandQueue> fallbackQueue = nil;
  id<MTLDevice> cachedDevice = nil;
  NSArray<id<MTLTexture>>* cachedTextures = nil;
  NSUInteger cachedWidth = 0;
  NSUInteger cachedHeight = 0;
  NSArray<id<MTLTexture>>* cachedIntegratedTextures = nil;
  NSUInteger cachedIntegratedWidth = 0;
  NSUInteger cachedIntegratedHeight = 0;

  bool render(const MetalExecutionRequest& request) {
    std::lock_guard<std::mutex> lock(mutex);
    diagnostics = {};
    const auto totalStart = Clock::now();
    std::string reason;
    if (request.renderWindow.empty() || request.source.components < 3 ||
        request.destination.components != request.source.components ||
        !validateView(request.source, request.renderWindow,
                      request.nativeHostBuffers, reason) ||
        !validateView(request.destination, request.renderWindow,
                      request.nativeHostBuffers, reason) ||
        (request.hasMask && !validateView(request.mask, request.renderWindow,
                                          request.nativeHostBuffers, reason))) {
      diagnostics.failure = MetalFailure::InvalidLayout;
      diagnostics.message = reason.empty() ? "Unsupported image layout" : reason;
      diagnostics.totalMs = milliseconds(totalStart, Clock::now());
      return false;
    }

    id<MTLCommandQueue> queue = request.nativeHostBuffers
        ? (__bridge id<MTLCommandQueue>)request.hostCommandQueue : fallbackQueue;
    id<MTLDevice> device = queue.device;
    if (!request.nativeHostBuffers && !queue) {
      device = MTLCreateSystemDefaultDevice();
      fallbackQueue = [device newCommandQueue];
      queue = fallbackQueue;
    }
    if (!device || !queue) {
      diagnostics.failure = MetalFailure::Unsupported;
      diagnostics.message = "Metal device or command queue is unavailable";
      return false;
    }
    diagnostics.deviceName = device.name.UTF8String ? device.name.UTF8String
                                                     : "Unknown Metal device";
    diagnostics.deviceAllocatedBytes = device.currentAllocatedSize;
    auto resources = resourcesFor(device);
    if (!resources->valid()) {
      diagnostics.failure = MetalFailure::Initialization;
      diagnostics.message = resources->error;
      return false;
    }

    const auto wrapStart = Clock::now();
    BufferBinding source, destination, mask;
    if (request.nativeHostBuffers) {
      source = nativeBinding(request.source, request.renderWindow, reason);
      destination = nativeBinding(request.destination, request.renderWindow, reason);
      if (request.hasMask) mask = nativeBinding(request.mask, request.renderWindow, reason);
      diagnostics.path = MetalPath::NativeHostBuffers;
    } else {
      source = cpuInputBinding(device, request.source, request.renderWindow, diagnostics);
      destination = cpuOutputBinding(device, request.destination, request.renderWindow);
      if (request.hasMask)
        mask = cpuInputBinding(device, request.mask, request.renderWindow, diagnostics);
      diagnostics.sourceNoCopy = source.noCopy;
      diagnostics.destinationNoCopy = destination.noCopy;
      diagnostics.path = (source.noCopy && destination.noCopy &&
                          (!request.hasMask || mask.noCopy))
          ? MetalPath::CpuNoCopy : MetalPath::CpuStaging;
    }
    if (!source.buffer || !destination.buffer || (request.hasMask && !mask.buffer)) {
      diagnostics.failure = request.nativeHostBuffers ? MetalFailure::InvalidLayout
                                                      : MetalFailure::Allocation;
      diagnostics.message = reason.empty() ? "Could not bind Metal image buffers" : reason;
      return false;
    }
    if (request.nativeHostBuffers &&
        (source.buffer.device != device || destination.buffer.device != device ||
         (request.hasMask && mask.buffer.device != device))) {
      diagnostics.failure = MetalFailure::InvalidLayout;
      diagnostics.message = "Host queue and native image buffers use different Metal devices";
      return false;
    }
    diagnostics.wrapOrUploadMs = milliseconds(wrapStart, Clock::now());

    const NSUInteger width = request.renderWindow.width();
    const NSUInteger height = request.renderWindow.height();
    NSArray<id<MTLTexture>>* textures = nil;
    const bool reused = !request.nativeHostBuffers && cachedTextures && cachedDevice == device &&
                        cachedWidth == width && cachedHeight == height;
    if (reused) {
      textures = cachedTextures;
    } else {
      NSMutableArray<id<MTLTexture>>* allocated = [NSMutableArray arrayWithCapacity:24];
      for (int i = 0; i < 24; ++i) {
        id<MTLTexture> value = texture(device, i < 21 ? MTLPixelFormatRGBA32Float
                                                      : MTLPixelFormatR32Float,
                                       width, height);
        if (!value) {
          diagnostics.failure = MetalFailure::Allocation;
          diagnostics.message = "Could not allocate private Metal scratch textures";
          return false;
        }
        [allocated addObject:value];
      }
      textures = [allocated copy];
      if (!request.nativeHostBuffers) {
        cachedDevice = device;
        cachedWidth = width;
        cachedHeight = height;
        cachedTextures = textures;
      }
    }
    for (id<MTLTexture> value in textures) {
      if (!value) {
        diagnostics.failure = MetalFailure::Allocation;
        diagnostics.message = "Could not allocate private Metal scratch textures";
        return false;
      }
      diagnostics.scratchBytes += value.allocatedSize;
      if (!reused) ++diagnostics.scratchAllocations;
    }
    id<MTLTexture> original = textures[0], blurred = textures[1], structure = textures[2];
    id<MTLTexture> seedA = textures[3], seedB = textures[4], variation = textures[5];
    id<MTLTexture> currentA = textures[6], currentB = textures[7], filtered = textures[8];
    id<MTLTexture> guidance = textures[9], coeffA = textures[10], coeffB = textures[11];
    id<MTLTexture> captured = textures[12], statWp = textures[13], statWg = textures[14];
    id<MTLTexture> statWgg = textures[15], statWgp = textures[16], meanWp = textures[17];
    id<MTLTexture> meanWg = textures[18], meanWgg = textures[19], meanWgp = textures[20];
    id<MTLTexture> strength = textures[21], boundary = textures[22], meanWeight = textures[23];

    id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
    if (!commandBuffer) {
      diagnostics.failure = MetalFailure::Encoding;
      diagnostics.message = "Could not create Metal command buffer";
      return false;
    }
    commandBuffer.label = @"Pigment Guided Mass";
    const GpuParams params = makeParams(request);
    const GpuImageLayout emptyMask{};
    const auto encodeStart = Clock::now();
    auto pipeline = [&](const char* name) { return resources->pipelines.at(name); };

    auto stageStart = Clock::now();
    dispatch(commandBuffer, pipeline("pigment_rgb_to_yab"), width, height,
             [&](id<MTLComputeCommandEncoder> e) {
      [e setBuffer:source.buffer offset:0 atIndex:0];
      [e setBytes:&source.layout length:sizeof(source.layout) atIndex:1];
      [e setBytes:&params length:sizeof(params) atIndex:2];
      [e setTexture:original atIndex:0];
    });
    diagnostics.rgbToYabEncodeMs = milliseconds(stageStart, Clock::now());
    stageStart = Clock::now();
    dispatch(commandBuffer, pipeline("pigment_processing_strength"), width, height,
             [&](id<MTLComputeCommandEncoder> e) {
      [e setTexture:original atIndex:0];
      [e setBuffer:request.hasMask ? mask.buffer : source.buffer offset:0 atIndex:0];
      const auto& layout = request.hasMask ? mask.layout : emptyMask;
      [e setBytes:&layout length:sizeof(layout) atIndex:1];
      [e setBytes:&params length:sizeof(params) atIndex:2];
      [e setTexture:strength atIndex:1];
    });
    diagnostics.strengthEncodeMs = milliseconds(stageStart, Clock::now());

    const float scaleX = static_cast<float>(request.geometry.renderScaleX /
        std::max(request.geometry.pixelAspect, 1e-6));
    const float scaleY = static_cast<float>(request.geometry.renderScaleY);
    auto oddDiameter = [](float radius) {
      const NSUInteger r = static_cast<NSUInteger>(std::max(1.0f, std::ceil(radius)));
      return std::min<NSUInteger>(255, 2 * r + 1);
    };
    const float structureSigma = std::max(0.01f, request.params.structureScale *
        0.5f * (scaleX + scaleY));
    stageStart = Clock::now();
    MPSImageGaussianBlur* structureBlur = [[MPSImageGaussianBlur alloc]
        initWithDevice:device sigma:structureSigma];
    structureBlur.edgeMode = MPSImageEdgeModeClamp;
    [structureBlur encodeToCommandBuffer:commandBuffer sourceTexture:original
                        destinationTexture:blurred];
    dispatch(commandBuffer, pipeline("pigment_structure_combine"), width, height,
             [&](id<MTLComputeCommandEncoder> e) {
      [e setTexture:original atIndex:0]; [e setTexture:blurred atIndex:1];
      [e setBytes:&params length:sizeof(params) atIndex:0];
      [e setTexture:structure atIndex:2];
    });
    diagnostics.structureEncodeMs = milliseconds(stageStart, Clock::now());
    stageStart = Clock::now();
    dispatch(commandBuffer, pipeline("pigment_boundary"), width, height,
             [&](id<MTLComputeCommandEncoder> e) {
      [e setTexture:structure atIndex:0];
      [e setBytes:&params length:sizeof(params) atIndex:0];
      [e setTexture:boundary atIndex:1];
    });
    diagnostics.boundaryEncodeMs = milliseconds(stageStart, Clock::now());

    const NSUInteger massX = oddDiameter(request.params.massScale * scaleX);
    const NSUInteger massY = oddDiameter(request.params.massScale * scaleY);
    MPSImageBox* massBox = [[MPSImageBox alloc] initWithDevice:device
        kernelWidth:massX kernelHeight:massY];
    massBox.edgeMode = MPSImageEdgeModeClamp;
    [massBox encodeToCommandBuffer:commandBuffer sourceTexture:original destinationTexture:seedA];
    [massBox encodeToCommandBuffer:commandBuffer sourceTexture:seedA destinationTexture:seedB];
    [massBox encodeToCommandBuffer:commandBuffer sourceTexture:seedB destinationTexture:seedA];
    const NSUInteger variationX = oddDiameter(request.params.massScale * scaleX * 0.25f);
    const NSUInteger variationY = oddDiameter(request.params.massScale * scaleY * 0.25f);
    MPSImageBox* variationBox = [[MPSImageBox alloc] initWithDevice:device
        kernelWidth:variationX kernelHeight:variationY];
    variationBox.edgeMode = MPSImageEdgeModeClamp;
    [variationBox encodeToCommandBuffer:commandBuffer sourceTexture:original
                      destinationTexture:variation];
    dispatch(commandBuffer, pipeline("pigment_update"), width, height,
             [&](id<MTLComputeCommandEncoder> e) {
      [e setTexture:original atIndex:0]; [e setTexture:seedA atIndex:1];
      [e setTexture:strength atIndex:2];
      [e setBytes:&params length:sizeof(params) atIndex:0];
      [e setTexture:currentA atIndex:3];
    });
    if (params.debugView == 2) {
      dispatch(commandBuffer, pipeline("pigment_copy_texture"), width, height,
               [&](id<MTLComputeCommandEncoder> e) {
        [e setTexture:seedA atIndex:0]; [e setBytes:&params length:sizeof(params) atIndex:0];
        [e setTexture:captured atIndex:1];
      });
    }

    const NSUInteger guidedWidth = std::min<NSUInteger>(255,
        std::max<NSUInteger>(3, (massX | 1)));
    const NSUInteger guidedHeight = std::min<NSUInteger>(255,
        std::max<NSUInteger>(3, (massY | 1)));
    MPSImageBox* guidedBox = [[MPSImageBox alloc] initWithDevice:device
        kernelWidth:guidedWidth kernelHeight:guidedHeight];
    guidedBox.edgeMode = MPSImageEdgeModeClamp;
    stageStart = Clock::now();
    id<MTLTexture> current = currentA;
    id<MTLTexture> next = currentB;
    for (std::uint32_t iteration = 0; iteration < 4; ++iteration) {
      dispatch(commandBuffer, pipeline("pigment_prepare_guidance"), width, height,
               [&](id<MTLComputeCommandEncoder> e) {
        [e setTexture:current atIndex:0]; [e setBytes:&params length:sizeof(params) atIndex:0];
        [e setTexture:guidance atIndex:1];
      });
      dispatch(commandBuffer, pipeline("pigment_guided_statistics"), width, height,
               [&](id<MTLComputeCommandEncoder> e) {
        [e setTexture:original atIndex:0]; [e setTexture:guidance atIndex:1];
        [e setTexture:boundary atIndex:2]; [e setBytes:&params length:sizeof(params) atIndex:0];
        [e setTexture:statWp atIndex:3]; [e setTexture:statWg atIndex:4];
        [e setTexture:statWgg atIndex:5]; [e setTexture:statWgp atIndex:6];
      });
      [guidedBox encodeToCommandBuffer:commandBuffer sourceTexture:boundary destinationTexture:meanWeight];
      [guidedBox encodeToCommandBuffer:commandBuffer sourceTexture:statWp destinationTexture:meanWp];
      [guidedBox encodeToCommandBuffer:commandBuffer sourceTexture:statWg destinationTexture:meanWg];
      [guidedBox encodeToCommandBuffer:commandBuffer sourceTexture:statWgg destinationTexture:meanWgg];
      [guidedBox encodeToCommandBuffer:commandBuffer sourceTexture:statWgp destinationTexture:meanWgp];
      dispatch(commandBuffer, pipeline("pigment_guided_solve"), width, height,
               [&](id<MTLComputeCommandEncoder> e) {
        [e setTexture:meanWeight atIndex:0]; [e setTexture:meanWp atIndex:1];
        [e setTexture:meanWg atIndex:2]; [e setTexture:meanWgg atIndex:3];
        [e setTexture:meanWgp atIndex:4]; [e setBytes:&params length:sizeof(params) atIndex:0];
        [e setTexture:coeffA atIndex:5]; [e setTexture:coeffB atIndex:6];
      });
      [guidedBox encodeToCommandBuffer:commandBuffer sourceTexture:coeffA destinationTexture:statWp];
      [guidedBox encodeToCommandBuffer:commandBuffer sourceTexture:coeffB destinationTexture:statWg];
      dispatch(commandBuffer, pipeline("pigment_guided_reconstruct"), width, height,
               [&](id<MTLComputeCommandEncoder> e) {
        [e setTexture:guidance atIndex:0]; [e setTexture:statWp atIndex:1];
        [e setTexture:statWg atIndex:2]; [e setBytes:&params length:sizeof(params) atIndex:0];
        [e setTexture:filtered atIndex:3];
      });
      dispatch(commandBuffer, pipeline("pigment_update"), width, height,
               [&](id<MTLComputeCommandEncoder> e) {
        [e setTexture:current atIndex:0]; [e setTexture:filtered atIndex:1];
        [e setTexture:strength atIndex:2]; [e setBytes:&params length:sizeof(params) atIndex:0];
        [e setTexture:next atIndex:3];
      });
      std::swap(current, next);
      if (params.debugView == 6 + iteration) {
        dispatch(commandBuffer, pipeline("pigment_copy_texture"), width, height,
                 [&](id<MTLComputeCommandEncoder> e) {
          [e setTexture:current atIndex:0]; [e setBytes:&params length:sizeof(params) atIndex:0];
          [e setTexture:captured atIndex:1];
        });
      }
    }
    diagnostics.iterationsEncodeMs = milliseconds(stageStart, Clock::now());
    stageStart = Clock::now();
    if (params.debugView == 13) {
      dispatch(commandBuffer, pipeline("pigment_copy_texture"), width, height,
               [&](id<MTLComputeCommandEncoder> e) {
        [e setTexture:current atIndex:0]; [e setBytes:&params length:sizeof(params) atIndex:0];
        [e setTexture:captured atIndex:1];
      });
    }
    diagnostics.reconstructionEncodeMs = milliseconds(stageStart, Clock::now());
    stageStart = Clock::now();
    dispatch(commandBuffer, pipeline("pigment_final"), width, height,
             [&](id<MTLComputeCommandEncoder> e) {
      [e setBuffer:source.buffer offset:0 atIndex:0];
      [e setBuffer:destination.buffer offset:0 atIndex:1];
      [e setBytes:&source.layout length:sizeof(source.layout) atIndex:2];
      [e setBytes:&destination.layout length:sizeof(destination.layout) atIndex:3];
      [e setBytes:&params length:sizeof(params) atIndex:4];
      [e setTexture:original atIndex:0]; [e setTexture:current atIndex:1];
      [e setTexture:variation atIndex:2]; [e setTexture:structure atIndex:3];
      [e setTexture:strength atIndex:4]; [e setTexture:boundary atIndex:5];
      [e setTexture:captured atIndex:6];
    });
    diagnostics.yabToRgbEncodeMs = milliseconds(stageStart, Clock::now());
    diagnostics.commandEncodingMs = milliseconds(encodeStart, Clock::now());

    if (request.nativeHostBuffers) {
      auto retainedResources = resources;
      [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
        (void)retainedResources;
        (void)textures;
      }];
      [commandBuffer commit];
      diagnostics.totalMs = milliseconds(totalStart, Clock::now());
      diagnostics.message = "Guided Metal using native host buffers (asynchronous)";
      return true;
    }

    const auto gpuStart = Clock::now();
    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];
    const auto gpuEnd = Clock::now();
    diagnostics.gpuMs = commandBuffer.GPUEndTime > commandBuffer.GPUStartTime
        ? (commandBuffer.GPUEndTime - commandBuffer.GPUStartTime) * 1000.0
        : milliseconds(gpuStart, gpuEnd);
    if (commandBuffer.status == MTLCommandBufferStatusError) {
      diagnostics.failure = MetalFailure::Execution;
      const char* error = commandBuffer.error.localizedDescription.UTF8String;
      diagnostics.message = error ? error : "Metal command buffer execution failed";
      diagnostics.totalMs = milliseconds(totalStart, Clock::now());
      return false;
    }
    const auto readbackStart = Clock::now();
    unpackOutput(destination, request.destination, request.renderWindow, diagnostics);
    diagnostics.readbackMs = milliseconds(readbackStart, Clock::now());
    diagnostics.totalMs = milliseconds(totalStart, Clock::now());
    diagnostics.deviceAllocatedBytes = device.currentAllocatedSize;
    diagnostics.message = diagnostics.path == MetalPath::CpuNoCopy
        ? "Guided Metal using no-copy CPU buffers"
        : "Guided Metal using shared staging buffers";
    return true;
  }

  bool renderIntegrated(const IntegratedMetalExecutionRequest& request) {
    std::lock_guard<std::mutex> lock(mutex);
    diagnostics = {};
    const auto totalStart = Clock::now();
    std::string reason;
    if (request.renderWindow.empty() || request.source.components < 3 ||
        request.destination.components != request.source.components ||
        !validateView(request.source, request.renderWindow, request.nativeHostBuffers, reason) ||
        !validateView(request.destination, request.renderWindow, request.nativeHostBuffers, reason) ||
        (request.hasMask && !validateView(request.mask, request.renderWindow,
                                          request.nativeHostBuffers, reason))) {
      diagnostics.failure = MetalFailure::InvalidLayout;
      diagnostics.message = reason.empty() ? "Unsupported integrated image layout" : reason;
      return false;
    }

    id<MTLCommandQueue> queue = request.nativeHostBuffers
        ? (__bridge id<MTLCommandQueue>)request.hostCommandQueue : fallbackQueue;
    id<MTLDevice> device = queue.device;
    if (!request.nativeHostBuffers && !queue) {
      device = MTLCreateSystemDefaultDevice();
      fallbackQueue = [device newCommandQueue];
      queue = fallbackQueue;
    }
    if (!device || !queue) {
      diagnostics.failure = MetalFailure::Unsupported;
      diagnostics.message = "Metal device or command queue is unavailable";
      return false;
    }
    diagnostics.deviceName = device.name.UTF8String ? device.name.UTF8String
                                                     : "Unknown Metal device";
    auto resources = resourcesFor(device);
    if (!resources->valid()) {
      diagnostics.failure = MetalFailure::Initialization;
      diagnostics.message = resources->error;
      return false;
    }

    const auto wrapStart = Clock::now();
    BufferBinding source, destination, mask;
    if (request.nativeHostBuffers) {
      source = nativeBinding(request.source, request.renderWindow, reason);
      destination = nativeBinding(request.destination, request.renderWindow, reason);
      if (request.hasMask) mask = nativeBinding(request.mask, request.renderWindow, reason);
      diagnostics.path = MetalPath::NativeHostBuffers;
    } else {
      source = cpuInputBinding(device, request.source, request.renderWindow, diagnostics);
      destination = cpuOutputBinding(device, request.destination, request.renderWindow);
      if (request.hasMask)
        mask = cpuInputBinding(device, request.mask, request.renderWindow, diagnostics);
      diagnostics.sourceNoCopy = source.noCopy;
      diagnostics.destinationNoCopy = destination.noCopy;
      diagnostics.path = source.noCopy && destination.noCopy &&
          (!request.hasMask || mask.noCopy) ? MetalPath::CpuNoCopy : MetalPath::CpuStaging;
    }
    if (!source.buffer || !destination.buffer || (request.hasMask && !mask.buffer)) {
      diagnostics.failure = request.nativeHostBuffers ? MetalFailure::InvalidLayout
                                                      : MetalFailure::Allocation;
      diagnostics.message = reason.empty() ? "Could not bind integrated Metal buffers" : reason;
      return false;
    }
    if (request.nativeHostBuffers &&
        (source.buffer.device != device || destination.buffer.device != device ||
         (request.hasMask && mask.buffer.device != device))) {
      diagnostics.failure = MetalFailure::InvalidLayout;
      diagnostics.message = "Host queue and native buffers use different Metal devices";
      return false;
    }
    diagnostics.wrapOrUploadMs = milliseconds(wrapStart, Clock::now());

    const NSUInteger width = request.renderWindow.width();
    const NSUInteger height = request.renderWindow.height();
    const NSUInteger halfWidth = std::max<NSUInteger>(1, (width + 1) / 2);
    const NSUInteger halfHeight = std::max<NSUInteger>(1, (height + 1) / 2);
    const NSUInteger quarterWidth = std::max<NSUInteger>(1, (width + 3) / 4);
    const NSUInteger quarterHeight = std::max<NSUInteger>(1, (height + 3) / 4);
    const bool reused = !request.nativeHostBuffers && cachedIntegratedTextures &&
        cachedDevice == device && cachedIntegratedWidth == width &&
        cachedIntegratedHeight == height;
    NSArray<id<MTLTexture>>* textures = cachedIntegratedTextures;
    if (!reused) {
      NSMutableArray<id<MTLTexture>>* allocated = [NSMutableArray arrayWithCapacity:53];
      for (int i = 0; i < 30; ++i)
        [allocated addObject:texture(device, MTLPixelFormatRGBA32Float, width, height)];
      for (int i = 0; i < 9; ++i)
        [allocated addObject:texture(device, MTLPixelFormatR32Float, width, height)];
      for (int i = 0; i < 2; ++i)
        [allocated addObject:texture(device, MTLPixelFormatRGBA32Float, halfWidth, halfHeight)];
      for (int i = 0; i < 2; ++i)
        [allocated addObject:texture(device, MTLPixelFormatRG32Float, halfWidth, halfHeight)];
      for (int i = 0; i < 2; ++i)
        [allocated addObject:texture(device, MTLPixelFormatRGBA32Float, quarterWidth, quarterHeight)];
      for (int i = 0; i < 2; ++i)
        [allocated addObject:texture(device, MTLPixelFormatRG32Float, quarterWidth, quarterHeight)];
      [allocated addObject:texture(device, MTLPixelFormatR32Float, halfWidth, halfHeight)];
      [allocated addObject:texture(device, MTLPixelFormatR32Float, quarterWidth, quarterHeight)];
      for (int i = 0; i < 2; ++i)
        [allocated addObject:texture(device, MTLPixelFormatRGBA32Float, halfWidth, halfHeight)];
      for (int i = 0; i < 2; ++i)
        [allocated addObject:texture(device, MTLPixelFormatRGBA32Float, quarterWidth, quarterHeight)];
      textures = [allocated copy];
      if (textures.count != 53) {
        diagnostics.failure = MetalFailure::Allocation;
        diagnostics.message = "Could not allocate integrated Metal scratch textures";
        return false;
      }
      if (!request.nativeHostBuffers) {
        cachedDevice = device;
        cachedIntegratedWidth = width;
        cachedIntegratedHeight = height;
        cachedIntegratedTextures = textures;
      }
    }
    for (id<MTLTexture> value in textures) {
      if (!value) {
        diagnostics.failure = MetalFailure::Allocation;
        diagnostics.message = "Could not allocate integrated Metal scratch texture";
        return false;
      }
      diagnostics.scratchBytes += value.allocatedSize;
      if (!reused) ++diagnostics.scratchAllocations;
    }

    id<MTLTexture> original = textures[0], blurNear = textures[1], blurFar = textures[2];
    id<MTLTexture> structure = textures[3], mass = textures[4], blurredMass = textures[6];
    id<MTLTexture> boundaryResult = textures[7], chromaBlur = textures[8];
    id<MTLTexture> chromaResult = textures[9], fineBlur = textures[10];
    id<MTLTexture> mediumBlur = textures[11], broadBlur = textures[12], finalYab = textures[13];
    id<MTLTexture> guidance = textures[14], statWp = textures[15], statWg = textures[16];
    id<MTLTexture> statWgg = textures[17], statWgp = textures[18], meanWp = textures[19];
    id<MTLTexture> meanWg = textures[20], meanWgg = textures[21], meanWgp = textures[22];
    id<MTLTexture> coeffA = textures[23], coeffB = textures[24], filtered = textures[25];
    id<MTLTexture> cleanupA = textures[26], cleanupB = textures[27];
    id<MTLTexture> dominantFull = textures[28], diagnosticFull = textures[29];
    id<MTLTexture> protection = textures[30], permeability = textures[31];
    id<MTLTexture> veil = textures[32], massField = textures[33], extinctionField = textures[34];
    id<MTLTexture> chromaField = textures[35], detailField = textures[36];
    id<MTLTexture> attraction = textures[37], meanWeight = textures[38];
    id<MTLTexture> halfYabA = textures[39], halfYabB = textures[40];
    id<MTLTexture> halfPosA = textures[41], halfPosB = textures[42];
    id<MTLTexture> quarterYabA = textures[43], quarterYabB = textures[44];
    id<MTLTexture> quarterPosA = textures[45], quarterPosB = textures[46];
    id<MTLTexture> halfDensity = textures[47], quarterDensity = textures[48];
    id<MTLTexture> halfDominant = textures[49], halfDiagnostic = textures[50];
    id<MTLTexture> quarterDominant = textures[51], quarterDiagnostic = textures[52];

    id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
    if (!commandBuffer) {
      diagnostics.failure = MetalFailure::Encoding;
      diagnostics.message = "Could not create integrated Metal command buffer";
      return false;
    }
    commandBuffer.label = @"Pigment Integrated Painterly Graph";
    const GpuIntegratedParams params = makeIntegratedParams(request);
    const GpuImageLayout emptyMask{};
    auto pipeline = [&](const char* name) { return resources->pipelines.at(name); };
    const auto encodeStart = Clock::now();
    auto stageStart = Clock::now();

    dispatch(commandBuffer, pipeline("pigment_integrated_rgb_to_yab"), width, height,
             [&](id<MTLComputeCommandEncoder> e) {
      [e setBuffer:source.buffer offset:0 atIndex:0];
      [e setBytes:&source.layout length:sizeof(source.layout) atIndex:1];
      [e setBytes:&params length:sizeof(params) atIndex:2]; [e setTexture:original atIndex:0];
    });
    diagnostics.rgbToYabEncodeMs = milliseconds(stageStart, Clock::now());

    const float scaleX = static_cast<float>(request.geometry.renderScaleX /
        std::max(request.geometry.pixelAspect, 1.0e-6));
    const float scaleY = static_cast<float>(request.geometry.renderScaleY);
    stageStart = Clock::now();
    MPSImageGaussianBlur* nearBlur = [[MPSImageGaussianBlur alloc] initWithDevice:device
        sigma:std::max(0.01f, request.params.structureScale * 0.5f * (scaleX + scaleY))];
    nearBlur.edgeMode = MPSImageEdgeModeClamp;
    [nearBlur encodeToCommandBuffer:commandBuffer sourceTexture:original destinationTexture:blurNear];
    MPSImageGaussianBlur* farBlur = [[MPSImageGaussianBlur alloc] initWithDevice:device
        sigma:std::max(0.01f, std::max(request.params.structureScale * 2.0f,
                                      request.params.boundaryScale) * 0.5f * (scaleX + scaleY))];
    farBlur.edgeMode = MPSImageEdgeModeClamp;
    [farBlur encodeToCommandBuffer:commandBuffer sourceTexture:original destinationTexture:blurFar];
    dispatch(commandBuffer, pipeline("pigment_integrated_structure"), width, height,
             [&](id<MTLComputeCommandEncoder> e) {
      [e setTexture:blurNear atIndex:0]; [e setTexture:blurFar atIndex:1];
      [e setBytes:&params length:sizeof(params) atIndex:0]; [e setTexture:structure atIndex:2];
      [e setTexture:protection atIndex:3]; [e setTexture:permeability atIndex:4];
    });
    diagnostics.structureEncodeMs = milliseconds(stageStart, Clock::now());

    stageStart = Clock::now();
    dispatch(commandBuffer, pipeline("pigment_integrated_fields"), width, height,
             [&](id<MTLComputeCommandEncoder> e) {
      [e setTexture:original atIndex:0];
      [e setBuffer:request.hasMask ? mask.buffer : source.buffer offset:0 atIndex:0];
      const auto& layout = request.hasMask ? mask.layout : emptyMask;
      [e setBytes:&layout length:sizeof(layout) atIndex:1];
      [e setBytes:&params length:sizeof(params) atIndex:2];
      [e setTexture:veil atIndex:1]; [e setTexture:massField atIndex:2];
      [e setTexture:extinctionField atIndex:3]; [e setTexture:chromaField atIndex:4];
      [e setTexture:detailField atIndex:5];
    });
    diagnostics.strengthEncodeMs = milliseconds(stageStart, Clock::now());

    const float quarterBlend = std::max(0.0f, std::min(1.0f,
        (request.params.massScale - 20.0f) / 8.0f));
    GpuRegionLevelParams halfLevel{static_cast<std::uint32_t>(halfWidth),
        static_cast<std::uint32_t>(halfHeight), 0.5f,
        std::max(0.25f, request.params.massScale * scaleX * 0.5f),
        std::max(0.25f, request.params.massScale * scaleY * 0.5f), quarterBlend};
    GpuRegionLevelParams quarterLevel{static_cast<std::uint32_t>(quarterWidth),
        static_cast<std::uint32_t>(quarterHeight), 0.25f,
        std::max(0.25f, request.params.massScale * scaleX * 0.25f),
        std::max(0.25f, request.params.massScale * scaleY * 0.25f), quarterBlend};
    auto initializeLevel = [&](const GpuRegionLevelParams& level,
                               id<MTLTexture> yab, id<MTLTexture> position) {
      dispatch(commandBuffer, pipeline("pigment_integrated_downsample"),
               level.width, level.height, [&](id<MTLComputeCommandEncoder> e) {
        [e setTexture:original atIndex:0]; [e setBytes:&level length:sizeof(level) atIndex:0];
        [e setTexture:yab atIndex:1]; [e setTexture:position atIndex:2];
      });
    };
    initializeLevel(halfLevel, halfYabA, halfPosA);
    initializeLevel(quarterLevel, quarterYabA, quarterPosA);
    auto iterateLevel = [&](const GpuRegionLevelParams& level,
                            id<MTLTexture> yabA, id<MTLTexture> yabB,
                            id<MTLTexture> posA, id<MTLTexture> posB,
                            id<MTLTexture> density, id<MTLTexture> dominant,
                            id<MTLTexture> diagnostic) {
      for (int iteration = 0; iteration < 3; ++iteration) {
        if (params.massEstimator != 0) {
          dispatch(commandBuffer, pipeline("pigment_integrated_local_density"),
                   level.width, level.height, [&](id<MTLComputeCommandEncoder> e) {
            [e setTexture:yabA atIndex:0]; [e setTexture:posA atIndex:1];
            [e setTexture:protection atIndex:2]; [e setTexture:extinctionField atIndex:3];
            [e setBytes:&params length:sizeof(params) atIndex:0];
            [e setBytes:&level length:sizeof(level) atIndex:1]; [e setTexture:density atIndex:4];
          });
          dispatch(commandBuffer, pipeline("pigment_integrated_representative_iteration"),
                   level.width, level.height, [&](id<MTLComputeCommandEncoder> e) {
            [e setTexture:yabA atIndex:0]; [e setTexture:posA atIndex:1];
            [e setTexture:density atIndex:2]; [e setTexture:protection atIndex:3];
            [e setTexture:extinctionField atIndex:4]; [e setTexture:massField atIndex:5];
            [e setBytes:&params length:sizeof(params) atIndex:0];
            [e setBytes:&level length:sizeof(level) atIndex:1];
            [e setTexture:yabB atIndex:6]; [e setTexture:posB atIndex:7];
            [e setTexture:dominant atIndex:8]; [e setTexture:diagnostic atIndex:9];
          });
        } else {
          dispatch(commandBuffer, pipeline("pigment_integrated_region_iteration"),
                   level.width, level.height, [&](id<MTLComputeCommandEncoder> e) {
            [e setTexture:yabA atIndex:0]; [e setTexture:posA atIndex:1];
            [e setTexture:protection atIndex:2]; [e setTexture:massField atIndex:3];
            [e setBytes:&params length:sizeof(params) atIndex:0];
            [e setBytes:&level length:sizeof(level) atIndex:1];
            [e setTexture:yabB atIndex:4]; [e setTexture:posB atIndex:5];
          });
        }
        std::swap(yabA, yabB); std::swap(posA, posB);
      }
      return @[yabA, posA, dominant, diagnostic];
    };
    stageStart = Clock::now();
    NSArray<id<MTLTexture>>* halfFinal =
        iterateLevel(halfLevel, halfYabA, halfYabB, halfPosA, halfPosB,
                     halfDensity, halfDominant, halfDiagnostic);
    NSArray<id<MTLTexture>>* quarterFinal =
        iterateLevel(quarterLevel, quarterYabA, quarterYabB, quarterPosA, quarterPosB,
                     quarterDensity, quarterDominant, quarterDiagnostic);
    halfYabA = halfFinal[0]; halfPosA = halfFinal[1];
    quarterYabA = quarterFinal[0]; quarterPosA = quarterFinal[1];
    dispatch(commandBuffer, pipeline("pigment_integrated_reconstruct_mass"), width, height,
             [&](id<MTLComputeCommandEncoder> e) {
      [e setTexture:original atIndex:0]; [e setTexture:halfYabA atIndex:1];
      [e setTexture:halfPosA atIndex:2]; [e setTexture:quarterYabA atIndex:3];
      [e setTexture:quarterPosA atIndex:4]; [e setTexture:halfDominant atIndex:5];
      [e setTexture:quarterDominant atIndex:6]; [e setTexture:halfDiagnostic atIndex:7];
      [e setTexture:quarterDiagnostic atIndex:8];
      [e setBytes:&params length:sizeof(params) atIndex:0];
      [e setBytes:&halfLevel length:sizeof(halfLevel) atIndex:1];
      [e setBytes:&quarterLevel length:sizeof(quarterLevel) atIndex:2];
      [e setTexture:mass atIndex:9]; [e setTexture:attraction atIndex:10];
      [e setTexture:dominantFull atIndex:11]; [e setTexture:diagnosticFull atIndex:12];
    });
    diagnostics.iterationsEncodeMs = milliseconds(stageStart, Clock::now());

    stageStart = Clock::now();
    MPSImageGaussianBlur* boundaryBlur = [[MPSImageGaussianBlur alloc] initWithDevice:device
        sigma:std::max(0.01f, request.params.boundaryScale * 0.5f * (scaleX + scaleY))];
    boundaryBlur.edgeMode = MPSImageEdgeModeClamp;
    [boundaryBlur encodeToCommandBuffer:commandBuffer sourceTexture:mass destinationTexture:blurredMass];
    dispatch(commandBuffer, pipeline("pigment_integrated_boundary_extinction"), width, height,
             [&](id<MTLComputeCommandEncoder> e) {
      [e setTexture:mass atIndex:0]; [e setTexture:blurredMass atIndex:1];
      [e setTexture:protection atIndex:2]; [e setTexture:extinctionField atIndex:3];
      [e setBytes:&params length:sizeof(params) atIndex:0];
      [e setTexture:boundaryResult atIndex:4];
    });
    diagnostics.boundaryEncodeMs = milliseconds(stageStart, Clock::now());

    id<MTLTexture> cleaned = boundaryResult;
    if (request.params.detailCleanup > 0.0f) {
      GpuParams cleanup{};
      cleanup.width = width; cleanup.height = height;
      cleanup.massStrength = request.params.detailCleanup;
      cleanup.toneSimilarity = request.params.toneSimilarity;
      cleanup.chromaSimilarity = request.params.chromaSimilarity;
      const NSUInteger diameter = std::min<NSUInteger>(63, std::max<NSUInteger>(3,
          (2 * static_cast<NSUInteger>(std::ceil(request.params.massScale * 0.2f)) + 1) | 1));
      MPSImageBox* guidedBox = [[MPSImageBox alloc] initWithDevice:device
          kernelWidth:diameter kernelHeight:diameter];
      guidedBox.edgeMode = MPSImageEdgeModeClamp;
      id<MTLTexture> current = boundaryResult;
      id<MTLTexture> next = cleanupA;
      for (int iteration = 0; iteration < 4; ++iteration) {
        dispatch(commandBuffer, pipeline("pigment_prepare_guidance"), width, height,
                 [&](id<MTLComputeCommandEncoder> e) {
          [e setTexture:current atIndex:0]; [e setBytes:&cleanup length:sizeof(cleanup) atIndex:0];
          [e setTexture:guidance atIndex:1];
        });
        dispatch(commandBuffer, pipeline("pigment_guided_statistics"), width, height,
                 [&](id<MTLComputeCommandEncoder> e) {
          [e setTexture:boundaryResult atIndex:0]; [e setTexture:guidance atIndex:1];
          [e setTexture:permeability atIndex:2]; [e setBytes:&cleanup length:sizeof(cleanup) atIndex:0];
          [e setTexture:statWp atIndex:3]; [e setTexture:statWg atIndex:4];
          [e setTexture:statWgg atIndex:5]; [e setTexture:statWgp atIndex:6];
        });
        [guidedBox encodeToCommandBuffer:commandBuffer sourceTexture:permeability destinationTexture:meanWeight];
        [guidedBox encodeToCommandBuffer:commandBuffer sourceTexture:statWp destinationTexture:meanWp];
        [guidedBox encodeToCommandBuffer:commandBuffer sourceTexture:statWg destinationTexture:meanWg];
        [guidedBox encodeToCommandBuffer:commandBuffer sourceTexture:statWgg destinationTexture:meanWgg];
        [guidedBox encodeToCommandBuffer:commandBuffer sourceTexture:statWgp destinationTexture:meanWgp];
        dispatch(commandBuffer, pipeline("pigment_guided_solve"), width, height,
                 [&](id<MTLComputeCommandEncoder> e) {
          [e setTexture:meanWeight atIndex:0]; [e setTexture:meanWp atIndex:1];
          [e setTexture:meanWg atIndex:2]; [e setTexture:meanWgg atIndex:3];
          [e setTexture:meanWgp atIndex:4]; [e setBytes:&cleanup length:sizeof(cleanup) atIndex:0];
          [e setTexture:coeffA atIndex:5]; [e setTexture:coeffB atIndex:6];
        });
        [guidedBox encodeToCommandBuffer:commandBuffer sourceTexture:coeffA destinationTexture:statWp];
        [guidedBox encodeToCommandBuffer:commandBuffer sourceTexture:coeffB destinationTexture:statWg];
        dispatch(commandBuffer, pipeline("pigment_guided_reconstruct"), width, height,
                 [&](id<MTLComputeCommandEncoder> e) {
          [e setTexture:guidance atIndex:0]; [e setTexture:statWp atIndex:1];
          [e setTexture:statWg atIndex:2]; [e setBytes:&cleanup length:sizeof(cleanup) atIndex:0];
          [e setTexture:filtered atIndex:3];
        });
        dispatch(commandBuffer, pipeline("pigment_update"), width, height,
                 [&](id<MTLComputeCommandEncoder> e) {
          [e setTexture:current atIndex:0]; [e setTexture:filtered atIndex:1];
          [e setTexture:massField atIndex:2]; [e setBytes:&cleanup length:sizeof(cleanup) atIndex:0];
          [e setTexture:next atIndex:3];
        });
        current = next; next = next == cleanupA ? cleanupB : cleanupA;
      }
      cleaned = current;
    }

    stageStart = Clock::now();
    MPSImageGaussianBlur* chromaGaussian = [[MPSImageGaussianBlur alloc] initWithDevice:device
        sigma:std::max(0.01f, request.params.chromaScale * 0.5f * (scaleX + scaleY))];
    chromaGaussian.edgeMode = MPSImageEdgeModeClamp;
    [chromaGaussian encodeToCommandBuffer:commandBuffer sourceTexture:cleaned destinationTexture:chromaBlur];
    dispatch(commandBuffer, pipeline("pigment_integrated_chroma"), width, height,
             [&](id<MTLComputeCommandEncoder> e) {
      [e setTexture:cleaned atIndex:0]; [e setTexture:chromaBlur atIndex:1];
      [e setTexture:protection atIndex:2]; [e setTexture:chromaField atIndex:3];
      [e setBytes:&params length:sizeof(params) atIndex:0]; [e setTexture:chromaResult atIndex:4];
    });

    MPSImageGaussianBlur* fineGaussian = [[MPSImageGaussianBlur alloc] initWithDevice:device sigma:1.0f];
    fineGaussian.edgeMode = MPSImageEdgeModeClamp;
    [fineGaussian encodeToCommandBuffer:commandBuffer sourceTexture:original destinationTexture:fineBlur];
    MPSImageGaussianBlur* mediumGaussian = [[MPSImageGaussianBlur alloc] initWithDevice:device
        sigma:std::max(1.5f, request.params.massScale * 0.2f * 0.5f * (scaleX + scaleY))];
    mediumGaussian.edgeMode = MPSImageEdgeModeClamp;
    [mediumGaussian encodeToCommandBuffer:commandBuffer sourceTexture:original destinationTexture:mediumBlur];
    MPSImageGaussianBlur* broadGaussian = [[MPSImageGaussianBlur alloc] initWithDevice:device
        sigma:std::max(2.0f, request.params.massScale * 0.5f * (scaleX + scaleY))];
    broadGaussian.edgeMode = MPSImageEdgeModeClamp;
    [broadGaussian encodeToCommandBuffer:commandBuffer sourceTexture:original destinationTexture:broadBlur];
    dispatch(commandBuffer, pipeline("pigment_integrated_reintegrate"), width, height,
             [&](id<MTLComputeCommandEncoder> e) {
      [e setTexture:original atIndex:0]; [e setTexture:chromaResult atIndex:1];
      [e setTexture:fineBlur atIndex:2]; [e setTexture:mediumBlur atIndex:3];
      [e setTexture:broadBlur atIndex:4]; [e setTexture:detailField atIndex:5];
      [e setBytes:&params length:sizeof(params) atIndex:0]; [e setTexture:finalYab atIndex:6];
    });
    diagnostics.reconstructionEncodeMs = milliseconds(stageStart, Clock::now());

    stageStart = Clock::now();
    dispatch(commandBuffer, pipeline("pigment_integrated_final"), width, height,
             [&](id<MTLComputeCommandEncoder> e) {
      [e setBuffer:source.buffer offset:0 atIndex:0]; [e setBuffer:destination.buffer offset:0 atIndex:1];
      [e setBytes:&source.layout length:sizeof(source.layout) atIndex:2];
      [e setBytes:&destination.layout length:sizeof(destination.layout) atIndex:3];
      [e setBytes:&params length:sizeof(params) atIndex:4];
      [e setTexture:original atIndex:0]; [e setTexture:structure atIndex:1];
      [e setTexture:veil atIndex:2]; [e setTexture:massField atIndex:3];
      [e setTexture:extinctionField atIndex:4]; [e setTexture:chromaField atIndex:5];
      [e setTexture:detailField atIndex:6]; [e setTexture:mass atIndex:7];
      [e setTexture:attraction atIndex:8]; [e setTexture:mass atIndex:9];
      [e setTexture:protection atIndex:10]; [e setTexture:boundaryResult atIndex:11];
      [e setTexture:chromaResult atIndex:12]; [e setTexture:fineBlur atIndex:13];
      [e setTexture:mediumBlur atIndex:14]; [e setTexture:broadBlur atIndex:15];
      [e setTexture:finalYab atIndex:16];
      [e setTexture:dominantFull atIndex:17]; [e setTexture:diagnosticFull atIndex:18];
    });
    diagnostics.yabToRgbEncodeMs = milliseconds(stageStart, Clock::now());
    diagnostics.commandEncodingMs = milliseconds(encodeStart, Clock::now());

    if (request.nativeHostBuffers) {
      auto retainedResources = resources;
      [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
        (void)retainedResources; (void)textures;
      }];
      [commandBuffer commit];
      diagnostics.totalMs = milliseconds(totalStart, Clock::now());
      diagnostics.message = "Integrated Pigment using native host buffers (asynchronous)";
      return true;
    }
    const auto gpuStart = Clock::now();
    [commandBuffer commit]; [commandBuffer waitUntilCompleted];
    const auto gpuEnd = Clock::now();
    diagnostics.gpuMs = commandBuffer.GPUEndTime > commandBuffer.GPUStartTime
        ? (commandBuffer.GPUEndTime - commandBuffer.GPUStartTime) * 1000.0
        : milliseconds(gpuStart, gpuEnd);
    if (commandBuffer.status == MTLCommandBufferStatusError) {
      diagnostics.failure = MetalFailure::Execution;
      const char* error = commandBuffer.error.localizedDescription.UTF8String;
      diagnostics.message = error ? error : "Integrated Metal command buffer failed";
      diagnostics.totalMs = milliseconds(totalStart, Clock::now());
      return false;
    }
    const auto readbackStart = Clock::now();
    unpackOutput(destination, request.destination, request.renderWindow, diagnostics);
    diagnostics.readbackMs = milliseconds(readbackStart, Clock::now());
    diagnostics.totalMs = milliseconds(totalStart, Clock::now());
    diagnostics.deviceAllocatedBytes = device.currentAllocatedSize;
    diagnostics.message = diagnostics.path == MetalPath::CpuNoCopy
        ? "Integrated Pigment using no-copy CPU buffers"
        : "Integrated Pigment using shared staging buffers";
    return true;
  }

  void releaseTransientResources() {
    std::lock_guard<std::mutex> lock(mutex);
    cachedTextures = nil;
    cachedIntegratedTextures = nil;
    cachedDevice = nil;
    cachedWidth = cachedHeight = 0;
    cachedIntegratedWidth = cachedIntegratedHeight = 0;
    diagnostics = {};
  }
};

MetalInstance::MetalInstance() : impl_(std::make_unique<Impl>()) {}
MetalInstance::~MetalInstance() = default;
bool MetalInstance::render(const MetalExecutionRequest& request) {
  return impl_->render(request);
}
bool MetalInstance::renderIntegrated(const IntegratedMetalExecutionRequest& request) {
  if (request.params.comparison == PigmentComparisonMode::GuidedDetailCollapse) {
    MetalExecutionRequest guided;
    guided.source = request.source; guided.destination = request.destination;
    guided.mask = request.mask; guided.hasMask = request.hasMask;
    guided.nativeHostBuffers = request.nativeHostBuffers;
    guided.hostCommandQueue = request.hostCommandQueue;
    guided.renderWindow = request.renderWindow; guided.geometry = request.geometry;
    guided.params.backend = DetailCollapseBackend::GuidedMetal;
    guided.params.amount = request.params.amount;
    guided.params.massScale = request.params.massScale;
    guided.params.structureScale = request.params.structureScale;
    guided.params.massStrength = request.params.massStrength;
    guided.params.toneSimilarity = request.params.toneSimilarity;
    guided.params.chromaSimilarity = request.params.chromaSimilarity;
    guided.params.boundaryPreserve = request.params.boundaryPreserve;
    guided.params.boundarySoftness = request.params.boundarySoftness;
    guided.params.structurePreserve = request.params.structurePreserve;
    guided.params.internalVariation = request.params.internalVariation;
    guided.params.lumaMassing = request.params.lumaAttraction;
    guided.params.chromaMassing = request.params.chromaAttraction;
    guided.params.invertMask = request.params.invertMask;
    guided.params.mix = request.params.mix;
    guided.params.premultiplied = request.params.premultiplied;
    guided.params.gamut = request.params.gamut;
    return impl_->render(guided);
  }
  return impl_->renderIntegrated(request);
}
const MetalDiagnostics& MetalInstance::diagnostics() const noexcept {
  return impl_->diagnostics;
}
void MetalInstance::releaseTransientResources() { impl_->releaseTransientResources(); }

}  // namespace pigment::metal
