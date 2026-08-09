# Matériel — OXY-LD2

Repris tel quel du matériel documenté dans `OXY-LD` (station de paillasse, alimentation secteur 220V AC). Référence détaillée : `OXY-LD/WIRING.md` et `OXY-LD/Schematic_Analyseur_2026-08-06.pdf`. Ce document résume ce qui est nécessaire pour démarrer OXY-LD2 ; se reporter à OXY-LD pour le détail complet (schémas ASCII, notes de niveau logique, checklist premier allumage).

## MCU

**ESP32-S3-WROOM-1 (N16R2)** — 16 MB flash, PSRAM QSPI. Board PlatformIO : `esp32-s3-devkitc-1`.

## BOM

| Composant | Rôle | Notes |
|---|---|---|
| ESP32-S3-WROOM-1 (N16R2) | MCU principal | 16 MB flash, PSRAM QSPI |
| ADS1115 (I2C, 0x48) | ADC 16 bits | gain ×16 (`GAIN_SIXTEEN`, ±256 mV, LSB 7.8 µV), alimentation 3.3 V |
| Écran TFT SPI 240×320 (ST7789) | Affichage | **Confirmé** — driver et réglages corrects sur la dernière version d'OXY-LD (`tft.init(240,320)`, `invertDisplay(true)`, `setRotation(1)` + `MADCTL=MV`) ; pas de tactile, pas de MISO câblé |
| RTC PCF8563T/5,518 (I2C, 0x51) | Horodatage | **Confirmé** — même référence que sur OXY-LD (résout l'incohérence PCF8563/DS3231/DS1307 entre les docs OXY-LD ; DS3231/DS1307 étaient des mentions obsolètes d'anciennes révisions) |
| 3× TTP223 | Boutons tactiles | sortie HIGH quand touché (jumper A par défaut) |
| Imprimante TSC TH240 | Étiqueteuse thermique | UART2, 115 200 bauds, TSPL ; RS-232 → MAX3232 obligatoire (pas MAX232, 5V) |
| Cellule O2 galvanique (ex. R-17 Med, R-22, OOM-202) | Capteur | sortie ~9–13 mV à l'air, mesure différentielle A0−A1 |
| DS18B20 (TO-92) + pull-up 4.7 kΩ | Température | optionnel, compensation ~0.3 %/°C |
| Module PN532 (I2C, 0x24, mode I2C via DIP SEL0=OFF/SEL1=ON) | RFID | optionnel, badges Mifare Classic 1K / NTAG213-215-216 |
| WS2812B (1 LED RGB) + level-shifter 74HCT245 | Indicateur d'état | ESP32-S3 = 3.3 V, WS2812B attend 3.5–5 V ; alternative dépannage : diode 1N4148 en série sur le 5V |
| Bloc secteur 5V DC 2A | Alimentation ESP32 | depuis 220V AC |
| Interrupteur secteur bipolaire + porte-fusible | Sécurité 220V | fusible 1A temporisé |
| Boîtier de paillasse ABS ignifuge | Intégration | passe-câbles, distance ≥4 mm pistes 220V / BT |

## Brochage (ESP32-S3, repris de OXY-LD/WIRING.md)

| Pin ESP32-S3 | Usage |
|---|---|
| IO8 (SDA) / IO9 (SCL) | Bus I2C — ADS1115 (0x48), RTC (0x51), PN532 (0x24) — pins par défaut `Wire.begin()` |
| IO5 / IO6 / IO7 | Boutons GAUCHE / CENTRE / DROITE (TTP223) |
| IO38 | OneWire DS18B20 (optionnel) |
| IO15 | LED RGB WS2812B (via 74HCT245) — **pas IO12/IO13, ce sont des strapping pins ESP32-S3** |
| IO40 / IO41 | PN532 IRQ / RESET (optionnel) |
| IO17 (TX) / IO18 (RX) | UART2 vers imprimante TSC |
| IO10 / IO12 / IO11 / IO13 / IO14 | TFT CS / SCK / MOSI / DC / RST |

> **Note strapping pins ESP32-S3** : IO0, IO3, IO45, IO46 (et IO12 sur certaines variantes) influencent le mode de boot — éviter d'y câbler un périphérique dont l'état au reset n'est pas maîtrisé. OXY-LD a appris cela a posteriori (LED déplacée deux fois) ; à vérifier *avant* câblage sur OXY-LD2, pas après un premier essai raté (voir [ARCHITECTURE.md](ARCHITECTURE.md)).

## Alimentation 220V AC — sécurité obligatoire

- Fusible 1A temporisé + interrupteur bipolaire (coupe phase + neutre)
- Toutes connexions 220V sous gaine thermo ou domino fermé, aucun contact exposé
- Distance ≥ 4 mm entre pistes 220V et basse tension
- Boîtier ABS ignifuge ou boîtier métallique relié à la terre
- Bloc secteur marqué CE, double isolation
- Imprimante TSC TH240 sur son propre bloc 24V — jamais alimentée depuis l'ESP32

## À reconfirmer avant implémentation

- [x] Référence exacte du RTC — **PCF8563T/5,518**, confirmé identique à OXY-LD
- [x] Driver écran et orientation/inversion couleurs — **confirmés corrects sur la dernière version d'OXY-LD**, à reprendre tels quels (`ST7789`, `invertDisplay(true)`, `setRotation(1)` + `MADCTL=MV`)
