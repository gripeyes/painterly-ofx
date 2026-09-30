#include "core/ChunkGradientSynthesis.h"
#include "core/ColorSpace.h"
#include "core/LatentPlateGraph.h"
#include "core/PlateSpill.h"
#include "core/RegionHierarchy.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct RgbImage {
  int width = 0, height = 0;
  std::vector<unsigned char> pixels;
};

std::string token(std::istream &input) {
  std::string value;
  while (input >> value) {
    if (!value.empty() && value[0] == '#') {
      std::string discard;
      std::getline(input, discard);
      continue;
    }
    return value;
  }
  throw std::runtime_error("Unexpected end of PPM header");
}

RgbImage readPpm(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input)
    throw std::runtime_error("Cannot open " + path);
  if (token(input) != "P6")
    throw std::runtime_error("Gate harness expects binary PPM (P6)");
  RgbImage image;
  image.width = std::stoi(token(input));
  image.height = std::stoi(token(input));
  if (std::stoi(token(input)) != 255)
    throw std::runtime_error("Only 8-bit PPM is supported");
  input.get();
  image.pixels.resize(size_t(image.width) * image.height * 3);
  input.read(reinterpret_cast<char *>(image.pixels.data()),
             std::streamsize(image.pixels.size()));
  if (!input)
    throw std::runtime_error("Truncated PPM");
  return image;
}

void writePgm(const std::filesystem::path &path,
              pigment::ConstFloatPlaneView field, bool signedField = false,
              bool normalizeField = false) {
  std::ofstream output(path, std::ios::binary);
  const auto b = field.bounds;
  output << "P5\n" << b.width() << ' ' << b.height() << "\n255\n";
  float scale = 1.0f;
  if (signedField || normalizeField) {
    float maximum = 0;
    for (int y = b.y1; y < b.y2; ++y)
      for (int x = b.x1; x < b.x2; ++x)
        maximum = std::max(maximum, std::abs(field.at(x, y)));
    scale = (signedField?.5f:1.0f) / std::max(1e-8f, maximum);
  }
  for (int y = b.y1; y < b.y2; ++y)
    for (int x = b.x1; x < b.x2; ++x) {
      float value = (signedField?.5f:0.0f) + scale * field.at(x, y);
      unsigned char byte = static_cast<unsigned char>(
          std::lround(255 * std::max(0.0f, std::min(1.0f, value))));
      output.write(reinterpret_cast<const char *>(&byte), 1);
    }
}

void writeComposite(const std::filesystem::path &path,
                    const pigment::PublicPlateSet &plates) {
  auto b = plates.bounds();
  std::ofstream output(path, std::ios::binary);
  output << "P6\n" << b.width() << ' ' << b.height() << "\n255\n";
  for (int y = b.y1; y < b.y2; ++y)
    for (int x = b.x1; x < b.x2; ++x) {
      float rgb[3]{};
      for (int i = 0; i < plates.count(); ++i) {
        float v = plates.alpha(i).at(x, y);
        rgb[0] += v * float((i * 97 + 29) % 255) / 255;
        rgb[1] += v * float((i * 57 + 83) % 255) / 255;
        rgb[2] += v * float((i * 131 + 17) % 255) / 255;
      }
      unsigned char bytes[3];
      for (int c = 0; c < 3; ++c)
        bytes[c] = static_cast<unsigned char>(
            std::lround(255 * std::max(0.0f, std::min(1.0f, rgb[c]))));
      output.write(reinterpret_cast<const char *>(bytes), 3);
    }
}

