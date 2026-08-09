#include <Arduino.h>

#include "ads1115_reader.h"
#include "calibration.h"
#include "conversion.h"
#include "display.h"
#include "glitch_filter.h"
#include "led_status.h"
#include "mod_table.h"
#include "o2_value.h"
#include "rtc_clock.h"
#include "stability.h"

// Boucle de mesure de bout en bout - PREMIERE INTEGRATION MATERIELLE.
// Voir ARCHITECTURE.md pour le detail des limites et de ce qui est verifie.
//
// Cable a ce jour : ESP32-S3 + ADS1115 + cellule O2 + ecran TFT + RTC
// PCF8563. DS18B20 et boutons ne sont PAS cables :
//  - millis() sert toujours d'horloge pour stability/calibration (uint32_t
//    now_ms brut, distinct de l'heure murale du RTC qui sert uniquement
//    a l'affichage)
//  - pas de compensation thermique (apply_thermal_compensation() non
//    appelee - has_temperature() resterait de toute facon faux)
//  - ppO2 fixe en dur a 1.6 (pas de bouton pour la choisir)
//
// Calibration : pas de bouton pour declencher une vraie calibration.
// Auto-calibration NAIVE sur la toute premiere lecture valide au
// demarrage - UNIQUEMENT pour ce premier smoke-test de bout en bout, PAS
// la vraie fonctionnalite d'auto-calibration d'OXY-LD (qui verifie la
// stabilite et se declenche apres un arret prolonge). A remplacer par un
// vrai declenchement (bouton) des que cable.

namespace {

Ads1115Reader ads;
Display display;
RtcClock rtc;
GlitchFilter glitch_filter;

bool rtc_ok = false;

// Plage de tension plausible non sourcee (pas de fiche technique
// verifiee pour la cellule montee) - point de depart large, a resserrer.
CalibrationTracker calibration(5.0f, 15.0f);

// eps/alpha/durees revues sur retour d'experience (analyseur du
// commerce), toujours pas calibrees sur cette cellule - cf. ARCHITECTURE.md.
StabilityConfig stability_config{0.02f, 0.3f, 5000, 3000, 10000};
StabilityTracker stability(stability_config);

constexpr float kPpo2Setpoint = 1.6f;  // fixe - pas de bouton cable ce soir
constexpr PPO2Setpoint kPpo2SetpointEnum = PPO2Setpoint::P16;  // doit rester coherent avec kPpo2Setpoint

bool ads_ok = false;

void show_error() {
  RgbColor c = led_status_for(SystemState::Error).color;
  display.show_measurement(0, -1, kPpo2Setpoint, c, /*stable=*/false, /*o2_value_visible=*/true);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println("OXY-LD2 - boot");

  ads_ok = ads.begin();
  if (!ads_ok) {
    Serial.println("ERREUR : ADS1115 non detecte (I2C 0x48)");
  } else {
    Serial.println("ADS1115 detecte (I2C 0x48)");
  }

  display.begin();
  Serial.println("display.begin() termine");
  display.show_splash();
  stability.reset(millis());

  rtc_ok = rtc.begin();
  if (!rtc_ok) {
    Serial.println("ERREUR : RTC non detecte (I2C 0x51)");
  } else if (rtc.lost_power()) {
    Serial.println("RTC detecte mais heure non fiable (perte d'alimentation) - a regler");
  } else {
    Serial.println("RTC detecte, heure OK");
  }
}

void loop() {
  if (rtc_ok) {
    int hour, minute;
    rtc.now(&hour, &minute);
    display.show_clock(hour, minute);
  }

  if (!ads_ok) {
    show_error();
    return;
  }

  uint32_t now = millis();
  float v_mv = ads.read_mv();
  glitch_filter.push(v_mv);
  float filtered_mv = glitch_filter.value();

  // Auto-calibration naive au tout premier passage - cf. avertissement en
  // tete de fichier, pas la vraie fonctionnalite d'auto-calibration.
  if (!calibration.is_calibrated()) {
    calibration.calibrate(filtered_mv, now);
  }

  float fo2_raw = voltage_to_o2_percent(filtered_mv, calibration.current_v_air_mv());
  if (!is_fo2_valid(fo2_raw)) {
    show_error();
    return;
  }

  stability.push_sample(fo2_raw, now);

  int o2_display = o2_display_value(fo2_raw);
  int mod = mod_lookup(o2_display, kPpo2SetpointEnum);

  SystemState state = stability.is_stable() ? SystemState::Stable : SystemState::Stabilizing;
  LedStatusOutput led = led_status_for(state);

  // Clignotement pendant la stabilisation (fenetre de 500ms sur millis()),
  // fixe une fois stable - cf. ARCHITECTURE.md "Retour UI attendu".
  bool o2_visible = stability.is_stable() || ((now / 500) % 2 == 0);

  display.show_measurement(o2_display, mod, kPpo2Setpoint, led.color, stability.is_stable(),
                            o2_visible);

  // Diagnostic serie periodique (1x/s) - ajoute pour verifier reellement le
  // pipeline cette nuit, pas seulement l'absence de crash. A retirer ou
  // reduire une fois le comportement confirme sur la vraie carte.
  static uint32_t last_log_ms = 0;
  if (now - last_log_ms >= 1000) {
    last_log_ms = now;
    int h = -1, m = -1;
    if (rtc_ok) {
      rtc.now(&h, &m);
    }
    Serial.printf(
        "v_mv=%.3f fo2=%.2f o2=%d mod=%d slope=%.4f stable=%d cal_v=%.3f rtc_ok=%d clock=%02d:%02d\n",
        filtered_mv, fo2_raw, o2_display, mod, stability.current_slope(),
        stability.is_stable() ? 1 : 0, calibration.current_v_air_mv(), rtc_ok ? 1 : 0, h, m);
  }
}
