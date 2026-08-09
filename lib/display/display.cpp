#include "display.h"

#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

#include <cstdio>
#include <cstring>

namespace {
constexpr int8_t kPinCs = 10;
constexpr int8_t kPinSck = 12;
constexpr int8_t kPinMosi = 11;
constexpr int8_t kPinDc = 13;
constexpr int8_t kPinRst = 14;

// Dimensions apres setRotation(1) (paysage) - reprises d'OXY-LD (TFT_W/TFT_H).
constexpr int16_t kScreenW = 320;
}  // namespace

Display::Display() : spi_(), tft_(&spi_, kPinCs, kPinDc, kPinRst) {}

void Display::begin() {
  // MISO non cable (pas de lecture d'ID au boot) - cf. HARDWARE.md.
  spi_.begin(kPinSck, /*miso=*/-1, kPinMosi, kPinCs);
  tft_.init(240, 320);
  tft_.invertDisplay(true);
  tft_.setRotation(1);

  // Corrige un miroir horizontal connu sur ce clone ST7789 - MADCTL=MV
  // seul, sans toucher au sens haut/bas deja correct via setRotation(1).
  uint8_t madctl = ST77XX_MADCTL_MV;
  tft_.sendCommand(ST77XX_MADCTL, &madctl, 1);

  // Fond noir pose explicitement des l'initialisation - couleur de fond
  // reprise d'OXY-LD, pas seulement une consequence indirecte du splash.
  tft_.fillScreen(ST77XX_BLACK);
}

void Display::force_redraw() { force_redraw_ = true; }

void Display::center_text(const char* s, int16_t y, const GFXfont* font, uint16_t color,
                          uint8_t scale) {
  tft_.setFont(font);
  tft_.setTextSize(scale);
  tft_.setTextColor(color);
  int16_t x1, y1;
  uint16_t w, h;
  tft_.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  tft_.setCursor((kScreenW - static_cast<int16_t>(w)) / 2 - x1, y - y1);
  tft_.print(s);
  tft_.setFont(nullptr);
}

void Display::show_splash() {
  tft_.fillScreen(ST77XX_BLACK);
  center_text("OXY-LD2", 60, &FreeSansBold24pt7b, ST77XX_WHITE);
  center_text("demarrage...", 110, &FreeSansBold9pt7b, ST77XX_WHITE);
  force_redraw_ = true;
}

void Display::show_measurement(int o2_percent, int mod_meters, float ppo2_setpoint,
                                RgbColor color, bool stable, bool o2_value_visible) {
  const bool force = force_redraw_;
  force_redraw_ = false;

  // -- Chiffre O2 (grand, centre) -- clignote pendant la stabilisation :
  // texte effectif vide quand o2_value_visible est faux, compare comme le
  // reste au cache pour eviter le scintillement pendant les phases stables
  // (visible ou invisible) tout en redessinant reellement au changement.
  char o2_text[8];
  if (o2_value_visible) {
    std::snprintf(o2_text, sizeof(o2_text), "%d%%", o2_percent);
  } else {
    o2_text[0] = '\0';
  }
  uint16_t o2_color = tft_.color565(color.r, color.g, color.b);
  if (force || std::strcmp(o2_text, prev_o2_text_) != 0 || o2_color != prev_o2_color_) {
    std::strcpy(prev_o2_text_, o2_text);
    prev_o2_color_ = o2_color;
    tft_.fillRect(0, 30, kScreenW, 118, ST77XX_BLACK);
    if (o2_text[0] != '\0') {
      // scale=2, y=48 : reprend exactement la taille et la position
      // d'affichage d'OXY-LD pour le grand chiffre (numTopY dans
      // OXY-LD/src/main.cpp) - la police FreeSansBold24pt7b (deja la plus
      // grande disponible) est doublee pour remplir l'espace disponible.
      center_text(o2_text, 48, &FreeSansBold24pt7b, o2_color, /*scale=*/2);
    }
  }

  // -- Badge stabilite (haut-droit) -- ne clignote pas, cf. header. Agrandi
  // (scale=2) suite au retour utilisateur ("texte dans le petit carre trop
  // petit") - la version initiale reprenait la taille exacte d'OXY-LD
  // (scale=1) mais s'est averee trop petite pour etre lisible ici.
  if (force || stable != prev_badge_stable_ || !prev_badge_shown_) {
    prev_badge_shown_ = true;
    prev_badge_stable_ = stable;
    constexpr int16_t bx = 224, by = 0, bw = 96, bh = 32;
    tft_.fillRect(bx, by, bw, bh, ST77XX_BLACK);
    uint16_t badge_col = stable ? ST77XX_GREEN : tft_.color565(230, 120, 0);
    tft_.fillRoundRect(bx + 2, by + 2, bw - 4, bh - 4, 5, badge_col);
    const char* txt = stable ? "OK" : "...";
    tft_.setFont(&FreeSansBold9pt7b);
    tft_.setTextSize(2);
    int16_t bx1, by1;
    uint16_t bw1, bh1;
    tft_.getTextBounds(txt, 0, 0, &bx1, &by1, &bw1, &bh1);
    tft_.setTextColor(ST77XX_BLACK);
    tft_.setCursor(bx + (bw - static_cast<int16_t>(bw1)) / 2 - bx1,
                   by + (bh - static_cast<int16_t>(bh1)) / 2 - by1);
    tft_.print(txt);
    tft_.setFont(nullptr);
  }

  // -- Bande basse : MOD + ppO2 --
  char mod_text[24];
  if (mod_meters >= 0) {
    std::snprintf(mod_text, sizeof(mod_text), "MOD %d m", mod_meters);
  } else {
    std::snprintf(mod_text, sizeof(mod_text), "MOD --");
  }
  char ppo2_text[16];
  std::snprintf(ppo2_text, sizeof(ppo2_text), "ppO2 %.1f", static_cast<double>(ppo2_setpoint));

  if (force || std::strcmp(mod_text, prev_mod_text_) != 0 ||
      std::strcmp(ppo2_text, prev_ppo2_text_) != 0) {
    std::strcpy(prev_mod_text_, mod_text);
    std::strcpy(prev_ppo2_text_, ppo2_text);
    tft_.fillRect(0, 150, kScreenW, 80, ST77XX_BLACK);
    center_text(mod_text, 165, &FreeSansBold12pt7b, ST77XX_WHITE);
    center_text(ppo2_text, 200, &FreeSansBold9pt7b, ST77XX_WHITE);
  }
}

void Display::show_clock(int hour, int minute) {
  char clock_text[8];
  std::snprintf(clock_text, sizeof(clock_text), "%02d:%02d", hour, minute);

  if (!force_redraw_ && std::strcmp(clock_text, prev_clock_text_) == 0) {
    return;
  }
  std::strcpy(prev_clock_text_, clock_text);

  // Zone haut-gauche (0,0,90,26) - reprise d'OXY-LD. Texte aligne a
  // gauche (pas centre sur l'ecran entier, contrairement a center_text()).
  tft_.fillRect(0, 0, 90, 26, ST77XX_BLACK);
  tft_.setFont(&FreeSansBold9pt7b);
  tft_.setTextSize(1);
  tft_.setTextColor(ST77XX_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  tft_.getTextBounds(clock_text, 0, 0, &x1, &y1, &w, &h);
  tft_.setCursor(4 - x1, 4 - y1);
  tft_.print(clock_text);
  tft_.setFont(nullptr);
}