void writeAppearance(const std::filesystem::path &path,
                     pigment::ConstYabPlanes appearance,
                     pigment::ConstFloatPlaneView alpha,
                     const pigment::MatrixOpponentTransform &transform) {
  const auto b = appearance.y.bounds;
  std::ofstream output(path, std::ios::binary);
  output << "P6\n" << b.width() << ' ' << b.height() << "\n255\n";
  for (int y = b.y1; y < b.y2; ++y)
    for (int x = b.x1; x < b.x2; ++x) {
      auto rgb = transform.toRgb({appearance.y.at(x, y), appearance.a.at(x, y),
                                  appearance.b.at(x, y)});
      float weight = std::sqrt(std::max(0.0f, alpha.at(x, y)));
      unsigned char bytes[3];
      for (int c = 0; c < 3; ++c) {
        float shown = .18f * (1 - weight) + weight * rgb[c];
        bytes[c] = static_cast<unsigned char>(
            std::lround(255 * std::max(0.0f, std::min(1.0f, shown))));
      }
      output.write(reinterpret_cast<const char *>(bytes), 3);
    }
}

// Float diagnostics expose cancellation and HDR/negative layer values that
// a display-clipped contact sheet cannot reveal. Channels are Y/A/B, not RGB.
void writeYabPfm(const std::filesystem::path &path, pigment::ConstYabPlanes value) {
  std::ofstream output(path, std::ios::binary);
  auto b=value.y.bounds;
  output << "PF\n" << b.width() << ' ' << b.height() << "\n-1.0\n";
  for(int y=b.y2-1;y>=b.y1;--y) for(int x=b.x1;x<b.x2;++x) {
    float pixel[3]{value.y.at(x,y),value.a.at(x,y),value.b.at(x,y)};
    output.write(reinterpret_cast<const char*>(pixel),sizeof(pixel));
  }
}

void writePlateReconstruction(
    const std::filesystem::path &path, const pigment::PublicPlateSet &plates,
    const pigment::MatrixOpponentTransform &transform) {
  const auto b = plates.bounds();
  std::ofstream output(path, std::ios::binary);
  output << "P6\n" << b.width() << ' ' << b.height() << "\n255\n";
  for (int y = b.y1; y < b.y2; ++y)
    for (int x = b.x1; x < b.x2; ++x) {
      pigment::YabPixel value{};
      for (int i = 0; i < plates.count(); ++i) {
        const float weight = plates.alpha(i).at(x, y);
        auto appearance = plates.appearance(i);
        value.y += weight * appearance.y.at(x, y);
        value.a += weight * appearance.a.at(x, y);
        value.b += weight * appearance.b.at(x, y);
      }
      auto rgb = transform.toRgb(value);
      unsigned char bytes[3];
      for (int channel = 0; channel < 3; ++channel)
        bytes[channel] = static_cast<unsigned char>(
            std::lround(255 * std::max(0.0f, std::min(1.0f, rgb[channel]))));
      output.write(reinterpret_cast<const char *>(bytes), 3);
    }
}

void writeLabels(const std::filesystem::path &path,
                 const std::vector<int> &labels, pigment::RectI bounds) {
  std::ofstream output(path, std::ios::binary);
  output << "P6\n" << bounds.width() << ' ' << bounds.height() << "\n255\n";
  for (int y = bounds.y1; y < bounds.y2; ++y)
    for (int x = bounds.x1; x < bounds.x2; ++x) {
      int label =
          labels[size_t(y - bounds.y1) * bounds.width() + x - bounds.x1];
      unsigned value = unsigned(label + 1) * 0x9e3779b9u;
      value ^= value >> 16;
      unsigned char bytes[3]{static_cast<unsigned char>(value & 255u),
                             static_cast<unsigned char>((value >> 8) & 255u),
                             static_cast<unsigned char>((value >> 16) & 255u)};
      output.write(reinterpret_cast<const char *>(bytes), 3);
    }
}

