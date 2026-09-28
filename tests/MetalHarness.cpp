#include "core/DetailCollapseResearch.h"
#include "core/ColorSpace.h"
#include "metal/PigmentMetal.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Options {
  int width = 128;
  int height = 96;
  int warmups = 1;
  int rounds = 3;
  float massScale = 8.0f;
  bool verify = false;
  bool integrated = false;
};

Options options(int argc, char** argv) {
  Options result;
  for (int i = 1; i < argc; ++i) {
    const std::string option = argv[i];
    const auto value = [&]() -> const char* {
      if (++i >= argc) throw std::runtime_error(option + " requires a value");
      return argv[i];
    };
    if (option == "--width") result.width = std::stoi(value());
    else if (option == "--height") result.height = std::stoi(value());
    else if (option == "--mass-scale") result.massScale = std::stof(value());
    else if (option == "--warmups") result.warmups = std::stoi(value());
    else if (option == "--rounds") result.rounds = std::stoi(value());
    else if (option == "--verify") result.verify = true;
    else if (option == "--integrated") result.integrated = true;
    else throw std::runtime_error("unknown option: " + option);
  }
  return result;
}

pigment::DetailCollapseResearchParams parameters(float massScale) {
  pigment::DetailCollapseResearchParams p;
  p.backend = pigment::DetailCollapseBackend::GuidedMetal;
  p.amount = 0.85f;
  p.massScale = massScale;
  p.structureScale = 5.0f;
  p.massStrength = 0.65f;
  p.toneSimilarity = 0.35f;
  p.chromaSimilarity = 0.12f;
  p.boundaryPreserve = 0.9f;
  p.boundarySoftness = 0.08f;
  p.internalVariation = 0.15f;
  return p;
}

pigment::IntegratedPigmentParams integratedParameters(float massScale) {
  pigment::IntegratedPigmentParams p;
  p.massScale = massScale;
  p.amount = 0.75f;
  p.massStrength = 0.6f;
  p.detailCleanup = 0.0f;
  return p;
}

void fill(std::vector<float>& pixels, int width, int height) {
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const std::size_t i = static_cast<std::size_t>(y * width + x) * 4;
      const float broad = x < width / 2 ? 0.08f : 1.6f;
      const float spec = ((x * 17 + y * 29) % 101 == 0) ? 6.0f : 0.0f;
      pixels[i] = broad + spec - 0.15f;
      pixels[i + 1] = broad * 0.72f + 0.04f * std::sin(0.07f * y);
      pixels[i + 2] = broad * 0.48f + 0.08f * std::sin(0.11f * x);
      pixels[i + 3] = 0.35f + 0.65f * static_cast<float>((x + y) % 13) / 12.0f;
    }
  }
}

