#include "label.h"

#include <cstddef>
#include <cstdio>

int build_label_tspl(const LabelData& data, char* buffer, int buffer_size) {
  char mod_str[16];
  if (data.mod_meters >= 0) {
    std::snprintf(mod_str, sizeof(mod_str), "%d m", data.mod_meters);
  } else {
    std::snprintf(mod_str, sizeof(mod_str), "--");
  }

  int written = std::snprintf(
      buffer, static_cast<size_t>(buffer_size),
      "SIZE 90 mm, 98 mm\r\n"
      "GAP 2 mm, 0\r\n"
      "DIRECTION 1\r\n"
      "CLS\r\n"
      "TEXT 100,30,\"3\",0,1,1,\"ANALYSE O2\"\r\n"
      "TEXT 50,80,\"2\",0,1,1,\"Plongeur: ______________\"\r\n"
      "TEXT 80,150,\"4\",0,1,1,\"O2: %d %%\"\r\n"
      "TEXT 80,220,\"3\",0,1,1,\"MOD: %s\"\r\n"
      "TEXT 80,270,\"2\",0,1,1,\"ppO2 ref: %.1f\"\r\n"
      "TEXT 60,320,\"1\",0,1,1,\"%s  %s\"\r\n"
      "PRINT 1,1\r\n",
      data.o2_percent, mod_str, static_cast<double>(data.ppo2_setpoint), data.date_str,
      data.time_str);

  if (written < 0 || written >= buffer_size) {
    return 0;
  }
  return written;
}
