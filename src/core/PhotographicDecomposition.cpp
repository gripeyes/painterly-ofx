#include "core/PhotographicDecomposition.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace pigment {
namespace {
float clamp01(float x) noexcept { return std::max(0.0f, std::min(1.0f, x)); }
float robustScale(ConstFloatPlaneView source) {
  std::vector<float> values; const RectI b=source.bounds;
  const int step=std::max(1,std::max(b.width(),b.height())/128);
  for(int y=b.y1;y<b.y2;y+=step)for(int x=b.x1;x<b.x2;x+=step)values.push_back(std::abs(source.at(x,y)));
  if(values.empty())return 1;auto m=values.begin()+values.size()/2;std::nth_element(values.begin(),m,values.end());return std::max(1e-4f,*m);
}
float energy(ConstFloatPlaneView u,ConstFloatPlaneView source,const std::vector<float>&vx,const std::vector<float>&vy,
             ScalarFieldView protection,float a1,float a0,float hx,float hy){
  RectI b=u.bounds;double e=0;auto id=[&](int x,int y){return size_t(y-b.y1)*b.width()+x-b.x1;};
  for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){int xr=std::min(b.x2-1,x+1),yu=std::min(b.y2-1,y+1);size_t i=id(x,y);float dx=(u.at(xr,y)-u.at(x,y))/hx,dy=(u.at(x,yu)-u.at(x,y))/hy;float dvxx=(vx[id(xr,y)]-vx[i])/hx,dvyy=(vy[id(x,yu)]-vy[i])/hy;float dvxy=.5f*((vx[id(x,yu)]-vx[i])/hy+(vy[id(xr,y)]-vy[i])/hx);float g=std::max(1e-3f,1-clamp01(protection.at(x,y)));float d=u.at(x,y)-source.at(x,y);e+=.5*d*d+a1*g*std::hypot(dx-vx[i],dy-vy[i])+a0*g*std::sqrt(dvxx*dvxx+dvyy*dvyy+2*dvxy*dvxy);}
  return float(e/std::max(1,b.width()*b.height()));
}
}

float tgvOperatorNormSquaredBound(float hx,float hy) noexcept {hx=std::max(1e-6f,hx);hy=std::max(1e-6f,hy);float grad=4/(hx*hx)+4/(hy*hy);return std::max(2*grad,2+grad);}

void sourceSmoothWls(ConstFloatPlaneView source,ScalarFieldView protection,float simplification,float preserve,const ImageGeometry&geometry,FloatPlaneView destination,const ExecutionContext&e){RectI b=source.bounds;OwnedPlane conductance(b);auto g=conductance.view();e.parallelRows(b.y1,b.y2,[&](int y0,int y1){for(int y=y0;y<y1;++y)for(int x=b.x1;x<b.x2;++x)g.at(x,y)=std::max(1e-4f,1-clamp01(preserve)*clamp01(protection.at(x,y)));});float radius=.5f+31.5f*clamp01(simplification)*clamp01(simplification);ScreenedMultigridParams p;p.lambdaX=radius*radius*float(geometry.renderScaleX*geometry.renderScaleX/std::max(1e-9,geometry.pixelAspect*geometry.pixelAspect));p.lambdaY=radius*radius*float(geometry.renderScaleY*geometry.renderScaleY);p.vCycles=4;solveScreenedMultigrid(source,1.0f,static_cast<const OwnedPlane&>(conductance).view(),destination,p,e);}

