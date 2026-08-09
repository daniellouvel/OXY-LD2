#include <Arduino.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

#include <cmath>

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
#include "web_content.h"

// Boucle de mesure de bout en bout + serveur web de configuration.
// Voir ARCHITECTURE.md pour le detail des limites et de ce qui est verifie.
//
// Cable a ce jour : ESP32-S3 + ADS1115 + cellule O2 + ecran TFT + RTC
// PCF8563. DS18B20 et boutons ne sont PAS cables :
//  - millis() sert toujours d'horloge pour stability/calibration (uint32_t
//    now_ms brut, distinct de l'heure murale du RTC qui sert uniquement
//    a l'affichage)
//  - pas de compensation thermique (apply_thermal_compensation() non
//    appelee - has_temperature() resterait de toute facon faux)
//  - ppO2 reglable depuis la page web /plongee (remplace le bouton qu'on
//    n'a pas) - plus une constexpr, cf. g_state ci-dessous
//
// Calibration : pas de bouton pour declencher une vraie calibration.
// Auto-calibration NAIVE sur la toute premiere lecture valide au
// demarrage - PAS la vraie fonctionnalite d'auto-calibration d'OXY-LD (qui
// verifie la stabilite et se declenche apres un arret prolonge). A
// remplacer par un vrai declenchement (bouton) des que cable.
//
// Serveur web : point d'acces WiFi (pas d'authentification HTTP - le mot
// de passe WiFi protege deja l'acces, decision explicite pour rester
// simple, cf. ARCHITECTURE.md). Concurrence : AsyncWebServer execute ses
// handlers sur sa propre tache FreeRTOS, potentiellement concurrente avec
// loop(). Seul loop() touche le materiel I2C (ADS1115, RTC) - les
// handlers web ne lisent jamais le materiel directement, uniquement le
// snapshot g_state protege par g_mutex, pour eviter tout acces concurrent
// au bus I2C entre deux taches (OXY-LD, lui, appelle rtc.now() directement
// depuis un handler web ; ecart deliberement different ici, plus prudent).

namespace {

Ads1115Reader ads;
Display display;
RtcClock rtc;
GlitchFilter glitch_filter;
AsyncWebServer server(80);
DNSServer dns_server;

bool rtc_ok = false;
bool ads_ok = false;

// Plage de tension plausible non sourcee (pas de fiche technique
// verifiee pour la cellule montee) - point de depart large, a resserrer.
CalibrationTracker calibration(5.0f, 15.0f);

// eps/alpha/durees revues sur retour d'experience (analyseur du
// commerce), toujours pas calibrees sur cette cellule - cf. ARCHITECTURE.md.
StabilityConfig stability_config{0.02f, 0.3f, 5000, 3000, 10000};
StabilityTracker stability(stability_config);

// Etat partage avec les handlers web, protege par g_mutex - cf. commentaire
// d'en-tete. ppo2_setpoint est le seul champ modifiable depuis le web
// (POST /ppo2) ; les autres sont mis a jour par loop() a chaque iteration.
struct SharedState {
  int o2_percent = -1;
  int mod_meters = -1;
  bool stable = false;
  float last_v_mv = 0.0f;
  int rtc_hour = 0;
  int rtc_minute = 0;
  float ppo2_setpoint = 1.6f;
};
SharedState g_state;
SemaphoreHandle_t g_mutex = nullptr;

constexpr TickType_t kMutexTimeout = pdMS_TO_TICKS(50);

PPO2Setpoint ppo2_enum(float ppo2) { return (ppo2 >= 1.5f) ? PPO2Setpoint::P16 : PPO2Setpoint::P14; }

// Lissage COSMETIQUE reserve a l'affichage, pour attenuer l'oscillation
// visible entre deux valeurs entieres adjacentes pres d'une frontiere
// (ex. 21%/22%) quand le bruit du signal brut chevauche cette frontiere.
// Ne remplace PAS fo2_raw pour stability.push_sample() : le calcul de
// pente doit rester sur le signal le moins transforme possible (cf.
// ARCHITECTURE.md - deja le principe retenu pour GlitchFilter, qui evite
// une moyenne glissante pour la meme raison). display_fo2_smoothed est un
// second chemin, parallele, uniquement pour ce qui est montre a l'ecran.
float display_fo2_smoothed = NAN;
constexpr float kDisplayEmaAlpha = 0.15f;

void show_error(float ppo2) {
  RgbColor c = led_status_for(SystemState::Error).color;
  display.show_measurement(0, -1, ppo2, c, /*stable=*/false, /*o2_value_visible=*/true);
}

// -- Handlers web : lisent/ecrivent uniquement g_state sous mutex, jamais
// le materiel directement (cf. commentaire d'en-tete). --

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

