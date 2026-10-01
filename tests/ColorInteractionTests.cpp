#include "core/ColorInteraction.h"
#include "core/PlateSpill.h"
#include "core/PigmentControls.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
int main(){try{
 auto require=[](bool v,const char*m){if(!v)throw std::runtime_error(m);};
 for(auto gamut:{pigment::WorkingGamut::ACEScg,pigment::WorkingGamut::LinearRec709,pigment::WorkingGamut::LinearRec2020,pigment::WorkingGamut::DisplayP3D65}){
  pigment::MatrixOpponentTransform transform(gamut);pigment::ColorInteraction interaction(gamut);
  for(auto law:{pigment::ColorInteractionLaw::Density,pigment::ColorInteractionLaw::SpectralPigment}){
   for(std::array<float,3> rgb: {std::array<float,3>{0,0,0},{.2f,.2f,.2f},{8.f,2.f,.5f},{-.2f,4.f,-.7f},{-1.f,-2.f,-3.f},{100000.f,2000.f,-100.f},{.000001f,.000002f,.000003f}}){
    auto yab=transform.toYab(rgb);auto m=interaction.encode(yab,law);auto back=interaction.reconstruct(m,law);
    require(std::isfinite(m.fitError) && std::isfinite(m.residualMagnitude),"material diagnostics finite");
    auto represented=transform.toRgb(yab); // isolate from existing float RGB/YAB roundtrip
    double scale=std::max({1.,std::abs(double(rgb[0])),std::abs(double(rgb[1])),std::abs(double(rgb[2]))});
    for(int k=0;k<3;++k){require(std::abs(back[k]-represented[k])<1e-10*scale,"material/residual split reverses its exact encoded color");require(std::abs(back[k]-rgb[k])<2e-6*scale,"HDR/negative scene roundtrip within existing float precision");}
    float weights[2]={1,0};pigment::InteractionMaterial pair[2]={m,m};auto endpoint=interaction.mix(pair,weights,2,yab,law,1);
    require(endpoint.y==yab.y && endpoint.a==yab.a && endpoint.b==yab.b,"zero-donor endpoint exact");
    float equalWeights[2]={1,.7f};pigment::SpectralEncodeTrace tracedEncode;auto traced=interaction.encode(yab,law,&tracedEncode);
    require(traced.coefficients==m.coefficients && traced.absorption==m.absorption && traced.residual==m.residual,"encode tracing leaves all material state bit-exact");
    pigment::SpectralMixTrace tracedMix;auto identical=interaction.mix(pair,equalWeights,2,yab,law,1,&tracedMix);
    auto untraced=interaction.mix(pair,equalWeights,2,yab,law,1);
    require(identical.y==untraced.y && identical.a==untraced.a && identical.b==untraced.b,"mix tracing leaves output bit-exact");
    auto same=transform.toRgb(identical);for(int k=0;k<3;++k)require(std::abs(same[k]-represented[k])<2e-6*scale,"identical material interaction preserves appearance within float precision");
   }
  auto warm=transform.toYab({.8f,.35f,.12f}),cyan=transform.toYab({.05f,.6f,.7f});
   pigment::InteractionMaterial greys[2]={interaction.encode(transform.toYab({.1f,.1f,.1f}),law),interaction.encode(transform.toYab({4.f,4.f,4.f}),law)};
   float greyWeights[2]={1,.4f};auto grey=transform.toRgb(interaction.mix(greys,greyWeights,2,transform.toYab({1.2142857f,1.2142857f,1.2142857f}),law,1));
   require(std::abs(grey[0]-grey[1])+std::abs(grey[1]-grey[2])<2e-6,"positive neutral HDR mixture remains neutral");
   pigment::InteractionMaterial m[2]={interaction.encode(warm,law),interaction.encode(cyan,law)};
   float w[2]={1,.7f};pigment::YabPixel linear{(warm.y+.7f*cyan.y)/1.7f,(warm.a+.7f*cyan.a)/1.7f,(warm.b+.7f*cyan.b)/1.7f};
   auto keep=interaction.mix(m,w,2,linear,law,0),dense=interaction.mix(m,w,2,linear,law,1),again=interaction.mix(m,w,2,linear,law,1);
   require(keep.y==linear.y,"Density zero preserves scene luminance exactly");
   require(dense.y==again.y && dense.a==again.a && dense.b==again.b,"color law deterministic");
   require(std::abs(dense.a-linear.a)+std::abs(dense.b-linear.b)>1e-4,"nonlinear law is functioning, not a placeholder");
   require(std::abs(dense.y-linear.y)>1e-4,"Density admits material absorption luminance");
   require(keep.a==dense.a && keep.b==dense.b,"Pigment Density changes luminance only, not chroma or transport");
   // Signed luminance cancellation must not become a chroma normalization pole.
   pigment::InteractionMaterial cancellation[2];
   cancellation[0]=interaction.encode(transform.toYab({-1.f,1.f,-1.f}),law);
   cancellation[1]=interaction.encode(transform.toYab({1.f,-1.f,1.f}),law);
   float equal[2]={1,1};auto stable=interaction.mix(cancellation,equal,2,{0,0,0},law,0);
   require(stable.y==0 && std::isfinite(stable.a) && std::isfinite(stable.b) && std::abs(stable.a)<4 && std::abs(stable.b)<4,"negative/HDR cancellation has no division-driven chroma spike");
  }
 }
 pigment::RectI b{0,0,2,1};pigment::PublicPlateSet plates(b,4);pigment::OwnedYabPlanes source(b);pigment::Phase4ChunkSynthesis s(b);
 {
  pigment::MatrixOpponentTransform tr(pigment::WorkingGamut::ACEScg);pigment::ColorInteraction candidate(pigment::WorkingGamut::ACEScg,true),baseline(pigment::WorkingGamut::ACEScg);
  auto grey=tr.toYab({4,4,4});auto black=tr.toYab({0,0,0});auto tiny=tr.toYab({1e-8f,1e-8f,1e-8f});float weights[2]={1,.5f};
  pigment::InteractionMaterial atZero[2]={candidate.encode(black,pigment::ColorInteractionLaw::SpectralPigment),candidate.encode(grey,pigment::ColorInteractionLaw::SpectralPigment)};
  pigment::InteractionMaterial atTiny[2]={candidate.encode(tiny,pigment::ColorInteractionLaw::SpectralPigment),atZero[1]};
  auto z=tr.toRgb(candidate.mix(atZero,weights,2,{1.3333333f,0,0},pigment::ColorInteractionLaw::SpectralPigment,1));
  auto t=tr.toRgb(candidate.mix(atTiny,weights,2,{1.3333333f,0,0},pigment::ColorInteractionLaw::SpectralPigment,1));
  for(int k=0;k<3;++k)require(std::abs(z[k]-t[k])<1e-6,"residual-only black has no finite absorption contribution or zero-intensity jump");
  for(auto rgb:{std::array<float,3>{-.2f,-.1f,-.3f},{8,2,-.1f},{.8f,.35f,.12f}}){
   auto c=tr.toYab(rgb);auto m=candidate.encode(c,pigment::ColorInteractionLaw::SpectralPigment);auto back=candidate.reconstruct(m,pigment::ColorInteractionLaw::SpectralPigment);
   auto expected=tr.toRgb(c);for(int k=0;k<3;++k)require(std::abs(back[k]-expected[k])<1e-8,"candidate residual remains reversible for HDR/negative");
   auto old=baseline.encode(c,pigment::ColorInteractionLaw::Density),keep=candidate.encode(c,pigment::ColorInteractionLaw::Density);
   require(old.absorption==keep.absorption && old.residual==keep.residual,"experimental spectral scene-mass policy cannot change Density");
  }
 }
 pigment::SparseAffinityGraph graph;graph.width=2;graph.height=1;graph.rowOffsets={0,1,1};graph.edges={{1,-2,.8f,1,0}};
 for(int i=0;i<4;++i){s.plateAppearance.emplace_back(b);for(int x=0;x<2;++x){plates.alpha(i).at(x,0)=.25f;plates.supportY(i).at(x,0)=.2f+i*.1f;plates.supportAB(i).at(x,0)=.7f;
  auto v=s.plateAppearance.back().view();v.y.at(x,0)=.2f+i*.1f;v.a.at(x,0)=.02f*i;v.b.at(x,0)=-.03f*i;}}
 pigment::Phase4Params p;p.spillAmount=.8f;p.spillReach=128;p.lumaSpill=0;p.chromaSpill=1;p.pigmentDensity=1;
 const auto &original=source;
 auto prepared=pigment::preparePhase4SpillTransport(plates,graph,p);auto base=pigment::applyPhase4Spill(original.view(),plates,s,graph,p,{},&prepared);
 for(auto law:{pigment::ColorInteractionLaw::Density,pigment::ColorInteractionLaw::SpectralPigment}){
  p.colorInteraction=law;auto r=pigment::applyPhase4Spill(original.view(),plates,s,graph,p,{},&prepared);
  for(int x=0;x<2;++x){require(r.composite.view().y.at(x,0)==base.composite.view().y.at(x,0),"AB-only law keeps composite Y bit exact");
   for(int i=0;i<4;++i){require(r.transportY[i].view().at(x,0)==base.transportY[i].view().at(x,0) && r.transportAB[i].view().at(x,0)==base.transportAB[i].view().at(x,0),"laws use identical geodesic transport");
    require(r.influence[i].view().at(x,0)==base.influence[i].view().at(x,0) && r.influenceY[i].view().at(x,0)==base.influenceY[i].view().at(x,0),"laws use identical appearance interaction weights");
    require(plates.alpha(i).at(x,0)==.25f,"alpha unchanged");}}
  p.spillAmount=0;auto zero=pigment::applyPhase4Spill(original.view(),plates,s,graph,p);auto linearP=p;linearP.colorInteraction=pigment::ColorInteractionLaw::LinearYAB;
  auto exact=pigment::applyPhase4Spill(original.view(),plates,s,graph,linearP);
  for(int x=0;x<2;++x)require(zero.composite.view().y.at(x,0)==exact.composite.view().y.at(x,0) && zero.composite.view().a.at(x,0)==exact.composite.view().a.at(x,0),"zero Spill exact in every law");p.spillAmount=.8f;
 }
 p.colorInteraction=pigment::ColorInteractionLaw::SpectralPigment;p.pigmentDensity=.7f;
 auto macros=pigment::mapPigmentControls({},p);require(macros.colorInteraction==p.colorInteraction && macros.pigmentDensity==p.pigmentDensity,"artist macros preserve isolated color layer");
 std::cout<<"Color interaction tests passed; photographic selection remains separate\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
