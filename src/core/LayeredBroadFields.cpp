#include "core/LayeredBroadFields.h"
#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace pigment { namespace {
constexpr double pi=3.14159265358979323846;
struct Observation {double x=0,y=0,w=0;std::array<double,2> value{};};
double median(std::vector<std::pair<double,double>> v) {
  if(v.empty())return 0;
  std::stable_sort(v.begin(),v.end(),[](auto a,auto b){return a.first<b.first;});
  double total=0;for(auto a:v)total+=a.second;
  double sum=0;for(auto a:v){sum+=a.second;if(sum>=total*.5)return a.first;}return v.back().first;
}
double opacity(const BroadSublayer &l,double x,double y,bool base=false) {
  if(base)return 1;
  const double r=std::hypot((x-l.cx)/l.rx,(y-l.cy)/l.ry);
  return r>=1?0:.72*.5*(1+std::cos(pi*r));
}
std::array<double,3> basis(const BroadSublayer &l,double x,double y) {
  double u=(x-l.cx)/l.rx,v=(y-l.cy)/l.ry;
  return l.radial?std::array<double,3>{1,std::hypot(u,v),0}:std::array<double,3>{1,u,v};
}
double evaluate(const BroadSublayer &l,double x,double y,int channel) {
  auto f=basis(l,x,y);auto c=l.coefficients[size_t(channel)];return f[0]*c[0]+f[1]*c[1]+f[2]*c[2];
}
std::array<double,2> stack(const std::vector<BroadSublayer>&layers,double x,double y) {
  std::array<double,2> r{};
  for(size_t j=0;j<layers.size();++j){double a=opacity(layers[j],x,y,j==0);
    for(int c=0;c<2;++c)r[size_t(c)]=(1-a)*r[size_t(c)]+a*evaluate(layers[j],x,y,c);}
  return r;
}
double fit(std::vector<BroadSublayer>&layers,const std::vector<Observation>&obs,int channels) {
  const int n=int(layers.size())*3;
  std::vector<double> robust(obs.size(),1);
  double scale=0;for(auto o:obs)for(int c=0;c<channels;++c)scale+=o.w*o.value[size_t(c)]*o.value[size_t(c)];
  double mass=0;for(auto o:obs)mass+=o.w;
  scale=std::max(1e-5,std::sqrt(scale/std::max(1.,mass*channels))*.1);
  for(int pass=0;pass<3;++pass){
    Eigen::MatrixXd A=Eigen::MatrixXd::Zero(n,n),rhs=Eigen::MatrixXd::Zero(n,channels);
    for(size_t z=0;z<obs.size();++z){auto o=obs[z];Eigen::VectorXd row=Eigen::VectorXd::Zero(n);double remaining=1;
      for(int j=int(layers.size())-1;j>=0;--j){double a=opacity(layers[size_t(j)],o.x,o.y,j==0);auto f=basis(layers[size_t(j)],o.x,o.y);
        for(int k=0;k<3;++k)row[j*3+k]=remaining*a*f[size_t(k)];remaining*=1-a;}
      double w=o.w*robust[z];A.noalias()+=w*row*row.transpose();
      for(int c=0;c<channels;++c)rhs.col(c).noalias()+=w*row*o.value[size_t(c)];
    }
    for(int j=0;j<n;++j)A(j,j)+=std::max(1.,mass)*(j%3==0?1e-9:1e-5);
    Eigen::MatrixXd solved=A.ldlt().solve(rhs);
    if(!solved.allFinite())throw std::runtime_error("C5 nonfinite layer fit");
    for(size_t j=0;j<layers.size();++j)for(int c=0;c<channels;++c)for(int k=0;k<3;++k)layers[j].coefficients[size_t(c)][size_t(k)]=solved(int(j)*3+k,c);
    for(size_t z=0;z<obs.size();++z){auto predicted=stack(layers,obs[z].x,obs[z].y);double e=0;
      for(int c=0;c<channels;++c)e+=std::pow(predicted[size_t(c)]-obs[z].value[size_t(c)],2);
      robust[z]=1/(1+e/(channels*scale*scale));}
  }
  double error=0;for(auto o:obs){auto r=stack(layers,o.x,o.y);for(int c=0;c<channels;++c)error+=o.w*std::pow(r[size_t(c)]-o.value[size_t(c)],2);}
  return error/std::max(1.,mass);
}
std::vector<Observation> observations(ConstYabPlanes a,ConstFloatPlaneView support,int family,int size,OwnedYabPlanes &target) {
  std::vector<Observation> result;auto b=a.y.bounds;
  for(int y0=b.y1;y0<b.y2;y0+=size)for(int x0=b.x1;x0<b.x2;x0+=size){
    Observation o;std::vector<std::pair<double,double>> v[2];
    for(int y=y0;y<std::min(b.y2,y0+size);++y)for(int x=x0;x<std::min(b.x2,x0+size);++x){double w=std::max(0.f,support.at(x,y));if(w<.005)continue;
      o.x+=w*x;o.y+=w*y;o.w+=w;v[0].emplace_back(family?a.a.at(x,y):a.y.at(x,y),w);if(family)v[1].emplace_back(a.b.at(x,y),w);}
    if(o.w<.1)continue;o.x/=o.w;o.y/=o.w;for(int c=0;c<(family?2:1);++c)o.value[size_t(c)]=median(v[c]);result.push_back(o);
    auto t=target.view();for(int y=y0;y<std::min(b.y2,y0+size);++y)for(int x=x0;x<std::min(b.x2,x0+size);++x){if(family){t.a.at(x,y)=float(o.value[0]);t.b.at(x,y)=float(o.value[1]);}else t.y.at(x,y)=float(o.value[0]);}
  }
  return result;
}
double coherence(ConstFloatPlaneView f,int x,int y) {
  auto b=f.bounds;auto sample=[&](int xx,int yy){return double(f.at(std::clamp(xx,b.x1,b.x2-1),std::clamp(yy,b.y1,b.y2-1)));};
  double nx=0,ny=0,length=0;
  for(int r:{1,4,8}){double dx=(sample(x+r,y)-sample(x-r,y))/(2*r),dy=(sample(x,y+r)-sample(x,y-r))/(2*r),n=std::hypot(dx,dy);
    if(n>1e-8){nx+=dx/n;ny+=dy/n;length+=1;}}
  return length?std::hypot(nx,ny)/length:0;
}
double micro(ConstFloatPlaneView f,int x,int y) {
  auto b=f.bounds;std::vector<std::pair<double,double>> samples;
  for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)samples.emplace_back(f.at(std::clamp(x+dx,b.x1,b.x2-1),std::clamp(y+dy,b.y1,b.y2-1)),1);
  double center=f.at(x,y),m=median(samples),sx=0,sy=0;
  if(x>b.x1 && x+1<b.x2)sx=(f.at(x-1,y)-center)*(f.at(x+1,y)-center);
  if(y>b.y1 && y+1<b.y2)sy=(f.at(x,y-1)-center)*(f.at(x,y+1)-center);
  return (sx>0 && sy>0)?center-m:0;
}
} // namespace
LayeredBroadResult layeredBroadFields(const PublicPlateSet &plates,const Phase4RegionHierarchy &h,const LayeredBroadOptions &options,const ExecutionContext &exec) {
  auto b=plates.bounds();LayeredBroadResult out(b);
  for(int p=0;p<plates.count();++p){
    if(exec.cancelled())throw std::runtime_error("C5 cancelled");
    out.appearance.emplace_back(b);out.broadTarget.emplace_back(b);out.broad.emplace_back(b);out.structure.emplace_back(b);out.medium.emplace_back(b);out.micro.emplace_back(b);
    out.protectionY.emplace_back(b);out.protectionAB.emplace_back(b);out.layers.emplace_back();
    auto source=plates.appearance(p);
    for(int family=0;family<2;++family){
      auto support=family?plates.supportAB(p):plates.supportY(p);
      auto obs=observations(source,support,family,std::max(4,int(family?options.observationScaleAB:options.observationScaleY)),out.broadTarget.back());
      BroadLayerFit d;d.plate=p;d.family=family;d.observations=int(obs.size());
      std::vector<BroadSublayer> layers;
      if(obs.size()>=3){
        BroadSublayer base(b);base.family=family;base.cx=(b.x1+b.x2-1)*.5;base.cy=(b.y1+b.y2-1)*.5;base.rx=std::max(1,b.width()/2);base.ry=std::max(1,b.height()/2);layers.push_back(std::move(base));
        double objective=fit(layers,obs,family?2:1);d.objectives.push_back(objective);
        int budget=std::clamp(family?options.maximumABLayers:options.maximumYLayers,1,4);
        for(int layer=1;layer<budget;++layer){
          size_t chosen=0;double greatest=-1;
          for(size_t z=0;z<obs.size();++z){auto r=stack(layers,obs[z].x,obs[z].y);double e=0;for(int c=0;c<(family?2:1);++c)e+=std::pow(r[size_t(c)]-obs[z].value[size_t(c)],2);e*=std::sqrt(obs[z].w);if(e>greatest){greatest=e;chosen=z;}}
          auto best=layers;double bestError=objective;
          for(double width:{.36,.55,.8})for(bool radial:{false,true}){
            auto trial=layers;BroadSublayer next(b);next.family=family;next.radial=radial;next.cx=obs[chosen].x;next.cy=obs[chosen].y;
            next.rx=std::max(4.,b.width()*std::max(width,family?.56:.36));next.ry=std::max(4.,b.height()*std::max(width,family?.56:.36));trial.push_back(std::move(next));
            double error=fit(trial,obs,family?2:1);if(error<bestError){bestError=error;best=std::move(trial);}}
          if(bestError>=objective*.97)break;layers=std::move(best);objective=bestError;d.objectives.push_back(objective);
        }
      }
      d.layers=int(layers.size());out.fits.push_back(d);
      auto fitted=out.appearance.back().view(),broad=out.broad.back().view(),structure=out.structure.back().view(),medium=out.medium.back().view(),fine=out.micro.back().view();
      auto protection=family?out.protectionAB.back().view():out.protectionY.back().view();
      for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){
        auto prediction=stack(layers,x,y);double persistent=coherence(source.y,x,y);
        double cue=h.boundaryStrength.bounds().width()?h.boundaryStrength.view().at(x,y):0;
        double P=std::clamp((cue-.25)/.45,0.,1.)*persistent*(family?.4:1.);
        if(size_t(p)<h.plates.size()){auto retained=family?h.plates[size_t(p)].abRetainedBoundaries.view():h.plates[size_t(p)].yRetainedBoundaries.view();if(cue>.7 && retained.at(x,y)>0)P=family?.6:1;}
        protection.at(x,y)=float(P);
        for(int c=0;c<(family?2:1);++c){auto in=family?(c?source.b:source.a):source.y;
          auto dst=family?(c?fitted.b:fitted.a):fitted.y;auto B=family?(c?broad.b:broad.a):broad.y;auto K=family?(c?structure.b:structure.a):structure.y;auto M=family?(c?medium.b:medium.a):medium.y;auto F=family?(c?fine.b:fine.a):fine.y;
          double original=in.at(x,y),value=layers.empty()||support.at(x,y)<.005?original:prediction[size_t(c)];
          double k=P*(original-value),f=(1-P)*micro(in,x,y),m=original-value-k-f;
          B.at(x,y)=float(value);K.at(x,y)=float(k);M.at(x,y)=float(m);F.at(x,y)=float(f);
          bool enabled=family?options.processAB:options.processY;
          dst.at(x,y)=!enabled||support.at(x,y)<.005?float(original):float(value+k+options.microSurvival*f);
        }
      }
      for(size_t j=0;j<layers.size();++j){auto &l=layers[j];auto m=l.membership.view();auto field=l.field.view();
        for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){m.at(x,y)=float(opacity(l,x,y,j==0));if(family){field.a.at(x,y)=float(evaluate(l,x,y,0));field.b.at(x,y)=float(evaluate(l,x,y,1));}else field.y.at(x,y)=float(evaluate(l,x,y,0));}
        out.layers.back().push_back(std::move(l));}
    }
  }
  auto composite=out.composite.view();for(int y=b.y1;y<b.y2;++y)for(int x=b.x1;x<b.x2;++x){double v[3]{};for(int p=0;p<plates.count();++p){auto a=out.appearance[size_t(p)].view();double w=plates.alpha(p).at(x,y);v[0]+=w*a.y.at(x,y);v[1]+=w*a.a.at(x,y);v[2]+=w*a.b.at(x,y);}composite.y.at(x,y)=float(v[0]);composite.a.at(x,y)=float(v[1]);composite.b.at(x,y)=float(v[2]);}
  return out;
}
} // namespace pigment
