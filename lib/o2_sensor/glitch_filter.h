#pragma once

// Filtre anti-glitch a 3 echantillons : rejette un pic isole (glitch ADC)
// sans lisser la vraie dynamique du signal. Volontairement pas une moyenne
// glissante - ca retarderait un vrai changement de mesure et fausserait le
// calcul de pente que fait deja stability::StabilityTracker sur la sortie
// de ce filtre. Voir ARCHITECTURE.md.
class GlitchFilter {
 public:
  // Ajoute un echantillon (buffer circulaire de 3).
  void push(float value);

  // Mediane des 3 derniers echantillons. Avant d'en avoir 3, renvoie le
  // dernier echantillon pousse (0.0f si aucun).
  float value() const;

 private:
  float buf_[3] = {0.0f, 0.0f, 0.0f};
  int count_ = 0;
  int next_index_ = 0;
};
