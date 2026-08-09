#include "calibration.h"

CalibrationTracker::CalibrationTracker(float v_air_min_mv, float v_air_max_mv)
    : v_air_min_mv_(v_air_min_mv), v_air_max_mv_(v_air_max_mv) {}

int CalibrationTracker::last_index() const {
  return (next_index_ + kHistoryCapacity - 1) % kHistoryCapacity;
}

int CalibrationTracker::oldest_index() const {
  return (count_ < kHistoryCapacity) ? 0 : next_index_;
}

bool CalibrationTracker::calibrate(float v_air_mv, uint32_t now_ms, float temp_c) {
  if (v_air_mv < v_air_min_mv_ || v_air_mv > v_air_max_mv_) {
    return false;
  }

  history_[next_index_] = CalibrationPoint{v_air_mv, temp_c, now_ms};
  next_index_ = (next_index_ + 1) % kHistoryCapacity;
  if (count_ < kHistoryCapacity) {
    ++count_;
  }
  return true;
}

bool CalibrationTracker::is_calibrated() const { return count_ > 0; }

float CalibrationTracker::current_v_air_mv() const {
  if (!is_calibrated()) {
    return NAN;
  }
  return history_[last_index()].v_air_mv;
}

float CalibrationTracker::current_temp_c() const {
  if (!is_calibrated()) {
    return NAN;
  }
  return history_[last_index()].temp_c;
}

bool CalibrationTracker::has_temperature() const {
  return is_calibrated() && !std::isnan(current_temp_c());
}

uint32_t CalibrationTracker::age_ms(uint32_t now_ms) const {
  if (!is_calibrated()) {
    return UINT32_MAX;
  }
  // Soustraction non signee - reste correcte au rollover millis().
  return now_ms - history_[last_index()].timestamp_ms;
}

bool CalibrationTracker::is_stale(uint32_t now_ms, uint32_t max_age_ms) const {
  if (!is_calibrated()) {
    return true;
  }
  return age_ms(now_ms) >= max_age_ms;
}

bool CalibrationTracker::is_cell_aging(float replacement_threshold_mv) const {
  return is_calibrated() && current_v_air_mv() <= replacement_threshold_mv;
}

bool CalibrationTracker::is_declining(float drop_threshold_mv) const {
  if (count_ < 2) {
    return false;
  }
  float oldest_v = history_[oldest_index()].v_air_mv;
  float newest_v = current_v_air_mv();
  return (oldest_v - newest_v) >= drop_threshold_mv;
}