void writeComponentDiagnostics(
    const std::filesystem::path &directory,
    const pigment::Phase4GateDiagnostics &diagnostics) {
  std::ofstream occupancy(directory / "component-occupancy.csv");
  occupancy << "component,occupancy,matting_energy\n";
  for (size_t i = 0; i < diagnostics.componentOccupancy.size(); ++i)
    occupancy << i << ',' << diagnostics.componentOccupancy[i] << ','
              << diagnostics.componentMattingEnergy[i] << '\n';
  std::ofstream correlation(directory / "component-correlation.csv");
  const int count = diagnostics.activeLatentCount;
  for (int i = 0; i < count; ++i) {
    for (int j = 0; j < count; ++j) {
      if (j)
        correlation << ',';
      correlation << diagnostics.componentCorrelation[size_t(i) * count + j];
    }
    correlation << '\n';
  }
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc < 3) {
      std::cerr << "Usage: pigment_phase4_gate <input.ppm> <output-dir> "
                   "[latent-count] [plate-count] [y-chunk] [ab-chunk] "
                   "[gradient-complexity] [spill] [reach] [luma-spill] "
                   "[chroma-spill] [artistic-donor] [donor-a-bias] "
                   "[donor-b-bias] [donor-weight]\n";
      return 2;
    }
    auto input = readPpm(argv[1]);
    std::filesystem::path outputDir = argv[2];
    std::filesystem::create_directories(outputDir);
    pigment::RectI bounds{0, 0, input.width, input.height};
    pigment::OwnedYabPlanes yab(bounds);
    pigment::MatrixOpponentTransform transform(pigment::WorkingGamut::ACEScg);
    auto view = yab.view();
    for (int y = 0; y < input.height; ++y)
      for (int x = 0; x < input.width; ++x) {
        size_t index = (size_t(y) * input.width + x) * 3;
        std::array<float, 3> rgb{input.pixels[index] / 255.0f,
                                 input.pixels[index + 1] / 255.0f,
                                 input.pixels[index + 2] / 255.0f};
        auto value = transform.toYab(rgb);
        view.y.at(x, y) = value.y;
        view.a.at(x, y) = value.a;
        view.b.at(x, y) = value.b;
      }
    pigment::Phase4Params params;
    params.latentCount = argc > 3 ? std::stoi(argv[3]) : 16;
    params.plateCount = argc > 4 ? std::stoi(argv[4]) : 6;
    params.lumaChunkScale = argc > 5 ? std::stof(argv[5]) : 0.0f;
    params.chromaChunkScale = argc > 6 ? std::stof(argv[6]) : 0.0f;
    params.gradientComplexity = argc > 7 ? std::stof(argv[7]) : .35f;
    params.spillAmount = argc > 8 ? std::stof(argv[8]) : 0.0f;
    params.spillReach = argc > 9 ? std::stof(argv[9]) : 48.0f;
    params.lumaSpill = argc > 10 ? std::stof(argv[10]) : .15f;
    params.chromaSpill = argc > 11 ? std::stof(argv[11]) : .75f;
    if(std::string(argv[argc-1])=="--hierarchy-sweep-strict")
      params.internalVariation=0;
    if(std::string(argv[argc-1])=="--tone-check")
      params.plates[0].tone=.05f;
    if (argc > 12 && std::string(argv[12]).rfind("--",0)!=0) {
      int donor = std::max(0, std::min(7, std::stoi(argv[12])));
      params.plates[size_t(donor)].biasA =
          argc > 13 ? std::stof(argv[13]) : 0.0f;
      params.plates[size_t(donor)].biasB =
          argc > 14 ? std::stof(argv[14]) : 0.0f;
      params.plates[size_t(donor)].weight =
          argc > 15 ? std::stof(argv[15]) : 1.0f;
    }
    auto result = pigment::buildPhase4AutomaticPlates(
        static_cast<const pigment::OwnedYabPlanes &>(yab).view(), params, {});
    const auto &constant = result;
    for (int i = 0; i < constant.latent.spectralModeCount(); ++i) {
      std::ostringstream name;
      name << "eigen-" << std::setw(2) << std::setfill('0') << i << ".pgm";
      writePgm(outputDir / name.str(), constant.latent.spectralMode(i), true);
    }
    for (int i = 0; i < constant.latent.count(); ++i) {
      std::ostringstream name;
      name << "latent-" << std::setw(2) << std::setfill('0') << i << ".pgm";
      writePgm(outputDir / name.str(), constant.latent.alpha(i));
      std::ostringstream appearanceName;
      appearanceName << "latent-appearance-" << std::setw(2)
                     << std::setfill('0') << i << ".ppm";
      writeAppearance(outputDir / appearanceName.str(),
                      constant.latent.appearance(i), constant.latent.alpha(i),
                      transform);
    }
    for (int i = 0; i < constant.plates.count(); ++i) {
      std::ostringstream name;
      name << "plate-" << char('A' + i) << "-alpha.pgm";
      writePgm(outputDir / name.str(), constant.plates.alpha(i));
      std::ostringstream ySupportName, abSupportName;
      ySupportName << "plate-" << char('A' + i) << "-support-y.pgm";
      abSupportName << "plate-" << char('A' + i) << "-support-ab.pgm";
      writePgm(outputDir / ySupportName.str(), constant.plates.supportY(i));
      writePgm(outputDir / abSupportName.str(), constant.plates.supportAB(i));
      std::ostringstream appearanceName;
      appearanceName << "plate-" << char('A' + i) << "-appearance.ppm";
      writeAppearance(outputDir / appearanceName.str(),
                      constant.plates.appearance(i), constant.plates.alpha(i),
                      transform);
      writeYabPfm(outputDir / (std::string("plate-")+char('A'+i)+"-appearance-yab.pfm"),
                  constant.plates.appearance(i));
    }
    writeComposite(outputDir / "plate-composite.ppm", constant.plates);
    writePlateReconstruction(outputDir / "gate-a-reconstruction.ppm",
                             constant.plates, transform);
    writePgm(outputDir / "latent-reconstruction-error.pgm",
             constant.latent.reconstructionError());
    writeComponentDiagnostics(outputDir, constant.diagnostics);
    writeYabPfm(outputDir / "source-yab.pfm",
                static_cast<const pigment::OwnedYabPlanes &>(yab).view());
    {
      std::ofstream report(outputDir/"gate-a.csv");
      const auto &d=constant.diagnostics;
      report << "eigen_residual,mode_correlation,requested,active,effective_rank,"
                "analysis_unmixing_rmse,full_resolution_rmse,public_analysis_rmse,public_rank\n"
             << std::setprecision(10) << d.maximumEigenResidual << ','
             << d.maximumEigenmodeCorrelation << ',' << d.requestedLatentCount
             << ',' << d.activeLatentCount << ',' << d.componentEffectiveRank
             << ',' << d.appearanceUnmixingError << ','
             << d.fullResolutionReconstructionError << ','
             << d.publicReconstructionError << ',' << d.publicPlateEffectiveRank << '\n';
    }
    if(std::string(argv[argc-1])=="--gate-a-only") {
      std::cout << "PHASE4_GATE_A_DIAGNOSTICS output=" << outputDir
                << " full_resolution_rmse="
                << constant.diagnostics.fullResolutionReconstructionError << '\n';
      return 0;
    }
    auto hierarchy = pigment::buildPhase4RegionHierarchy(
        static_cast<const pigment::OwnedYabPlanes &>(yab).view(),
        constant.plates, params);
    writePgm(
        outputDir / "source-boundary-strength.pgm",
        static_cast<const pigment::OwnedPlane &>(hierarchy.boundaryStrength)
            .view());
    writePgm(
        outputDir / "atomic-regions.pgm",
        static_cast<const pigment::OwnedPlane &>(hierarchy.atomicRegionDisplay)
            .view());
    for (int plate = 0; plate < constant.plates.count(); ++plate) {
      const auto &plateHierarchy = hierarchy.plates[size_t(plate)];
      std::string prefix = std::string("plate-") + char('A' + plate);
      writeLabels(outputDir / (prefix + "-y-chunks.ppm"), plateHierarchy.yChunk,
                  hierarchy.bounds);
      writeLabels(outputDir / (prefix + "-ab-chunks.ppm"),
                  plateHierarchy.abChunk, hierarchy.bounds);
      writePgm(outputDir / (prefix + "-y-removed.pgm"),
               plateHierarchy.yRemovedBoundaries.view());
      writePgm(outputDir / (prefix + "-y-retained.pgm"),
               plateHierarchy.yRetainedBoundaries.view());
      writePgm(outputDir / (prefix + "-ab-removed.pgm"),
               plateHierarchy.abRemovedBoundaries.view());
      writePgm(outputDir / (prefix + "-ab-retained.pgm"),
               plateHierarchy.abRetainedBoundaries.view());
    }
    if (std::string(argv[argc - 1]).rfind("--hierarchy-sweep",0)==0) {
      // Reuse one frozen automatic solution; do not rerun spectral extraction
      // just to evaluate cuts of the plate-conditioned region trees.
      std::ofstream statistics(outputDir / "hierarchy-sweep.csv");
      statistics << "scale,plate,atomic_regions,y_chunks,ab_chunks,removed_y_edges,removed_ab_edges\n";
      for(float scale : {0.0f,1.0f,8.0f,24.0f,64.0f,128.0f}) {
        auto cutParams=params;
        cutParams.lumaChunkScale=cutParams.chromaChunkScale=scale;
        auto cut=pigment::buildPhase4RegionHierarchy(
            static_cast<const pigment::OwnedYabPlanes &>(yab).view(),
            constant.plates,cutParams);
        auto directory=outputDir/("scale-"+std::to_string(int(scale)));
        std::filesystem::create_directories(directory);
        for(int plate=0;plate<constant.plates.count();++plate) {
          const auto &h=cut.plates[size_t(plate)];
          std::string prefix=std::string("plate-")+char('A'+plate);
          writeLabels(directory/(prefix+"-y-chunks.ppm"),h.yChunk,cut.bounds);
          writeLabels(directory/(prefix+"-ab-chunks.ppm"),h.abChunk,cut.bounds);
          writePgm(directory/(prefix+"-y-retained.pgm"),h.yRetainedBoundaries.view());
          writePgm(directory/(prefix+"-ab-retained.pgm"),h.abRetainedBoundaries.view());
          size_t removedY=0,removedAB=0;
          for(int y=0;y<cut.bounds.height();++y) for(int x=0;x<cut.bounds.width();++x) {
            size_t p=size_t(y)*cut.bounds.width()+x;
            for(size_t q : {x+1<cut.bounds.width()?p+1:p,
                            y+1<cut.bounds.height()?p+cut.bounds.width():p})
              if(q!=p && cut.atomicRegion[p]!=cut.atomicRegion[q]) {
                removedY+=h.yChunk[p]==h.yChunk[q];
                removedAB+=h.abChunk[p]==h.abChunk[q];
              }
          }
          statistics << scale << ',' << plate << ',' << cut.atomicRegionCount << ','
                     << h.yChunkCount << ',' << h.abChunkCount << ','
                     << removedY << ',' << removedAB << '\n';
        }
      }
      std::cout << "PHASE4_GATE_B_SWEEP output=" << outputDir << '\n';
      return 0;
    }
    if (std::string(argv[argc - 1]) == "--hierarchy-only") {
      std::ofstream statistics(outputDir / "hierarchy.csv");
      statistics << "plate,atomic_regions,y_chunks,ab_chunks,y_nodes,ab_nodes\n";
      for (size_t i = 0; i < hierarchy.plates.size(); ++i) {
        const auto &h = hierarchy.plates[i];
        statistics << i << ',' << hierarchy.atomicRegionCount << ','
                   << h.yChunkCount << ',' << h.abChunkCount << ','
                   << h.yTree.size() << ',' << h.abTree.size() << '\n';
      }
      std::cout << "PHASE4_GATE_B atomic_regions=" << hierarchy.atomicRegionCount
                << " plate_A_y_chunks=" << hierarchy.plates.front().yChunkCount
                << " plate_A_ab_chunks=" << hierarchy.plates.front().abChunkCount
                << " output=" << outputDir << '\n';
      return 0;
    }
    pigment::Phase4BroadFormOptions broadForm;
    const auto experiment=std::string(argv[argc-1]);
    broadForm.enabled=experiment=="--interior-experiment" || experiment=="--interior-strong";
    if(experiment=="--interior-strong") {
      broadForm.strengthY*=4; broadForm.strengthAB*=4;
    }
    if(std::string(argv[argc-1])=="--no-interior") broadForm.enabled=false;
    if(broadForm.enabled) {
      auto baselineOptions=broadForm;baselineOptions.enabled=false;
      auto baseline=pigment::synthesizePhase4Chunks(
          static_cast<const pigment::OwnedYabPlanes &>(yab).view(),
          constant.plates,hierarchy,params,{},baselineOptions);
      writeYabPfm(outputDir/"no-interior-yab.pfm",
                 static_cast<const pigment::OwnedYabPlanes &>(baseline.preSpill).view());
      pigment::OwnedPlane alpha(bounds,1);
      writeAppearance(outputDir/"no-interior.ppm",
                      static_cast<const pigment::OwnedYabPlanes &>(baseline.preSpill).view(),
                      static_cast<const pigment::OwnedPlane &>(alpha).view(),transform);
    }
    auto synthesis = pigment::synthesizePhase4Chunks(
        static_cast<const pigment::OwnedYabPlanes &>(yab).view(),
        constant.plates, hierarchy, params,{},broadForm);
    writeYabPfm(outputDir / "source-yab.pfm",
                static_cast<const pigment::OwnedYabPlanes &>(yab).view());
    writeYabPfm(outputDir / "synthesized-yab.pfm",
                static_cast<const pigment::OwnedYabPlanes &>(synthesis.preSpill).view());
    std::ofstream poisson(outputDir / "poisson.csv");
    poisson << "plate,channel,iterations,relative_residual,converged,broad_constraints,broad_result_rmse\n";
    bool converged = true;
    for (size_t i = 0; i < synthesis.solver.size(); ++i)
      for (int channel = 0; channel < 3; ++channel) {
        const auto &s = synthesis.solver[i][size_t(channel)];
        poisson << i << ',' << channel << ',' << s.iterations << ','
                << s.relativeResidual << ',' << s.converged << ','
                << s.broadConstraints << ',' << s.broadResultRmse << '\n';
        converged &= s.converged;
      }
    poisson.close();
    for (size_t i = 0; i < synthesis.solver.size(); ++i) {
      std::string prefix = std::string("plate-") + char('A' + i);
      writeYabPfm(outputDir / (prefix+"-synthesized-yab.pfm"),
                  static_cast<const pigment::OwnedYabPlanes &>(synthesis.plateAppearance[i]).view());
      writePgm(outputDir / (prefix + "-primitive.pgm"),
               static_cast<const pigment::OwnedPlane &>(synthesis.primitiveSelection[i]).view());
      writePgm(outputDir / (prefix + "-fit-error.pgm"),
               static_cast<const pigment::OwnedPlane &>(synthesis.fitError[i]).view());
      writePgm(outputDir / (prefix + "-source-gradient.pgm"),
               static_cast<const pigment::OwnedPlane &>(synthesis.sourceGradient[i]).view());
      writePgm(outputDir / (prefix + "-simplified-gradient.pgm"),
               static_cast<const pigment::OwnedPlane &>(synthesis.simplifiedGradient[i]).view());
      const auto &target=synthesis.broadConstraintTargets[i];
      const auto &influence=synthesis.broadConstraintInfluence[i];
      writeYabPfm(outputDir/(prefix+"-broad-target-yab.pfm"),
                  static_cast<const pigment::OwnedYabPlanes &>(target).view());
      writePgm(outputDir/(prefix+"-broad-y-influence.pgm"),
               static_cast<const pigment::OwnedYabPlanes &>(influence).view().y,false,true);
      writePgm(outputDir/(prefix+"-broad-ab-influence.pgm"),
               static_cast<const pigment::OwnedYabPlanes &>(influence).view().a,false,true);
    }
    pigment::OwnedPlane fullAlpha(bounds, 1.0f);
    writeAppearance(outputDir / "synthesized-composite.ppm",
                    static_cast<const pigment::OwnedYabPlanes &>(synthesis.preSpill).view(),
                    static_cast<const pigment::OwnedPlane &>(fullAlpha).view(), transform);
    if (!converged) {
      std::cerr << "Gate C blocked: bounded Poisson residual exceeds 1e-5; diagnostics="
                << outputDir << '\n';
      return 3;
    }
    auto preSpillParams = params;
    preSpillParams.spillAmount = 0.0f;
    auto artisticPreSpill = pigment::applyPhase4Spill(
        static_cast<const pigment::OwnedYabPlanes &>(yab).view(),
        constant.plates, synthesis, constant.analysisGraph, preSpillParams);
    writeAppearance(
        outputDir / "pre-spill.ppm",
        static_cast<const pigment::OwnedYabPlanes &>(artisticPreSpill.composite)
            .view(),
        static_cast<const pigment::OwnedPlane &>(fullAlpha).view(), transform);
    for (int plate = 0; plate < constant.plates.count(); ++plate) {
      std::string name =
          std::string("plate-") + char('A' + plate) + "-synthesized.ppm";
      writeAppearance(outputDir / name,
                      static_cast<const pigment::OwnedYabPlanes &>(
                          synthesis.plateAppearance[size_t(plate)])
                          .view(),
                      constant.plates.alpha(plate), transform);
    }
    auto spill = pigment::applyPhase4Spill(
                                           static_cast<const pigment::OwnedYabPlanes &>(yab).view(),
                                           constant.plates, synthesis,
                                           constant.analysisGraph, params);
    writeAppearance(
        outputDir / "post-spill.ppm",
        static_cast<const pigment::OwnedYabPlanes &>(spill.composite).view(),
        static_cast<const pigment::OwnedPlane &>(fullAlpha).view(), transform);
    for (int plate = 0; plate < constant.plates.count(); ++plate) {
      std::string prefix = std::string("plate-") + char('A' + plate);
      writePgm(outputDir / (prefix + "-spill-influence.pgm"),
               static_cast<const pigment::OwnedPlane &>(
                   spill.influence[size_t(plate)])
                   .view());
    }
    std::cout
        << std::setprecision(8) << "PHASE4_A1 max_eigen_residual="
        << constant.diagnostics.maximumEigenResidual
        << " mean_eigen_residual=" << constant.diagnostics.meanEigenResidual
        << " max_mode_correlation="
        << constant.diagnostics.maximumEigenmodeCorrelation << '\n'
        << "PHASE4_A2 requested_components="
        << constant.diagnostics.requestedLatentCount
        << " active_components=" << constant.diagnostics.activeLatentCount
        << " effective_components="
        << constant.diagnostics.meanEffectiveComponents
        << " effective_rank=" << constant.diagnostics.componentEffectiveRank
        << " entropy=" << constant.diagnostics.meanComponentEntropy
        << " hard_fraction=" << constant.diagnostics.hardPixelFraction
        << " projection_error=" << constant.diagnostics.componentProjectionError
        << '\n'
        << "PHASE4_A3 unmixing_rmse="
        << constant.diagnostics.appearanceUnmixingError
        << " full_resolution_rmse=" << constant.diagnostics.fullResolutionReconstructionError
        << " spatial_variation="
        << constant.diagnostics.appearanceSpatialVariation << '\n'
        << "PHASE4_PUBLIC reconstruction_rmse="
        << constant.diagnostics.publicReconstructionError
        << " effective_rank=" << constant.diagnostics.publicPlateEffectiveRank
        << " max_correlation="
        << constant.diagnostics.maximumPublicPlateCorrelation << '\n'
        << "PHASE4_GATE_B atomic_regions=" << hierarchy.atomicRegionCount
        << " plate_A_y_chunks=" << hierarchy.plates.front().yChunkCount
        << " plate_A_ab_chunks=" << hierarchy.plates.front().abChunkCount
        << " min_y=" << hierarchy.plates.front().yMinimumDisappearance
        << " min_ab=" << hierarchy.plates.front().abMinimumDisappearance << '\n'
        << "PHASE4_OUTPUT " << outputDir << '\n';
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
