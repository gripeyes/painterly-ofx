#include "core/ColorSpace.h"
#include "core/LatentPlateGraph.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>

namespace {
struct RgbImage { int width=0,height=0; std::vector<unsigned char> pixels; };

std::string token(std::istream& input) {
  std::string value;
  while (input >> value) {
    if (!value.empty() && value[0] == '#') { std::string discard; std::getline(input,discard); continue; }
    return value;
  }
  throw std::runtime_error("Unexpected end of PPM header");
}

RgbImage readPpm(const std::string& path) {
  std::ifstream input(path,std::ios::binary); if(!input)throw std::runtime_error("Cannot open "+path);
  if(token(input)!="P6")throw std::runtime_error("Gate harness expects binary PPM (P6)");
  RgbImage image;image.width=std::stoi(token(input));image.height=std::stoi(token(input));
  if(std::stoi(token(input))!=255)throw std::runtime_error("Only 8-bit PPM is supported");
  input.get();image.pixels.resize(size_t(image.width)*image.height*3);
  input.read(reinterpret_cast<char*>(image.pixels.data()),std::streamsize(image.pixels.size()));
  if(!input)throw std::runtime_error("Truncated PPM");return image;
}

void writePgm(const std::filesystem::path& path, pigment::ConstFloatPlaneView field,
              bool signedField=false) {
  std::ofstream output(path,std::ios::binary);const auto b=field.bounds;
  output<<"P5\n"<<b.width()<<' '<<b.height()<<"\n255\n";
  float scale=1.0f;if(signedField){float maximum=0;for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x)maximum=std::max(maximum,std::abs(field.at(x,y)));scale=.5f/std::max(1e-8f,maximum);}
  for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){float value=signedField?.5f+scale*field.at(x,y):field.at(x,y);unsigned char byte=static_cast<unsigned char>(std::lround(255*std::max(0.0f,std::min(1.0f,value))));output.write(reinterpret_cast<const char*>(&byte),1);}
}

void writeComposite(const std::filesystem::path& path,const pigment::PublicPlateSet& plates){auto b=plates.bounds();std::ofstream output(path,std::ios::binary);output<<"P6\n"<<b.width()<<' '<<b.height()<<"\n255\n";for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){float rgb[3]{};for(int i=0;i<plates.count();++i){float v=plates.alpha(i).at(x,y);rgb[0]+=v*float((i*97+29)%255)/255;rgb[1]+=v*float((i*57+83)%255)/255;rgb[2]+=v*float((i*131+17)%255)/255;}unsigned char bytes[3];for(int c=0;c<3;++c)bytes[c]=static_cast<unsigned char>(std::lround(255*std::max(0.0f,std::min(1.0f,rgb[c]))));output.write(reinterpret_cast<const char*>(bytes),3);}}

void writeAppearance(const std::filesystem::path& path,pigment::ConstYabPlanes appearance,
                     pigment::ConstFloatPlaneView alpha,
                     const pigment::MatrixOpponentTransform& transform) {
  const auto b=appearance.y.bounds;std::ofstream output(path,std::ios::binary);
  output<<"P6\n"<<b.width()<<' '<<b.height()<<"\n255\n";
  for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){auto rgb=transform.toRgb({appearance.y.at(x,y),appearance.a.at(x,y),appearance.b.at(x,y)});float weight=std::sqrt(std::max(0.0f,alpha.at(x,y)));unsigned char bytes[3];for(int c=0;c<3;++c){float shown=.18f*(1-weight)+weight*rgb[c];bytes[c]=static_cast<unsigned char>(std::lround(255*std::max(0.0f,std::min(1.0f,shown))));}output.write(reinterpret_cast<const char*>(bytes),3);}
}
}

int main(int argc,char**argv){try{if(argc<3){std::cerr<<"Usage: pigment_phase4_gate <input.ppm> <output-dir> [latent-count] [plate-count]\n";return 2;}auto input=readPpm(argv[1]);std::filesystem::path outputDir=argv[2];std::filesystem::create_directories(outputDir);pigment::RectI bounds{0,0,input.width,input.height};pigment::OwnedYabPlanes yab(bounds);pigment::MatrixOpponentTransform transform(pigment::WorkingGamut::ACEScg);auto view=yab.view();for(int y=0;y<input.height;++y)for(int x=0;x<input.width;++x){size_t index=(size_t(y)*input.width+x)*3;std::array<float,3>rgb{input.pixels[index]/255.0f,input.pixels[index+1]/255.0f,input.pixels[index+2]/255.0f};auto value=transform.toYab(rgb);view.y.at(x,y)=value.y;view.a.at(x,y)=value.a;view.b.at(x,y)=value.b;}pigment::Phase4Params params;params.latentCount=argc>3?std::stoi(argv[3]):16;params.plateCount=argc>4?std::stoi(argv[4]):6;params.lumaChunkScale=0;params.chromaChunkScale=0;params.spillAmount=0;auto result=pigment::buildPhase4AutomaticPlates(static_cast<const pigment::OwnedYabPlanes&>(yab).view(),params,{});const auto&constant=result;for(int i=0;i<constant.latent.spectralModeCount();++i){std::ostringstream name;name<<"eigen-"<<std::setw(2)<<std::setfill('0')<<i<<".pgm";writePgm(outputDir/name.str(),constant.latent.spectralMode(i),true);}for(int i=0;i<constant.latent.count();++i){std::ostringstream name;name<<"latent-"<<std::setw(2)<<std::setfill('0')<<i<<".pgm";writePgm(outputDir/name.str(),constant.latent.alpha(i));std::ostringstream appearanceName;appearanceName<<"latent-appearance-"<<std::setw(2)<<std::setfill('0')<<i<<".ppm";writeAppearance(outputDir/appearanceName.str(),constant.latent.appearance(i),constant.latent.alpha(i),transform);}for(int i=0;i<constant.plates.count();++i){std::ostringstream name;name<<"plate-"<<char('A'+i)<<"-alpha.pgm";writePgm(outputDir/name.str(),constant.plates.alpha(i));std::ostringstream appearanceName;appearanceName<<"plate-"<<char('A'+i)<<"-appearance.ppm";writeAppearance(outputDir/appearanceName.str(),constant.plates.appearance(i),constant.plates.alpha(i),transform);}writeComposite(outputDir/"plate-composite.ppm",constant.plates);writePgm(outputDir/"latent-reconstruction-error.pgm",constant.latent.reconstructionError());std::cout<<std::setprecision(8)<<"PHASE4_A1 max_eigen_residual="<<constant.diagnostics.maximumEigenResidual<<" mean_eigen_residual="<<constant.diagnostics.meanEigenResidual<<" max_mode_correlation="<<constant.diagnostics.maximumEigenmodeCorrelation<<'\n'<<"PHASE4_A2 effective_components="<<constant.diagnostics.meanEffectiveComponents<<" hard_fraction="<<constant.diagnostics.hardPixelFraction<<" projection_error="<<constant.diagnostics.componentProjectionError<<'\n'<<"PHASE4_A3 unmixing_rmse="<<constant.diagnostics.appearanceUnmixingError<<'\n'<<"PHASE4_OUTPUT "<<outputDir<<'\n';return 0;}catch(const std::exception&error){std::cerr<<error.what()<<'\n';return 1;}}
