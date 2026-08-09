#include <unity.h>

#include <string>

#include "web_content.h"

void setUp(void) {}
void tearDown(void) {}

void test_page_shell_contains_title_body_and_nav(void) {
  std::string page = page_shell("Titre Test", "<p>corps</p>");
  TEST_ASSERT_TRUE(page.find("Titre Test") != std::string::npos);
  TEST_ASSERT_TRUE(page.find("<p>corps</p>") != std::string::npos);
  TEST_ASSERT_TRUE(page.find("/materiel") != std::string::npos);
  TEST_ASSERT_TRUE(page.find("/plongee") != std::string::npos);
  TEST_ASSERT_TRUE(page.find("/tables") != std::string::npos);
}

void test_hardware_page_shows_ok_status(void) {
  HardwareStatus hw{/*ads_ok=*/true, /*rtc_ok=*/true, 14, 32, 8.125f};
  std::string page = build_hardware_page(hw);
  TEST_ASSERT_TRUE(page.find("detecte") != std::string::npos);
  TEST_ASSERT_TRUE(page.find("8.125") != std::string::npos);
  TEST_ASSERT_TRUE(page.find("14:32") != std::string::npos);
  // Le CSS ".status-bad{...}" est toujours present via page_shell - on
  // verifie l'USAGE reel de la classe (class="status-bad"), pas juste la
  // presence du nom quelque part dans la page.
  TEST_ASSERT_TRUE(page.find("class=\"status-bad\"") == std::string::npos);
}

void test_hardware_page_shows_absent_status(void) {
  HardwareStatus hw{/*ads_ok=*/false, /*rtc_ok=*/false, 0, 0, 0.0f};
  std::string page = build_hardware_page(hw);
  TEST_ASSERT_TRUE(page.find("absent") != std::string::npos);
  TEST_ASSERT_TRUE(page.find("class=\"status-ok\"") == std::string::npos);
}

void test_dive_page_marks_correct_ppo2_selected(void) {
  DiveStatus d14{21, 66, 1.4f, false};
  std::string page14 = build_dive_page(d14);
  TEST_ASSERT_TRUE(page14.find("value=\"1.4\" selected") != std::string::npos);
  TEST_ASSERT_TRUE(page14.find("value=\"1.6\" selected") == std::string::npos);

  DiveStatus d16{21, 66, 1.6f, false};
  std::string page16 = build_dive_page(d16);
  TEST_ASSERT_TRUE(page16.find("value=\"1.6\" selected") != std::string::npos);
  TEST_ASSERT_TRUE(page16.find("value=\"1.4\" selected") == std::string::npos);
}

void test_dive_page_no_measurement_yet(void) {
  DiveStatus d{-1, -1, 1.6f, false};
  std::string page = build_dive_page(d);
  TEST_ASSERT_TRUE(page.find("Pas de mesure valide") != std::string::npos);
}

void test_dive_page_shows_measurement_and_stability(void) {
  DiveStatus d{21, 66, 1.6f, true};
  std::string page = build_dive_page(d);
  TEST_ASSERT_TRUE(page.find("O2 : 21 %") != std::string::npos);
  TEST_ASSERT_TRUE(page.find("MOD : 66 m") != std::string::npos);
  TEST_ASSERT_TRUE(page.find("status-ok\">OK") != std::string::npos);
}

void test_tables_page_contains_validated_reference_rows(void) {
  std::string page = build_tables_page();
  // Points deja valides contre la table papier du club (cf. ARCHITECTURE.md).
  TEST_ASSERT_TRUE(page.find("<tr><td>21%</td><td>56 m</td><td>66 m</td></tr>") !=
                    std::string::npos);
  TEST_ASSERT_TRUE(page.find("<tr><td>32%</td><td>33 m</td><td>40 m</td></tr>") !=
                    std::string::npos);
  TEST_ASSERT_TRUE(page.find("<tr><td>100%</td><td>4 m</td><td>6 m</td></tr>") !=
                    std::string::npos);
}

void test_status_json_contains_expected_fields(void) {
  HardwareStatus hw{true, true, 10, 0, 8.0f};
  DiveStatus d{21, 66, 1.6f, true};
  std::string json = build_status_json(hw, d);
  TEST_ASSERT_TRUE(json.find("\"o2\":21") != std::string::npos);
  TEST_ASSERT_TRUE(json.find("\"mod\":66") != std::string::npos);
  TEST_ASSERT_TRUE(json.find("\"ppo2\":1.6") != std::string::npos);
  TEST_ASSERT_TRUE(json.find("\"stable\":true") != std::string::npos);
  TEST_ASSERT_TRUE(json.find("\"ads_ok\":true") != std::string::npos);
  TEST_ASSERT_TRUE(json.find("\"rtc_ok\":true") != std::string::npos);
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_page_shell_contains_title_body_and_nav);
  RUN_TEST(test_hardware_page_shows_ok_status);
  RUN_TEST(test_hardware_page_shows_absent_status);
  RUN_TEST(test_dive_page_marks_correct_ppo2_selected);
  RUN_TEST(test_dive_page_no_measurement_yet);
  RUN_TEST(test_dive_page_shows_measurement_and_stability);
  RUN_TEST(test_tables_page_contains_validated_reference_rows);
  RUN_TEST(test_status_json_contains_expected_fields);
  return UNITY_END();
}
