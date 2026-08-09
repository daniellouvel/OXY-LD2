#include "o2_value.h"

#include <cmath>

int o2_display_value(float fO2_raw_percent) {
  return static_cast<int>(std::ceil(fO2_raw_percent));
}

bool is_fo2_valid(float fO2_raw_percent) {
  return fO2_raw_percent >= 0.0f && fO2_raw_percent <= 100.0f;
}
