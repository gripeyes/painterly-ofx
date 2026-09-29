#include "core/LatentPlateGraph.h"
#include "core/PigmentPhase4.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace {
int failures=0;
void check(bool v,const char*m){if(!v){++failures;std::cerr<<"FAIL: "<<m<<'\n';}}

pigment::OwnedYabPlanes fixture(pigment::RectI b){pigment::OwnedYabPlanes image(b);auto v=image.view();for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){float fx=float(x-b.x1)/std::max(1,b.width()-1),fy=float(y-b.y1)/std::max(1,b.height()-1);v.y.at(x,y)=-.3f+2.6f*fx+.07f*std::sin(.7f*x);v.a.at(x,y)=.4f*(fy-.5f)+.08f*std::sin(.3f*x);v.b.at(x,y)=fx<.45f?-.3f:.5f;}return image;}

void parameterSemantics(){pigment::Phase4Params p;p.plateScale=50;p.plateOverlap=0;check(pigment::phase4SupportRadiusY(p)==0&&pigment::phase4SupportRadiusAB(p)==0,"zero overlap has no support expansion");p.plateOverlap=.5f;p.lumaChunkScale=25;p.chromaChunkScale=100;p.lumaChromaCoupling=0;check(std::abs(pigment::phase4SupportRadiusY(p)-25)<1e-6f,"Y support radius is Scale times Overlap");check(std::abs(pigment::phase4SupportRadiusAB(p)-50)<1e-6f,"AB support uses independent chunk-scale ratio");p.lumaChromaCoupling=1;check(std::abs(pigment::phase4SupportRadiusAB(p)-25)<1e-6f,"full coupling equalizes support radii");check(std::abs(pigment::phase4PlateEntropyCoefficient(.5f)-.25f)<1e-6f,"overlap entropy coefficient is squared");}

void automaticPlates(){pigment::RectI b{0,0,48,36};auto image=fixture(b);pigment::Phase4Params p;p.latentCount=12;p.plateCount=4;p.plateScale=12;p.plateOverlap=.55f;p.lumaChunkScale=8;p.chromaChunkScale=24;auto result=pigment::buildPhase4AutomaticPlates(static_cast<const pigment::OwnedYabPlanes&>(image).view(),p,{});check(result.latent.count()==12&&result.plates.count()==4,"requested latent and public counts are preserved");double reconstruction=0;for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){float ls=0,ps=0;for(int i=0;i<12;++i)ls+=result.latent.alpha(i).at(x,y);for(int i=0;i<4;++i)ps+=result.plates.alpha(i).at(x,y);check(std::abs(ls-1)<3e-3f,"latent alpha sums to one");check(std::abs(ps-1)<3e-3f,"plate alpha sums to one");reconstruction+=result.latent.reconstructionError().at(x,y);}check(std::isfinite(reconstruction),"appearance reconstruction is finite");check(result.diagnostics.meanEffectiveComponents>1.0f,"component recovery is fuzzy rather than entirely hard");check(result.diagnostics.eigenspaceFinite&&result.diagnostics.componentsFinite&&result.diagnostics.appearanceFinite,"Gate A diagnostics are finite");}

void renderIdentityAndAlpha(){pigment::RectI b{-2,3,18,17};int stride=b.width()*4+3;std::vector<float>src(size_t(stride)*b.height(),-5),dst(src.size(),-9);for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){size_t i=size_t(y-b.y1)*stride+(x-b.x1)*4;float a=.2f+.8f*float((x+y+20)%9)/8;src[i]=a*(.1f+.03f*x);src[i+1]=a*(-.2f+.04f*y);src[i+2]=a*((x%5)?0.3f:2.0f);src[i+3]=a;}pigment::IntegratedPigmentParams p;p.comparison=pigment::PigmentComparisonMode::AutomaticPlateGraph;p.premultiplied=true;p.phase4.latentCount=12;p.phase4.plateCount=4;p.phase4.plateScale=8;p.amount=0;pigment::Phase4RenderInputs in{{src.data(),stride,b,4},{dst.data(),stride,b,4},b,p,{}};pigment::processPigmentPhase4(in);for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){size_t i=size_t(y-b.y1)*stride+(x-b.x1)*4;for(int c=0;c<4;++c)check(dst[i+c]==src[i+c],"Amount zero is bit-exact and preserves alpha");}}
}
int main(){parameterSemantics();automaticPlates();renderIdentityAndAlpha();if(failures)return 1;std::cout<<"All Phase 4 Gate-A tests passed\n";}
