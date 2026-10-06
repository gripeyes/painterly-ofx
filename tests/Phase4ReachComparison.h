// Included after the comparative pipeline helpers. Does not regenerate A1-B.
namespace research {
inline void repairedReachComparison(const std::filesystem::path &root,
    pigment::ConstYabPlanes source,const pigment::Phase4AutomaticResult &a,
    const pigment::Phase4Params &settings,const pigment::MatrixOpponentTransform &transform){
  auto b=source.y.bounds;auto out=root/"repaired-reach";std::filesystem::create_directories(out);
  pigment::OwnedPlane opaque(b,1);std::ofstream metrics(out/"reach.csv");
  metrics<<"mode,reach,delta_Y_rms,delta_AB_rms,transport_AB_gain,transport_Y_gain,max_Y_delta_AB_only,weak_AB_supported_samples_with_gain\n";
  for(auto name:{std::string("C0-A3"),std::string("C1-Poisson"),std::string("C4-SparseCurve")}){
    pigment::Phase4ChunkSynthesis synthesis(b);for(int i=0;i<a.plates.count();++i){synthesis.plateAppearance.emplace_back(b);
      if(name=="C0-A3"){auto v=a.plates.appearance(i);auto d=synthesis.plateAppearance.back().view();for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){d.y.at(x,y)=v.y.at(x,y);d.a.at(x,y)=v.a.at(x,y);d.b.at(x,y)=v.b.at(x,y);}}
      else {auto path=name=="C1-Poisson"?root/name/"matched-current-fields":std::filesystem::path("build/phase4-c-sparse-transition")/root.filename();readYab(path/(std::string("plate-")+char('A'+i)+"-synthesized-yab.pfm"),synthesis.plateAppearance.back().view());}}
    auto dir=out/name;std::filesystem::create_directories(dir);
    auto p=settings;p.lumaSpill=0;p.chromaSpill=1;auto pre=p;pre.spillAmount=0;
    const auto baseline=pigment::applyPhase4Spill(source,a.plates,synthesis,a.analysisGraph,pre);
    writeAppearance(dir/"pre.ppm",baseline.composite.view(),static_cast<const pigment::OwnedPlane&>(opaque).view(),transform);
    for(float reach:{0.f,12.f,48.f,128.f}){
      p.spillReach=reach;const auto post=pigment::applyPhase4Spill(source,a.plates,synthesis,a.analysisGraph,p);
      std::string tag="reach-"+std::to_string(int(reach));writeYabPfm(dir/(tag+".pfm"),post.composite.view());
      writeAppearance(dir/(tag+".ppm"),post.composite.view(),static_cast<const pigment::OwnedPlane&>(opaque).view(),transform);
      double dy=0,dc=0,gy=0,gc=0,maxY=0;size_t weakGain=0;
      for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){auto v=post.composite.view();auto z=baseline.composite.view();double delta=v.y.at(x,y)-z.y.at(x,y);maxY=std::max(maxY,std::abs(delta));dy+=delta*delta;
        for(double d:{double(v.a.at(x,y)-z.a.at(x,y)),double(v.b.at(x,y)-z.b.at(x,y))})dc+=d*d;
        for(int i=0;i<a.plates.count();++i){float seed=a.plates.supportAB(i).at(x,y),gain=std::max(0.f,post.transportAB[size_t(i)].view().at(x,y)-seed);gy+=std::max(0.f,post.transportY[size_t(i)].view().at(x,y)-a.plates.supportY(i).at(x,y));gc+=gain;weakGain+=(seed>0 && seed<.2f && gain>.001f);}}
      if(maxY!=0)throw std::runtime_error("Repaired AB Spill changed Y");
      metrics<<name<<','<<reach<<','<<std::sqrt(dy/(b.width()*b.height()))<<','<<std::sqrt(dc/(2*b.width()*b.height()))<<','<<gc<<','<<gy<<','<<maxY<<','<<weakGain<<'\n';
      for(int i=0;i<a.plates.count();++i){std::string plate="plate-";plate+=char('A'+i);writePgm(dir/(plate+"-"+tag+"-AB-transport.pgm"),post.transportAB[size_t(i)].view());writePgm(dir/(plate+"-"+tag+"-Y-transport.pgm"),post.transportY[size_t(i)].view());}
    }
  }
}
}
