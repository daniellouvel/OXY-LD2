#pragma once

#include <Adafruit_ST7789.h>
#include <SPI.h>

#include "led_status.h"

// Rendu ecran TFT ST7789 (SPI, 320x240 apres rotation). Police et gabarit
// repris de la derniere version fonctionnelle d'OXY-LD (OXY-LD/src/main.cpp,
// displayRead()) : polices vectorielles FreeSansBold plutot que la police
// bitmap standard, technique anti-scintillement par cache de contenu.
//
// Scintillement constate sur la carte reelle la nuit du 2026-08-09 (meme
// symptome documente par OXY-LD, qui a des environnements de diagnostic
// dedies dans son platformio.ini) : un fillRect(noir) suivi d'un redraw a
// CHAQUE appel, meme quand rien n'a change, produit un flash noir visible
// sur ce panel. Chaque zone compare son contenu au dernier contenu dessine
// et saute le fillRect+redraw si identique - meme technique qu'OXY-LD.

class Display {
 public:
  Display();

  void begin();
  void show_splash();

  // Force un redraw complet au prochain show_measurement(), malgre le
  // cache (ex: apres un changement de mode qui a modifie l'ecran ailleurs).
  void force_redraw();

  // color pilote la teinte du chiffre O2 (vert=stable, orange=en cours,
  // rouge=erreur - cf. led_status.h). stable pilote le badge "OK"/"..." en
  // haut a droite (independant du clignotement - le badge ne clignote pas,
  // comme sur OXY-LD). o2_value_visible=false efface la zone du chiffre O2
  // sans toucher au reste - pilote le clignotement pendant la
  // stabilisation (cf. ARCHITECTURE.md "Retour UI attendu"), decide par
  // l'appelant (main.cpp), pas par ce module. calibrated pilote le badge
  // "CALIBRE"/"NON CAL" en bas a droite (retour utilisateur : rien
  // n'indiquait l'etat de calibration sur l'ecran physique).
  void show_measurement(int o2_percent, int mod_meters, float ppo2_setpoint,
                        RgbColor color, bool stable, bool o2_value_visible,
                        bool calibrated);

  // Horloge en haut a gauche (zone 0,0,90,26 - reprise d'OXY-LD). Ne
  // redessine que si l'heure affichee a change (meme cache anti-scintillement).
  void show_clock(int hour, int minute);

  // Affiche un message temporaire (remplace la bande MOD/ppO2 pendant
  // kCalibrationFeedbackMs) suite a une tentative de calibration MANUELLE
  // (bouton web /plongee). Retour utilisateur : cliquer "Calibrer" ne
  // produisait aucun retour visible sur l'ecran physique, impossible de
  // savoir si la demande avait ete recue (elle est silencieusement
  // ignoree si la cellule n'est pas stable au moment de l'appui). A
  // appeler depuis main.cpp des que loop() traite une demande manuelle,
  // succes ou echec.
  void notify_calibration_attempt(bool success);

 private:
  // scale=1 pour la plupart des textes ; le grand chiffre O2 utilise
  // scale=2 sur la police deja la plus grande (FreeSansBold24pt7b) pour
  // remplir l'espace disponible - meme technique qu'OXY-LD (voir son
  // commentaire "le chiffre est affiche a l'echelle 2 pour la remplir").
  void center_text(const char* s, int16_t y, const GFXfont* font, uint16_t color,
                   uint8_t scale = 1);

  SPIClass spi_;
  Adafruit_ST7789 tft_;
  bool force_redraw_ = true;

  // Cache du dernier contenu dessine par zone - cf. commentaire de classe.
  char prev_o2_text_[8] = "";
  uint16_t prev_o2_color_ = 0xFFFF;
  bool prev_badge_shown_ = false;
  bool prev_badge_stable_ = false;
  char prev_mod_text_[24] = "";
  char prev_ppo2_text_[16] = "";
  char prev_clock_text_[8] = "";
  bool prev_cal_badge_shown_ = false;
  bool prev_cal_badge_calibrated_ = false;

  bool calibration_feedback_active_ = false;
  bool calibration_feedback_success_ = false;
  uint32_t calibration_feedback_until_ms_ = 0;
  bool prev_feedback_shown_ = false;
};
