#pragma once

#include <cstdint>
#include <string>

#include "mod_table.h"

// Generation pure du contenu HTML/JSON du serveur web de configuration
// (accessible a l'encadrant via le point d'acces WiFi de la station).
// Aucune dependance materielle ici (pas d'AsyncWebServer, pas de WiFi) -
// meme principe que lib/printer/ : ces fonctions prennent des donnees deja
// lues/calculees et retournent une chaine, testable en `native`. Le
// cablage reel (WiFi.softAP, routes AsyncWebServer, mutex de partage
// d'etat avec la boucle de mesure) vit dans src/main.cpp, comme chez
// OXY-LD, qui a les memes contraintes d'acces a l'etat partage.

struct HardwareStatus {
  bool ads_ok;
  bool rtc_ok;
  int rtc_hour;    // ignore si rtc_ok est faux
  int rtc_minute;  // ignore si rtc_ok est faux
  float last_v_mv;  // derniere tension cellule lue, mV
};

struct DiveStatus {
  int o2_percent;      // -1 si pas encore de mesure valide
  int mod_meters;       // -1 si hors table ou pas de mesure valide
  float ppo2_setpoint;   // 1.4 ou 1.6
  bool stable;
  bool calibrated;      // faux si aucune calibration valide (jamais faite / rejetee)
  uint32_t rest_hours = 24;  // temps de repos avant auto-calibration, configurable via /rest
};

// Habillage commun (titre + navigation entre les pages) autour d'un corps
// HTML deja construit.
std::string page_shell(const std::string& title, const std::string& body_html);

// Page "materiel" : statut des modules detectes (ADS1115, RTC + heure).
// Diagnostic seul pour l'instant, pas de reglage modifiable ici (peu de
// materiel configurable tant que boutons/RFID/imprimante ne sont pas cables).
std::string build_hardware_page(const HardwareStatus& status);

// Page "plongee" : ppO2 de reference (1.4/1.6, formulaire de selection) +
// mesure courante (O2%, MOD, stabilite) pour donner le contexte au choix.
std::string build_dive_page(const DiveStatus& status);

// Page "tables" : table MOD complete (O2 21-100%, ppO2 1.4 et 1.6),
// reutilise directement les donnees validees de lib/mod_calc/mod_table.h -
// pas de nouvelle source de verite, la meme table que celle qui pilote le
// calcul reel.
std::string build_tables_page();

// JSON de statut consomme par la page "plongee" (rafraichissement live
// sans recharger la page) : o2, mod, ppo2, stable, ads_ok, rtc_ok.
std::string build_status_json(const HardwareStatus& hw, const DiveStatus& dive);
