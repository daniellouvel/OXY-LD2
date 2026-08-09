#include <Arduino.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <WiFi.h>

#include <cmath>

#include "ads1115_reader.h"
#include "calibration.h"
#include "calibration_storage.h"
#include "conversion.h"
#include "display.h"
#include "glitch_filter.h"
#include "led_status.h"
#include "mod_table.h"
#include "o2_value.h"
#include "rtc_clock.h"
#include "stability.h"
#include "web_content.h"

// Boucle de mesure de bout en bout + serveur web de configuration.
// Voir ARCHITECTURE.md pour le detail des limites et de ce qui est verifie.
//
// Cable a ce jour : ESP32-S3 + ADS1115 + cellule O2 + ecran TFT + RTC
// PCF8563. DS18B20 et boutons ne sont PAS cables :
//  - pas de compensation thermique (apply_thermal_compensation() non
//    appelee - has_temperature() resterait de toute facon faux)
//  - ppO2 reglable depuis la page web /plongee - plus une constexpr
//
// Deux horloges distinctes, deliberement : millis() pour stability (court
// terme, pas besoin de survivre a un reboot) ; horloge RTC (secondes Unix,
// calibration_clock() ci-dessous) pour calibration (doit survivre a un
// redemarrage pour que "temps depuis la derniere calibration" ait un sens).
//
// Calibration - conception detaillee avec l'utilisateur (cf. conversation) :
//  - Persistee reellement en flash (Preferences NVS), chargee au boot.
//  - Auto-calibration armee au demarrage SEULEMENT si une calibration
//    precedente valide existe ET que >= kMinRestSeconds (24h) se sont
//    ecoules depuis - protege du redemarrage rapide (gaz residuel dans le
//    circuit). Jamais armee au tout premier boot (aucune calibration a
//    proteger) - calibration manuelle requise la premiere fois.
//  - Se declenche seulement quand stability.is_stable() est vrai - jamais
//    sur une lecture instantanee.
//  - stability tourne sur la tension BRUTE (filtered_mv), pas sur le %O2
//    converti : la conversion depend de la calibration, qui elle-meme
//    depend de la stabilite -> il faut une source de stabilite qui ne
//    depende pas d'etre deja calibre. %O2 = k*mV etant lineaire, la
//    detection de pente donne le meme resultat qualitatif sur les deux,
//    seul le seuil epsilon change d'unite (mV/s au lieu de %O2/s).
//  - Calibration manuelle (bouton web /calibrate) : memes garde-fous
//    (stable + tension dans la plage plausible de CalibrationTracker),
//    mais PAS de temps de repos - une action explicite de l'encadrant
//    n'a pas besoin d'etre protegee contre elle-meme.
//
// Serveur web : point d'acces WiFi (pas d'authentification HTTP - le mot
// de passe WiFi protege deja l'acces). Concurrence : AsyncWebServer
// execute ses handlers sur sa propre tache FreeRTOS, potentiellement
// concurrente avec loop(). Seul loop() touche le materiel (I2C, flash) et
// mute `calibration` - les handlers web ne font que lire/ecrire le
// snapshot g_state protege par g_mutex (y compris la demande de
// calibration manuelle, un simple flag que loop() execute).

