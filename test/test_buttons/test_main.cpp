#include <unity.h>

#include <cstdint>

#include "button.h"

void setUp(void) {}
void tearDown(void) {}

static ButtonTracker make_tracker() { return ButtonTracker(ButtonConfig{300}); }

void test_never_pressed_always_none(void) {
  ButtonTracker t = make_tracker();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::None),
                         static_cast<int>(t.update(false, 0)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::None),
                         static_cast<int>(t.update(false, 1000)));
}

void test_short_press_fires_on_release(void) {
  ButtonTracker t = make_tracker();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::None),
                         static_cast<int>(t.update(true, 0)));    // press starts
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::None),
                         static_cast<int>(t.update(true, 100)));  // still held, under threshold
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::ShortPress),
                         static_cast<int>(t.update(false, 150)));  // released before 300ms
}

void test_long_press_fires_once_while_held(void) {
  ButtonTracker t = make_tracker();
  t.update(true, 0);  // press starts
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::None),
                         static_cast<int>(t.update(true, 200)));  // still under threshold
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::LongPress),
                         static_cast<int>(t.update(true, 300)));  // threshold crossed
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::None),
                         static_cast<int>(t.update(true, 400)));  // still held, no repeat
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::None),
                         static_cast<int>(t.update(true, 900)));  // still held, no repeat
}

void test_release_after_long_press_is_not_a_short_press(void) {
  ButtonTracker t = make_tracker();
  t.update(true, 0);
  t.update(true, 300);  // fires LongPress
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::None),
                         static_cast<int>(t.update(false, 500)));  // release - no duplicate event
}

void test_multiple_press_cycles_are_independent(void) {
  ButtonTracker t = make_tracker();
  // Premier cycle : appui court
  t.update(true, 0);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::ShortPress),
                         static_cast<int>(t.update(false, 100)));

  // Deuxieme cycle : appui long - le chrono doit repartir de zero, pas
  // s'appuyer sur le press_start_ms_ du cycle precedent.
  t.update(true, 1000);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::None),
                         static_cast<int>(t.update(true, 1200)));  // 200ms depuis 1000, sous le seuil
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::LongPress),
                         static_cast<int>(t.update(true, 1300)));  // 300ms depuis 1000
}

void test_survives_millis_rollover(void) {
  ButtonTracker t = make_tracker();
  const uint32_t R = UINT32_MAX - 100u;
  t.update(true, R);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::None),
                         static_cast<int>(t.update(true, R + 200u)));  // 200ms, sous le seuil
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ButtonEvent::LongPress),
                         static_cast<int>(t.update(true, R + 300u)));  // 300ms, boucle au-dela de UINT32_MAX
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_never_pressed_always_none);
  RUN_TEST(test_short_press_fires_on_release);
  RUN_TEST(test_long_press_fires_once_while_held);
  RUN_TEST(test_release_after_long_press_is_not_a_short_press);
  RUN_TEST(test_multiple_press_cycles_are_independent);
  RUN_TEST(test_survives_millis_rollover);
  return UNITY_END();
}
