#include "metal/PigmentMetal.h"
#include "core/ColorSpace.h"
#include "Phase4ResearchSnapshot.h"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <cmath>

using namespace pigment;
namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
static double ms(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
static void require(bool v,const char* msg){if(!v)throw std::runtime_error(msg);}
static void readYab(const fs::path& path,YabPlanes out){std::ifstream f(path,std::ios::binary);std::string magic;int w,h;float scale;f>>magic>>w>>h>>scale;f.get();require(magic=="PF" && scale==-1 && w==out.y.bounds.width() && h==out.y.bounds.height(),"PFM layout");
  for(int y=out.y.bounds.y2-1;y>=out.y.bounds.y1;--y)for(int x=out.y.bounds.x1;x<out.y.bounds.x2;++x){float v[3];f.read((char*)v,sizeof(v));out.y.at(x,y)=v[0];out.a.at(x,y)=v[1];out.b.at(x,y)=v[2];}require(bool(f),"PFM truncated");}
static void writeYab(const fs::path& path,ConstYabPlanes in){std::ofstream f(path,std::ios::binary);auto b=in.y.bounds;f<<"PF\n"<<b.width()<<' '<<b.height()<<"\n-1.0\n";for(int y=b.y2-1;y>=b.y1;--y)for(int x=b.x1;x<b.x2;++x){float v[]={in.y.at(x,y),in.a.at(x,y),in.b.at(x,y)};f.write((const char*)v,sizeof(v));}}
static void compare(metal::MetalInstance& gpu,ConstYabPlanes source,const PublicPlateSet& plates,
  const Phase4ChunkSynthesis& synthesis,const SparseAffinityGraph& graph,Phase4Params p,
  WorkingGamut gamut,std::ostream& csv,const std::string& label,const fs::path& output={}) {
  auto t=Clock::now();auto transport=preparePhase4SpillTransport(plates,graph,p);double transportMs=ms(t);
  Phase4SpillTransport accelerated;t=Clock::now();require(gpu.renderPhase4Transport(plates,graph,p,accelerated),"Metal transport did not converge");double metalTransportMs=ms(t);
  for(int i=0;i<plates.count();++i)for(int n=0;n<graph.nodeCount();++n){require(transport.y[i][n]==accelerated.y[i][n],"Metal Y graph fixed point changed");require(transport.ab[i][n]==accelerated.ab[i][n],"Metal AB graph fixed point changed");}
  t=Clock::now();auto cpu=applyPhase4Spill(source,plates,synthesis,graph,p,{},&transport,gamut);double cpuMs=ms(t);
  Phase4SpillResult result(plates.bounds());require(gpu.renderPhase4Spill(source,plates,synthesis,graph,p,accelerated,gamut,result),"Metal dispatch failed");
  const auto d=gpu.diagnostics();
  double error=0,squared=0,n=0;auto b=plates.bounds();auto a=cpu.composite.view(),c=result.composite.view();
  for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){float diffs[]={a.y.at(x,y)-c.y.at(x,y),a.a.at(x,y)-c.a.at(x,y),a.b.at(x,y)-c.b.at(x,y)};for(float v:diffs){require(std::isfinite(v),"Metal nonfinite");error=std::max(error,double(std::abs(v)));squared+=v*v;++n;}
    for(int i=0;i<plates.count();++i){require(cpu.transportY[i].view().at(x,y)==result.transportY[i].view().at(x,y),"Y transport changed");require(cpu.transportAB[i].view().at(x,y)==result.transportAB[i].view().at(x,y),"AB transport changed");
      require(std::abs(cpu.influence[i].view().at(x,y)-result.influence[i].view().at(x,y))<1e-5,"AB influence changed");require(std::abs(cpu.influenceY[i].view().at(x,y)-result.influenceY[i].view().at(x,y))<1e-5,"Y influence changed");}}
  require(error<5e-5 && std::sqrt(squared/n)<5e-6,"CPU/Metal appearance tolerance");
  if(p.lumaSpill==0 || p.spillAmount==0){auto zero=p;zero.spillAmount=0;Phase4SpillResult pre(b);require(gpu.renderPhase4Spill(source,plates,synthesis,graph,zero,transport,gamut,pre),"Metal zero dispatch");
    for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x)require(pre.composite.view().y.at(x,y)==c.y.at(x,y),"AB-only Metal Y must be bit-exact");}
  csv<<label<<','<<int(p.colorInteraction)<<','<<p.spillAmount<<','<<p.spillReach<<','<<p.lumaSpill<<','<<transportMs<<','<<metalTransportMs<<','<<cpuMs<<','<<d.totalMs<<','<<d.gpuMs<<','<<d.wrapOrUploadMs<<','<<d.readbackMs<<','<<error<<','<<std::sqrt(squared/n)<<','<<d.scratchBytes<<'\n';
  if(!output.empty()){writeYab(output/"cpu-yab.pfm",static_cast<const OwnedYabPlanes&>(cpu.composite).view());writeYab(output/"metal-yab.pfm",static_cast<const OwnedYabPlanes&>(result.composite).view());}
}
int main(int argc,char** argv){try{metal::MetalInstance gpu;std::ostream& log=std::cout;
  log<<"case,law,spill,reach,Yspill,transport_ms,metal_transport_ms,cpu_interaction_ms,metal_total_ms,gpu_ms,pack_ms,unpack_ms,max_error,rmse,scratch_bytes\n";
  RectI b{-9,7,30,30};OwnedYabPlanes source(b);PublicPlateSet plates(b,4);Phase4ChunkSynthesis s(b);SparseAffinityGraph graph;graph.width=3;graph.height=2;graph.rowOffsets={0,1,2,3,4,5,5};graph.edges={{1,-4,.7f,2,0},{2,8,0,1,0},{3,-2,.8f,1,1},{4,9,.8f,1,0},{5,-1,.6f,1,0}};
  for(int i=0;i<4;++i){s.plateAppearance.emplace_back(b);for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){plates.alpha(i).at(x,y)=.25f;plates.supportY(i).at(x,y)=.03f*(i+1);plates.supportAB(i).at(x,y)=.12f*(i+1);}}
  for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){plates.supportY(0).at(x,y)=x==b.x1 && y==b.y1?1.f:0;plates.supportAB(0).at(x,y)=x==b.x1 && y==b.y1?.02f:0;}
  {Phase4Params p;p.spillReach=128;p.structureRespect=1;Phase4SpillTransport t;
    require(gpu.renderPhase4Transport(plates,graph,p,t),"Directed transport fixture");
    require(t.y[0][1]>0 && t.ab[0][1]>0,"Weak and strong seeds travel");
    require(std::abs(t.ab[0][1]/t.y[0][1]-.02f)<1e-6f,"Weak amplitude does not consume Reach");
    require(t.y[0][2]==0 && t.ab[0][2]==0,"Zero F blocks GPU transport");
    require(t.y[0][3]==0 && t.ab[0][3]==0,"Locked barrier blocks GPU transport");}
  for(auto gamut:{WorkingGamut::ACEScg,WorkingGamut::LinearRec709,WorkingGamut::LinearRec2020,WorkingGamut::DisplayP3D65}){
    MatrixOpponentTransform transform(gamut);for(int i=0;i<4;++i){auto v=s.plateAppearance[i].view();for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){auto q=transform.toYab({i==0?-2.f:.1f*i, .03f*(x-b.x1),i==3?8.f:-.2f});v.y.at(x,y)=q.y;v.a.at(x,y)=q.a;v.b.at(x,y)=q.b;}}
    for(auto law:{ColorInteractionLaw::LinearYAB,ColorInteractionLaw::Density})for(float reach:{0.f,128.f})for(float spill:{0.f,1.f}){Phase4Params p;p.colorInteraction=law;p.pigmentDensity=.7f;p.spillReach=reach;p.spillAmount=spill;p.lumaSpill=0;p.chromaSpill=1;p.structureRespect=1;compare(gpu,static_cast<const OwnedYabPlanes&>(source).view(),plates,s,graph,p,gamut,log,"HDR-negative-origin");}}
  if(argc>1){fs::path root=argv[1],out=argc>2?argv[2]:"build/phase4-metal-parity";fs::create_directories(out);std::ofstream csv(out/"parity.csv");csv<<"case,law,spill,reach,Yspill,transport_ms,metal_transport_ms,cpu_interaction_ms,metal_total_ms,gpu_ms,pack_ms,unpack_ms,max_error,rmse,scratch_bytes\n";
    for(auto fixture:{"fashion","knee","lowlight"}){fs::path dir=root/fixture;std::ifstream f(dir/"shared-upstream.snapshot",std::ios::binary);uint64_t magic,key;RectI bounds;int count;research::io(f,magic);research::io(f,key);research::io(f,bounds);research::io(f,count);Phase4AutomaticResult a(bounds,16,count);Phase4RegionHierarchy h;research::loadSnapshot(dir/"shared-upstream.snapshot",key,a,h);OwnedYabPlanes original(bounds);readYab(dir/"source-yab.pfm",original.view());
      for(auto mode:{"C0","C1","C4"}){Phase4ChunkSynthesis fields(bounds);for(int i=0;i<count;++i){fields.plateAppearance.emplace_back(bounds);auto v=fields.plateAppearance.back().view();if(std::string(mode)=="C0"){auto from=a.plates.appearance(i);for(int y=bounds.y1;y<bounds.y2;++y)for(int x=bounds.x1;x<bounds.x2;++x){v.y.at(x,y)=from.y.at(x,y);v.a.at(x,y)=from.a.at(x,y);v.b.at(x,y)=from.b.at(x,y);}}else{fs::path path=std::string(mode)=="C1"?fs::path("build/phase4-comparative")/fixture/"C1-Poisson/matched-current-fields":fs::path("build/phase4-c-sparse-transition")/fixture;readYab(path/(std::string("plate-")+char('A'+i)+"-synthesized-yab.pfm"),v);}}
        for(auto law:{ColorInteractionLaw::LinearYAB,ColorInteractionLaw::Density})for(float reach:{0.f,128.f})for(float spill:{0.f,1.f}){Phase4Params p;p.colorInteraction=law;p.pigmentDensity=.5f;p.spillReach=reach;p.spillAmount=spill;p.lumaSpill=.05f;p.chromaSpill=1;auto label=std::string(fixture)+"-"+mode+"-"+std::to_string(int(law))+"-"+std::to_string(int(reach))+"-"+std::to_string(int(spill));auto images=out/label;fs::create_directories(images);compare(gpu,static_cast<const OwnedYabPlanes&>(original).view(),a.plates,fields,a.analysisGraph,p,WorkingGamut::ACEScg,csv,label,images);csv.flush();}
      }
    }
  }
  return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
