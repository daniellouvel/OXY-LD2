#pragma once

#include <RTClib.h>

// Wrapper materiel autour du RTC PCF8563 (I2C 0x51, cf. HARDWARE.md). Ne
// peut pas vivre dans un module pur - meme raison que ads1115_reader :
// PlatformIO compile tout le contenu d'un dossier lib/ des qu'un fichier
// est inclus, ce qui casserait pio test -e native en tirant RTClib/Arduino.h.
//
// RTC_PCF8563 confirme present dans RTClib @ 2.1.4 (RTC_PCF8563.cpp
// compile avec succes des le premier pio run -e esp32s3 de cette session).

class RtcClock {
 public:
  // Retourne false si rtc.begin() echoue (pas de reponse I2C a 0x51).
  bool begin();

  // Vrai si le RTC a perdu l'alimentation (pile CR2032 absente/HS ou
  // premiere mise sous tension) - l'heure lue n'est alors pas fiable tant
  // qu'elle n'a pas ete reglee.
  bool lost_power();

  // now()/lostPower() de RTClib ne sont pas const-qualifiees (lecture I2C) -
  // ces methodes ne le sont donc pas non plus, par coherence.
  void now(int* hour, int* minute);

  // Secondes depuis l'epoque Unix - seule source de temps qui survit a un
  // redemarrage (contrairement a millis()). Utilise pour l'age de
  // calibration (cf. main.cpp), PAS pour stability qui reste sur millis()
  // (pas besoin de survivre au reboot pour du court terme).
  uint32_t unix_time();

 private:
  RTC_PCF8563 rtc_;
};
