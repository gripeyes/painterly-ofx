#include "core/ColorInteraction.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
int main(int argc,char**argv){
  std::ofstream f(argc>1?argv[1]:"spectral-continuity.csv");f<<std::setprecision(17);
  f<<"sweep,step,t,R,G,B,magnitude,c0,c1,c2,fit_error,residual,coefficient_bound,reflectance_floor,r_min,r_max,ks_max,mixed_R,mixed_G,mixed_B,max_coefficient_step,max_reflectance_step,max_ks_step,max_rgb_step\n";
  bool sceneMass=argc>2 && std::string(argv[2])=="--mass-weighted";
  auto chosenLaw=argc>2 && std::string(argv[2])=="--density"?pigment::ColorInteractionLaw::Density:pigment::ColorInteractionLaw::SpectralPigment;
  pigment::MatrixOpponentTransform tr(pigment::WorkingGamut::ACEScg);pigment::ColorInteraction law(pigment::WorkingGamut::ACEScg,sceneMass);
  std::ofstream edge(std::string(argc>1?argv[1]:"spectral-continuity.csv")+".endpoints.csv");edge<<std::setprecision(17)<<"weight,R,G,B,ks_max,fit_error\n";
  auto warm=tr.toYab({.8f,.35f,.12f}),cyan=tr.toYab({.05f,.6f,.7f});
  pigment::InteractionMaterial endpoints[2]={law.encode(warm,chosenLaw),law.encode(cyan,chosenLaw)};
  for(float t:{0.f,1e-12f,1e-10f,1e-8f,1e-6f,1e-4f,1e-3f,.01f,.1f,1.f}){
    float w[2]={1,t};pigment::YabPixel linear{(warm.y+t*cyan.y)/(1+t),(warm.a+t*cyan.a)/(1+t),(warm.b+t*cyan.b)/(1+t)};
    auto mixed=tr.toRgb(law.mix(endpoints,w,2,linear,chosenLaw,1));edge<<t;for(auto v:mixed)edge<<','<<v;
    edge<<','<<*std::max_element(endpoints[1].absorption.begin(),endpoints[1].absorption.end())<<','<<endpoints[1].fitError<<'\n';
  }
  struct Sweep{const char*name;std::array<float,3> a,b;};
  std::vector<Sweep> sweeps={{"neutral",{0,0,0},{4,4,4}},{"warm-cyan",{.8,.35,.12},{.05,.6,.7}},
    {"warm-green",{.8,.35,.12},{.02,.7,.06}},{"cyan-blueblack",{.05,.6,.7},{.0001,.001,.005}},
    {"saturated-black",{1,0,0},{0,0,0}},{"low-positive",{1e-9,2e-9,3e-9},{1e-4,2e-4,3e-4}},
    {"negative",{-.01,.1,.2},{.01,.1,.2}},{"HDR",{.1,.2,.3},{4,2,8}},
    {"weight",{.8,.35,.12},{.05,.6,.7}},{"density",{.8,.35,.12},{.05,.6,.7}}};
  for(auto sweep:sweeps){pigment::InteractionMaterial prev; pigment::SpectralEncodeTrace previous;std::array<float,3> last{};
    double jumps[4]{};
    for(int step=0;step<=2048;++step){float t=step/2048.f;std::array<float,3> rgb;
      bool weight=std::string(sweep.name)=="weight",density=std::string(sweep.name)=="density";
      for(int k=0;k<3;++k)rgb[k]=(weight||density)?sweep.a[k]:sweep.a[k]+t*(sweep.b[k]-sweep.a[k]);
      auto c=tr.toYab(rgb);pigment::SpectralEncodeTrace trace;auto m=law.encode(c,chosenLaw,&trace);
      auto donor=tr.toYab(sweep.b);pigment::InteractionMaterial pair[2]={m,law.encode(donor,chosenLaw)};
      float w[2]={1,weight?t:.5f};pigment::YabPixel linear{(c.y+w[1]*donor.y)/(1+w[1]),(c.a+w[1]*donor.a)/(1+w[1]),(c.b+w[1]*donor.b)/(1+w[1])};
      auto out=law.mix(pair,w,2,linear,chosenLaw,density?t:1);auto mixed=tr.toRgb(out);
      double dc=0,dr=0,dk=0,dm=0;
      if(step){for(int k=0;k<3;++k){dc=std::max(dc,std::abs(m.coefficients[k]-prev.coefficients[k]));dm=std::max(dm,std::abs(double(mixed[k]-last[k])));}
        for(int k=0;k<pigment::kInteractionSamples;++k){dr=std::max(dr,std::abs(trace.reflectance[k]-previous.reflectance[k]));dk=std::max(dk,std::abs(m.absorption[k]-prev.absorption[k]));}}
      jumps[0]=std::max(jumps[0],dc);jumps[1]=std::max(jumps[1],dr);jumps[2]=std::max(jumps[2],dk);jumps[3]=std::max(jumps[3],dm);
      f<<sweep.name<<','<<step<<','<<t;for(auto v:rgb)f<<','<<v;f<<','<<m.magnitude;for(auto v:m.coefficients)f<<','<<v;
      f<<','<<m.fitError<<','<<m.residualMagnitude<<','<<trace.coefficientBound<<','<<trace.reflectanceFloor<<','<<*std::min_element(trace.reflectance.begin(),trace.reflectance.end())<<','<<*std::max_element(trace.reflectance.begin(),trace.reflectance.end())<<','<<*std::max_element(m.absorption.begin(),m.absorption.end());
      for(auto v:mixed)f<<','<<v;f<<','<<dc<<','<<dr<<','<<dk<<','<<dm<<'\n';prev=m;previous=trace;last=mixed;
    }
    std::cout<<sweep.name<<" max adjacent jumps c/R/KS/RGB: "<<jumps[0]<<' '<<jumps[1]<<' '<<jumps[2]<<' '<<jumps[3]<<'\n';
  }
}
