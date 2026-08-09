#include <unity.h>

#include <cmath>
#include <cstdint>

#include "calibration.h"

void setUp(void) {}
void tearDown(void) {}

static CalibrationTracker make_tracker() { return CalibrationTracker(9.0f, 13.0f); }

// --- Etat initial ---

void test_not_calibrated_initially(void) {
  CalibrationTracker t = make_tracker();
  TEST_ASSERT_FALSE(t.is_calibrated());
  TEST_ASSERT_TRUE(std::isnan(t.current_v_air_mv()));
  TEST_ASSERT_FALSE(t.has_temperature());
  TEST_ASSERT_TRUE(t.is_stale(1000, 500));  // non calibre => toujours perime
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, t.age_ms(1000));
}

// --- calibrate() : validation de plage ---

void test_calibrate_rejects_out_of_range_voltage(void) {
  CalibrationTracker t = make_tracker();
  TEST_ASSERT_FALSE(t.calibrate(5.0f, 1000));
  TEST_ASSERT_FALSE(t.calibrate(20.0f, 1000));
  TEST_ASSERT_FALSE(t.is_calibrated());
}

void test_calibrate_accepts_valid_voltage_without_temperature(void) {
  CalibrationTracker t = make_tracker();
  TEST_ASSERT_TRUE(t.calibrate(10.0f, 1000));
  TEST_ASSERT_TRUE(t.is_calibrated());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, t.current_v_air_mv());
  TEST_ASSERT_FALSE(t.has_temperature());
  TEST_ASSERT_TRUE(std::isnan(t.current_temp_c()));
}

void test_calibrate_with_temperature(void) {
  CalibrationTracker t = make_tracker();
  TEST_ASSERT_TRUE(t.calibrate(10.0f, 1000, 22.5f));
  TEST_ASSERT_TRUE(t.has_temperature());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 22.5f, t.current_temp_c());
}

// --- Fraicheur ---

void test_age_and_staleness(void) {
  CalibrationTracker t = make_tracker();
  t.calibrate(10.0f, 1000);
  TEST_ASSERT_EQUAL_UINT32(0, t.age_ms(1000));
  TEST_ASSERT_EQUAL_UINT32(499, t.age_ms(1499));
  TEST_ASSERT_FALSE(t.is_stale(1499, 500));  // 499 < 500
  TEST_ASSERT_TRUE(t.is_stale(1500, 500));   // 500 >= 500
}

// --- Vieillissement de cellule ---

void test_is_cell_aging_threshold(void) {
  // Seuil de remplacement independant de la plage de validation de calibrate().
  CalibrationTracker healthy = CalibrationTracker(5.0f, 13.0f);
  healthy.calibrate(9.0f, 1000);
  TEST_ASSERT_FALSE(healthy.is_cell_aging(7.0f));  // 9.0 > 7.0

  CalibrationTracker aging = CalibrationTracker(5.0f, 13.0f);
  aging.calibrate(6.5f, 1000);
  TEST_ASSERT_TRUE(aging.is_cell_aging(7.0f));  // 6.5 <= 7.0

  CalibrationTracker at_threshold = CalibrationTracker(5.0f, 13.0f);
  at_threshold.calibrate(7.0f, 1000);
  TEST_ASSERT_TRUE(at_threshold.is_cell_aging(7.0f));  // egal au seuil => aging (conservateur)
}

// --- Dérive / tendance ---

void test_is_declining_false_with_single_calibration(void) {
  CalibrationTracker t = make_tracker();
  t.calibrate(10.0f, 1000);
  TEST_ASSERT_FALSE(t.is_declining(0.5f));  // pas assez d'historique
}

void test_is_declining_true_on_downward_trend(void) {
  CalibrationTracker t = make_tracker();
  t.calibrate(12.0f, 1000);
  t.calibrate(11.0f, 2000);
  t.calibrate(10.0f, 3000);
  TEST_ASSERT_TRUE(t.is_declining(1.5f));   // chute totale 2.0 >= 1.5
  TEST_ASSERT_FALSE(t.is_declining(2.5f));  // chute totale 2.0 < 2.5
}

void test_is_declining_false_when_stable_or_rising(void) {
  CalibrationTracker t = make_tracker();
  t.calibrate(10.0f, 1000);
  t.calibrate(10.0f, 2000);
  TEST_ASSERT_FALSE(t.is_declining(0.1f));

  CalibrationTracker t2 = make_tracker();
  t2.calibrate(10.0f, 1000);
  t2.calibrate(11.0f, 2000);  // tension qui remonte
  TEST_ASSERT_FALSE(t2.is_declining(0.1f));
}

// --- Buffer circulaire (capacite 10) ---

void test_ring_buffer_eviction_shifts_oldest_used_by_declining(void) {
  CalibrationTracker t = make_tracker();
  // 10 calibrations remplissant exactement la capacite : 12.0 -> 10.2 (pas de 0.2)
  for (int i = 0; i < 10; ++i) {
    t.calibrate(12.0f - 0.2f * i, 1000 + i * 100);
  }
  // Historique plein : plus ancien encore present = 12.0 (index 0), plus recent = 10.2
  TEST_ASSERT_TRUE(t.is_declining(1.5f));  // chute totale 1.8 >= 1.5

  // Un 11e appel evince le plus ancien (12.0) - le nouveau plus ancien est 11.8
  TEST_ASSERT_TRUE(t.calibrate(9.5f, 2000));
  // plus ancien desormais 11.8 (l'ancien index 0 a ete ecrase), plus recent 9.5 -> chute 2.3
  TEST_ASSERT_TRUE(t.is_declining(2.0f));
  TEST_ASSERT_FALSE(t.is_declining(2.5f));  // 2.3 < 2.5 - prouve que ce n'est plus 12.0->9.5 (2.5)
}

// --- Rollover millis() ---

void test_age_ms_survives_millis_rollover(void) {
  CalibrationTracker t = make_tracker();
  const uint32_t R = UINT32_MAX - 100u;
  t.calibrate(10.0f, R);
  uint32_t later = R + 250u;  // boucle au-dela de UINT32_MAX
  TEST_ASSERT_EQUAL_UINT32(250, t.age_ms(later));
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_not_calibrated_initially);
  RUN_TEST(test_calibrate_rejects_out_of_range_voltage);
  RUN_TEST(test_calibrate_accepts_valid_voltage_without_temperature);
  RUN_TEST(test_calibrate_with_temperature);
  RUN_TEST(test_age_and_staleness);
  RUN_TEST(test_is_cell_aging_threshold);
  RUN_TEST(test_is_declining_false_with_single_calibration);
  RUN_TEST(test_is_declining_true_on_downward_trend);
  RUN_TEST(test_is_declining_false_when_stable_or_rising);
  RUN_TEST(test_ring_buffer_eviction_shifts_oldest_used_by_declining);
  RUN_TEST(test_age_ms_survives_millis_rollover);
  return UNITY_END();
}
