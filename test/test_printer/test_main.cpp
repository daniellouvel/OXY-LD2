#include <unity.h>

#include <cstring>

#include "label.h"

void setUp(void) {}
void tearDown(void) {}

void test_label_contains_expected_fields(void) {
  LabelData data{32, 33, 1.4f, "27/04/2026", "14:32"};
  char buffer[kLabelMaxLength];
  int len = build_label_tspl(data, buffer, sizeof(buffer));

  TEST_ASSERT_GREATER_THAN_INT(0, len);
  TEST_ASSERT_NOT_NULL(std::strstr(buffer, "O2: 32 %"));
  TEST_ASSERT_NOT_NULL(std::strstr(buffer, "MOD: 33 m"));
  TEST_ASSERT_NOT_NULL(std::strstr(buffer, "ppO2 ref: 1.4"));
  TEST_ASSERT_NOT_NULL(std::strstr(buffer, "27/04/2026"));
  TEST_ASSERT_NOT_NULL(std::strstr(buffer, "14:32"));
  TEST_ASSERT_NOT_NULL(std::strstr(buffer, "PRINT 1,1"));
}

void test_label_invalid_mod_shows_placeholder(void) {
  LabelData data{20, -1, 1.4f, "01/01/2026", "08:00"};
  char buffer[kLabelMaxLength];
  int len = build_label_tspl(data, buffer, sizeof(buffer));

  TEST_ASSERT_GREATER_THAN_INT(0, len);
  TEST_ASSERT_NOT_NULL(std::strstr(buffer, "MOD: --"));
  TEST_ASSERT_NULL(std::strstr(buffer, "MOD: -1"));
}

void test_label_ppo2_16_formats_correctly(void) {
  LabelData data{36, 28, 1.6f, "05/05/2026", "10:15"};
  char buffer[kLabelMaxLength];
  build_label_tspl(data, buffer, sizeof(buffer));

  TEST_ASSERT_NOT_NULL(std::strstr(buffer, "ppO2 ref: 1.6"));
}

void test_label_buffer_too_small_returns_zero(void) {
  LabelData data{32, 33, 1.4f, "27/04/2026", "14:32"};
  char tiny_buffer[8];
  int len = build_label_tspl(data, tiny_buffer, sizeof(tiny_buffer));
  TEST_ASSERT_EQUAL_INT(0, len);
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_label_contains_expected_fields);
  RUN_TEST(test_label_invalid_mod_shows_placeholder);
  RUN_TEST(test_label_ppo2_16_formats_correctly);
  RUN_TEST(test_label_buffer_too_small_returns_zero);
  return UNITY_END();
}
