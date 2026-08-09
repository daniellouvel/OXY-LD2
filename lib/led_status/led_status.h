#pragma once

#include <cstdint>

// Table pure etat -> couleur/clignotement de la LED RGB WS2812B. Ne touche
// pas au materiel (pas d'appel NeoPixel ici, ni de gestion de luminosite -
// ce sont des details de rendu materiel, hors scope de cette table de
// decision). Couleurs reprises du schema documente dans OXY-LD/WIRING.md.
//
// Scope limite a ce que la premiere boucle materielle peut determiner
// reellement ce soir : pas de badge RFID, impression, historique, reglage
// horaire - ces modules n'existent pas encore, pas d'etats speculatifs
// pour des chemins inatteignables.

enum class SystemState {
  Splash,       // demarrage
  Stabilizing,  // stability.is_stable() faux, mesure en cours
  Stable,       // stability.is_stable() vrai, calibre
  Error,        // pas calibre, ou autre erreur
};

struct RgbColor {
  uint8_t r, g, b;
};

struct LedStatusOutput {
  RgbColor color;
  bool blinking;
};

LedStatusOutput led_status_for(SystemState state);
