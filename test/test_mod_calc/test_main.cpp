#include <unity.h>

#include <cmath>

#include "mod_table.h"
#include "o2_value.h"

void setUp(void) {}
void tearDown(void) {}

// --- o2_display_value : toujours entier, toujours arrondi vers le haut ---

void test_o2_display_value_rounds_up_on_fraction(void) {
  TEST_ASSERT_EQUAL_INT(33, o2_display_value(32.1f));
  TEST_ASSERT_EQUAL_INT(22, o2_display_value(21.001f));
}

void test_o2_display_value_exact_integer_unchanged(void) {
  TEST_ASSERT_EQUAL_INT(32, o2_display_value(32.0f));
}

void test_is_fo2_valid_range(void) {
  TEST_ASSERT_TRUE(is_fo2_valid(0.0f));
  TEST_ASSERT_TRUE(is_fo2_valid(100.0f));
  TEST_ASSERT_TRUE(is_fo2_valid(20.9f));
  TEST_ASSERT_FALSE(is_fo2_valid(-0.1f));
  TEST_ASSERT_FALSE(is_fo2_valid(100.1f));
}

// --- mod_lookup : bornes ---

void test_mod_lookup_out_of_range_returns_minus_one(void) {
  TEST_ASSERT_EQUAL_INT(-1, mod_lookup(20, PPO2Setpoint::P14));
  TEST_ASSERT_EQUAL_INT(-1, mod_lookup(101, PPO2Setpoint::P16));
}

void test_mod_lookup_table_bounds(void) {
  // Air (21%) et O2 pur (100%) - non presents dans la table papier du club,
  // formule seule. O2 pur/ppO2=1.6 -> 6m confirme par la legende de la
  // table papier ("O2 pur profondeur maximale d'utilisation : 6m").
  TEST_ASSERT_EQUAL_INT(56, mod_lookup(21, PPO2Setpoint::P14));
  TEST_ASSERT_EQUAL_INT(66, mod_lookup(21, PPO2Setpoint::P16));
  TEST_ASSERT_EQUAL_INT(4, mod_lookup(100, PPO2Setpoint::P14));
  TEST_ASSERT_EQUAL_INT(6, mod_lookup(100, PPO2Setpoint::P16));
}

// --- mod_lookup : concordance avec la table papier du club ---
// Table.png (F. Gentili / sommeildesepaves.com), ppO2=1.6, O2 22-99%.
// Comparaison exhaustive - toute regression de la table statique fait
// echouer ce test, pas seulement un point isole.

void test_mod_lookup_matches_club_reference_table_ppo2_1_6(void) {
  static const int club_table_22_to_99[78] = {
      62, 59, 56, 54, 51, 49, 47, 45, 43,       // 22-30 (9 valeurs)
      41, 40, 38, 37, 35, 34, 33, 32, 31, 30,   // 31-40
      29, 28, 27, 26, 25, 24, 24, 23, 22, 22,   // 41-50
      21, 20, 20, 19, 19, 18, 18, 17, 17, 16,   // 51-60
      16, 15, 15, 15, 14, 14, 13, 13, 13, 12,   // 61-70
      12, 12, 11, 11, 11, 11, 10, 10, 10, 10,   // 71-80
      9,  9,  9,  9,  8,  8,  8,  8,  7,  7,    // 81-90
      7,  7,  7,  7,  6,  6,  6,  6,  6,        // 91-99 (9 valeurs)
  };
  for (int o2 = 22; o2 <= 99; ++o2) {
    int expected = club_table_22_to_99[o2 - 22];
    int actual = mod_lookup(o2, PPO2Setpoint::P16);
    TEST_ASSERT_EQUAL_INT_MESSAGE(expected, actual, "MOD@1.6 ne correspond plus a la table du club");
  }
}

// --- Coherence interne : la table doit rester egale a la formule generatrice ---
// Garde-fou si quelqu'un modifie mod_table.cpp a la main sans repasser par
// tools/generate_mod_table.py.

// ppo2 en double (pas float) - doit matcher exactement la precision du
// generateur Python (dont les float sont des double). Un ppo2 en float32
// perd assez de precision sur 1.4/1.6 pour decaler certaines lignes de 1m
// (trouve en executant ce test : o2=25%, ppO2=1.4 -> 46 attendu, 45 obtenu
// avec un float32).
static int mod_floor_reference(int o2_percent, double ppo2) {
  const double eps = 1e-9;
  double mod_exact = ((ppo2 / (o2_percent / 100.0)) - 1) * 10;
  return static_cast<int>(std::floor(mod_exact + eps));
}

void test_mod_table_matches_generating_formula(void) {
  for (int o2 = kModTableMinO2; o2 <= kModTableMaxO2; ++o2) {
    TEST_ASSERT_EQUAL_INT(mod_floor_reference(o2, 1.4), mod_lookup(o2, PPO2Setpoint::P14));
    TEST_ASSERT_EQUAL_INT(mod_floor_reference(o2, 1.6), mod_lookup(o2, PPO2Setpoint::P16));
  }
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_o2_display_value_rounds_up_on_fraction);
  RUN_TEST(test_o2_display_value_exact_integer_unchanged);
  RUN_TEST(test_is_fo2_valid_range);
  RUN_TEST(test_mod_lookup_out_of_range_returns_minus_one);
  RUN_TEST(test_mod_lookup_table_bounds);
  RUN_TEST(test_mod_lookup_matches_club_reference_table_ppo2_1_6);
  RUN_TEST(test_mod_table_matches_generating_formula);
  return UNITY_END();
}
