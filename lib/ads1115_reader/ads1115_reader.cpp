#include "ads1115_reader.h"

bool Ads1115Reader::begin() {
  if (!ads_.begin()) {
    return false;
  }
  ads_.setGain(GAIN_SIXTEEN);
  return true;
}

float Ads1115Reader::read_mv() {
  int16_t raw = ads_.readADC_Differential_0_1();
  return ads_.computeVolts(raw) * 1000.0f;
}
