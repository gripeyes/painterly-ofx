#include "core/DetailCollapseResearch.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
  int width = 256, height = 256, rounds = 3;
  float massScale = 8.0f;
  pigment::DetailCollapseBackend backend = pigment::DetailCollapseBackend::GuidedCpu;
  for (int i = 1; i < argc; ++i) {
    const std::string key = argv[i];
    const auto value = [&]() -> std::string {
      if (++i >= argc) throw std::runtime_error(key + " requires a value");
      return argv[i];
    };
    if (key == "--width") width = std::stoi(value());
    else if (key == "--height") height = std::stoi(value());
    else if (key == "--rounds") rounds = std::stoi(value());
    else if (key == "--mass-scale") massScale = std::stof(value());
    else if (key == "--backend") {
      const std::string name = value();
      if (name == "reference") backend = pigment::DetailCollapseBackend::ReferenceBilateralCpu;
      else if (name == "guided") backend = pigment::DetailCollapseBackend::GuidedCpu;
      else if (name == "domain") backend = pigment::DetailCollapseBackend::DomainTransformCpu;
      else throw std::runtime_error("unknown backend: " + name);
    } else throw std::runtime_error("unknown option: " + key);
  }
  const pigment::RectI bounds{0, 0, width, height};
  std::vector<float> source(static_cast<std::size_t>(width * height) * 4);
  std::vector<float> output(source.size());
  for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
    const std::size_t i = static_cast<std::size_t>(y * width + x) * 4;
    const float field = x < width / 2 ? 0.08f : 1.3f;
    const float detail = ((x * 17 + y * 29) % 101 == 0) ? 4.0f :
        0.05f * std::sin(0.19f * x) * std::cos(0.13f * y);
    source[i] = field + detail;
    source[i + 1] = field * 0.72f + detail * 0.4f;
    source[i + 2] = field * 0.48f - detail * 0.2f;
    source[i + 3] = 1.0f;
  }
  pigment::DetailCollapseResearchParams p;
  p.backend = backend;
  p.amount = 0.85f;
  p.massScale = massScale;
  p.structureScale = 5.0f;
  p.toneSimilarity = 0.35f;
  p.chromaSimilarity = 0.12f;
  std::vector<double> elapsed;
  double attenuation = 0.0;
  for (int n = 0; n < rounds; ++n) {
    const auto start = std::chrono::steady_clock::now();
    pigment::processDetailCollapseResearch(
        {source.data(), width * 4, bounds, 4},
        {output.data(), width * 4, bounds, 4}, bounds, p, {}, nullptr);
    elapsed.push_back(std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count());
  }
  double inputFine = 0.0, outputFine = 0.0;
  for (int y = 0; y < height; ++y) for (int x = 1; x < width; ++x) {
    if (std::abs(x - width / 2) < 8) continue;
    const std::size_t i = static_cast<std::size_t>(y * width + x) * 4;
    inputFine += std::abs(source[i] - source[i - 4]);
    outputFine += std::abs(output[i] - output[i - 4]);
  }
  attenuation = outputFine / std::max(inputFine, 1.0e-12);
  std::sort(elapsed.begin(), elapsed.end());
  const char* name = backend == pigment::DetailCollapseBackend::GuidedCpu ? "guided-cpu" :
      backend == pigment::DetailCollapseBackend::DomainTransformCpu ? "domain-cpu" : "reference-cpu";
  std::cout << std::fixed << std::setprecision(3) << name << ' ' << width << 'x' << height
            << " mass=" << massScale << " median_ms=" << elapsed[elapsed.size() / 2]
            << " p95_ms=" << elapsed.back() << " fine_energy_ratio=" << attenuation << '\n';
}