  // Parite complete avec la sequence de demarrage AP d'OXY-LD (mode
  // explicite, IP/passerelle/masque explicites - la passerelle notamment
  // fait partie de ce que le telephone evalue pour juger si le reseau a
  // "Internet" - puis softAP avec canal/visibilite/max connexions) : les
  // deux premieres tentatives (routes HTTP seules, puis DNS seul sans
  // cette config) n'ont pas suffi pour le telephone de l'utilisateur.
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                     IPAddress(255, 255, 255, 0));
  bool ap_ok = WiFi.softAP("OXY-LD2", "plongee24", /*channel=*/6, /*hidden=*/false,
                            /*max_connection=*/4);
  Serial.printf("Point d'acces WiFi OXY-LD2 %s, IP : %s\n", ap_ok ? "OK" : "ECHEC",
                WiFi.softAPIP().toString().c_str());

  delay(200);  // laisse le temps a l'interface AP d'etre prete avant DNS (cf. OXY-LD)

  // Repond a TOUS les noms de domaine avec l'IP de la carte - necessaire
  // pour que la verification de connectivite des telephones (qui resout
  // un nom de domaine avant de faire la requete HTTP) aboutisse sur cet AP
  // sans Internet reel. Sans ca, les routes /generate_204 etc. ci-dessous
  // ne sont jamais atteintes : la resolution DNS echoue avant meme
  // d'essayer de contacter le serveur web. Meme technique qu'OXY-LD.
  dns_server.start(53, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET,
            [](AsyncWebServerRequest* r) { r->redirect("/materiel"); });
  server.on("/materiel", HTTP_GET, handle_materiel);
  server.on("/plongee", HTTP_GET, handle_plongee);
  server.on("/tables", HTTP_GET, handle_tables);
  server.on("/ppo2", HTTP_POST, handle_set_ppo2);

  // Ce point d'acces n'a pas d'Internet reel (AP local pur). Sans ces
  // routes, les telephones (Android/iOS/Windows) interceptent le trafic
  // web pour verifier la connectivite et bloquent la navigation normale -
  // symptome constate (retour utilisateur : le telephone se connecte au
  // WiFi mais n'arrive pas a ouvrir la page, alors qu'un PC y arrive sans
  // souci). Reprise des routes de contournement d'OXY-LD, qui a le meme
  // probleme sur le meme type de point d'acces.
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
  // Avant tout retour anticipe (ads_ok faux, mesure invalide) - le DNS
  // doit continuer a repondre meme quand la mesure ne tourne pas.
  dns_server.processNextRequest();

  int hour = 0, minute = 0;
  if (rtc_ok) {
    rtc.now(&hour, &minute);
    display.show_clock(hour, minute);
  }

  float ppo2_setpoint = 1.6f;
  if (xSemaphoreTake(g_mutex, kMutexTimeout) == pdTRUE) {
    ppo2_setpoint = g_state.ppo2_setpoint;
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

  // Auto-calibration naive au tout premier passage - cf. avertissement en
  // tete de fichier, pas la vraie fonctionnalite d'auto-calibration.
  if (!calibration.is_calibrated()) {
    calibration.calibrate(filtered_mv, now);
  }

  float fo2_raw = voltage_to_o2_percent(filtered_mv, calibration.current_v_air_mv());
  if (!is_fo2_valid(fo2_raw)) {
    show_error(ppo2_setpoint);
    return;
  }

  stability.push_sample(fo2_raw, now);

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

  // Clignotement pendant la stabilisation (fenetre de 800ms sur millis(),
  // ralenti depuis 500ms sur retour utilisateur), fixe une fois stable -
  // cf. ARCHITECTURE.md "Retour UI attendu".
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
    xSemaphoreGive(g_mutex);
  }

  // Diagnostic serie periodique (1x/s) - ajoute pour verifier reellement le
  // pipeline cette nuit, pas seulement l'absence de crash. A retirer ou
  // reduire une fois le comportement confirme sur la vraie carte.
  static uint32_t last_log_ms = 0;
  if (now - last_log_ms >= 1000) {
    last_log_ms = now;
    Serial.printf(
        "v_mv=%.3f fo2=%.2f fo2_smooth=%.2f o2=%d mod=%d slope=%.4f stable=%d cal_v=%.3f "
        "rtc_ok=%d clock=%02d:%02d ppo2=%.1f\n",
        filtered_mv, fo2_raw, display_fo2_smoothed, o2_display, mod, stability.current_slope(),
        stability.is_stable() ? 1 : 0, calibration.current_v_air_mv(), rtc_ok ? 1 : 0, hour,
        minute, static_cast<double>(ppo2_setpoint));
  }
}
