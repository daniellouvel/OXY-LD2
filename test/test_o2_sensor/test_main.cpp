#include <unity.h>

#include <cmath>

#include "conversion.h"
#include "glitch_filter.h"

void setUp(void) {}
void tearDown(void) {}

// --- voltage_to_o2_percent ---

void test_voltage_ratio_1_1_gives_reference_percent(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.9f, voltage_to_o2_percent(10.0f, 10.0f));
}

void test_voltage_ratio_2_1_doubles_percent(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 41.8f, voltage_to_o2_percent(20.0f, 10.0f));
}

void test_voltage_custom_reference(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 32.0f, voltage_to_o2_percent(16.0f, 10.0f, 20.0f));
}

void test_voltage_uncalibrated_returns_nan(void) {
  TEST_ASSERT_TRUE(std::isnan(voltage_to_o2_percent(10.0f, 0.0f)));
  TEST_ASSERT_TRUE(std::isnan(voltage_to_o2_percent(10.0f, -5.0f)));
}

// --- apply_thermal_compensation ---

void test_thermal_compensation_zero_delta_unchanged(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 32.0f, apply_thermal_compensation(32.0f, 22.0f, 22.0f, 0.3f));
}

void test_thermal_compensation_positive_coefficient(void) {
  // +5 degres, coefficient +0.3 %O2/degC -> +1.5
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 33.5f, apply_thermal_compensation(32.0f, 27.0f, 22.0f, 0.3f));
}

void test_thermal_compensation_negative_coefficient(void) {
  // +5 degres, coefficient -0.3 %O2/degC -> -1.5
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 30.5f, apply_thermal_compensation(32.0f, 27.0f, 22.0f, -0.3f));
}

void test_thermal_compensation_negative_delta(void) {
  // -5 degres (plus froid qu'a la calibration), coefficient +0.3 -> -1.5
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 30.5f, apply_thermal_compensation(32.0f, 17.0f, 22.0f, 0.3f));
}

// --- GlitchFilter ---

void test_glitch_filter_empty_returns_zero(void) {
  GlitchFilter f;
  TEST_ASSERT_EQUAL_FLOAT(0.0f, f.value());
}

void test_glitch_filter_below_3_returns_last_pushed(void) {
  GlitchFilter f;
  f.push(5.0f);
  TEST_ASSERT_EQUAL_FLOAT(5.0f, f.value());
  f.push(7.0f);
  TEST_ASSERT_EQUAL_FLOAT(7.0f, f.value());
}

void test_glitch_filter_median_already_sorted(void) {
  GlitchFilter f;
  f.push(1.0f);
  f.push(2.0f);
  f.push(3.0f);
  TEST_ASSERT_EQUAL_FLOAT(2.0f, f.value());
}

void test_glitch_filter_median_reverse_sorted(void) {
  GlitchFilter f;
  f.push(3.0f);
  f.push(2.0f);
  f.push(1.0f);
  TEST_ASSERT_EQUAL_FLOAT(2.0f, f.value());
}

void test_glitch_filter_rejects_spike_middle(void) {
  GlitchFilter f;
  f.push(1.0f);
  f.push(100.0f);
  f.push(2.0f);
  TEST_ASSERT_EQUAL_FLOAT(2.0f, f.value());
}

void test_glitch_filter_rejects_spike_first(void) {
  GlitchFilter f;
  f.push(100.0f);
  f.push(1.0f);
  f.push(2.0f);
  TEST_ASSERT_EQUAL_FLOAT(2.0f, f.value());
}

void test_glitch_filter_rejects_spike_last(void) {
  GlitchFilter f;
  f.push(1.0f);
  f.push(2.0f);
  f.push(100.0f);
  TEST_ASSERT_EQUAL_FLOAT(2.0f, f.value());
}

void test_glitch_filter_duplicates(void) {
  GlitchFilter f;
  f.push(5.0f);
  f.push(5.0f);
  f.push(5.0f);
  TEST_ASSERT_EQUAL_FLOAT(5.0f, f.value());
}

void test_glitch_filter_rolling_buffer_evicts_oldest(void) {
  GlitchFilter f;
  f.push(1.0f);
  f.push(2.0f);
  f.push(3.0f);
  TEST_ASSERT_EQUAL_FLOAT(2.0f, f.value());  // median{1,2,3}

  f.push(4.0f);  // evince le 1 -> buffer logique {2,3,4}
  TEST_ASSERT_EQUAL_FLOAT(3.0f, f.value());  // median{2,3,4}
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_voltage_ratio_1_1_gives_reference_percent);
  RUN_TEST(test_voltage_ratio_2_1_doubles_percent);
  RUN_TEST(test_voltage_custom_reference);
  RUN_TEST(test_voltage_uncalibrated_returns_nan);
  RUN_TEST(test_thermal_compensation_zero_delta_unchanged);
  RUN_TEST(test_thermal_compensation_positive_coefficient);
  RUN_TEST(test_thermal_compensation_negative_coefficient);
  RUN_TEST(test_thermal_compensation_negative_delta);
  RUN_TEST(test_glitch_filter_empty_returns_zero);
  RUN_TEST(test_glitch_filter_below_3_returns_last_pushed);
  RUN_TEST(test_glitch_filter_median_already_sorted);
  RUN_TEST(test_glitch_filter_median_reverse_sorted);
  RUN_TEST(test_glitch_filter_rejects_spike_middle);
  RUN_TEST(test_glitch_filter_rejects_spike_first);
  RUN_TEST(test_glitch_filter_rejects_spike_last);
  RUN_TEST(test_glitch_filter_duplicates);
  RUN_TEST(test_glitch_filter_rolling_buffer_evicts_oldest);
  return UNITY_END();
}
