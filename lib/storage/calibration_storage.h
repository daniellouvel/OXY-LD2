#pragma once

#include <cstdint>

#include "calibration.h"

// Serialisation versionnee d'un CalibrationPoint pour survivre a une
// coupure d'alimentation. L'ecriture/lecture flash reelle (Preferences
// NVS) est hors scope ici - module materiel separe, a ecrire au cablage
// (meme raison que le driver ADS1115 : pas testable sans le vrai
// materiel). Seule la mise en forme des octets est pure et testee.
//
// Limite assumee : seul le DERNIER point de calibration est persiste, pas
// tout l'historique de derive (10 points) que garde CalibrationTracker en
// memoire. La detection de derive (is_declining) ne voit donc que les
// calibrations faites depuis le dernier redemarrage. Persister
// l'historique complet demanderait d'exposer l'etat interne de
// CalibrationTracker - pas fait ici, note comme travail futur.

inline constexpr uint8_t kCalibrationStorageVersion = 1;
// version(1) + v_air_mv(4) + temp_c(4) + timestamp_ms(4) + checksum(4)
inline constexpr int kCalibrationStorageSize = 17;

// Ecrit point dans buffer (kCalibrationStorageSize octets minimum).
void serialize_calibration(const CalibrationPoint& point, uint8_t* buffer);

// Relit buffer vers *out_point. Retourne false (et laisse *out_point
// inchange) si la version ne correspond pas ou si le checksum est
// invalide - donnees corrompues, ou jamais ecrites (flash vierge = 0xFF,
// rejete des la lecture de version : 0xFF != kCalibrationStorageVersion).
bool deserialize_calibration(const uint8_t* buffer, CalibrationPoint* out_point);
