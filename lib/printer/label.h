#pragma once

// Genere la commande TSPL complete pour une etiquette d'analyse O2 (cas
// simple, sans badge RFID - rfid_badge n'existe pas encore). Logique pure,
// pas d'ecriture UART ici (module materiel separe, a ecrire au cablage de
// l'imprimante TSC TH240 - non cablee actuellement).
//
// Positions/tailles de police TSPL : point de depart raisonnable repris du
// format documente dans OXY-LD/README.md, jamais verifie visuellement (pas
// d'imprimante branchee) - a ajuster une fois la TSC TH240 cablee.

struct LabelData {
  int o2_percent;        // deja arrondi via mod_calc::o2_display_value()
  int mod_meters;         // -1 si hors table (mod_calc::mod_lookup())
  float ppo2_setpoint;     // 1.4 ou 1.6
  const char* date_str;    // "JJ/MM/AAAA", deja formate (pas de RTC dans ce module)
  const char* time_str;    // "HH:MM"
};

inline constexpr int kLabelMaxLength = 512;

// Ecrit la commande TSPL dans buffer (buffer_size doit etre >= la longueur
// necessaire). Retourne la longueur ecrite (hors terminateur nul), ou 0 si
// buffer_size est insuffisant - dans ce cas le contenu de buffer n'est pas
// garanti utilisable, l'appelant ne doit rien envoyer a l'imprimante.
int build_label_tspl(const LabelData& data, char* buffer, int buffer_size);
