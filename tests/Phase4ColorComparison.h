// Uses frozen snapshots and saved downstream fields; never regenerates A1–B.
namespace research {
inline void colorComparison(const std::filesystem::path &root,pigment::ConstYabPlanes source,
 const pigment::Phase4AutomaticResult &a,const pigment::Phase4Params &settings,
 const pigment::MatrixOpponentTransform &transform,bool materialOnly=false){
 auto b=source.y.bounds;auto output=root/"color-interaction";std::filesystem::create_directories(output);
 auto working=std::filesystem::path("build/phase4-comparative")/root.filename();
 identical(working/"shared-upstream.snapshot",root/"shared-upstream.snapshot");
 identical(working/"source-yab.pfm",root/"source-yab.pfm");
 auto sparse=std::filesystem::path("build/phase4-c-sparse-transition")/root.filename();
 identical(sparse/"source-yab.pfm",root/"source-yab.pfm");
 for(int i=0;i<a.plates.count();++i){auto stem=std::string("plate-")+char('A'+i);
  for(auto suffix:{"-appearance-yab.pfm","-alpha.pgm","-support-y.pgm","-support-ab.pgm","-y-chunks.ppm","-ab-chunks.ppm","-y-retained.pgm","-ab-retained.pgm"})identical(sparse/(stem+suffix),working/(stem+suffix));}
 const pigment::OwnedPlane opaque(b,1);auto p=settings;p.pigmentDensity=.5f;
 const auto transport=pigment::preparePhase4SpillTransport(a.plates,a.analysisGraph,p);
 pigment::ColorInteraction color(pigment::WorkingGamut::ACEScg);
 std::ofstream metrics,provenance;
 if(!materialOnly){metrics.open(output/"comparison.csv");provenance.open(output/"inputs.txt");}
 if(!materialOnly)metrics<<"representation,law,density,seconds,delta_Y_rms,delta_AB_rms,transport_max_error,influence_max_error,fit_error_mean,scene_residual_mean\n";
 if(!materialOnly)provenance<<"Frozen shared-upstream.snapshot; original alpha/support/F; same prepared directed transport for all laws.\n"
 <<"spill="<<p.spillAmount<<" y="<<p.lumaSpill<<" ab="<<p.chromaSpill<<" reach="<<p.spillReach<<" asymmetry="<<p.spillAsymmetry<<" respect="<<p.structureRespect<<"\n"
 <<"Source is the existing harness's assigned linear ACEScg fixture values, not newly color-managed photographs. No source reinterpretation or output transform. PNG/PPM are clipped diagnostic previews only; PFM preserves signed HDR.\n";
 for(auto name:{std::string("C0-A3"),std::string("C1-Poisson"),std::string("C4-SparseCurve")}){
  auto dir=output/name;std::filesystem::create_directories(dir);pigment::Phase4ChunkSynthesis synthesis(b);
  for(int i=0;i<a.plates.count();++i){synthesis.plateAppearance.emplace_back(b);
   if(name=="C0-A3"){auto v=a.plates.appearance(i);auto d=synthesis.plateAppearance.back().view();for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){d.y.at(x,y)=v.y.at(x,y);d.a.at(x,y)=v.a.at(x,y);d.b.at(x,y)=v.b.at(x,y);}}
   else{auto fields=name=="C1-Poisson"?working/name/"matched-current-fields":sparse;
    if(name=="C1-Poisson"){std::ifstream config(fields/"config.txt");float complexity=0;config>>complexity;if(!config || complexity!=settings.gradientComplexity)throw std::runtime_error("Saved C1 complexity mismatch");}
    readYab(fields/(std::string("plate-")+char('A'+i)+"-synthesized-yab.pfm"),synthesis.plateAppearance.back().view());}}
  if(materialOnly){
   for(auto law:{pigment::ColorInteractionLaw::Density,pigment::ColorInteractionLaw::SpectralPigment}){
    std::string label=law==pigment::ColorInteractionLaw::Density?"density":"spectral-pigment";
    std::ofstream f(dir/(label+"-material-fits.csv"));f<<"plate,x,y,magnitude,c0,c1,c2,fit_error,residual_R,residual_G,residual_B\n";
    for(int i=0;i<a.plates.count();++i)for(int y=b.y1;y<b.y2;y+=16)for(int x=b.x1;x<b.x2;x+=16){auto v=synthesis.plateAppearance[i].view();auto c=p.plates[i];auto m=color.encode({v.y.at(x,y)+c.tone,v.a.at(x,y)+c.biasA,v.b.at(x,y)+c.biasB},law);
     f<<i<<','<<x<<','<<y<<','<<m.magnitude<<','<<m.coefficients[0]<<','<<m.coefficients[1]<<','<<m.coefficients[2]<<','<<m.fitError<<','<<m.residual[0]<<','<<m.residual[1]<<','<<m.residual[2]<<'\n';}
   }continue;
  }
  auto pre=p;pre.spillAmount=0;const auto before=pigment::applyPhase4Spill(source,a.plates,synthesis,a.analysisGraph,pre,{},&transport);
  writeYabPfm(dir/"pre-spill-yab.pfm",before.composite.view());writeAppearance(dir/"pre-spill.ppm",before.composite.view(),opaque.view(),transform);
  p.colorInteraction=pigment::ColorInteractionLaw::LinearYAB;
  const auto baseline=pigment::applyPhase4Spill(source,a.plates,synthesis,a.analysisGraph,p,{},&transport);
  for(auto law:{pigment::ColorInteractionLaw::LinearYAB,pigment::ColorInteractionLaw::Density,pigment::ColorInteractionLaw::SpectralPigment}){
   p.colorInteraction=law;std::string label=law==pigment::ColorInteractionLaw::LinearYAB?"linear-yab":law==pigment::ColorInteractionLaw::Density?"density":"spectral-pigment";
   auto start=std::chrono::steady_clock::now();const auto post=pigment::applyPhase4Spill(source,a.plates,synthesis,a.analysisGraph,p,{},&transport);
   double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
   writeYabPfm(dir/(label+"-yab.pfm"),post.composite.view());writeAppearance(dir/(label+".ppm"),post.composite.view(),opaque.view(),transform);
   double dy=0,dc=0,maxTransport=0,maxInfluence=0,fit=0,residual=0;size_t samples=0;
   std::ofstream materialFits(dir/(label+"-material-fits.csv"));materialFits<<"plate,x,y,magnitude,c0,c1,c2,fit_error,residual_R,residual_G,residual_B\n";
   pigment::OwnedYabPlanes difference(b),spillDifference(b);auto dv=difference.view(),sv=spillDifference.view();auto v=post.composite.view(),l=baseline.composite.view(),preV=before.composite.view();
   for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){
    dv.y.at(x,y)=v.y.at(x,y)-l.y.at(x,y);dv.a.at(x,y)=v.a.at(x,y)-l.a.at(x,y);dv.b.at(x,y)=v.b.at(x,y)-l.b.at(x,y);
    sv.y.at(x,y)=v.y.at(x,y)-preV.y.at(x,y);sv.a.at(x,y)=v.a.at(x,y)-preV.a.at(x,y);sv.b.at(x,y)=v.b.at(x,y)-preV.b.at(x,y);
    dy+=double(dv.y.at(x,y))*dv.y.at(x,y);dc+=double(dv.a.at(x,y))*dv.a.at(x,y)+double(dv.b.at(x,y))*dv.b.at(x,y);
    for(int i=0;i<a.plates.count();++i){for(bool ab:{false,true}){
      auto t=ab?post.transportAB[i].view():post.transportY[i].view(),bt=ab?baseline.transportAB[i].view():baseline.transportY[i].view();
      auto inf=ab?post.influence[i].view():post.influenceY[i].view(),bi=ab?baseline.influence[i].view():baseline.influenceY[i].view();
      maxTransport=std::max(maxTransport,std::abs(double(t.at(x,y)-bt.at(x,y))));maxInfluence=std::max(maxInfluence,std::abs(double(inf.at(x,y)-bi.at(x,y))));}
     if(law!=pigment::ColorInteractionLaw::LinearYAB && (x-b.x1)%16==0 && (y-b.y1)%16==0){auto f=synthesis.plateAppearance[i].view();auto c=p.plates[i];auto m=color.encode({f.y.at(x,y)+c.tone,f.a.at(x,y)+c.biasA,f.b.at(x,y)+c.biasB},law);fit+=m.fitError;residual+=m.residualMagnitude;++samples;
      materialFits<<i<<','<<x<<','<<y<<','<<m.magnitude<<','<<m.coefficients[0]<<','<<m.coefficients[1]<<','<<m.coefficients[2]<<','<<m.fitError<<','<<m.residual[0]<<','<<m.residual[1]<<','<<m.residual[2]<<'\n';}}
   }
   if(maxTransport!=0 || maxInfluence!=0)throw std::runtime_error("Color law altered spatial transport/interaction");
   writeYabPfm(dir/(label+"-minus-linear-signed-yab.pfm"),pigment::asConst(difference.view()));writeYabPfm(dir/(label+"-spill-signed-yab.pfm"),pigment::asConst(spillDifference.view()));
   auto showDifference=[&](pigment::YabPlanes f,const std::filesystem::path &path){for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){f.y.at(x,y)=.5f+8*f.y.at(x,y);f.a.at(x,y)*=8;f.b.at(x,y)*=8;}writeAppearance(path,pigment::asConst(f),opaque.view(),transform);};
   showDifference(difference.view(),dir/(label+"-minus-linear-x8.ppm"));showDifference(spillDifference.view(),dir/(label+"-spill-x8.ppm"));
   metrics<<name<<','<<label<<','<<p.pigmentDensity<<','<<seconds<<','<<std::sqrt(dy/(b.width()*b.height()))<<','<<std::sqrt(dc/(2*b.width()*b.height()))<<','<<maxTransport<<','<<maxInfluence<<','<<(samples?fit/samples:0)<<','<<(samples?residual/samples:0)<<'\n';metrics.flush();
   std::cout<<root.filename()<<' '<<name<<' '<<label<<" seconds="<<seconds<<std::endl;
  }
  // Choose a photograph-derived warm receiver / cyan donor with valid overlap.
  int px=b.x1,py=b.y1,receiver=0,donor=1;double best=-1;
  for(int y=b.y1;y<b.y2;y+=8)for(int x=b.x1;x<b.x2;x+=8)for(int i=0;i<a.plates.count();++i)for(int j=0;j<a.plates.count();++j)if(i!=j){
   auto iv=synthesis.plateAppearance[i].view(),jv=synthesis.plateAppearance[j].view();auto ic=transform.toRgb({iv.y.at(x,y),iv.a.at(x,y),iv.b.at(x,y)}),jc=transform.toRgb({jv.y.at(x,y),jv.a.at(x,y),jv.b.at(x,y)});
   double score=std::max(0.f,ic[0]-ic[2])*std::max(0.f,jc[1]+jc[2]-2*jc[0])*std::sqrt(a.plates.supportAB(i).at(x,y)*a.plates.supportAB(j).at(x,y));
   if(score>best){best=score;px=x;py=y;receiver=i;donor=j;}}
  std::ofstream trajectory(dir/"donor-receiver-trajectories.csv");trajectory<<"law,density,receiver,donor,x,y,spill_scale,donor_weight,R,G,B,Y,A,B_opponent\n";
  for(bool reverse:{false,true}){int i=reverse?donor:receiver,j=reverse?receiver:donor;
   auto iv=synthesis.plateAppearance[i].view(),jv=synthesis.plateAppearance[j].view();auto ci=p.plates[i],cj=p.plates[j];pigment::YabPixel c[2]={{iv.y.at(px,py)+ci.tone,iv.a.at(px,py)+ci.biasA,iv.b.at(px,py)+ci.biasB},{jv.y.at(px,py)+cj.tone,jv.a.at(px,py)+cj.biasA,jv.b.at(px,py)+cj.biasB}};
   float tj=baseline.transportAB[j].view().at(px,py),ti=baseline.transportAB[i].view().at(px,py);
   float h=(p.spillAsymmetry*tj+(1-p.spillAsymmetry)*.5f*(tj+ti))*std::sqrt(a.plates.supportAB(i).at(px,py)*a.plates.supportAB(j).at(px,py))*cj.spillOut*ci.receiveSpill;
   for(auto law:{pigment::ColorInteractionLaw::LinearYAB,pigment::ColorInteractionLaw::Density,pigment::ColorInteractionLaw::SpectralPigment}){
    pigment::InteractionMaterial m[2]={color.encode(c[0],law),color.encode(c[1],law)};
    for(float density:{0.f,.5f,1.f})for(int step=0;step<=32;++step){float k=(step/32.f)*p.chromaSpill*h,w[2]={1,k};pigment::YabPixel linear{(c[0].y+k*c[1].y)/(1+k),(c[0].a+k*c[1].a)/(1+k),(c[0].b+k*c[1].b)/(1+k)};
     auto mixed=color.mix(m,w,2,linear,law,density);auto rgb=transform.toRgb(mixed);trajectory<<int(law)<<','<<density<<','<<i<<','<<j<<','<<px<<','<<py<<','<<step/32.f<<','<<k<<','<<rgb[0]<<','<<rgb[1]<<','<<rgb[2]<<','<<mixed.y<<','<<mixed.a<<','<<mixed.b<<'\n';}
   }
  }
 }
}
}
