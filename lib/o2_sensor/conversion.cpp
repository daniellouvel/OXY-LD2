#include "conversion.h"

#include <cmath>

float voltage_to_o2_percent(float v_measured_mv, float v_calibration_mv,
                             float o2_reference_percent) {
  if (v_calibration_mv <= 0.0f) {
    return NAN;
  }
  return (v_measured_mv / v_calibration_mv) * o2_reference_percent;
}

float apply_thermal_compensation(float fO2_percent, float temp_now_c,
                                  float temp_calibration_c,
                                  float coefficient_percent_per_c) {
  return fO2_percent + coefficient_percent_per_c * (temp_now_c - temp_calibration_c);
}
