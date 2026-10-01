// Standalone bridge to the unmodified Jakob/Hanika authors' implementation.
// Not compiled into pigment_core or OFX. Reference license stays in its checkout.
#include "rgb2spec.h"
#include <cmath>
extern "C" int reference_spectra(const char *table,const float *rgb,int count,float *coeff,float *spectra){
  RGB2Spec *model=rgb2spec_load(table);if(!model)return 1;
  for(int j=0;j<count;++j){float input[3]={rgb[3*j],rgb[3*j+1],rgb[3*j+2]};
    rgb2spec_fetch_opt(model,input,coeff+3*j);
    for(int k=0;k<21;++k)spectra[21*j+k]=rgb2spec_eval_precise(coeff+3*j,380.f+20.f*k);
  }
  rgb2spec_free(model);return 0;
}
extern "C" int reference_native_error(const char *table,const float *rgb,int count,float *errors){
  RGB2Spec *model=rgb2spec_load(table);if(!model)return 1;
  for(int j=0;j<count;++j){float input[3]={rgb[3*j],rgb[3*j+1],rgb[3*j+2]},coeff[3];rgb2spec_fetch_opt(model,input,coeff);
    double decoded[3]={};
    for(unsigned i=0;i<model->nfine;++i){float R=rgb2spec_eval_precise(coeff,360.f+470.f*model->fwd[i]);
      for(int k=0;k<3;++k)decoded[k]+=R*model->fwd[model->nfine*(k+1)+i];}
    double error=0;for(int k=0;k<3;++k)error+=(decoded[k]-input[k])*(decoded[k]-input[k]);errors[j]=float(std::sqrt(error));
  }
  rgb2spec_free(model);return 0;
}
