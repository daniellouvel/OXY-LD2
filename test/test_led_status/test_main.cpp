#include <unity.h>

#include "led_status.h"

void setUp(void) {}
void tearDown(void) {}

void test_splash_is_white_not_blinking(void) {
  LedStatusOutput out = led_status_for(SystemState::Splash);
  TEST_ASSERT_EQUAL_UINT8(255, out.color.r);
  TEST_ASSERT_EQUAL_UINT8(255, out.color.g);
  TEST_ASSERT_EQUAL_UINT8(255, out.color.b);
  TEST_ASSERT_FALSE(out.blinking);
}

void test_stabilizing_is_orange_not_blinking(void) {
  LedStatusOutput out = led_status_for(SystemState::Stabilizing);
  TEST_ASSERT_EQUAL_UINT8(255, out.color.r);
  TEST_ASSERT_EQUAL_UINT8(165, out.color.g);
  TEST_ASSERT_EQUAL_UINT8(0, out.color.b);
  TEST_ASSERT_FALSE(out.blinking);
}

void test_stable_is_green_not_blinking(void) {
  LedStatusOutput out = led_status_for(SystemState::Stable);
  TEST_ASSERT_EQUAL_UINT8(0, out.color.r);
  TEST_ASSERT_EQUAL_UINT8(255, out.color.g);
  TEST_ASSERT_EQUAL_UINT8(0, out.color.b);
  TEST_ASSERT_FALSE(out.blinking);
}

void test_error_is_red_blinking(void) {
  LedStatusOutput out = led_status_for(SystemState::Error);
  TEST_ASSERT_EQUAL_UINT8(255, out.color.r);
  TEST_ASSERT_EQUAL_UINT8(0, out.color.g);
  TEST_ASSERT_EQUAL_UINT8(0, out.color.b);
  TEST_ASSERT_TRUE(out.blinking);
}

void test_all_states_distinct_colors(void) {
  LedStatusOutput splash = led_status_for(SystemState::Splash);
  LedStatusOutput stabilizing = led_status_for(SystemState::Stabilizing);
  LedStatusOutput stable = led_status_for(SystemState::Stable);
  LedStatusOutput error = led_status_for(SystemState::Error);

  // Aucune paire ne doit partager exactement la meme couleur - sinon deux
  // etats seraient visuellement indiscernables sur la LED.
  auto same = [](RgbColor a, RgbColor b) { return a.r == b.r && a.g == b.g && a.b == b.b; };
  TEST_ASSERT_FALSE(same(splash.color, stabilizing.color));
  TEST_ASSERT_FALSE(same(splash.color, stable.color));
  TEST_ASSERT_FALSE(same(splash.color, error.color));
  TEST_ASSERT_FALSE(same(stabilizing.color, stable.color));
  TEST_ASSERT_FALSE(same(stabilizing.color, error.color));
  TEST_ASSERT_FALSE(same(stable.color, error.color));
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_splash_is_white_not_blinking);
  RUN_TEST(test_stabilizing_is_orange_not_blinking);
  RUN_TEST(test_stable_is_green_not_blinking);
  RUN_TEST(test_error_is_red_blinking);
  RUN_TEST(test_all_states_distinct_colors);
  return UNITY_END();
}
