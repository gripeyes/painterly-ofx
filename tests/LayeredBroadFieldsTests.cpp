#include "core/LayeredBroadFields.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
int main(){try{
  auto require=[](bool v,const char *s){if(!v)throw std::runtime_error(s);};
  pigment::RectI b{-2,3,94,99};pigment::PublicPlateSet plates(b,4);pigment::Phase4RegionHierarchy h;h.bounds=b;h.boundaryStrength=pigment::OwnedPlane(b);
  for(int p=0;p<4;++p)for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){auto a=plates.appearance(p);a.y.at(x,y)=-.1f+.01f*(x-b.x1)+.003f*(y-b.y1)+.02f*std::sin(x*2.);
    a.a.at(x,y)=.04f*(x-b.x1)/96;a.b.at(x,y)=0;plates.alpha(p).at(x,y)=.25;plates.supportY(p).at(x,y)=plates.supportAB(p).at(x,y)=1;}
  pigment::LayeredBroadOptions o;o.observationScaleY=12;o.observationScaleAB=24;
  auto result=pigment::layeredBroadFields(plates,h,o),repeat=pigment::layeredBroadFields(plates,h,o);
  auto yOnly=o;yOnly.processAB=false;auto only=pigment::layeredBroadFields(plates,h,yOnly);
  for(int p=0;p<4;++p)for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){auto a=result.appearance[size_t(p)].view();
    require(std::isfinite(a.y.at(x,y)),"finite HDR/negative output");require(a.y.at(x,y)==repeat.appearance[size_t(p)].view().y.at(x,y),"deterministic");
    require(only.appearance[size_t(p)].view().a.at(x,y)==plates.appearance(p).a.at(x,y),"Y-only AB exact bypass");
    double sum=result.broad[size_t(p)].view().y.at(x,y)+result.structure[size_t(p)].view().y.at(x,y)+result.medium[size_t(p)].view().y.at(x,y)+result.micro[size_t(p)].view().y.at(x,y);
    require(std::abs(sum-plates.appearance(p).y.at(x,y))<2e-6,"exact information decomposition");require(a.b.at(x,y)==0,"neutral chroma remains neutral");}
  for(const auto &layers:result.layers)for(const auto &l:layers)for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x)require(l.membership.view().at(x,y)>=0 && l.membership.view().at(x,y)<=1,"nonnegative bounded sublayer occupancy");
  for(int p=0;p<4;++p)plates.supportY(p).at(12,13)=0;
  auto hole=pigment::layeredBroadFields(plates,h,o);for(int p=0;p<4;++p)require(hole.appearance[size_t(p)].view().y.at(12,13)==plates.appearance(p).y.at(12,13),"unsupported safe bypass");
  std::cout<<"C5 isolated reference tests passed; photographic acceptance separate\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