TgvDiagnostics tgvRegularizedSmooth(ConstFloatPlaneView source,ScalarFieldView protection,float simplification,float preserve,const ImageGeometry&geometry,FloatPlaneView destination,const ExecutionContext&e){
  TgvDiagnostics d;RectI b=source.bounds;int w=b.width(),h=b.height();size_t n=size_t(w)*h;float hx=float(std::max(1e-6,geometry.pixelAspect/std::max(1e-6,geometry.renderScaleX))),hy=float(1/std::max(1e-6,geometry.renderScaleY));d.gradientNormSquaredBound=4/(hx*hx)+4/(hy*hy);d.operatorNormSquaredBound=tgvOperatorNormSquaredBound(hx,hy);d.tau=d.sigma=.95f/std::sqrt(d.operatorNormSquaredBound);
  float radius=.5f+31.5f*clamp01(simplification)*clamp01(simplification),q=robustScale(source),a1=q*radius,a0=.5f*q*radius*radius;
  std::vector<float>u(n),old(n),ubar(n),vx(n),vy(n),vxo(n),vyo(n),vxb(n),vyb(n),px(n),py(n),qxx(n),qyy(n),qxy(n);auto id=[&](int x,int y){return size_t(y-b.y1)*w+x-b.x1;};for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x)u[id(x,y)]=ubar[id(x,y)]=source.at(x,y);d.initialEnergy=energy({u.data(),w,b},source,vx,vy,protection,a1,a0,hx,hy);
  for(int iteration=0;iteration<80&&!e.cancelled();++iteration){
    // Dual ascent and projection.
    for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){int xr=std::min(b.x2-1,x+1),yu=std::min(b.y2-1,y+1);size_t i=id(x,y);float gx=(ubar[id(xr,y)]-ubar[i])/hx-vxb[i],gy=(ubar[id(x,yu)]-ubar[i])/hy-vyb[i];px[i]+=d.sigma*gx;py[i]+=d.sigma*gy;float limit=a1*std::max(1e-3f,1-clamp01(preserve)*clamp01(protection.at(x,y))),pn=std::hypot(px[i],py[i]);if(pn>limit){px[i]*=limit/pn;py[i]*=limit/pn;}float exx=(vxb[id(xr,y)]-vxb[i])/hx,eyy=(vyb[id(x,yu)]-vyb[i])/hy,exy=.5f*((vxb[id(x,yu)]-vxb[i])/hy+(vyb[id(xr,y)]-vyb[i])/hx);qxx[i]+=d.sigma*exx;qyy[i]+=d.sigma*eyy;qxy[i]+=d.sigma*exy;float qlimit=a0*std::max(1e-3f,1-clamp01(preserve)*clamp01(protection.at(x,y))),qn=std::sqrt(qxx[i]*qxx[i]+qyy[i]*qyy[i]+2*qxy[i]*qxy[i]);if(qn>qlimit){qxx[i]*=qlimit/qn;qyy[i]*=qlimit/qn;qxy[i]*=qlimit/qn;}}
    old=u;vxo=vx;vyo=vy;
    // Primal proximal update using K^T.
    double change=0;for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){int xl=std::max(b.x1,x-1),yd=std::max(b.y1,y-1);size_t i=id(x,y);float divp=(px[i]-(x==b.x1?0:px[id(xl,y)]))/hx+(py[i]-(y==b.y1?0:py[id(x,yd)]))/hy;float ktU=-divp;u[i]=(u[i]-d.tau*ktU+d.tau*source.at(x,y))/(1+d.tau);float estX=-(qxx[i]-(x==b.x1?0:qxx[id(xl,y)]))/hx-(qxy[i]-(y==b.y1?0:qxy[id(x,yd)]))/hy;float estY=-(qxy[i]-(x==b.x1?0:qxy[id(xl,y)]))/hx-(qyy[i]-(y==b.y1?0:qyy[id(x,yd)]))/hy;vx[i]-=d.tau*(-px[i]+estX);vy[i]-=d.tau*(-py[i]+estY);float du=u[i]-old[i];change+=du*du;}
    d.primalChangeRms=float(std::sqrt(change/std::max<size_t>(1,n)));for(size_t i=0;i<n;++i){ubar[i]=2*u[i]-old[i];vxb[i]=2*vx[i]-vxo[i];vyb[i]=2*vy[i]-vyo[i];}
  }
  d.finite=true;for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){float v=u[id(x,y)];destination.at(x,y)=v;d.finite&=std::isfinite(v);}d.finalEnergy=energy({u.data(),w,b},source,vx,vy,protection,a1,a0,hx,hy);return d;
}

void decomposePhotographicLuminance(ConstFloatPlaneView source,ConstFloatPlaneView smooth,ScalarFieldView protection,PhotographicComponents&r,const ExecutionContext&e){RectI b=source.bounds;OwnedPlane q(b),medium(b),conductance(b);auto qv=q.view(),gv=conductance.view();e.parallelRows(b.y1,b.y2,[&](int y0,int y1){for(int y=y0;y<y1;++y)for(int x=b.x1;x<b.x2;++x){float residual=source.at(x,y)-smooth.at(x,y),p=clamp01(protection.at(x,y));r.smooth.view().at(x,y)=smooth.at(x,y);r.structure.view().at(x,y)=p*residual;qv.at(x,y)=(1-p)*residual;gv.at(x,y)=std::max(1e-4f,1-p);}});ScreenedMultigridParams mg;mg.lambdaX=mg.lambdaY=4;mg.vCycles=3;solveScreenedMultigrid(static_cast<const OwnedPlane&>(q).view(),1.0f,static_cast<const OwnedPlane&>(conductance).view(),medium.view(),mg,e);e.parallelRows(b.y1,b.y2,[&](int y0,int y1){for(int y=y0;y<y1;++y)for(int x=b.x1;x<b.x2;++x){r.medium.view().at(x,y)=medium.view().at(x,y);r.fine.view().at(x,y)=qv.at(x,y)-medium.view().at(x,y);}});}

void conditionalSmoothField(ConstFloatPlaneView signal,ConstFloatPlaneView membership,ScalarFieldView edge,float lambda,FloatPlaneView destination,const ExecutionContext&e){RectI b=signal.bounds;OwnedPlane confidence(b),conductance(b);auto c=confidence.view(),g=conductance.view();e.parallelRows(b.y1,b.y2,[&](int y0,int y1){for(int y=y0;y<y1;++y)for(int x=b.x1;x<b.x2;++x){float p=clamp01(membership.at(x,y));c.at(x,y)=1e-4f+p;g.at(x,y)=std::max(1e-5f,edge.at(x,y)*std::sqrt(std::max(1e-8f,p)));}});ScreenedMultigridParams mg;mg.lambdaX=mg.lambdaY=std::max(0.0f,lambda);mg.vCycles=3;solveScreenedMultigrid(signal,static_cast<const OwnedPlane&>(confidence).view(),static_cast<const OwnedPlane&>(conductance).view(),destination,mg,e);}
}  // namespace pigment
