#pragma once

// Conversion tension cellule -> %O2, et compensation thermique. Logique
// pure, aucune dependance materielle - la lecture ADS1115 elle-meme vit
// dans un module separe (pas encore ecrit, cf. ARCHITECTURE.md) pour ne
// pas casser la compilation en environnement `native`.

// fO2 = (v_measured_mv / v_calibration_mv) * o2_reference_percent.
// v_calibration_mv est la tension mesuree a l'air lors de la derniere
// calibration (fournie par le module calibration, pas possedee ici).
// Retourne NAN si v_calibration_mv <= 0 (pas calibre).
float voltage_to_o2_percent(float v_measured_mv, float v_calibration_mv,
                             float o2_reference_percent = 20.9f);

// Compensation thermique lineaire : fO2 + coefficient * (temp_now - temp_cal).
// coefficient_percent_per_c est signe et fourni par l'appelant - PAS de
// valeur par defaut assumee ici. La cellule (R-17 Med / R-22 / OOM-202)
// n'a pas de coefficient verifie pour ce projet ; a sourcer depuis une
// fiche technique ou une mesure empirique avant d'etre utilise en pratique.
float apply_thermal_compensation(float fO2_percent, float temp_now_c,
                                  float temp_calibration_c,
                                  float coefficient_percent_per_c);
