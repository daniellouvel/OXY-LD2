#include "led_status.h"

LedStatusOutput led_status_for(SystemState state) {
  switch (state) {
    case SystemState::Splash:
      return LedStatusOutput{RgbColor{255, 255, 255}, false};
    case SystemState::Stabilizing:
      return LedStatusOutput{RgbColor{255, 165, 0}, false};
    case SystemState::Stable:
      return LedStatusOutput{RgbColor{0, 255, 0}, false};
    case SystemState::Error:
      return LedStatusOutput{RgbColor{255, 0, 0}, true};
  }
  // Etat inconnu (ne devrait pas arriver avec un enum class exhaustif ci-dessus) :
  // rouge clignotant, cote conservateur - signaler une erreur plutot que
  // rester silencieux.
  return LedStatusOutput{RgbColor{255, 0, 0}, true};
}
