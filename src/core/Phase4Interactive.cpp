#include "core/Phase4Interactive.h"
#include "core/ColorSpace.h"
#include <Eigen/Dense>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace pigment {
namespace {
std::array<float,3> straight(const float* s,int components,bool premult){
  float alpha=components==4?s[3]:1;
  float divisor=premult && std::abs(alpha)>1e-6f?alpha:1;
  return {s[0]/divisor,s[1]/divisor,s[2]/divisor};
}
}
Phase4RenderDiagnostics processPhase4Interactive(const Phase4RenderInputs& in,
    Phase4InteractiveCache& cache,int analysisLongEdge,const ExecutionContext& execution){
  const auto start=std::chrono::steady_clock::now();
  const auto b=in.source.bounds;const auto& p=in.params;
  if(p.debugView==PigmentDebugView::Phase4Source || (p.debugView==PigmentDebugView::Final && (p.amount==0 || p.mix==0)))
    return processPigmentPhase4(in,execution);
  analysisLongEdge=std::clamp(analysisLongEdge,64,512);
  float scale=std::min(1.f,float(analysisLongEdge)/std::max(b.width(),b.height()));
  RectI low{0,0,std::max(1,int(std::lround(b.width()*scale))),std::max(1,int(std::lround(b.height()*scale)))};
  uint64_t hash=1469598103934665603ull;
  for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x)for(int c=0;c<in.source.components;++c){uint32_t bits;std::memcpy(&bits,in.source.pixel(x,y)+c,4);hash^=bits;hash*=1099511628211ull;}
  std::vector<double> key{double(hash>>32),double(uint32_t(hash)),double(b.x1),double(b.y1),double(b.x2),double(b.y2),double(low.x2),double(low.y2),double(p.gamut),double(p.premultiplied)};
  if(cache.sourceKey!=key){
    std::vector<float> source(size_t(low.width())*low.height()*4);
    execution.parallelRows(0,low.height(),[&](int first,int last){for(int y=first;y<last;++y)for(int x=0;x<low.width();++x){
      int x0=b.x1+int(int64_t(x)*b.width()/low.width()),x1=b.x1+int(int64_t(x+1)*b.width()/low.width());
      int y0=b.y1+int(int64_t(y)*b.height()/low.height()),y1=b.y1+int(int64_t(y+1)*b.height()/low.height());
      std::array<double,3> sum{};int count=0;
      for(int yy=y0;yy<y1;++yy)for(int xx=x0;xx<x1;++xx){auto rgb=straight(in.source.pixel(xx,yy),in.source.components,p.premultiplied);for(int c=0;c<3;++c)sum[c]+=rgb[c];++count;}
      for(int c=0;c<3;++c)source[(size_t(y)*low.width()+x)*4+c]=float(sum[c]/count);
      source[(size_t(y)*low.width()+x)*4+3]=1;
    }});
    MatrixOpponentTransform transform(p.gamut);
    std::vector<Eigen::Vector3d> colors(size_t(low.width())*low.height());
    Eigen::Vector3d mean=Eigen::Vector3d::Zero(),variance=Eigen::Vector3d::Zero();
    for(size_t i=0;i<colors.size();++i){auto v=transform.toYab({source[i*4],source[i*4+1],source[i*4+2]});colors[i]={v.y,v.a,v.b};mean+=colors[i];}
    mean/=double(colors.size());for(auto v:colors)variance+=(v-mean).cwiseAbs2();
    Eigen::Vector3d conditioning=(variance/double(colors.size())).cwiseSqrt().cwiseMax(1e-4);
    std::vector<Phase4GuidedSample> guidance(size_t(b.width())*b.height());
    execution.parallelRows(b.y1,b.y2,[&](int first,int last){for(int y=first;y<last;++y){if(execution.cancelled())throw std::runtime_error("Interactive guidance cancelled");for(int x=b.x1;x<b.x2;++x){
      double ax=std::clamp((double(x-b.x1)+.5)*low.width()/b.width()-.5,0.,double(low.width()-1));
      double ay=std::clamp((double(y-b.y1)+.5)*low.height()/b.height()-.5,0.,double(low.height()-1));
      int xx=int(ax),yy=int(ay);double tx=ax-xx,ty=ay-yy;
      auto rgb=straight(in.source.pixel(x,y),in.source.components,p.premultiplied);auto v=transform.toYab(rgb);Eigen::Vector3d target(v.y,v.a,v.b);
      auto& sample=guidance[size_t(y-b.y1)*b.width()+x-b.x1];
      Eigen::Matrix<double,3,4> residual;Eigen::Vector4d prior;
      for(int i=0;i<4;++i){int ox=i%2,oy=i/2;int q=std::min(low.height()-1,yy+oy)*low.width()+std::min(low.width()-1,xx+ox);sample.index[i]=q;
        residual.col(i)=(colors[size_t(q)]-target).cwiseQuotient(conditioning);prior[i]=(ox?tx:1-tx)*(oy?ty:1-ty);}
      // Color-mixture affine reconstruction, not an image smoothing pass.
      // The full-resolution source guides only interpolation coefficients.
      Eigen::Matrix4d gram=residual.transpose()*residual;gram.diagonal().array()+=.05;
      auto factor=gram.ldlt();Eigen::Vector4d ones=factor.solve(Eigen::Vector4d::Ones()),solution=factor.solve(.05*prior);
      solution+=ones*((1-solution.sum())/ones.sum());solution=solution.cwiseMax(0.);
      double mass=solution.sum();if(!std::isfinite(mass) || mass<=1e-12)solution=prior;else solution/=mass;
      for(int i=0;i<4;++i)sample.weight[i]=float(solution[i]);
    }}});
    if(execution.cancelled())throw std::runtime_error("Interactive source cancelled");
    cache.source=std::move(source);cache.guidance=std::move(guidance);cache.analysisBounds=low;cache.sourceKey=key;cache.result.resize(cache.source.size());++cache.guidanceBuilds;
  }
  Phase4RenderInputs reduced=in;
  reduced.source={cache.source.data(),low.width()*4,low,4};reduced.destination={cache.result.data(),low.width()*4,low,4};reduced.renderWindow=low;
  reduced.mask=nullptr;reduced.cache=&cache.stages;reduced.params.premultiplied=false;reduced.params.amount=1;reduced.params.mix=1;reduced.params.invertMask=false;
  // Explicit preview geometry approximation; the Full path is never rescaled.
  reduced.params.phase4.plateScale*=scale;reduced.params.phase4.spillReach*=scale;
  reduced.params.phase4.lumaChunkScale*=scale;reduced.params.phase4.chromaChunkScale*=scale;
  reduced.geometry.renderScaleX*=double(low.width())/b.width();reduced.geometry.renderScaleY*=double(low.height())/b.height();
  auto diagnostics=processPigmentPhase4(reduced,execution);
  execution.parallelRows(in.renderWindow.y1,in.renderWindow.y2,[&](int first,int last){for(int y=first;y<last;++y){if(execution.cancelled())throw std::runtime_error("Interactive output cancelled");for(int x=in.renderWindow.x1;x<in.renderWindow.x2;++x){
    auto s=in.source.pixel(x,y);auto d=in.destination.pixel(x,y);auto original=straight(s,in.source.components,p.premultiplied);
    // Lift the organized effect, not a downsampled photograph. This explicit
    // preview retains unresolved source detail; it is not Full parity.
    const bool final=p.debugView==PigmentDebugView::Final;
    const auto& sample=cache.guidance[size_t(y-b.y1)*b.width()+x-b.x1];std::array<float,3> target=final?original:std::array<float,3>{};
    for(int i=0;i<4;++i)for(int c=0;c<3;++c){size_t q=size_t(sample.index[i])*4+c;target[c]+=sample.weight[i]*(cache.result[q]-(final?cache.source[q]:0.f));}
    float alpha=in.source.components==4?s[3]:1,gate=1;
    if(p.debugView==PigmentDebugView::Final){float mask=in.mask?(in.mask->bounds.contains(x,y)?std::clamp(in.mask->pixel(x,y)[0],0.f,1.f):0.f):1.f;if(p.invertMask)mask=1-mask;gate=std::clamp(p.amount,0.f,1.f)*mask*std::clamp(p.mix,0.f,1.f);}
    for(int c=0;c<3;++c){float value=original[c]+gate*(target[c]-original[c]);if(p.premultiplied && std::abs(alpha)>1e-6f)value*=alpha;if((p.premultiplied && std::abs(alpha)<=1e-6f)||gate==0)value=s[c];d[c]=value;}
    if(in.destination.components==4)d[3]=alpha;
  }}});
  diagnostics.totalMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return diagnostics;
}
}
