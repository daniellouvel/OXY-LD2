#include "display.h"

namespace {
constexpr int8_t kPinCs = 10;
constexpr int8_t kPinSck = 12;
constexpr int8_t kPinMosi = 11;
constexpr int8_t kPinDc = 13;
constexpr int8_t kPinRst = 14;

constexpr int16_t kO2ValueX = 10;
constexpr int16_t kO2ValueY = 10;
constexpr int16_t kO2ValueW = 200;
constexpr int16_t kO2ValueH = 40;
}  // namespace

Display::Display() : spi_(), tft_(&spi_, kPinCs, kPinDc, kPinRst) {}

uint16_t Display::to_rgb565(RgbColor c) {
  return static_cast<uint16_t>(((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3));
}

void Display::begin() {
  // MISO non cable (pas de lecture d'ID au boot) - cf. HARDWARE.md.
  spi_.begin(kPinSck, /*miso=*/-1, kPinMosi, kPinCs);
  tft_.init(240, 320);
  tft_.invertDisplay(true);
  tft_.setRotation(1);

  // Corrige un miroir horizontal connu sur ce clone ST7789 - MADCTL=MV
  // seul, sans toucher au sens haut/bas deja correct via setRotation(1).
  // Cf. HARDWARE.md / historique OXY-LD.
  uint8_t madctl = ST77XX_MADCTL_MV;
  tft_.sendCommand(ST77XX_MADCTL, &madctl, 1);
}

void Display::show_splash() {
  tft_.fillScreen(ST77XX_BLACK);
  tft_.setTextColor(ST77XX_WHITE);
  tft_.setTextSize(2);
  tft_.setCursor(10, 10);
  tft_.print("OXY-LD2");
  tft_.setTextSize(1);
  tft_.setCursor(10, 40);
  tft_.print("demarrage...");
}

void Display::show_measurement(int o2_percent, int mod_meters, float ppo2_setpoint,
                                RgbColor color, bool o2_value_visible) {
  if (o2_value_visible) {
    tft_.fillRect(kO2ValueX, kO2ValueY, kO2ValueW, kO2ValueH, ST77XX_BLACK);
    tft_.setTextColor(to_rgb565(color));
    tft_.setTextSize(3);
    tft_.setCursor(kO2ValueX, kO2ValueY);
    tft_.print("O2: ");
    tft_.print(o2_percent);
    tft_.print(" %");
  } else {
    tft_.fillRect(kO2ValueX, kO2ValueY, kO2ValueW, kO2ValueH, ST77XX_BLACK);
  }

  tft_.fillRect(0, 60, 320, 60, ST77XX_BLACK);
  tft_.setTextColor(ST77XX_WHITE);
  tft_.setTextSize(2);
  tft_.setCursor(10, 60);
  if (mod_meters >= 0) {
    tft_.print("MOD: ");
    tft_.print(mod_meters);
    tft_.print(" m");
  } else {
    tft_.print("MOD: --");
  }
  tft_.setCursor(10, 90);
  tft_.print("ppO2 ref: ");
  tft_.print(ppo2_setpoint, 1);
}
