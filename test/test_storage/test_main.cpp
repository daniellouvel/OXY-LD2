#include <unity.h>

#include <cmath>
#include <cstdint>
#include <cstring>

#include "calibration_storage.h"

void setUp(void) {}
void tearDown(void) {}

void test_storage_size_matches_documented_layout(void) {
  TEST_ASSERT_EQUAL_INT(17, kCalibrationStorageSize);
}

void test_roundtrip_with_temperature(void) {
  CalibrationPoint in{10.5f, 22.3f, 123456u};
  uint8_t buffer[kCalibrationStorageSize];
  serialize_calibration(in, buffer);

  CalibrationPoint out{};
  TEST_ASSERT_TRUE(deserialize_calibration(buffer, &out));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 10.5f, out.v_air_mv);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 22.3f, out.temp_c);
  TEST_ASSERT_EQUAL_UINT32(123456u, out.timestamp_ms);
}

void test_roundtrip_without_temperature(void) {
  CalibrationPoint in{9.8f, NAN, 999u};
  uint8_t buffer[kCalibrationStorageSize];
  serialize_calibration(in, buffer);

  CalibrationPoint out{};
  TEST_ASSERT_TRUE(deserialize_calibration(buffer, &out));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 9.8f, out.v_air_mv);
  TEST_ASSERT_TRUE(std::isnan(out.temp_c));
  TEST_ASSERT_EQUAL_UINT32(999u, out.timestamp_ms);
}

void test_deserialize_rejects_wrong_version(void) {
  CalibrationPoint in{10.0f, 20.0f, 1000u};
  uint8_t buffer[kCalibrationStorageSize];
  serialize_calibration(in, buffer);
  buffer[0] = kCalibrationStorageVersion + 1;  // simule un format futur incompatible

  CalibrationPoint out{-1.0f, -1.0f, 42u};  // sentinelles pour verifier l'absence d'ecriture
  TEST_ASSERT_FALSE(deserialize_calibration(buffer, &out));
  TEST_ASSERT_EQUAL_FLOAT(-1.0f, out.v_air_mv);
  TEST_ASSERT_EQUAL_UINT32(42u, out.timestamp_ms);
}

void test_deserialize_rejects_corrupted_checksum(void) {
  CalibrationPoint in{10.0f, 20.0f, 1000u};
  uint8_t buffer[kCalibrationStorageSize];
  serialize_calibration(in, buffer);
  buffer[1] ^= 0xFF;  // corrompt v_air_mv sans mettre a jour le checksum

  CalibrationPoint out{};
  TEST_ASSERT_FALSE(deserialize_calibration(buffer, &out));
}

void test_deserialize_rejects_blank_flash(void) {
  uint8_t buffer[kCalibrationStorageSize];
  std::memset(buffer, 0xFF, sizeof(buffer));  // etat flash jamais ecrite

  CalibrationPoint out{};
  TEST_ASSERT_FALSE(deserialize_calibration(buffer, &out));
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_storage_size_matches_documented_layout);
  RUN_TEST(test_roundtrip_with_temperature);
  RUN_TEST(test_roundtrip_without_temperature);
  RUN_TEST(test_deserialize_rejects_wrong_version);
  RUN_TEST(test_deserialize_rejects_corrupted_checksum);
  RUN_TEST(test_deserialize_rejects_blank_flash);
  return UNITY_END();
}