namespace {

Ads1115Reader ads;
Display display;
RtcClock rtc;
GlitchFilter glitch_filter;
AsyncWebServer server(80);
DNSServer dns_server;
Preferences prefs;

bool rtc_ok = false;
bool ads_ok = false;

// Plage de tension plausible non sourcee (pas de fiche technique
// verifiee pour la cellule montee) - point de depart large, a resserrer.
CalibrationTracker calibration(5.0f, 15.0f);
uint32_t g_last_calibration_ts = 0;  // secondes Unix, pour la serialisation flash

bool auto_cal_armed = false;
constexpr uint32_t kMinRestSeconds = 24u * 3600u;  // 24h, comme OXY-LD

// Pente en mV/s (pas %O2/s, cf. commentaire d'en-tete).
// Historique : 0.01 (extrapolation de l'ancien seuil %O2/s) jamais
// atteignable -> 0.5 (ancre sur le plafond de bruit observe, 0.02-0.4
// mV/s) atteignable mais NE TIENT PAS (retour utilisateur, confirme dans
// mes propres logs : stable=1 quelques secondes puis retombe a 0) - le
// seuil etait trop proche du plafond de bruit reel, pas assez de marge.
// Deux ajustements combines cette fois :
//  - alpha 0.3 -> 0.1 : lissage EMA de la pente nettement plus fort,
//    reduit la sensibilite au bruit instantane plutot que de juste
//    deplacer le seuil (le seuil seul ne peut pas compenser un signal
//    bruite, il faut aussi lisser le signal lui-meme).
//  - seuil 0.5 -> 0.6 mV/s : marge au-dessus du plafond de bruit observe
//    plutot que colle dessus.
// Toujours pas verifie pour distinguer un vrai changement de gaz du bruit
// - a affiner en soufflant/couvrant la cellule pour de vrai (pas possible
// a distance). A revoir si la stabilite ne tient toujours pas.
StabilityConfig stability_config{0.6f, 0.1f, 5000, 3000, 10000};
StabilityTracker stability(stability_config);

uint32_t calibration_clock() {
  // Secondes Unix (survit a un redemarrage) - PAS de millis() ici, qui
  // repart a zero a chaque boot et rendrait "temps depuis la derniere
  // calibration" toujours nul. Repli sur millis()/1000 si RTC absent
  // (ne survit pas au reboot dans ce cas degrade, mais reste coherent au
  // sein d'une seule session).
  return rtc_ok ? rtc.unix_time() : millis() / 1000;
}

void save_calibration_to_flash() {
  CalibrationPoint pt{calibration.current_v_air_mv(), calibration.current_temp_c(),
                      g_last_calibration_ts};
  uint8_t buf[kCalibrationStorageSize];
  serialize_calibration(pt, buf);
  prefs.putBytes("cal", buf, kCalibrationStorageSize);
}

// Etat partage avec les handlers web, protege par g_mutex - cf. commentaire
// d'en-tete. ppo2_setpoint et calibration_requested sont les seuls champs
// modifiables depuis le web ; les autres sont mis a jour par loop().
struct SharedState {
  int o2_percent = -1;
  int mod_meters = -1;
  bool stable = false;
  float last_v_mv = 0.0f;
  int rtc_hour = 0;
  int rtc_minute = 0;
  float ppo2_setpoint = 1.6f;
  bool calibrated = false;
  bool calibration_requested = false;
};
SharedState g_state;
SemaphoreHandle_t g_mutex = nullptr;

constexpr TickType_t kMutexTimeout = pdMS_TO_TICKS(50);

PPO2Setpoint ppo2_enum(float ppo2) { return (ppo2 >= 1.5f) ? PPO2Setpoint::P16 : PPO2Setpoint::P14; }

// Lissage COSMETIQUE reserve a l'affichage, pour attenuer l'oscillation
// visible entre deux valeurs entieres adjacentes pres d'une frontiere
// (ex. 21%/22%) quand le bruit du signal brut chevauche cette frontiere.
// Ne remplace PAS fo2_raw pour stability.push_sample() : cf. commentaire
// d'en-tete sur le choix de la tension brute pour stability.
float display_fo2_smoothed = NAN;
constexpr float kDisplayEmaAlpha = 0.15f;

void show_error(float ppo2) {
  RgbColor c = led_status_for(SystemState::Error).color;
  display.show_measurement(0, -1, ppo2, c, /*stable=*/false, /*o2_value_visible=*/true);
}

// -- Handlers web : lisent/ecrivent uniquement g_state sous mutex, jamais
// le materiel ni `calibration` directement (cf. commentaire d'en-tete). --

void handle_materiel(AsyncWebServerRequest* request) {
  HardwareStatus hw{};
  hw.ads_ok = ads_ok;
  if (xSemaphoreTake(g_mutex, kMutexTimeout) == pdTRUE) {
    hw.rtc_ok = rtc_ok;
    hw.rtc_hour = g_state.rtc_hour;
    hw.rtc_minute = g_state.rtc_minute;
    hw.last_v_mv = g_state.last_v_mv;
    xSemaphoreGive(g_mutex);
  }
  request->send(200, "text/html", build_hardware_page(hw).c_str());
}

void handle_plongee(AsyncWebServerRequest* request) {
  DiveStatus d{};
  if (xSemaphoreTake(g_mutex, kMutexTimeout) == pdTRUE) {
    d.o2_percent = g_state.o2_percent;
    d.mod_meters = g_state.mod_meters;
    d.stable = g_state.stable;
    d.ppo2_setpoint = g_state.ppo2_setpoint;
    xSemaphoreGive(g_mutex);
  }
  request->send(200, "text/html", build_dive_page(d).c_str());
}

void handle_tables(AsyncWebServerRequest* request) {
  request->send(200, "text/html", build_tables_page().c_str());
}

void handle_set_ppo2(AsyncWebServerRequest* request) {
  if (request->hasParam("ppo2", true)) {
    float v = request->getParam("ppo2", true)->value().toFloat();
    if (v == 1.4f || v == 1.6f) {
      if (xSemaphoreTake(g_mutex, kMutexTimeout) == pdTRUE) {
        g_state.ppo2_setpoint = v;
        xSemaphoreGive(g_mutex);
      }
    }
  }
  request->redirect("/plongee");
}

void handle_calibrate(AsyncWebServerRequest* request) {
  // Ne fait que poser un flag - loop() est seul a executer la calibration
  // (touche `calibration` et la flash, cf. commentaire d'en-tete).
  if (xSemaphoreTake(g_mutex, kMutexTimeout) == pdTRUE) {
    g_state.calibration_requested = true;
    xSemaphoreGive(g_mutex);
  }
  request->redirect("/plongee");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println("OXY-LD2 - boot");

  g_mutex = xSemaphoreCreateMutex();

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

  // -- Calibration : chargement flash + decision d'auto-armement --
  prefs.begin("oxyld2", false);
  bool loaded = false;
  if (prefs.getBytesLength("cal") == kCalibrationStorageSize) {
    uint8_t buf[kCalibrationStorageSize];
    prefs.getBytes("cal", buf, kCalibrationStorageSize);
    CalibrationPoint pt;
    if (deserialize_calibration(buf, &pt) && calibration.calibrate(pt.v_air_mv, pt.timestamp_ms, pt.temp_c)) {
      g_last_calibration_ts = pt.timestamp_ms;
      loaded = true;
      Serial.printf("Calibration chargee depuis la flash : %.3f mV\n", pt.v_air_mv);
    }
  }
  if (loaded) {
    uint32_t age_s = calibration.age_ms(calibration_clock());
    if (age_s >= kMinRestSeconds) {
      auto_cal_armed = true;
      Serial.printf("Auto-calibration armee (%lu h depuis la derniere calibration)\n",
                    static_cast<unsigned long>(age_s / 3600));
    } else {
      Serial.printf(
          "Calibration recente (%lu h) - pas d'auto-calibration avant %lu h (repos)\n",
          static_cast<unsigned long>(age_s / 3600),
          static_cast<unsigned long>(kMinRestSeconds / 3600));
    }
  } else {
    Serial.println("Aucune calibration valide en flash - calibration manuelle requise (web /plongee)");
  }

  // Parite complete avec la sequence de demarrage AP d'OXY-LD (mode
  // explicite, IP/passerelle/masque explicites - la passerelle notamment
  // fait partie de ce que le telephone evalue pour juger si le reseau a
  // "Internet" - puis softAP avec canal/visibilite/max connexions).
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                     IPAddress(255, 255, 255, 0));
  bool ap_ok = WiFi.softAP("OXY-LD2", "plongee24", /*channel=*/6, /*hidden=*/false,
                            /*max_connection=*/4);
  Serial.printf("Point d'acces WiFi OXY-LD2 %s, IP : %s\n", ap_ok ? "OK" : "ECHEC",
                WiFi.softAPIP().toString().c_str());

