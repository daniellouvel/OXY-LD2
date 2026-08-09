#include "button.h"

ButtonTracker::ButtonTracker(ButtonConfig config) : config_(config) {}

ButtonEvent ButtonTracker::update(bool raw_pressed, uint32_t now_ms) {
  if (raw_pressed && !was_pressed_) {
    was_pressed_ = true;
    long_fired_ = false;
    press_start_ms_ = now_ms;
    return ButtonEvent::None;
  }

  if (raw_pressed && was_pressed_) {
    // Soustraction non signee - reste correcte au rollover millis().
    uint32_t held_ms = now_ms - press_start_ms_;
    if (!long_fired_ && held_ms >= config_.long_press_ms) {
      long_fired_ = true;
      return ButtonEvent::LongPress;
    }
    return ButtonEvent::None;
  }

  if (!raw_pressed && was_pressed_) {
    was_pressed_ = false;
    if (!long_fired_) {
      return ButtonEvent::ShortPress;
    }
    return ButtonEvent::None;  // appui long deja signale, pas de doublon au relachement
  }

  return ButtonEvent::None;  // pas touche, n'etait pas touche
}
