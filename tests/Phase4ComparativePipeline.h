// Included after the harness's existing image writers. Research only.
namespace research {
inline void readYab(const std::filesystem::path &path,pigment::YabPlanes out){
  std::ifstream f(path,std::ios::binary);std::string type;int w,h;float scale;f>>type>>w>>h>>scale;f.get();
  if(type!="PF" || scale!=-1 || w!=out.y.bounds.width() || h!=out.y.bounds.height())throw std::runtime_error("Invalid saved field "+path.string());
  auto b=out.y.bounds;for(int y=b.y2-1;y>=b.y1;--y)for(int x=b.x1;x<b.x2;++x){float v[3];f.read(reinterpret_cast<char*>(v),12);if(!f)throw std::runtime_error("Truncated saved field");out.y.at(x,y)=v[0];out.a.at(x,y)=v[1];out.b.at(x,y)=v[2];}
}
inline void identical(const std::filesystem::path&a,const std::filesystem::path&b){std::ifstream x(a,std::ios::binary),y(b,std::ios::binary);
  if(!x || !y || std::string(std::istreambuf_iterator<char>(x),{})!=std::string(std::istreambuf_iterator<char>(y),{}))throw std::runtime_error("Saved upstream mismatch: "+a.string());}
inline void comparePipeline(const std::filesystem::path &root,const std::string &fixture,
    pigment::ConstYabPlanes source,const pigment::Phase4AutomaticResult &a,const pigment::Phase4RegionHierarchy &h,
    const pigment::Phase4Params &params,const pigment::MatrixOpponentTransform &transform){
  if(fixture!="fashion" && fixture!="knee" && fixture!="lowlight")throw std::runtime_error("First comparison limited to fashion/knee/lowlight");
  auto b=source.y.bounds;pigment::OwnedPlane opaque(b,1);
  std::ofstream provenance(root/"comparative-provenance.csv");provenance<<"mode,representation,input,gate_status\n";
  std::ofstream settings(root/"comparative-settings.csv");settings<<"spill_amount,luma_spill,chroma_spill,reach,asymmetry,structure_respect,gradient_complexity\n"
    <<params.spillAmount<<','<<params.lumaSpill<<','<<params.chromaSpill<<','<<params.spillReach<<','<<params.spillAsymmetry<<','<<params.structureRespect<<','<<params.gradientComplexity<<'\n';
  std::ofstream separation(root/"separation.csv");separation<<"plate,y_chunks,ab_chunks,retained_pixel_disagreement\n";
  for(int i=0;i<a.plates.count();++i){const auto &p=h.plates[size_t(i)];size_t different=0;
    for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x)different+=(p.yRetainedBoundaries.view().at(x,y)>0)!=(p.abRetainedBoundaries.view().at(x,y)>0);
    separation<<i<<','<<p.yChunkCount<<','<<p.abChunkCount<<','<<different<<'\n';
    pigment::OwnedYabPlanes fields(b);auto v=fields.view();for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){v.y.at(x,y)=a.plates.alpha(i).at(x,y);v.a.at(x,y)=a.plates.supportY(i).at(x,y);v.b.at(x,y)=a.plates.supportAB(i).at(x,y);}
    writeYabPfm(root/(std::string("plate-")+char('A'+i)+"-ownership-support.pfm"),static_cast<const pigment::OwnedYabPlanes&>(fields).view());
  }
  const std::array<std::string,5> names{"C0-A3","C1-Poisson","C2-SecondMoments","C3-RegionalEigen","C4-SparseCurve"};
  const auto sharedTransport=pigment::preparePhase4SpillTransport(a.plates,a.analysisGraph,params);
  auto zeroReach=params;zeroReach.spillReach=0;
  const auto intrinsicTransport=pigment::preparePhase4SpillTransport(a.plates,a.analysisGraph,zeroReach);
  const std::array<std::filesystem::path,5> inputs{root,std::filesystem::path("build/phase4-c-survival-corrected")/fixture,
    std::filesystem::path("build/phase4-c-second")/fixture,std::filesystem::path("build/phase4-c-regional-eigen")/fixture,
    std::filesystem::path("build/phase4-c-sparse-transition")/fixture};
  for(int mode=0;mode<5;++mode){auto dir=root/names[size_t(mode)];std::filesystem::create_directories(dir);pigment::Phase4ChunkSynthesis synthesis(b);
    auto saved=inputs[size_t(mode)];if(mode==3)saved/="modes-2-1";
    bool currentPoisson=mode==1;
    if(currentPoisson){saved=dir/"matched-current-fields";std::filesystem::create_directories(saved);
      if(!std::filesystem::exists(saved/"config.txt")){
        const auto fresh=pigment::synthesizePhase4Chunks(source,a.plates,h,params);
        for(int i=0;i<a.plates.count();++i)writeYabPfm(saved/(std::string("plate-")+char('A'+i)+"-synthesized-yab.pfm"),fresh.plateAppearance[size_t(i)].view());
        std::ofstream config(saved/"config.txt");config<<std::setprecision(10)<<params.gradientComplexity<<'\n';
      }else{std::ifstream config(saved/"config.txt");float complexity;config>>complexity;if(complexity!=params.gradientComplexity)throw std::runtime_error("C1 cache complexity mismatch");}
    }
    if(mode>1){identical(inputs[size_t(mode)]/"source-yab.pfm",root/"source-yab.pfm");
      for(int i=0;i<a.plates.count();++i){std::string prefix=std::string("plate-")+char('A'+i);
        for(auto suffix:{"-appearance-yab.pfm","-alpha.pgm","-support-y.pgm","-support-ab.pgm","-y-chunks.ppm","-ab-chunks.ppm","-y-retained.pgm","-ab-retained.pgm"})identical(inputs[size_t(mode)]/(prefix+suffix),root/(prefix+suffix));}}
    for(int i=0;i<a.plates.count();++i){synthesis.plateAppearance.emplace_back(b);auto out=synthesis.plateAppearance.back().view();
      if(mode==0){auto v=a.plates.appearance(i);for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){out.y.at(x,y)=v.y.at(x,y);out.a.at(x,y)=v.a.at(x,y);out.b.at(x,y)=v.b.at(x,y);}}
      else readYab(saved/(std::string("plate-")+char('A'+i)+"-synthesized-yab.pfm"),out);}
    provenance<<names[size_t(mode)]<<','<<(mode==0?"frozen public passthrough":"preserved CPU research representation")<<','<<saved.string()<<','<<(mode==0?"upstream baseline":"not accepted")<<'\n';
    auto zero=params;zero.spillAmount=0;
    const auto pre=pigment::applyPhase4Spill(source,a.plates,synthesis,a.analysisGraph,zero,{},&sharedTransport);
    for(int i=0;i<a.plates.count();++i)writeAppearance(dir/(std::string("plate-")+char('A'+i)+"-pre-appearance.ppm"),pre.plateAppearance[size_t(i)].view(),a.plates.alpha(i),transform);
    writeYabPfm(dir/"pre-spill-yab.pfm",pre.composite.view());
    writeAppearance(dir/"pre-spill.ppm",pre.composite.view(),static_cast<const pigment::OwnedPlane&>(opaque).view(),transform);
    for(auto variant:{std::string("spill"),std::string("ab-only"),std::string("ab-reach-zero")}){
      auto settings=params;if(variant!="spill")settings.lumaSpill=0;if(variant=="ab-reach-zero")settings.spillReach=0;
      const auto post=pigment::applyPhase4Spill(source,a.plates,synthesis,a.analysisGraph,settings,{},variant=="ab-reach-zero"?&intrinsicTransport:&sharedTransport);
      writeYabPfm(dir/(variant+"-yab.pfm"),post.composite.view());writeAppearance(dir/(variant+".ppm"),post.composite.view(),static_cast<const pigment::OwnedPlane&>(opaque).view(),transform);
      if(variant=="spill")for(int i=0;i<a.plates.count();++i)writeAppearance(dir/(std::string("plate-")+char('A'+i)+"-post-appearance.ppm"),post.plateAppearance[size_t(i)].view(),a.plates.alpha(i),transform);
      for(int i=0;i<a.plates.count();++i){std::string prefix=std::string("plate-")+char('A'+i);pigment::OwnedYabPlanes fields(b),transport(b);auto v=fields.view(),t=transport.view();
        for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){v.y.at(x,y)=post.influenceY[size_t(i)].view().at(x,y);v.a.at(x,y)=post.influence[size_t(i)].view().at(x,y);v.b.at(x,y)=0;
          t.y.at(x,y)=post.transportY[size_t(i)].view().at(x,y);t.a.at(x,y)=post.transportAB[size_t(i)].view().at(x,y);t.b.at(x,y)=0;
          if(variant!="spill" && pre.composite.view().y.at(x,y)!=post.composite.view().y.at(x,y))throw std::runtime_error("AB-only spill altered Y");}
        writeYabPfm(dir/(prefix+"-"+variant+"-influence.pfm"),static_cast<const pigment::OwnedYabPlanes&>(fields).view());
        writeYabPfm(dir/(prefix+"-"+variant+"-transport.pfm"),static_cast<const pigment::OwnedYabPlanes&>(transport).view());
        if(variant=="spill"){writePgm(dir/(prefix+"-spill-y-influence.pgm"),post.influenceY[size_t(i)].view());writePgm(dir/(prefix+"-spill-ab-influence.pgm"),post.influence[size_t(i)].view());}
      }
    }
  }
}
}
