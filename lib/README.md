# lib/

Bibliothèques privées PlatformIO, une par responsabilité matérielle ou fonctionnelle (voir la table dans [ARCHITECTURE.md](../ARCHITECTURE.md)). Les modules sont créés au fil de l'implémentation, pas d'avance.

- `mod_calc/` — MOD par table statique (O2 21-100%, ppO2 1.4/1.6) + valeur O2 canonique (arrondi entier vers le haut). Testé en `native`.
- `stability/` — détection de stabilité par pente lissée (EMA), `min_settle_ms`/`sustained_ms` cumulatifs. Reçoit désormais la **tension brute** (pas le %O2, qui dépend d'une calibration pas encore forcément faite). Testé en `native`.
- `o2_sensor/` — conversion tension→%O2, compensation thermique (coefficient non assumé, à sourcer), filtre anti-glitch médiane-de-3. Testé en `native`.
- `calibration/` — état de calibration (tension à l'air, température optionnelle via `NAN`), historique circulaire pour détecter une dérive de cellule. Persistance flash via `storage`, écriture réelle câblée dans `main.cpp`. Testé en `native`.
- `storage/` — sérialisation versionnée + checksum du dernier point de calibration (17 octets). **Écriture flash réelle (`Preferences`) implémentée et vérifiée** dans `main.cpp`. Testé en `native`.
- `printer/` — génération de la commande TSPL de l'étiquette (cas simple, sans badge). Positions/police non vérifiées visuellement (pas d'imprimante câblée). Testé en `native`.
- `led_status/` — table pure état→couleur/clignotement, pas d'appel matériel. Testé en `native`.
- `buttons/` — détection appui court/long, appui long détecté pendant le maintien (pas au relâchement). Lecture TTP223 réelle non incluse (non câblés). Testé en `native`.
- `ads1115_reader/` — lecture différentielle ADS1115 réelle (matériel, `pio run -e esp32s3` uniquement). **Flashé et vérifié par log série sur la vraie carte.**
- `display/` — rendu écran TFT ST7789, police vectorielle et gabarit repris d'OXY-LD, cache anti-scintillement (matériel, `pio run -e esp32s3` uniquement). **Confirmé visuellement sur la carte.** Badge de calibration permanent (bas-droit, vert/rouge) + message temporaire de résultat de calibration manuelle ajoutés — flashés et exercés en conditions réelles (log série), rendu visuel pas encore confirmé par l'utilisateur.
- `rtc_clock/` — horodatage RTC PCF8563 (matériel, `pio run -e esp32s3` uniquement). **Confirmé par log série** (heure lue plausible).
- `web_ui/` — génération pure du HTML/JSON du serveur de configuration (pages matériel/plongée/tables), `std::string`, aucune dépendance WiFi. Testé en `native`. Le câblage réel (`WiFi.softAP`, routes `AsyncWebServer`, mutex) vit dans `src/main.cpp`.