  delay(200);  // laisse le temps a l'interface AP d'etre prete avant DNS (cf. OXY-LD)

  // Repond a TOUS les noms de domaine avec l'IP de la carte - necessaire
  // pour que la verification de connectivite des telephones aboutisse sur
  // cet AP sans Internet reel (cf. ARCHITECTURE.md pour l'historique du
  // diagnostic). Meme technique qu'OXY-LD.
  dns_server.start(53, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET,
            [](AsyncWebServerRequest* r) { r->redirect("/materiel"); });
  server.on("/materiel", HTTP_GET, handle_materiel);
  server.on("/plongee", HTTP_GET, handle_plongee);
  server.on("/tables", HTTP_GET, handle_tables);
  server.on("/ppo2", HTTP_POST, handle_set_ppo2);
  server.on("/calibrate", HTTP_POST, handle_calibrate);

  server.on("/generate_204", HTTP_ANY,
            [](AsyncWebServerRequest* r) { r->send(204, "text/plain", ""); });
  server.on("/hotspot-detect.html", HTTP_ANY, [](AsyncWebServerRequest* r) {
    r->send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
  });
  server.on("/library/test/success.html", HTTP_ANY,
            [](AsyncWebServerRequest* r) { r->send(200, "text/html", "Success"); });
  server.on("/connecttest.txt", HTTP_ANY,
            [](AsyncWebServerRequest* r) { r->send(200, "text/plain", "Microsoft Connect Test"); });
  server.on("/ncsi.txt", HTTP_ANY,
            [](AsyncWebServerRequest* r) { r->send(200, "text/plain", "Microsoft NCSI"); });
  server.onNotFound([](AsyncWebServerRequest* r) { r->redirect("/materiel"); });

