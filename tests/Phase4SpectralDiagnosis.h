#include <zlib.h>
namespace research {
// Raw little-endian IEEE754 doubles, gzip lossless. Schema records every stage,
// including the two independent transport mixtures before recombination.
inline void spectralDiagnosis(const std::filesystem::path &root,pigment::ConstYabPlanes source,
 const pigment::Phase4AutomaticResult &a,const pigment::Phase4Params &settings,
 const pigment::MatrixOpponentTransform &transform,bool massCandidate=false){
 auto b=source.y.bounds;auto dir=root/(massCandidate?"spectral-material-mass":"spectral-diagnosis");std::filesystem::create_directories(dir);
 auto p=settings;p.pigmentDensity=.5f;
 pigment::Phase4ChunkSynthesis synthesis(b);
 for(int i=0;i<a.plates.count();++i){synthesis.plateAppearance.emplace_back(b);auto v=a.plates.appearance(i);auto d=synthesis.plateAppearance.back().view();
  for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){d.y.at(x,y)=v.y.at(x,y);d.a.at(x,y)=v.a.at(x,y);d.b.at(x,y)=v.b.at(x,y);}}
 const auto transport=pigment::preparePhase4SpillTransport(a.plates,a.analysisGraph,p);
 const pigment::OwnedPlane opaque(b,1);pigment::OwnedYabPlanes yRGB(b),abRGB(b);
 auto gz=massCandidate?nullptr:gzopen((dir/"trace.f64.gz").c_str(),"wb1");if(!massCandidate && !gz)throw std::runtime_error("Cannot write spectral trace");
 std::ofstream schema(dir/"trace-schema.txt");bool first=true;size_t records=0;
 auto observer=[&](const pigment::SpectralSpillSample &s){
  if(!massCandidate){std::vector<double> row;
  auto scalar=[&](const std::string &name,double v){row.push_back(v);if(first)schema<<row.size()-1<<' '<<name<<'\n';};
  auto array=[&](const std::string &name,const auto &vs){int n=0;for(auto v:vs)scalar(name+"."+std::to_string(n++),v);};
  scalar("x",s.x);scalar("y",s.y);scalar("plate",s.receiver);scalar("alpha",s.alpha);
  array("input_scene_RGB",s.encode.sceneRGB);scalar("positive_scene_magnitude",s.material.magnitude);
  array("bounded_material_RGB",s.encode.materialRGB);array("negative_residual",s.encode.negativeResidual);
  array("out_of_model_residual",s.encode.fitResidual);array("restored_residual",s.material.residual);
  array("coefficients",s.material.coefficients);array("reflectance",s.encode.reflectance);
  array("safe_reflectance",s.encode.safeReflectance);scalar("RGB_fit_error",s.material.fitError);array("KS",s.encode.ks);
  scalar("grey_clamp",s.encode.greyClamp);scalar("reflectance_floor",s.encode.reflectanceFloor);
  scalar("coefficient_bound_attempts",s.encode.coefficientBound);scalar("invalid_input",s.encode.invalid);
  scalar("iterations",s.encode.iterations);scalar("rejected_steps",s.encode.rejectedSteps);
  for(int k=0;k<3;++k)scalar("final_coefficient_bound."+std::to_string(k),std::abs(s.material.coefficients[k])>=100);
  array("weights_Y",s.weightsY);array("weights_AB",s.weightsAB);
  auto mixture=[&](const std::string &name,const pigment::SpectralMixTrace &m){
   scalar(name+".magnitude",m.magnitude);array(name+".mixed_KS",m.ks);array(name+".mixed_reflectance",m.reflectance);
   array(name+".XYZ",m.xyz);array(name+".working_RGB",m.materialRGB);array(name+".scene_before_residual",m.sceneBeforeResidual);
   array(name+".residual",m.residual);array(name+".nonlinear_RGB",m.nonlinearRGB);
   array(name+".nonlinear_YAB",std::array<float,3>{m.nonlinearYab.y,m.nonlinearYab.a,m.nonlinearYab.b});
   array(name+".final_YAB",std::array<float,3>{m.finalYab.y,m.finalYab.a,m.finalYab.b});
   scalar(name+".density_clamp",m.densityClamp);scalar(name+".invalid",m.invalid);scalar(name+".bypass",m.bypass);
  };mixture("Y",s.yMix);mixture("AB",s.abMix);
  array("recombined_plate_YAB",std::array<float,3>{s.output.y,s.output.a,s.output.b});
  if(gzwrite(gz,row.data(),unsigned(row.size()*sizeof(double)))!=int(row.size()*sizeof(double)))throw std::runtime_error("Spectral trace write failed");
  first=false;}++records;
  auto accumulate=[&](pigment::YabPlanes d,const pigment::SpectralMixTrace &m){
   auto c=m.bypass?transform.toYab({float(s.encode.sceneRGB[0]),float(s.encode.sceneRGB[1]),float(s.encode.sceneRGB[2])}):m.nonlinearYab;
   d.y.at(s.x,s.y)+=s.alpha*c.y;d.a.at(s.x,s.y)+=s.alpha*c.a;d.b.at(s.x,s.y)+=s.alpha*c.b;
  };accumulate(yRGB.view(),s.yMix);accumulate(abRGB.view(),s.abMix);
 };
 std::ofstream notes(dir/"provenance.txt");notes<<"C0 frozen upstream snapshot; exact same prepared transport for every law. No A1-B rerender.\n"
  <<"spill="<<p.spillAmount<<" reach="<<p.spillReach<<" Y="<<p.lumaSpill<<" AB="<<p.chromaSpill<<" density="<<p.pigmentDensity<<"\n"
  <<"Trace is native little-endian doubles, one record per receiver per pixel; schema lists columns.\n"
  <<"No gamut fallback or NaN repair exists in this law: invalid flags are measurement only. Floors and coefficient clamps are counted; extrema recoverable from all 21 samples.\n";
 writeYabPfm(dir/"source-yab.pfm",source);
 for(auto law:{pigment::ColorInteractionLaw::LinearYAB,pigment::ColorInteractionLaw::Density,pigment::ColorInteractionLaw::SpectralPigment}){
  p.colorInteraction=law;std::string label=law==pigment::ColorInteractionLaw::LinearYAB?"linear":law==pigment::ColorInteractionLaw::Density?"density":"spectral";
  const auto out=pigment::applyPhase4Spill(source,a.plates,synthesis,a.analysisGraph,p,{},&transport,pigment::WorkingGamut::ACEScg,
   law==pigment::ColorInteractionLaw::SpectralPigment?pigment::SpectralSpillObserver(observer):pigment::SpectralSpillObserver{},massCandidate);
  writeYabPfm(dir/(label+"-yab.pfm"),out.composite.view());writeAppearance(dir/(label+".ppm"),out.composite.view(),opaque.view(),transform);
  if(law==pigment::ColorInteractionLaw::SpectralPigment){
   // Prove instrumentation does not change the renderer, rather than assume it.
   const auto plain=pigment::applyPhase4Spill(source,a.plates,synthesis,a.analysisGraph,p,{},&transport,pigment::WorkingGamut::ACEScg,{},massCandidate);
   for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){auto v=plain.composite.view(),w=out.composite.view();
    if(v.y.at(x,y)!=w.y.at(x,y)||v.a.at(x,y)!=w.a.at(x,y)||v.b.at(x,y)!=w.b.at(x,y))throw std::runtime_error("Tracing changed spectral output");}
  }
 }
 if(gz)gzclose(gz);notes<<"records="<<records<<" dimensions="<<b.width()<<'x'<<b.height()<<" plates="<<a.plates.count()<<" observer/no-observer bit-exact; scene-mass-candidate="<<massCandidate<<"\n";
 for(bool ab:{false,true}){auto v=ab?abRGB.view():yRGB.view();auto label=ab?"spectral-AB-weights-RGB":"spectral-Y-weights-RGB";
  writeYabPfm(dir/(std::string(label)+".pfm"),pigment::asConst(v));writeAppearance(dir/(std::string(label)+".ppm"),pigment::asConst(v),opaque.view(),transform);}
}
}
