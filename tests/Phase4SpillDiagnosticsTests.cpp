#include "core/PlateSpill.h"
#include "Phase4ResearchSnapshot.h"
#include <iostream>
int main(){try {
  auto require=[](bool ok,const char *why){if(!ok)throw std::runtime_error(why);};
  pigment::RectI b{0,0,2,1};pigment::OwnedYabPlanes source(b);pigment::Phase4AutomaticResult upstream(b,12,4);
  pigment::Phase4ChunkSynthesis synthesis(b);pigment::Phase4RegionHierarchy hierarchy;hierarchy.bounds=b;hierarchy.boundaryStrength=pigment::OwnedPlane(b);hierarchy.atomicRegion={0,1};
  auto &graph=upstream.analysisGraph;graph.width=2;graph.height=1;graph.rowOffsets={0,1,1};graph.edges={{1,-10,.9,1,0}};
  for(int p=0;p<4;++p){synthesis.plateAppearance.emplace_back(b);hierarchy.plates.emplace_back(b);auto &h=hierarchy.plates.back();h.yChunk={0,1};h.abChunk={0,0};h.yChunkCount=2;h.abChunkCount=1;h.yRetainedBoundaries.view().at(0,0)=1;
    for(int x=0;x<2;++x){upstream.plates.alpha(p).at(x,0)=.25;upstream.plates.supportY(p).at(x,0)=upstream.plates.supportAB(p).at(x,0)=p==0?(x==0?1:0):.5;
      auto v=upstream.plates.appearance(p);v.y.at(x,0)=.1f*p;v.a.at(x,0)=.05f*p;v.b.at(x,0)=-.04f*p;
      auto s=synthesis.plateAppearance.back().view();s.y.at(x,0)=v.y.at(x,0);s.a.at(x,0)=v.a.at(x,0);s.b.at(x,0)=v.b.at(x,0);}}
  pigment::Phase4Params params;params.spillReach=100;params.spillAmount=.8;params.lumaSpill=0;params.chromaSpill=1;
  auto none=params;none.spillAmount=0;auto pre=pigment::applyPhase4Spill(static_cast<const pigment::OwnedYabPlanes&>(source).view(),upstream.plates,synthesis,graph,none);
  auto post=pigment::applyPhase4Spill(static_cast<const pigment::OwnedYabPlanes&>(source).view(),upstream.plates,synthesis,graph,params);
  require(post.transportY[0].view().at(1,0)>0,"directed F transports influence");
  auto prepared=pigment::preparePhase4SpillTransport(upstream.plates,graph,params);
  auto reused=pigment::applyPhase4Spill(static_cast<const pigment::OwnedYabPlanes&>(source).view(),upstream.plates,synthesis,graph,params,{},&prepared);
  require(reused.composite.view().a.at(1,0)==post.composite.view().a.at(1,0),"transport reuse preserves exact output");
  for(int x=0;x<2;++x){require(pre.composite.view().y.at(x,0)==post.composite.view().y.at(x,0),"AB-only spill keeps Y bit exact");for(int p=0;p<4;++p)require(upstream.plates.alpha(p).at(x,0)==.25f,"alpha ownership immutable");}
  graph.edges[0].signedMixtureWeight=100;auto signedChange=pigment::applyPhase4Spill(static_cast<const pigment::OwnedYabPlanes&>(source).view(),upstream.plates,synthesis,graph,params);
  require(post.composite.view().a.at(1,0)==signedChange.composite.view().a.at(1,0),"signed W never used as transport capacity");
  graph.edges[0].weight=0;auto blocked=pigment::applyPhase4Spill(static_cast<const pigment::OwnedYabPlanes&>(source).view(),upstream.plates,synthesis,graph,params);
  require(blocked.transportY[0].view().at(1,0)==0,"zero F is a hard transport barrier");
  graph.edges[0].weight=.9;graph.edges[0].boundary=1;params.structureRespect=1;
  auto locked=pigment::applyPhase4Spill(static_cast<const pigment::OwnedYabPlanes&>(source).view(),upstream.plates,synthesis,graph,params);
  require(locked.transportY[0].view().at(1,0)==0,"zero structural capacity blocks transport");
  auto path=std::filesystem::temp_directory_path()/"pigment-comparative-snapshot-test.bin";
  auto key=research::key(static_cast<const pigment::OwnedYabPlanes&>(source).view(),params);
  research::saveSnapshot(path,key,upstream,hierarchy);pigment::Phase4AutomaticResult loaded(b,12,4);pigment::Phase4RegionHierarchy restored;
  research::loadSnapshot(path,key,loaded,restored);std::filesystem::remove(path);
  require(loaded.analysisGraph.edges[0].signedMixtureWeight==100 && loaded.analysisGraph.edges[0].weight==.9f,"snapshot preserves signed/nonnegative graph semantics");
  require(restored.plates[0].yRetainedBoundaries.view().at(0,0)==1 && restored.plates[0].yChunk==hierarchy.plates[0].yChunk,"snapshot preserves barriers and chunk labels");
  for(int p=0;p<4;++p)for(int x=0;x<2;++x)require(loaded.plates.supportAB(p).at(x,0)==upstream.plates.supportAB(p).at(x,0),"full precision support replay");
  std::cout<<"Spill instrumentation and full-precision snapshot tests passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
