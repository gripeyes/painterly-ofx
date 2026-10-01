namespace research {
inline void layeredComparison(const std::filesystem::path &root,
    const pigment::Phase4AutomaticResult &automatic,
    const pigment::Phase4RegionHierarchy &hierarchy,
    const pigment::MatrixOpponentTransform &transform) {
  auto dir=root/"C5-LayeredBroadFields";std::filesystem::create_directories(dir);
  const auto &plates=automatic.plates;auto b=plates.bounds();pigment::OwnedPlane opaque(b,1);
  const auto result=pigment::layeredBroadFields(plates,hierarchy);
  std::ofstream stats(dir/"fits.csv");stats<<"plate,family,observations,layers,iteration,objective\n";
  for(const auto &d:result.fits)for(size_t j=0;j<d.objectives.size();++j)stats<<d.plate<<','<<d.family<<','<<d.observations<<','<<d.layers<<','<<j<<','<<std::setprecision(12)<<d.objectives[j]<<'\n';
  std::ofstream geometry(dir/"layers.csv");geometry<<"plate,family,layer,radial,cx,cy,rx,ry,c0,c1,c2,d0,d1,d2\n";
  for(int p=0;p<plates.count();++p){std::string prefix=std::string("plate-")+char('A'+p);
    for(auto entry:{std::make_pair("broad-target",&result.broadTarget),{"broad",&result.broad},{"structure",&result.structure},{"medium",&result.medium},{"micro",&result.micro},{"pre-spill",&result.appearance}}){
      auto field=(*entry.second)[size_t(p)].view();writeYabPfm(dir/(prefix+"-"+entry.first+".pfm"),field);
      if(std::string(entry.first)=="pre-spill" || std::string(entry.first)=="broad")writeAppearance(dir/(prefix+"-"+entry.first+".ppm"),field,plates.alpha(p),transform);}
    writePgm(dir/(prefix+"-protection-y.pgm"),result.protectionY[size_t(p)].view());writePgm(dir/(prefix+"-protection-ab.pgm"),result.protectionAB[size_t(p)].view());
    int index=0;for(const auto &l:result.layers[size_t(p)]){auto stem=prefix+"-layer-"+std::to_string(index++);
      writePgm(dir/(stem+"-membership.pgm"),l.membership.view());writeYabPfm(dir/(stem+"-field.pfm"),l.field.view());
      geometry<<p<<','<<l.family<<','<<index-1<<','<<l.radial<<','<<l.cx<<','<<l.cy<<','<<l.rx<<','<<l.ry;
      for(auto c:l.coefficients)for(auto v:c)geometry<<','<<v;geometry<<'\n';}
  }
  for(auto variant:{std::string("C0"),std::string("Y-only"),std::string("AB-only"),std::string("Y-AB")}){
    pigment::OwnedYabPlanes composite(b);auto out=composite.view();
    for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){double v[3]{};
      for(int p=0;p<plates.count();++p){auto source=plates.appearance(p),modified=result.appearance[size_t(p)].view();double w=plates.alpha(p).at(x,y);
        v[0]+=w*((variant=="Y-only"||variant=="Y-AB")?modified.y.at(x,y):source.y.at(x,y));
        v[1]+=w*((variant=="AB-only"||variant=="Y-AB")?modified.a.at(x,y):source.a.at(x,y));
        v[2]+=w*((variant=="AB-only"||variant=="Y-AB")?modified.b.at(x,y):source.b.at(x,y));}
      out.y.at(x,y)=float(v[0]);out.a.at(x,y)=float(v[1]);out.b.at(x,y)=float(v[2]);}
    writeYabPfm(dir/(variant+".pfm"),pigment::asConst(out));writeAppearance(dir/(variant+".ppm"),pigment::asConst(out),static_cast<const pigment::OwnedPlane&>(opaque).view(),transform);
  }
  std::ofstream manifest(dir/"provenance.txt");manifest<<"Baseline 564d0b8; frozen shared-upstream.snapshot reused; no A1-A3/B recomputation.\nLinear YAB; Spill zero; public alpha/support/geometry unchanged.\nY maximum 4 layers; AB maximum 3. No acceptance inferred from fit error.\n";
}
}
