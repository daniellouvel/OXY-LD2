#pragma once

#include <Adafruit_ADS1X15.h>

// Wrapper materiel autour de l'ADS1115 (mesure differentielle A0-A1, gain
// x16 - cf. HARDWARE.md). Ne peut pas vivre dans lib/o2_sensor/ :
// PlatformIO compile tout le contenu d'un dossier lib/ des qu'un fichier
// est inclus, ce qui casserait pio test -e native en tirant
// Adafruit_ADS1X15/Arduino.h dans l'environnement natif.
//
// ECRIT CETTE NUIT, NON VERIFIE SUR MATERIEL REEL AU MOMENT DE L'ECRITURE.
// Compile avec succes contre les vraies bibliotheques (Adafruit ADS1X15 @
// 2.6.2, verifiees dans .pio/libdeps/esp32s3/) mais jamais flashe ni
// observe en fonctionnement sur la carte. A verifier au reveil : begin()
// reussit (reponse I2C a 0x48), read_mv() donne une valeur plausible
// (~9-13 mV a l'air), et varie si on couvre/souffle sur la cellule.

class Ads1115Reader {
 public:
  // Retourne false si ads.begin() echoue (pas de reponse I2C a l'adresse
  // 0x48 - ADDR relie a GND, cf. HARDWARE.md).
  bool begin();

  // Lecture differentielle A0-A1 (readADC_Differential_0_1), gain x16 deja
  // configure par begin(). Retourne la tension en mV - peut etre negative
  // si la cellule est cablee en polarite inversee (cf. HARDWARE.md : (+)
  // sur A0, (-) sur A1).
  float read_mv();

 private:
  Adafruit_ADS1115 ads_;
};
