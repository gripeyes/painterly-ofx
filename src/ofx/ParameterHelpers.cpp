#include "ofx/ParameterHelpers.h"

namespace pigment::ofx {

OFX::DoubleParamDescriptor* defineDouble(OFX::ImageEffectDescriptor& d,
    const char* name, const char* label, double value, double minimum,
    double maximum, double displayMinimum, double displayMaximum,
    double increment, const char* hint, OFX::DoubleTypeEnum type) {
  auto* p = d.defineDoubleParam(name);
  p->setLabels(label, label, label);
  p->setScriptName(name);
  p->setHint(hint);
  p->setDefault(value);
  p->setRange(minimum, maximum);
  p->setDisplayRange(displayMinimum, displayMaximum);
  p->setIncrement(increment);
  p->setDoubleType(type);
  return p;
}

OFX::BooleanParamDescriptor* defineBoolean(OFX::ImageEffectDescriptor& d,
    const char* name, const char* label, bool value, const char* hint) {
  auto* p = d.defineBooleanParam(name);
  p->setLabels(label, label, label);
  p->setScriptName(name);
  p->setHint(hint);
  p->setDefault(value);
  return p;
}

}  // namespace pigment::ofx

