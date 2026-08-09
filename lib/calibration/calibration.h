#pragma once

#include <cmath>
#include <cstdint>

// Etat de calibration (tension a l'air, temperature optionnelle) et
// detection de vieillissement de cellule. Logique pure, aucune ecriture
// flash ici - la persistance reelle est la responsabilite du module
// storage (pas encore ecrit), qui serialise/restaure un CalibrationPoint.

struct CalibrationPoint {
  float v_air_mv;
  float temp_c;        // NAN si pas de sonde DS18B20 / temperature non fournie
  uint32_t timestamp_ms;
};

class CalibrationTracker {
 public:
  // v_air_min_mv/max_mv : plage plausible pour une cellule fonctionnelle -
  // calibrate() rejette silencieusement (retourne false) hors de cette plage.
  CalibrationTracker(float v_air_min_mv, float v_air_max_mv);

  // temp_c optionnel (NAN par defaut) - DS18B20 absent du montage courant.
  // Retourne false et laisse l'etat inchange si v_air_mv est hors plage.
  bool calibrate(float v_air_mv, uint32_t now_ms, float temp_c = NAN);

  bool is_calibrated() const;

  // NAN si is_calibrated() est faux.
  float current_v_air_mv() const;

  // NAN si is_calibrated() est faux OU si la calibration courante n'a pas
  // de temperature associee. has_temperature() distingue les deux cas.
  float current_temp_c() const;
  bool has_temperature() const;

  // UINT32_MAX si non calibre (conventionnellement "infiniment perime").
  uint32_t age_ms(uint32_t now_ms) const;

  // Vrai si non calibre, ou si age_ms(now_ms) >= max_age_ms.
  bool is_stale(uint32_t now_ms, uint32_t max_age_ms) const;

  // Vrai si is_calibrated() et current_v_air_mv() <= replacement_threshold_mv.
  bool is_cell_aging(float replacement_threshold_mv) const;

  // Compare la calibration la plus ancienne encore presente dans
  // l'historique (pas forcement la toute premiere jamais faite, si le
  // buffer a deja tourne) a la plus recente. Vrai si la baisse de tension
  // atteint drop_threshold_mv. Faux si moins de 2 calibrations en memoire.
  bool is_declining(float drop_threshold_mv) const;

 private:
  static constexpr int kHistoryCapacity = 10;

  int last_index() const;
  int oldest_index() const;

  CalibrationPoint history_[kHistoryCapacity] = {};
  int count_ = 0;
  int next_index_ = 0;
  float v_air_min_mv_;
  float v_air_max_mv_;
};
