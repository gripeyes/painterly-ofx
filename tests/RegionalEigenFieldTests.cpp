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
    std::cout<<"Regional eigenfield reference tests passed; photographic gate separate\n";
    return 0;
  } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
