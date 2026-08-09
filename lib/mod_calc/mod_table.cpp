#include "mod_table.h"

// Index 0 = O2 21%. Genere par tools/generate_mod_table.py - ne pas editer
// a la main, regenerer le script si la methode change.

const int8_t kModTable_PPO2_1_4[80] = {
    56, 53, 50, 48, 46, 43, 41, 40, 38, 36,  // O2 21-30%
    35, 33, 32, 31, 30, 28, 27, 26, 25, 25,  // O2 31-40%
    24, 23, 22, 21, 21, 20, 19, 19, 18, 18,  // O2 41-50%
    17, 16, 16, 15, 15, 15, 14, 14, 13, 13,  // O2 51-60%
    12, 12, 12, 11, 11, 11, 10, 10, 10, 10,  // O2 61-70%
     9,  9,  9,  8,  8,  8,  8,  7,  7,  7,  // O2 71-80%
     7,  7,  6,  6,  6,  6,  6,  5,  5,  5,  // O2 81-90%
     5,  5,  5,  4,  4,  4,  4,  4,  4,  4,  // O2 91-100%
};

const int8_t kModTable_PPO2_1_6[80] = {
    66, 62, 59, 56, 54, 51, 49, 47, 45, 43,  // O2 21-30%
    41, 40, 38, 37, 35, 34, 33, 32, 31, 30,  // O2 31-40%
    29, 28, 27, 26, 25, 24, 24, 23, 22, 22,  // O2 41-50%
    21, 20, 20, 19, 19, 18, 18, 17, 17, 16,  // O2 51-60%
    16, 15, 15, 15, 14, 14, 13, 13, 13, 12,  // O2 61-70%
    12, 12, 11, 11, 11, 11, 10, 10, 10, 10,  // O2 71-80%
     9,  9,  9,  9,  8,  8,  8,  8,  7,  7,  // O2 81-90%
     7,  7,  7,  7,  6,  6,  6,  6,  6,  6,  // O2 91-100%
};

int mod_lookup(int o2_percent, PPO2Setpoint setpoint) {
  if (o2_percent < kModTableMinO2 || o2_percent > kModTableMaxO2) {
    return -1;
  }
  const int8_t* table =
      (setpoint == PPO2Setpoint::P14) ? kModTable_PPO2_1_4 : kModTable_PPO2_1_6;
  return table[o2_percent - kModTableMinO2];
}
