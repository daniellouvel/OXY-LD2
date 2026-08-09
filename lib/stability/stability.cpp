#include "stability.h"

#include <cmath>

StabilityTracker::StabilityTracker(StabilityConfig config) : config_(config) {}

void StabilityTracker::reset(uint32_t now_ms) {
  reset_time_ms_ = now_ms;
  has_prev_sample_ = false;
  has_slope_ = false;
  smoothed_slope_ = 0.0f;
  below_threshold_ = false;
  below_since_ms_ = now_ms;
}

bool StabilityTracker::push_sample(float fO2_percent, uint32_t now_ms) {
  if (!has_prev_sample_) {
    prev_value_ = fO2_percent;
    prev_time_ms_ = now_ms;
    has_prev_sample_ = true;
    return true;
  }

  // Soustraction non signee - reste correcte meme si now_ms a boucle
  // (rollover millis() ~49 jours) tant que l'ecart reel est < 2^32 ms.
  uint32_t dt_ms = now_ms - prev_time_ms_;

  if (dt_ms == 0) {
    return false;  // timestamp duplique, division par zero evitee
  }

  if (dt_ms > config_.max_sample_gap_ms) {
    // Discontinuite : les deux echantillons ne sont plus consideres
    // continus physiquement. On redemarre la baseline sur ce point sans
    // calculer de pente ni pretendre a la stabilite.
    prev_value_ = fO2_percent;
    prev_time_ms_ = now_ms;
    below_threshold_ = false;
    return false;
  }

  float dt_s = static_cast<float>(dt_ms) / 1000.0f;
  float raw_slope = (fO2_percent - prev_value_) / dt_s;

  if (!has_slope_) {
    // Pas de blending sur la toute premiere pente calculee : demarrer l'EMA
    // a 0 biaiserait artificiellement vers "stable" au tout debut.
    smoothed_slope_ = raw_slope;
    has_slope_ = true;
  } else {
    smoothed_slope_ =
        config_.ema_alpha * raw_slope + (1.0f - config_.ema_alpha) * smoothed_slope_;
  }

  prev_value_ = fO2_percent;
  prev_time_ms_ = now_ms;

  bool now_below = std::fabs(smoothed_slope_) < config_.slope_threshold_percent_per_s;
  if (now_below && !below_threshold_) {
    below_since_ms_ = now_ms;
  }
  below_threshold_ = now_below;

  return true;
}

bool StabilityTracker::is_stable() const {
  if (!has_slope_ || !below_threshold_) {
    return false;
  }
  uint32_t elapsed_since_reset = prev_time_ms_ - reset_time_ms_;
  uint32_t elapsed_below = prev_time_ms_ - below_since_ms_;
  return elapsed_since_reset >= config_.min_settle_ms && elapsed_below >= config_.sustained_ms;
}

float StabilityTracker::current_slope() const { return smoothed_slope_; }
