# lib/

Bibliothèques privées PlatformIO, une par responsabilité matérielle ou fonctionnelle (voir la table dans [ARCHITECTURE.md](../ARCHITECTURE.md)). Les modules sont créés au fil de l'implémentation, pas d'avance.

- `mod_calc/` — MOD par table statique (O2 21-100%, ppO2 1.4/1.6) + valeur O2 canonique (arrondi entier vers le haut). Testé en `native`.
- `stability/` — détection de stabilité par pente lissée (EMA) de la mesure O2, `min_settle_ms`/`sustained_ms` cumulatifs. Testé en `native`.
- `o2_sensor/` — conversion tension→%O2, compensation thermique (coefficient non assumé, à sourcer), filtre anti-glitch médiane-de-3. Testé en `native`. Lecture ADS1115 réelle volontairement absente d'ici (module matériel séparé, à écrire au câblage).
- `calibration/` — état de calibration (tension à l'air, température optionnelle via `NAN`), historique circulaire pour détecter une dérive de cellule. Persistance flash déléguée à `storage`. Testé en `native`.
- `storage/` — sérialisation versionnée + checksum du dernier point de calibration (17 octets). Écriture flash réelle (NVS) volontairement absente d'ici, comme le driver ADS1115. Testé en `native`.
- `printer/` — génération de la commande TSPL de l'étiquette (cas simple, sans badge). Positions/police non vérifiées visuellement (pas d'imprimante câblée). Testé en `native`.
- `led_status/` — table pure état→couleur/clignotement, pas d'appel matériel. Testé en `native`.
- `buttons/` — détection appui court/long, appui long détecté pendant le maintien (pas au relâchement). Lecture TTP223 réelle non incluse (non câblés). Testé en `native`.
