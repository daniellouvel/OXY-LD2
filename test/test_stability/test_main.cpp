#include <unity.h>

#include <cstdint>

#include "stability.h"

void setUp(void) {}
void tearDown(void) {}

// Config commune aux tests : seuil et durees a echelle reduite pour des
// sequences rapides a ecrire, meme logique que les valeurs de production
// (cf. ARCHITECTURE.md) - seuls les ordres de grandeur changent.
static StabilityConfig test_config() {
  StabilityConfig c;
  c.slope_threshold_percent_per_s = 0.1f;
  c.ema_alpha = 1.0f;  // pas de lissage : isole la machine a etats du calcul EMA
  c.min_settle_ms = 200;
  c.sustained_ms = 100;
  c.max_sample_gap_ms = 500;
  return c;
}

void test_not_stable_before_two_samples(void) {
  StabilityTracker t(test_config());
  t.reset(0);
  TEST_ASSERT_FALSE(t.is_stable());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, t.current_slope());

  t.push_sample(20.0f, 0);  // premier echantillon : pas encore de pente
  TEST_ASSERT_FALSE(t.is_stable());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, t.current_slope());
}

void test_fast_change_never_stable(void) {
  StabilityTracker t(test_config());
  t.reset(0);
  float v = 20.0f;
  for (uint32_t ms = 0; ms <= 400; ms += 50) {
    t.push_sample(v, ms);
    v += 10.0f;  // pente ~200 %O2/s, tres au-dessus du seuil 0.1
    TEST_ASSERT_FALSE(t.is_stable());
  }
}

void test_flat_signal_becomes_stable_after_min_settle_and_sustained(void) {
  StabilityTracker t(test_config());
  t.reset(0);
  t.push_sample(20.0f, 0);
  t.push_sample(20.0f, 50);   // pente=0, below_since=50
  TEST_ASSERT_FALSE(t.is_stable());  // elapsed_since_reset=50 < 200

  t.push_sample(20.0f, 150);
  TEST_ASSERT_FALSE(t.is_stable());  // elapsed_since_reset=150 < 200

  t.push_sample(20.0f, 200);
  // elapsed_since_reset=200 (OK), elapsed_below=200-50=150 >= sustained_ms(100)
  TEST_ASSERT_TRUE(t.is_stable());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, t.current_slope());
}

void test_noise_spike_restarts_sustained_timer(void) {
  StabilityTracker t(test_config());
  t.reset(0);
  t.push_sample(20.0f, 0);
  t.push_sample(20.0f, 50);
  t.push_sample(20.0f, 150);

  t.push_sample(50.0f, 200);  // pic : pente = 30/0.05 = 600 %O2/s
  TEST_ASSERT_FALSE(t.is_stable());

  t.push_sample(50.0f, 250);  // plat repris, below_since=250
  t.push_sample(50.0f, 300);
  TEST_ASSERT_FALSE(t.is_stable());  // elapsed_below = 300-250 = 50 < 100

  t.push_sample(50.0f, 350);
  // elapsed_below = 350-250 = 100 (OK), elapsed_since_reset = 350 >= 200 (OK)
  TEST_ASSERT_TRUE(t.is_stable());
}

void test_redrift_after_stable_is_not_latched(void) {
  StabilityTracker t(test_config());
  t.reset(0);
  t.push_sample(20.0f, 0);
  t.push_sample(20.0f, 50);
  t.push_sample(20.0f, 150);
  t.push_sample(20.0f, 200);
  TEST_ASSERT_TRUE(t.is_stable());

  t.push_sample(80.0f, 250);  // grosse derive reprend
  TEST_ASSERT_FALSE(t.is_stable());
}

void test_large_gap_resets_baseline_silently(void) {
  StabilityTracker t(test_config());
  t.reset(0);
  t.push_sample(20.0f, 0);
  TEST_ASSERT_TRUE(t.push_sample(20.0f, 50));  // below_since=50

  // Ecart de 501ms > max_sample_gap_ms(500) : discontinuite
  TEST_ASSERT_FALSE(t.push_sample(20.0f, 551));
  TEST_ASSERT_FALSE(t.is_stable());  // below_threshold_ reinitialise

  // Reprise normale apres le gap - le chrono redemarre a partir d'ici, pas
  // de la periode plate d'avant le gap.
  t.push_sample(20.0f, 601);  // dt=50 depuis 551, pente=0, below_since=601
  TEST_ASSERT_FALSE(t.is_stable());  // elapsed_below = 601-601 = 0 < 100

  t.push_sample(20.0f, 701);  // elapsed_below = 701-601 = 100 (OK)
  // elapsed_since_reset = 701 >= 200 (OK)
  TEST_ASSERT_TRUE(t.is_stable());
}

void test_duplicate_timestamp_ignored(void) {
  StabilityTracker t(test_config());
  t.reset(0);
  t.push_sample(20.0f, 100);
  TEST_ASSERT_FALSE(t.push_sample(25.0f, 100));  // dt=0, ignore
}

void test_ema_smoothing_blends_successive_slopes(void) {
  StabilityConfig c = test_config();
  c.ema_alpha = 0.5f;
  StabilityTracker t(c);
  t.reset(0);

  t.push_sample(0.0f, 0);
  t.push_sample(10.0f, 100);  // dt=0.1s, raw_slope=100 -> premiere pente, pas de blend
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, t.current_slope());

  t.push_sample(10.0f, 200);  // dt=0.1s, raw_slope=0 -> blend : 0.5*0 + 0.5*100 = 50
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, t.current_slope());
}

void test_stability_survives_millis_rollover(void) {
  StabilityTracker t(test_config());
  const uint32_t R = UINT32_MAX - 300u;  // reset proche du rollover uint32

  t.reset(R);
  t.push_sample(20.0f, R + 0u);
  t.push_sample(20.0f, R + 50u);
  t.push_sample(20.0f, R + 150u);
  t.push_sample(20.0f, R + 200u);
  TEST_ASSERT_TRUE(t.is_stable());  // stable juste avant le rollover (R+200 = UINT32_MAX-100)

  // Ces echeances traversent UINT32_MAX (rollover de millis()).
  t.push_sample(20.0f, R + 250u);  // = UINT32_MAX-50
  t.push_sample(20.0f, R + 300u);  // = UINT32_MAX
  t.push_sample(20.0f, R + 350u);  // boucle a 49 (UINT32_MAX+50 mod 2^32)

  TEST_ASSERT_TRUE(t.is_stable());  // toujours stable, calcul correct malgre le rollover
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, t.current_slope());
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_not_stable_before_two_samples);
  RUN_TEST(test_fast_change_never_stable);
  RUN_TEST(test_flat_signal_becomes_stable_after_min_settle_and_sustained);
  RUN_TEST(test_noise_spike_restarts_sustained_timer);
  RUN_TEST(test_redrift_after_stable_is_not_latched);
  RUN_TEST(test_large_gap_resets_baseline_silently);
  RUN_TEST(test_duplicate_timestamp_ignored);
  RUN_TEST(test_ema_smoothing_blends_successive_slopes);
  RUN_TEST(test_stability_survives_millis_rollover);
  return UNITY_END();
}
