#pragma once

// Valeur O2 canonique utilisee partout en aval : affichage, etiquette, et
// cle de recherche dans mod_table. Toujours entiere, toujours arrondie vers
// le haut - cote conservateur (plus d'O2 mesure => MOD plus courte). Non
// configurable : contrairement a OXY-LD, il n'y a pas de mode "aucun
// arrondi" ou "arrondi vers le bas" pour cette valeur. Voir ARCHITECTURE.md.
int o2_display_value(float fO2_raw_percent);

// Validite physique d'une lecture brute (0.0-100.0). Ne verifie pas
// l'appartenance a la plage de la table MOD (21-100) - c'est le role de
// mod_lookup(), qui retourne -1 hors plage.
bool is_fo2_valid(float fO2_raw_percent);
