#include "core/ColorSpace.h"
#include "core/Masking.h"
#include "core/RollingYabMass.h"
#include "core/ScratchArena.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct HarnessOptions {
  float massScale = 4.0f;
  float structureScale = 5.0f;
  std::filesystem::path outputDirectory;
};

HarnessOptions parseOptions(int argc, char** argv) {
  HarnessOptions options;
  for (int i = 1; i < argc; ++i) {
    const std::string argument(argv[i]);
    const auto requireValue = [&](const char* name) -> std::string {
      if (++i >= argc) throw std::invalid_argument(std::string(name) + " requires a value");
      return argv[i];
    };
    if (argument == "--mass-scale") {
      options.massScale = std::stof(requireValue("--mass-scale"));
    } else if (argument == "--structure-scale") {
      options.structureScale = std::stof(requireValue("--structure-scale"));
    } else if (argument == "--output") {
      options.outputDirectory = requireValue("--output");
    } else if (argument == "--help") {
      std::cout << "usage: pigment_mass_research [--mass-scale pixels] "
                   "[--structure-scale pixels] [--output directory]\n";
      std::exit(0);
    } else if (argument.rfind("--", 0) == 0) {
      throw std::invalid_argument("unknown option: " + argument);
    } else {
      options.outputDirectory = argument;  // compatibility with the original harness
    }
  }
  if (options.massScale <= 0.0f || options.structureScale < 0.0f)
    throw std::invalid_argument("scales must be positive (Structure Scale may be zero)");
  return options;
}

void writePfm(const std::filesystem::path& path, pigment::ConstYabPlanes image) {
  pigment::MatrixOpponentTransform transform(pigment::WorkingGamut::ACEScg);
  std::ofstream stream(path, std::ios::binary);
  if (!stream) throw std::runtime_error("cannot create " + path.string());
  stream << "PF\n" << image.y.bounds.width() << ' ' << image.y.bounds.height()
         << "\n-1.0\n";
  for (int y = image.y.bounds.y2 - 1; y >= image.y.bounds.y1; --y) {
    for (int x = image.y.bounds.x1; x < image.y.bounds.x2; ++x) {
      const auto rgb = transform.toRgb(
          {image.y.at(x, y), image.a.at(x, y), image.b.at(x, y)});
      stream.write(reinterpret_cast<const char*>(rgb.data()),
                   static_cast<std::streamsize>(rgb.size() * sizeof(float)));
    }
  }
}

void writeFieldPfm(const std::filesystem::path& path,
                   pigment::ConstFloatPlaneView field) {
  std::ofstream stream(path, std::ios::binary);
  if (!stream) throw std::runtime_error("cannot create " + path.string());
  stream << "Pf\n" << field.bounds.width() << ' ' << field.bounds.height()
         << "\n-1.0\n";
  for (int y = field.bounds.y2 - 1; y >= field.bounds.y1; --y)
    for (int x = field.bounds.x1; x < field.bounds.x2; ++x) {
      const float value = field.at(x, y);
      stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const HarnessOptions harness = parseOptions(argc, argv);
    const pigment::RectI bounds{0, 0, 96, 64};
    pigment::OwnedYabPlanes source(bounds), result(bounds);
    auto image = source.view();
    for (int y = 0; y < bounds.y2; ++y) {
      for (int x = 0; x < bounds.x2; ++x) {
        const bool rightMass = x >= 48;
        const float broad = rightMass ? 0.62f : 0.20f;
        const bool microSpecular = ((x * 13 + y * 7) % 29) == 0;
        image.y.at(x, y) = broad + (microSpecular ? 2.5f : 0.025f * std::sin(0.31f * x));
        image.a.at(x, y) = (rightMass ? 0.07f : -0.04f) +
                           (microSpecular ? -0.12f : 0.015f * std::sin(0.27f * y));
        image.b.at(x, y) = rightMass ? -0.05f : 0.035f;
      }
    }

    pigment::OwnedPlane protection(bounds, 1.0f);
    pigment::StructureBoundaryOptions boundaryOptions;
    boundaryOptions.structureScale = harness.structureScale;
    boundaryOptions.protection = 0.95f;
    boundaryOptions.softness = 0.08f;
    boundaryOptions.axisAWeight = 0.5f;
    boundaryOptions.axisBWeight = 0.5f;
    pigment::buildStructureBoundaryField(
        pigment::asConst(image), protection.view(), boundaryOptions);

    pigment::detail::RollingYabMassOptions options;
    options.massScale = harness.massScale;
    options.massStrength = 0.65f;
    options.internalVariation = 0.15f;
    options.similarity.luminanceScale = 0.35f;
    options.similarity.chromaScale = 0.12f;
    pigment::detail::RollingYabMassOperator rolling(options);
    pigment::ScratchArena scratch;
    pigment::ExecutionContext execution;
    execution.scratch = &scratch;
    const auto protectionView = static_cast<const pigment::OwnedPlane&>(protection).view();
    rolling.apply({static_cast<const pigment::OwnedYabPlanes&>(source).view(),
                   result.view(), pigment::ScalarFieldView(1.0f),
                   pigment::ScalarFieldView(protectionView), bounds, {}}, execution);

    double sourceFineEnergy = 0.0;
    double resultFineEnergy = 0.0;
    const auto output = static_cast<const pigment::OwnedYabPlanes&>(result).view();
    for (int y = 1; y < bounds.y2 - 1; ++y) {
      for (int x = 1; x < bounds.x2 - 1; ++x) {
        if (std::abs(x - 48) < 8) continue;
        sourceFineEnergy += std::abs(image.y.at(x + 1, y) - image.y.at(x, y));
        resultFineEnergy += std::abs(output.y.at(x + 1, y) - output.y.at(x, y));
      }
    }

    std::cout << "source fine energy: " << sourceFineEnergy << '\n'
              << "result fine energy: " << resultFineEnergy << '\n'
              << "Mass Scale: " << harness.massScale << '\n'
              << "Structure Scale: " << harness.structureScale << '\n'
              << "boundary permeability at silhouette: "
              << protectionView.at(47, 32) << '\n'
              << "boundary permeability at small specular detail: "
              << protectionView.at(6, 16) << '\n';

    if (!harness.outputDirectory.empty()) {
      const std::filesystem::path directory(harness.outputDirectory);
      std::filesystem::create_directories(directory);
      writePfm(directory / "rolling-yab-source.pfm",
               static_cast<const pigment::OwnedYabPlanes&>(source).view());
      writePfm(directory / "rolling-yab-result.pfm", output);
      writeFieldPfm(directory / "rolling-yab-boundary-permeability.pfm",
                    protectionView);
      std::cout << "wrote PFM research images to " << directory << '\n';
    }
    return resultFineEnergy < sourceFineEnergy ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 2;
  }
}
