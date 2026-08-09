#pragma once

#include <cstdint>

// Detection de stabilite par critere de pente lissee : |d(%O2)/dt| < epsilon,
// maintenu en continu pendant au moins sustained_ms, et au moins
// min_settle_ms ecoules depuis le dernier reset() (calibration / changement
// de gaz). Voir ARCHITECTURE.md section "Fiabilite" pour la justification
// et les valeurs de depart.
//
// Module pur : aucune dependance materielle, pas de notion d'affichage. Le
// clignotement de la valeur affichee tant que is_stable() est faux est une
// responsabilite du module display, qui consomme is_stable() en entree.

struct StabilityConfig {
  float slope_threshold_percent_per_s;  // epsilon (%O2/s) - en dessous, la pente est consideree nulle
  float ema_alpha;                      // lissage exponentiel de la pente, 0 < alpha <= 1 (1 = pas de lissage)
  uint32_t min_settle_ms;               // plancher dur depuis reset() avant de pouvoir etre stable
  uint32_t sustained_ms;                // duree pendant laquelle la pente doit rester sous le seuil en continu
  uint32_t max_sample_gap_ms;           // au-dela, deux echantillons ne sont plus consideres continus
};

class StabilityTracker {
 public:
  explicit StabilityTracker(StabilityConfig config);

  // A appeler au debut d'une nouvelle analyse (calibration, changement de gaz).
  void reset(uint32_t now_ms);

  // A appeler a chaque nouvelle lecture O2 valide (deja filtree par
  // is_fo2_valid() en amont). Retourne false si l'echantillon est ignore :
  // premier point de la sequence, timestamp duplique (dt=0), ou ecart
  // superieur a max_sample_gap_ms (discontinuite - la baseline de pente est
  // silencieusement redemarree sur ce point).
  bool push_sample(float fO2_percent, uint32_t now_ms);

  // Reflete l'etat au moment du dernier push_sample() - pas une horloge
  // vivante interrogeable a tout instant. Vrai seulement si :
  // (1) min_settle_ms ecoulees depuis reset(), ET
  // (2) pente lissee restee sous le seuil en continu depuis >= sustained_ms.
  // Non verrouille : redevient faux si la pente repasse au-dessus du seuil.
  bool is_stable() const;

  // Pente lissee courante (%O2/s), signee. Utile pour debug / barre de
  // progression UI. Vaut 0.0 avant qu'une premiere pente ait pu etre
  // calculee (moins de deux echantillons valides).
  float current_slope() const;

 private:
  StabilityConfig config_;
  uint32_t reset_time_ms_ = 0;

  bool has_prev_sample_ = false;
  float prev_value_ = 0.0f;
  uint32_t prev_time_ms_ = 0;

  bool has_slope_ = false;
  float smoothed_slope_ = 0.0f;

  bool below_threshold_ = false;
  uint32_t below_since_ms_ = 0;
};
