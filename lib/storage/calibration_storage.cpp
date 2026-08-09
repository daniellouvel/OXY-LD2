#include "calibration_storage.h"

#include <cstring>

namespace {

// FNV-1a 32 bits : simple, bonne dispersion, suffisant pour detecter une
// ecriture partielle/corrompue - pas un usage cryptographique.
uint32_t fnv1a(const uint8_t* data, int len) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < len; ++i) {
    hash ^= data[i];
    hash *= 16777619u;
  }
  return hash;
}

constexpr int kPayloadSize = 13;  // version(1) + v_air_mv(4) + temp_c(4) + timestamp_ms(4)

}  // namespace

void serialize_calibration(const CalibrationPoint& point, uint8_t* buffer) {
  buffer[0] = kCalibrationStorageVersion;
  std::memcpy(buffer + 1, &point.v_air_mv, sizeof(float));
  std::memcpy(buffer + 5, &point.temp_c, sizeof(float));
  std::memcpy(buffer + 9, &point.timestamp_ms, sizeof(uint32_t));

  uint32_t checksum = fnv1a(buffer, kPayloadSize);
  std::memcpy(buffer + kPayloadSize, &checksum, sizeof(uint32_t));
}

bool deserialize_calibration(const uint8_t* buffer, CalibrationPoint* out_point) {
  if (buffer[0] != kCalibrationStorageVersion) {
    return false;
  }

  uint32_t expected_checksum = fnv1a(buffer, kPayloadSize);
  uint32_t stored_checksum;
  std::memcpy(&stored_checksum, buffer + kPayloadSize, sizeof(uint32_t));
  if (expected_checksum != stored_checksum) {
    return false;
  }

  std::memcpy(&out_point->v_air_mv, buffer + 1, sizeof(float));
  std::memcpy(&out_point->temp_c, buffer + 5, sizeof(float));
  std::memcpy(&out_point->timestamp_ms, buffer + 9, sizeof(uint32_t));
  return true;
}
