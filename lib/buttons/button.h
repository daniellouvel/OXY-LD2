#pragma once

#include <cstdint>

// Detection appui court/long sur une entree numerique (TTP223, HIGH quand
// touche - cf. HARDWARE.md). Machine a etats temporelle pure, meme schema
// que StabilityTracker : l'appelant pousse l'etat brut a chaque lecture, le
// module decide court/long. Pas de dependance materielle - la lecture
// digitalRead() reelle est a la charge de l'appelant (pas encore ecrit,
// TTP223 pas cables actuellement).
//
// L'appui long est detecte au moment ou le seuil est franchi, PENDANT que
// le bouton est maintenu (pas a l'relachement) - feedback immediat, evite
// aussi de declencher un ShortPress en plus au relachement d'un appui deja
// reconnu comme long.

enum class ButtonEvent { None, ShortPress, LongPress };

struct ButtonConfig {
  uint32_t long_press_ms;  // duree mini maintenue pour etre un appui long
};

class ButtonTracker {
 public:
  explicit ButtonTracker(ButtonConfig config);

  // A appeler a chaque lecture (raw_pressed = true si le bouton est
  // touche). Retourne l'evenement detecte a CET appel - LongPress une
  // seule fois au moment du franchissement du seuil, ShortPress au
  // relachement si le seuil n'a jamais ete atteint, None sinon.
  ButtonEvent update(bool raw_pressed, uint32_t now_ms);

 private:
  ButtonConfig config_;
  bool was_pressed_ = false;
  bool long_fired_ = false;
  uint32_t press_start_ms_ = 0;
};
