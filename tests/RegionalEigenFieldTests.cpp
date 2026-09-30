#include "core/RegionalEigenField.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

int main() {
  try {
    auto require=[](bool condition,const char *message){if(!condition)throw std::runtime_error(message);};
    pigment::RectI b{-3,5,29,37};pigment::PublicPlateSet plates(b,4);
    pigment::Phase4RegionHierarchy h;h.bounds=b;
    for(int p=0;p<4;++p) {
      h.plates.emplace_back(b);auto &region=h.plates.back();
      region.yChunk.assign(1024,0);region.abChunk.assign(1024,0);
      region.yChunkCount=region.abChunkCount=1;
      for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x) {
        double xx=x-b.x1,yy=y-b.y1;
        double broad=-.2+.01*xx+.02*yy+2*std::sin(M_PI*xx/31)*std::sin(M_PI*yy/31);
        double fine=.03*std::sin(12*M_PI*xx/31)*std::sin(11*M_PI*yy/31);
        auto a=plates.appearance(p);a.y.at(x,y)=float(broad+fine);a.a.at(x,y)=a.b.at(x,y)=0;
        plates.alpha(p).at(x,y)=.25;plates.supportY(p).at(x,y)=plates.supportAB(p).at(x,y)=1;
      }
    }
    auto result=pigment::regionalEigenFieldSweep(plates,h);
    auto repeat=pigment::regionalEigenFieldSweep(plates,h);
    require(result.results.size()==4,"four independent complexity pairs");
    double error=0;
    for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x) {
      auto out=static_cast<const pigment::OwnedYabPlanes &>(result.results[0].appearance[0]).view();
      auto again=static_cast<const pigment::OwnedYabPlanes &>(repeat.results[0].appearance[0]).view();
      double xx=x-b.x1,yy=y-b.y1;
      double broad=-.2+.01*xx+.02*yy+2*std::sin(M_PI*xx/31)*std::sin(M_PI*yy/31);
      error+=std::pow(out.y.at(x,y)-broad,2);
      require(out.y.at(x,y)==again.y.at(x,y),"regional eigenfield deterministic rerun");
      require(out.a.at(x,y)==0 && out.b.at(x,y)==0,"neutral chroma stable");
      if(xx==0 || yy==0 || xx==31 || yy==31)
        require(out.y.at(x,y)==plates.appearance(0).y.at(x,y),"fixed contours exact");
    }
    require(std::sqrt(error/1024)<1e-5,"lowest modes recover curved form while excluding high frequency");
    for(const auto &mode:result.modes)require(mode.residual<1e-6 && mode.eigenvalue>0,"valid Dirichlet eigenmodes");
    // A support hole is a safe unchanged region, not an extrapolated target.
    for(int p=0;p<4;++p)for(int y=15;y<20;++y)for(int x=6;x<11;++x)
      plates.supportY(p).at(x,y)=plates.supportAB(p).at(x,y)=0;
    auto hole=pigment::regionalEigenFieldSweep(plates,h);
    for(const auto &trial:hole.results)for(int p=0;p<4;++p) {
      auto out=trial.appearance[size_t(p)].view();
      for(int y=15;y<20;++y)for(int x=6;x<11;++x)
        require(out.y.at(x,y)==plates.appearance(p).y.at(x,y),"negligible support retains safe appearance");
    }
    // Diagnostic boundary changes appearance, never side geometry or ownership.
    for(int p=0;p<4;++p)for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x) {
      int xx=x-b.x1,yy=y-b.y1,index=yy*32+xx;
      h.plates[size_t(p)].yChunk[size_t(index)]=h.plates[size_t(p)].abChunk[size_t(index)]=xx<16?0:1;
      auto a=plates.appearance(p);a.y.at(x,y)=float((xx<16?.2:.8)+.02*yy+.06*std::sin(yy*2.1));
      a.a.at(x,y)=xx<16?-.1f:.1f;
      plates.supportY(p).at(x,y)=plates.supportAB(p).at(x,y)=1;
    }
    auto side=pigment::regionalEigenFieldSweep(plates,h,{},true,true);
    auto sideRepeat=pigment::regionalEigenFieldSweep(plates,h,{},true,true);
    require(side.results.size()==1,"single low budget, not another sweep");
    double noiseBefore=0,noiseAfter=0;
    for(int y=b.y1;y<b.y2;++y)for(int xx:{15,16}) {
      int x=xx+b.x1;double expected=(xx<16?.2:.8)+.02*(y-b.y1);
      auto broad=side.boundaryAppearance[0].view();
      noiseBefore+=std::pow(plates.appearance(0).y.at(x,y)-expected,2);
      noiseAfter+=std::pow(broad.y.at(x,y)-expected,2);
      require(std::abs(broad.a.at(x,y)-(xx<16?-.1:.1))<1e-6,"independent side chroma, no cross-boundary mixing");
      require(side.results[0].appearance[0].view().y.at(x,y)==broad.y.at(x,y),"new boundary value imposed exactly");
      require(broad.y.at(x,y)==sideRepeat.boundaryAppearance[0].view().y.at(x,y),"boundary fit deterministic");
    }
    require(noiseAfter<noiseBefore*.1,"robust level/trend removes boundary description");
    std::cout<<"Regional eigenfield reference tests passed; photographic gate separate\n";
    return 0;
  } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
