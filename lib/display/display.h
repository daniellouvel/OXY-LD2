#pragma once

#include <Adafruit_ST7789.h>
#include <SPI.h>

#include "led_status.h"

// Rendu ecran TFT ST7789 (SPI, 240x320). Reglages repris tels quels de
// HARDWARE.md, confirmes par l'utilisateur comme identiques a la derniere
// version fonctionnelle d'OXY-LD : tft.init(240,320), invertDisplay(true),
// setRotation(1) + MADCTL=MV force (corrige un miroir horizontal connu sur
// ce clone ST7789, cf. HARDWARE.md).
//
// Ne connait pas la notion de "clignotant" - juste afficher/masquer la
// valeur O2 sur demande. Le rythme de clignotement (piloter show_measurement
// avec o2_value_visible qui alterne) est decide par l'appelant (main.cpp),
// pas par ce module - cf. ARCHITECTURE.md "Retour UI attendu".
//
// ECRIT CETTE NUIT, NON VERIFIE VISUELLEMENT AU MOMENT DE L'ECRITURE.
// Compile avec succes contre les vraies bibliotheques (Adafruit ST7735 and
// ST7789 Library @ 1.11.0, Adafruit GFX Library @ 1.12.6, verifiees dans
// .pio/libdeps/esp32s3/) mais jamais flashe ni observe en fonctionnement
// sur l'ecran reel. A verifier au reveil : l'ecran s'allume, le texte est
// lisible (pas de miroir, pas de couleurs inversees malgre invertDisplay),
// positions/tailles de police a ajuster a l'oeil - jamais calibrees.

class Display {
 public:
  Display();

  void begin();

  // Efface l'ecran, affiche un texte de demarrage.
  void show_splash();

  // Affiche O2%, MOD, ppO2 de reference. color pilote la teinte du texte
  // O2 (vert=stable, orange=en cours, rouge=erreur - cf. led_status.h).
  // o2_value_visible=false efface uniquement la zone de la valeur O2 (pour
  // le clignotement pendant la stabilisation) sans toucher au reste.
  void show_measurement(int o2_percent, int mod_meters, float ppo2_setpoint,
                        RgbColor color, bool o2_value_visible);

 private:
  static uint16_t to_rgb565(RgbColor c);

  SPIClass spi_;
  Adafruit_ST7789 tft_;
};
