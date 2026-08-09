#pragma once

#include <cstdint>

// Table statique MOD (Maximum Operating Depth), O2 21-100%, ppO2 1.4 ou 1.6.
// Generee hors-ligne par tools/generate_mod_table.py (formule + troncature
// vers le bas) - aucun calcul flottant en firmware. Voir ARCHITECTURE.md et
// docs/mod_table_review.html pour la methode et la validation contre la
// table papier du club (concordance exacte, 78/78 valeurs, ppO2=1.6).

enum class PPO2Setpoint { P14, P16 };

inline constexpr int kModTableMinO2 = 21;
inline constexpr int kModTableMaxO2 = 100;

extern const int8_t kModTable_PPO2_1_4[80];
extern const int8_t kModTable_PPO2_1_6[80];

// Retourne la MOD en metres pour un O2% entier et un setpoint ppO2.
// Retourne -1 si o2_percent est hors [kModTableMinO2, kModTableMaxO2] -
// erreur explicite a gerer par l'appelant, jamais de valeur silencieuse.
int mod_lookup(int o2_percent, PPO2Setpoint setpoint);
