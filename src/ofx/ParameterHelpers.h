#pragma once
#include <string>
#include "ofxsImageEffect.h"

namespace pigment::ofx {

OFX::DoubleParamDescriptor* defineDouble(OFX::ImageEffectDescriptor& descriptor,
    const char* name, const char* label, double defaultValue, double minimum,
    double maximum, double displayMinimum, double displayMaximum,
    double increment, const char* hint,
    OFX::DoubleTypeEnum type = OFX::eDoubleTypePlain);

OFX::BooleanParamDescriptor* defineBoolean(OFX::ImageEffectDescriptor& descriptor,
    const char* name, const char* label, bool defaultValue, const char* hint);

}  // namespace pigment::ofx