  server.begin();
  Serial.println("Serveur web demarre");
}

void loop() {
  // Partagee entre tous les chemins de loop() (y compris les retours
  // anticipes ci-dessous) - sinon le diagnostic serie reste muet tant que
  // is_fo2_valid() echoue (ex: pas encore calibre), ce qui masque
  // exactement ce qu'il faut voir pour verifier la calibration.
  static uint32_t last_log_ms = 0;

  // Avant tout retour anticipe - le DNS doit continuer a repondre meme
  // quand la mesure ne tourne pas.
  dns_server.processNextRequest();

  int hour = 0, minute = 0;
  if (rtc_ok) {
    rtc.now(&hour, &minute);
    display.show_clock(hour, minute);
  }

  float ppo2_setpoint = 1.6f;
  bool calibration_requested = false;
  if (xSemaphoreTake(g_mutex, kMutexTimeout) == pdTRUE) {
    ppo2_setpoint = g_state.ppo2_setpoint;
    calibration_requested = g_state.calibration_requested;
    g_state.calibration_requested = false;
    xSemaphoreGive(g_mutex);
  }

  if (!ads_ok) {
    show_error(ppo2_setpoint);
    return;
  }

  uint32_t now = millis();
  float v_mv = ads.read_mv();
  glitch_filter.push(v_mv);
  float filtered_mv = glitch_filter.value();

  // stability sur la tension BRUTE, pas le %O2 - cf. commentaire d'en-tete.
  stability.push_sample(filtered_mv, now);

  // -- Calibration : auto (armee au boot) ou manuelle (bouton web) --
  // Toutes deux exigent la stabilite ; toutes deux beneficient de la plage
  // de tension plausible deja appliquee par calibration.calibrate(). Seule
  // l'auto-calibration est en plus soumise au temps de repos (deja
  // verifie une fois au boot pour l'armement).
  if ((auto_cal_armed || calibration_requested) && stability.is_stable()) {
    uint32_t ts = calibration_clock();
    if (calibration.calibrate(filtered_mv, ts, NAN)) {
      g_last_calibration_ts = ts;
      auto_cal_armed = false;
      save_calibration_to_flash();
      Serial.printf("Calibration effectuee : %.3f mV (%s)\n", filtered_mv,
                    calibration_requested ? "manuelle" : "auto");
    } else if (calibration_requested) {
      Serial.printf("Calibration manuelle refusee : tension %.3f mV hors plage plausible\n",
                    filtered_mv);
    }
  }

  float fo2_raw = voltage_to_o2_percent(filtered_mv, calibration.current_v_air_mv());
  if (!is_fo2_valid(fo2_raw)) {
    show_error(ppo2_setpoint);
    if (xSemaphoreTake(g_mutex, kMutexTimeout) == pdTRUE) {
      g_state.calibrated = calibration.is_calibrated();
      xSemaphoreGive(g_mutex);
    }
    if (now - last_log_ms >= 1000) {
      last_log_ms = now;
      Serial.printf(
          "v_mv=%.3f (NON CALIBRE) slope_mv=%.4f stable=%d auto_armed=%d cal_requested_seen=%d "
          "rtc_ok=%d clock=%02d:%02d\n",
          filtered_mv, stability.current_slope(), stability.is_stable() ? 1 : 0,
          auto_cal_armed ? 1 : 0, calibration_requested ? 1 : 0, rtc_ok ? 1 : 0, hour, minute);
    }
    return;
  }

  if (std::isnan(display_fo2_smoothed)) {
    display_fo2_smoothed = fo2_raw;
  } else {
    display_fo2_smoothed =
        kDisplayEmaAlpha * fo2_raw + (1.0f - kDisplayEmaAlpha) * display_fo2_smoothed;
  }
  int o2_display = o2_display_value(display_fo2_smoothed);
  int mod = mod_lookup(o2_display, ppo2_enum(ppo2_setpoint));

  SystemState state = stability.is_stable() ? SystemState::Stable : SystemState::Stabilizing;
  LedStatusOutput led = led_status_for(state);

  // Clignotement pendant la stabilisation (fenetre de 800ms sur millis()),
  // fixe une fois stable - cf. ARCHITECTURE.md "Retour UI attendu".
  bool o2_visible = stability.is_stable() || ((now / 800) % 2 == 0);

  display.show_measurement(o2_display, mod, ppo2_setpoint, led.color, stability.is_stable(),
                            o2_visible);

  if (xSemaphoreTake(g_mutex, kMutexTimeout) == pdTRUE) {
    g_state.o2_percent = o2_display;
    g_state.mod_meters = mod;
    g_state.stable = stability.is_stable();
    g_state.last_v_mv = filtered_mv;
    g_state.rtc_hour = hour;
    g_state.rtc_minute = minute;
    g_state.calibrated = calibration.is_calibrated();
    xSemaphoreGive(g_mutex);
  }

  // Diagnostic serie periodique (1x/s).
  if (now - last_log_ms >= 1000) {
    last_log_ms = now;
    Serial.printf(
        "v_mv=%.3f fo2=%.2f fo2_smooth=%.2f o2=%d mod=%d slope_mv=%.4f stable=%d cal_v=%.3f "
        "auto_armed=%d rtc_ok=%d clock=%02d:%02d ppo2=%.1f\n",
        filtered_mv, fo2_raw, display_fo2_smoothed, o2_display, mod, stability.current_slope(),
        stability.is_stable() ? 1 : 0, calibration.current_v_air_mv(), auto_cal_armed ? 1 : 0,
        rtc_ok ? 1 : 0, hour, minute, static_cast<double>(ppo2_setpoint));
  }
}