double percentile(std::vector<double> values, double fraction) {
  std::sort(values.begin(), values.end());
  const std::size_t index = std::min(values.size() - 1,
      static_cast<std::size_t>(std::ceil(fraction * values.size()) - 1));
  return values[index];
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options o = options(argc, argv);
    const pigment::RectI bounds{0, 0, o.width, o.height};
    std::vector<float> source(static_cast<std::size_t>(o.width * o.height) * 4);
    std::vector<float> output(source.size(), 0.0f);
    fill(source, o.width, o.height);
    pigment::metal::MetalInstance instance;
    pigment::metal::MetalExecutionRequest request;
    request.source = {source.data(), source.size() * sizeof(float),
                      o.width * 4 * static_cast<int>(sizeof(float)), bounds, 4};
    request.destination = {output.data(), output.size() * sizeof(float),
                      o.width * 4 * static_cast<int>(sizeof(float)), bounds, 4};
    request.renderWindow = bounds;
    request.params = parameters(o.massScale);
    std::vector<double> times;
    for (int iteration = 0; iteration < o.warmups + o.rounds; ++iteration) {
      const auto begin = std::chrono::steady_clock::now();
      bool succeeded = false;
      if (o.integrated) {
        pigment::metal::IntegratedMetalExecutionRequest integrated;
        integrated.source = request.source;
        integrated.destination = request.destination;
        integrated.renderWindow = bounds;
        integrated.params = integratedParameters(o.massScale);
        succeeded = instance.renderIntegrated(integrated);
      } else {
        succeeded = instance.render(request);
      }
      if (!succeeded) {
        std::cerr << "Metal failure: " << instance.diagnostics().message << '\n';
        return 2;
      }
      const auto end = std::chrono::steady_clock::now();
      if (iteration >= o.warmups)
        times.push_back(std::chrono::duration<double, std::milli>(end - begin).count());
    }
    double sum = 0.0;
    for (double value : times) sum += value;
    double variance = 0.0;
    for (double value : times) variance += (value - sum / times.size()) *
                                           (value - sum / times.size());
    const auto& d = instance.diagnostics();
    std::cout << std::fixed << std::setprecision(3)
              << (o.integrated ? "integrated " : "guided ")
              << o.width << 'x' << o.height << " mass=" << o.massScale
              << " median_ms=" << percentile(times, 0.5)
              << " p95_ms=" << percentile(times, 0.95)
              << " cv=" << std::sqrt(variance / times.size()) / (sum / times.size())
              << " gpu_ms=" << d.gpuMs << " upload_ms=" << d.wrapOrUploadMs
              << " encode_ms=" << d.commandEncodingMs
              << " rgb_yab_encode_ms=" << d.rgbToYabEncodeMs
              << " strength_encode_ms=" << d.strengthEncodeMs
              << " structure_encode_ms=" << d.structureEncodeMs
              << " boundary_encode_ms=" << d.boundaryEncodeMs
              << " iterations_encode_ms=" << d.iterationsEncodeMs
              << " reconstruction_encode_ms=" << d.reconstructionEncodeMs
              << " yab_rgb_encode_ms=" << d.yabToRgbEncodeMs
              << " readback_ms=" << d.readbackMs
              << " upload_bytes=" << d.uploadedBytes
              << " download_bytes=" << d.downloadedBytes
              << " scratch_bytes=" << d.scratchBytes
              << " device_allocated_bytes=" << d.deviceAllocatedBytes
              << " device=\"" << d.deviceName << "\" path=" << static_cast<int>(d.path)
              << '\n';

    if (o.verify && !o.integrated) {
      std::vector<float> cpu(source.size(), 0.0f);
      auto p = request.params;
      p.backend = pigment::DetailCollapseBackend::GuidedCpu;
      pigment::processDetailCollapseResearch(
          {source.data(), o.width * 4, bounds, 4},
          {cpu.data(), o.width * 4, bounds, 4}, bounds, p, {}, nullptr);
      double squared = 0.0;
      std::size_t count = 0;
      float cpuMinimum = cpu[0], cpuMaximum = cpu[0];
      float metalMinimum = output[0], metalMaximum = output[0];
      for (std::size_t i = 0; i < cpu.size(); ++i) {
        if ((i & 3U) == 3U) continue;
        cpuMinimum = std::min(cpuMinimum, cpu[i]);
        cpuMaximum = std::max(cpuMaximum, cpu[i]);
        metalMinimum = std::min(metalMinimum, output[i]);
        metalMaximum = std::max(metalMaximum, output[i]);
        const double delta = static_cast<double>(cpu[i]) - output[i];
        squared += delta * delta;
        ++count;
      }
      const double rmse = std::sqrt(squared / count);
      std::cout << std::setprecision(8) << "guided_cpu_metal_rgb_rmse=" << rmse
                << " cpu_range=[" << cpuMinimum << ',' << cpuMaximum << ']'
                << " metal_range=[" << metalMinimum << ',' << metalMaximum << "]\n";
      if (!std::isfinite(rmse)) return 3;

      // Isolate the guided reconstruction itself from the separately approximated
      // structure-guide blur. This deterministic fixture is the CPU/Metal parity gate.
      auto parityParams = request.params;
      parityParams.boundaryPreserve = 0.0f;
      request.params = parityParams;
      if (!instance.render(request)) return 4;
      p = parityParams;
      p.backend = pigment::DetailCollapseBackend::GuidedCpu;
      pigment::processDetailCollapseResearch(
          {source.data(), o.width * 4, bounds, 4},
          {cpu.data(), o.width * 4, bounds, 4}, bounds, p, {}, nullptr);
      pigment::MatrixOpponentTransform transform(p.gamut);
      double normalizedSquared = 0.0;
      count = 0;
      for (int y = 0; y < o.height; ++y) for (int x = 0; x < o.width; ++x) {
        const std::size_t i = static_cast<std::size_t>(y * o.width + x) * 4;
        const auto cy = transform.toYab({cpu[i], cpu[i + 1], cpu[i + 2]});
        const auto my = transform.toYab({output[i], output[i + 1], output[i + 2]});
        for (const double delta : {
                 (cy.y - my.y) / std::max(0.01f, p.toneSimilarity),
                 (cy.a - my.a) / std::max(0.01f, p.chromaSimilarity),
                 (cy.b - my.b) / std::max(0.01f, p.chromaSimilarity)}) {
          normalizedSquared += delta * delta;
          ++count;
        }
      }
      const double normalizedRmse = std::sqrt(normalizedSquared / count);
      std::cout << "guided_cpu_metal_normalized_yab_rmse=" << normalizedRmse << '\n';
      if (normalizedRmse > 2.0e-4) return 5;

      const int sw = 37, sh = 23, rowFloats = sw * 4 + 7;
      const pigment::RectI stagedBounds{-4, 7, -4 + sw, 7 + sh};
      std::vector<float> stagedSource(static_cast<std::size_t>(rowFloats * sh), -99.0f);
      std::vector<float> stagedOutput(stagedSource.size(), -77.0f);
      for (int y = 0; y < sh; ++y) for (int x = 0; x < sw; ++x) {
        const std::size_t i = static_cast<std::size_t>(y * rowFloats + x * 4);
        const float alpha = 0.2f + 0.8f * static_cast<float>((x + y) % 11) / 10.0f;
        stagedSource[i] = alpha * (0.1f * x - 0.2f);
        stagedSource[i + 1] = alpha * (0.04f * y + 0.1f);
        stagedSource[i + 2] = alpha * (((x + y) % 9 == 0) ? 3.0f : -0.05f);
        stagedSource[i + 3] = alpha;
      }
      pigment::metal::MetalExecutionRequest staged;
      staged.source = {stagedSource.data(), stagedSource.size() * sizeof(float),
                       rowFloats * static_cast<int>(sizeof(float)), stagedBounds, 4};
      staged.destination = {stagedOutput.data(), stagedOutput.size() * sizeof(float),
                       rowFloats * static_cast<int>(sizeof(float)), stagedBounds, 4};
      staged.renderWindow = stagedBounds;
      staged.params = parameters(5.0f);
      staged.params.premultiplied = true;
      if (!instance.render(staged) ||
          instance.diagnostics().path != pigment::metal::MetalPath::CpuStaging)
        return 6;
      for (int y = 0; y < sh; ++y) for (int x = 0; x < sw; ++x) {
        const std::size_t i = static_cast<std::size_t>(y * rowFloats + x * 4);
        if (stagedOutput[i + 3] != stagedSource[i + 3] || !std::isfinite(stagedOutput[i]))
          return 7;
      }
      std::cout << "staged_strided_premultiplied_rgba=pass\n";

      std::fill(output.begin(), output.end(), 0.0f);
      pigment::metal::IntegratedMetalExecutionRequest integrated;
      integrated.source = request.source;
      integrated.destination = request.destination;
      integrated.renderWindow = bounds;
      integrated.params = integratedParameters(o.massScale);
      if (!instance.renderIntegrated(integrated)) {
        std::cerr << "Integrated Metal failure: " << instance.diagnostics().message << '\n';
        return 8;
      }
      bool changed = false;
      for (std::size_t i = 0; i < output.size(); ++i) {
        if (!std::isfinite(output[i])) return 9;
        if ((i & 3U) == 3U) {
          if (output[i] != source[i]) return 10;
        } else if (std::abs(output[i] - source[i]) > 1.0e-6f) {
          changed = true;
        }
      }
      if (!changed) return 11;
      integrated.params.amount = 0.0f;
      if (!instance.renderIntegrated(integrated)) return 12;
      if (output != source) return 13;
      std::cout << "integrated_metal_alpha_hdr_identity=pass\n";
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
