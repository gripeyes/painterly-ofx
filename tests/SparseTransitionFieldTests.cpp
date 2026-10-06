#include "core/SparseTransitionField.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
int main(){try {
  auto require=[](bool ok,const char *why){if(!ok)throw std::runtime_error(why);};
  pigment::RectI b{-4,7,124,135};pigment::PublicPlateSet plates(b,4);pigment::Phase4RegionHierarchy h;h.bounds=b;
  h.boundaryStrength=pigment::OwnedPlane(b);
  for(int p=0;p<4;++p){h.plates.emplace_back(b);h.plates.back().yChunk.assign(128*128,0);h.plates.back().abChunk.assign(128*128,0);
    for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){double xx=x-b.x1,yy=y-b.y1;
      auto a=plates.appearance(p);a.y.at(x,y)=float(-.2+xx/80+.08*std::sin(xx*2.4)*std::cos(yy*2.1));
      a.a.at(x,y)=float(xx/400);a.b.at(x,y)=0;plates.alpha(p).at(x,y)=.25;
      plates.supportY(p).at(x,y)=plates.supportAB(p).at(x,y)=1;
    }}
  auto field=pigment::sparseTransitionField(plates,h);auto repeat=pigment::sparseTransitionField(plates,h);
  require(!field.curves.empty(),"automatic broad transition curves exist");
  int y=0,ab=0;for(auto &c:field.curves){if(c.family)++ab;else ++y;require(c.negativeSamples>=8 && c.positiveSamples>=8,"supported two-sided values");}
  require(y<=48 && ab<=24,"fixed sparse budgets");require(ab<y,"fewer chromatic transitions");
  for(auto &s:field.solves)if(s.solved)require(s.residual<1e-7,"harmonic residual sane");
  for(int yy=b.y1;yy<b.y2;++yy)for(int x=b.x1;x<b.x2;++x){auto v=field.appearance[0].view();
    require(std::isfinite(v.y.at(x,yy)),"finite unclipped output");require(v.b.at(x,yy)==0,"neutral B stable");
    require(v.y.at(x,yy)==repeat.appearance[0].view().y.at(x,yy),"deterministic field");}
  for(int p=0;p<4;++p)for(int yy=45;yy<55;++yy)for(int x=45;x<55;++x)plates.supportY(p).at(x,yy)=plates.supportAB(p).at(x,yy)=0;
  auto hole=pigment::sparseTransitionField(plates,h);
  for(int yy=45;yy<55;++yy)for(int x=45;x<55;++x)require(hole.appearance[0].view().y.at(x,yy)==plates.appearance(0).y.at(x,yy),"unsupported safe fallback");
  // Fixed source barrier coordinates and high-confidence values survive.
  for(int p=0;p<4;++p)for(int yy=0;yy<128;++yy)for(int xx=0;xx<128;++xx){int id=yy*128+xx;
    h.plates[size_t(p)].yChunk[size_t(id)]=h.plates[size_t(p)].abChunk[size_t(id)]=xx<64?0:1;
    if(xx==63 || xx==64)h.boundaryStrength.view().at(xx+b.x1,yy+b.y1)=1;}
  auto barrier=pigment::sparseTransitionField(plates,h);
  for(int yy=b.y1;yy<b.y2;++yy)for(int xx:{59,60})require(barrier.appearance[0].view().y.at(xx,yy)==plates.appearance(0).y.at(xx,yy),"hard structural appearance fixed");
  // Fine checker oscillation alone must not manufacture broad curves.
  for(int p=0;p<4;++p)for(int yy=0;yy<128;++yy)for(int xx=0;xx<128;++xx){
    auto a=plates.appearance(p);a.y.at(xx+b.x1,yy+b.y1)=((xx+yy)%2)?-.1f:.1f;a.a.at(xx+b.x1,yy+b.y1)=a.b.at(xx+b.x1,yy+b.y1)=0;
    plates.supportY(p).at(xx+b.x1,yy+b.y1)=plates.supportAB(p).at(xx+b.x1,yy+b.y1)=1;}
  auto oscillation=pigment::sparseTransitionField(plates,h);
  require(oscillation.curves.empty(),"fine oscillation does not generate dense curves");
  pigment::ExecutionContext cancelled;cancelled.cancelled=[] {return true;};bool stopped=false;
  try{pigment::sparseTransitionField(plates,h,cancelled);}catch(const std::runtime_error &){stopped=true;}
  require(stopped,"reference cancellation");
  std::cout<<"Sparse transition reference tests passed; photographic gate separate\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
