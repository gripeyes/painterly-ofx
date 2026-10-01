#include "core/ColorInteraction.h"
#include <Eigen/Dense>
#include <algorithm>
#include <cmath>

namespace pigment {
namespace {
using V=Eigen::Vector3d;using M=Eigen::Matrix3d;
// Subsampled CIE datasets. Attribution/license in SpectralQuadratureData.md.
constexpr double observer[kInteractionSamples][3]={
 {.001368,.000039,.006450001},{.01431,.000396,.06785001},
 {.13438,.004,.6456},{.34828,.023,1.74706},{.2908,.06,1.6692},
 {.09564,.13902,.8129501},{.0049,.323,.272},{.06327,.71,.07824999},
 {.2904,.954,.0203},{.5945,.995,.0039},{.9163,.87,.001650001},
 {1.0622,.631,.0008},{.8544499,.381,.00019},{.4479,.175,.00002},
 {.1649,.061,0},{.04677,.017,0},{.01135916,.004102,0},
 {.002899327,.001047,0},{.0006900786,.0002492,0},
 {.0001661505,.000060,0},{.00004150994,.00001499,0}};
constexpr double illuminant[kInteractionSamples]={49.9755,82.7549,93.4318,104.865,117.812,115.923,109.354,104.79,104.405,100,95.788,90.0062,87.6987,83.6992,80.2146,78.2842,71.6091,61.604,75.087,46.4182,63.3828};
V basis(int i){double t=(i-10)/10.;return {1,t,t*t};}
double reflectance(double p){return .5+.5*p/std::hypot(1.,p);}
double safe(double r){return std::clamp(r,1e-6,1.-1e-6);}
}
ColorInteraction::ColorInteraction(WorkingGamut gamut):opponent_(gamut){
  auto data=opponentMatrixData(gamut);M toXYZ,toRGB;
  for(int i=0;i<9;++i){toXYZ(i/3,i%3)=data.rgbToXyz[i];toRGB(i/3,i%3)=data.xyzToRgb[i];}
  V white=toXYZ*V::Ones();white/=white[1];
  M bradford;bradford<<.8951,.2664,-.1614,-.7502,1.7135,.0367,.0389,-.0685,1.0296;
  V d65(.95047,1,1.08883);
  V ratio=(bradford*white).cwiseQuotient(bradford*d65);
  M adaptation=bradford.inverse()*ratio.asDiagonal()*bradford;
  V sum=V::Zero();for(int i=0;i<kInteractionSamples;++i)
    for(int k=0;k<3;++k)sum[k]+=observer[i][k]*illuminant[i]*(i==0 || i==20?.5:1);
  // Compensate coarse quadrature's white error, not arbitrary image colors.
  for(int i=0;i<kInteractionSamples;++i){V xyz;
    for(int k=0;k<3;++k)xyz[k]=observer[i][k]*illuminant[i]*(i==0 || i==20?.5:1)*d65[k]/sum[k];
    V rgb=toRGB*adaptation*xyz;for(int k=0;k<3;++k)integration_[k][i]=rgb[k];}
}
std::array<double,3> ColorInteraction::integrate(const std::array<double,kInteractionSamples>&r) const {
  std::array<double,3> out{};for(int k=0;k<3;++k)for(int i=0;i<kInteractionSamples;++i)out[k]+=integration_[k][i]*r[i];return out;
}
InteractionMaterial ColorInteraction::encode(YabPixel color,ColorInteractionLaw law) const {
  InteractionMaterial out;auto rgb=opponent_.toRgb(color);
  out.magnitude=std::max({0.,double(rgb[0]),double(rgb[1]),double(rgb[2])})/.9;
  V target=V::Zero();if(out.magnitude>0)for(int k=0;k<3;++k)target[k]=std::max(0.,double(rgb[k]))/out.magnitude;
  std::array<double,3> decoded{};
  if(law!=ColorInteractionLaw::SpectralPigment){
    for(int k=0;k<3;++k){decoded[k]=std::max(1e-6,target[k]);out.absorption[k]=-std::log(decoded[k]);}
  }else{
    // Direct deterministic damped Gauss-Newton fit in the J/H function space.
    // No imported LUT, pigment data, or endpoint gamut clipping.
    double grey=std::clamp(target.mean(),1e-4,1.-1e-4),s=2*grey-1;
    V c(s/std::sqrt(1-s*s),0,0);
    auto eval=[&](V v,std::array<double,kInteractionSamples>*samples=nullptr,M*jac=nullptr){
      V value=V::Zero();if(jac)jac->setZero();
      for(int i=0;i<kInteractionSamples;++i){V b=basis(i);double p=v.dot(b),r=reflectance(p);
        if(samples)(*samples)[i]=r;
        double derivative=.5/std::pow(1+p*p,1.5);
        for(int k=0;k<3;++k){value[k]+=integration_[k][i]*r;if(jac)jac->row(k)+=integration_[k][i]*derivative*b.transpose();}}
      return value;
    };
    double damping=1e-5;
    for(int iteration=0;iteration<24;++iteration){M jac;V value=eval(c,nullptr,&jac),error=value-target;
      if(error.squaredNorm()<1e-14)break;
      M normal=jac.transpose()*jac+damping*M::Identity();V delta=normal.ldlt().solve(jac.transpose()*error);
      bool accepted=false;
      for(int step=0;step<8;++step){V next=(c-std::ldexp(1.,-step)*delta).cwiseMax(-100).cwiseMin(100);
        if((eval(next)-target).squaredNorm()<error.squaredNorm()){c=next;accepted=true;break;}}
      damping=accepted?std::max(1e-10,damping*.5):std::min(1.,damping*10);
    }
    std::array<double,kInteractionSamples> spectrum;V value=eval(c,&spectrum);
    out.fitError=(value-target).norm();for(int k=0;k<3;++k){out.coefficients[k]=c[k];decoded[k]=value[k];}
    for(int i=0;i<kInteractionSamples;++i){double r=safe(spectrum[i]);out.absorption[i]=(1-r)*(1-r)/(2*r);}
    // Decode the exact bounded numerical spectrum used by K/S.
    for(double &r:spectrum)r=safe(r);decoded=integrate(spectrum);
  }
  for(int k=0;k<3;++k){out.residual[k]=double(rgb[k])-out.magnitude*decoded[k];out.residualMagnitude+=out.residual[k]*out.residual[k];}
  out.residualMagnitude=std::sqrt(out.residualMagnitude);return out;
}
YabPixel ColorInteraction::mix(const InteractionMaterial *m,const float *w,int count,
    YabPixel linear,ColorInteractionLaw law,float density) const {
  double total=0;int occupied=0;for(int j=0;j<count;++j){total+=w[j];occupied+=w[j]>0;}
  if(law==ColorInteractionLaw::LinearYAB || occupied<=1 || total<=0)return linear;
  double magnitude=0;std::array<double,3> residual{};std::array<double,kInteractionSamples> absorption{},spectrum{};
  for(int j=0;j<count;++j)if(w[j]>0){double t=w[j]/total;magnitude+=t*m[j].magnitude;
    for(int k=0;k<3;++k)residual[k]+=t*m[j].residual[k];
    int n=law==ColorInteractionLaw::Density?3:kInteractionSamples;for(int i=0;i<n;++i)absorption[i]+=t*m[j].absorption[i];}
  std::array<double,3> material;
  if(law==ColorInteractionLaw::Density)for(int k=0;k<3;++k)material[k]=std::exp(-absorption[k]);
  else{for(int i=0;i<kInteractionSamples;++i){double k=absorption[i];spectrum[i]=1/(1+k+std::sqrt(k*k+2*k));}material=integrate(spectrum);}
  std::array<float,3> rgb;for(int k=0;k<3;++k)rgb[k]=float(magnitude*material[k]+residual[k]);
  auto nonlinear=opponent_.toYab(rgb);float d=std::isfinite(density)?std::clamp(density,0.f,1.f):0.f;
  float targetY=d==0?linear.y:linear.y+d*(nonlinear.y-linear.y);
  // Add/remove neutral scene light; do not divide by signed scene luminance.
  // Negative residuals may cancel material Y. Scaling by that near-zero sum
  // caused chroma spikes in the initial diagnostic. Neutral light changes Y
  // alone in this opponent space and leaves all RGB residuals unscaled.
  nonlinear.y=targetY;return nonlinear;
}
std::array<double,3> ColorInteraction::reconstruct(const InteractionMaterial&m,ColorInteractionLaw law) const {
  std::array<double,3> material{};
  if(law!=ColorInteractionLaw::SpectralPigment)for(int k=0;k<3;++k)material[k]=std::exp(-m.absorption[k]);
  else {std::array<double,kInteractionSamples> r{};for(int i=0;i<kInteractionSamples;++i){double k=m.absorption[i];r[i]=1/(1+k+std::sqrt(k*k+2*k));}material=integrate(r);}
  for(int k=0;k<3;++k)material[k]=m.magnitude*material[k]+m.residual[k];return material;
}
}
