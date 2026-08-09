# Architecture — OXY-LD2

Décisions structurantes pour la réécriture, en réponse directe aux limites observées sur OXY-LD. Trois axes, à poids égal : **architecture**, **fiabilité**, **précision**.

---

## 1. Architecture modulaire

OXY-LD concentre capteur, affichage, stockage, réseau, impression et RFID dans un seul `main.cpp` de 2510 lignes. OXY-LD2 découpe par responsabilité, chaque module vivant dans `lib/<nom>/` (convention PlatformIO — bibliothèque privée) avec une interface `.h` minimale et une seule responsabilité :

| Module (prévu) | Responsabilité |
|---|---|
| `o2_sensor` ✅ | Conversion tension → %O2, compensation thermique, filtrage — **implémenté**, voir ci-dessous |
| `ads1115_reader` ✅ | Lecture différentielle ADS1115 réelle — **implémenté et vérifié sur la carte**, voir ci-dessous |
| `calibration` ✅ | État de calibration (air, cellule optionnellement thermique), détection de vieillissement — **implémenté**, voir ci-dessous. Persistance flash déléguée à `storage` |
| `stability` ✅ | Détection de stabilité par pente lissée — **implémenté**, voir ci-dessous |
| `mod_calc` ✅ | MOD par table statique — **implémenté**, voir ci-dessous |
| `display` ✅ | Rendu écran TFT (ST7789) — **implémenté et confirmé visuellement**, voir ci-dessous. Police/format repris d'OXY-LD |
| `storage` ✅ | Sérialisation versionnée de la calibration — **partiellement implémenté**, voir ci-dessous. Écriture flash réelle (NVS) reportée. Historique complet et réglages pas encore couverts |
| `rtc_clock` ✅ | Horodatage RTC PCF8563 — **implémenté et vérifié sur la carte**, voir ci-dessous |
| `rfid_badge` | Lecture badges PN532 (nom, licence FFESSM) |
| `printer` ✅ | Génération étiquette TSPL — **implémenté**, voir ci-dessous. Envoi UART reporté (imprimante non câblée) |
| `led_status` ✅ | Codes couleur LED d'état — **implémenté**, voir ci-dessous. Rendu NeoPixel réel non inclus |
| `buttons` ✅ | Détection appui court/long — **implémenté**, voir ci-dessous. Lecture TTP223 réelle non incluse (boutons non câblés) |
| `web_ui` | Point d'accès WiFi + serveur web |

Chaque module ne connaît que son propre matériel — `o2_sensor` ignore l'existence de l'écran, `display` ignore l'ADS1115. Le fichier `src/main.cpp` orchestre, sans logique métier propre.

**Ce qui est pur (sans dépendance Arduino/matériel) doit le rester** : `mod_calc`, la logique d'arrondi, le calcul de compensation thermique. C'est ce qui rend possible le point 2.

### `mod_calc` — premier module implémenté

Contrairement au plan initial (formule calculée au runtime), la MOD est servie par une **table statique** (`lib/mod_calc/mod_table.cpp`, O2 21–100 % × ppO2 1.4/1.6) générée hors-ligne par [`tools/generate_mod_table.py`](tools/generate_mod_table.py) — **aucun calcul flottant dans le chemin critique du firmware**. Décision prise après avoir vérifié que la FFESSM n'édite pas de table numérique officielle (leur manuel enseigne le calcul) : la table est donc générée par script (formule + troncature vers le bas, jamais vers le haut) puis **validée ligne par ligne contre la table papier réellement utilisée par le club** — 78/78 valeurs identiques pour ppO2=1.6 (22–99 %). Détail de la validation : [`docs/mod_table_review.html`](docs/mod_table_review.html).

Piège rencontré et corrigé : un `ppo2` en `float` (32 bits) au lieu de `double` décale certaines lignes de 1 m par imprécision de représentation binaire (ex. 1.4/0.25 attendu 46.0 exact, obtenu 45.999...). Le générateur Python et le test de cohérence (`test/test_mod_calc`) utilisent tous les deux du `double` pour rester identiques bit à bit.

`o2_display_value()` (`lib/mod_calc/o2_value.h`) fournit la valeur O2 canonique utilisée partout en aval (affichage, étiquette, clé de la table MOD) : **toujours un entier, toujours arrondi vers le haut**, non configurable — contrairement à OXY-LD qui proposait aucun/↑/↓. Arrondir vers le haut est la direction conservatrice pour la MOD (plus d'O2 mesuré ⇒ MOD plus courte).

Tests (`pio test -e native`) : bornes de la table, concordance avec la table du club, cohérence table/formule génératrice, arrondi O2. **Note environnement** : `pio test -e native` nécessite un compilateur hôte (g++/gcc) — absent par défaut sur cette machine Windows (seule la toolchain croisée ESP32-S3 était présente). Installé via `scoop install gcc` (pas de droits admin requis, contrairement à choco qui a échoué ici faute d'élévation).

## 2. Fiabilité

Bugs déjà rencontrés sur OXY-LD à traiter structurellement plutôt qu'au cas par cas :

- **Corruption EEPROM par décalage de champs** (fixée en urgence sur OXY-LD en déplaçant des champs à une adresse ≥1300) → sur OXY-LD2, la persistance passe par un stockage structuré avec **versionnage de schéma explicite** (un champ `version` en tête de struct, migration ou reset propre si la version ne correspond pas), au lieu d'offsets bruts codés en dur. **Tranché** : format hybride — blob versionné + checksum maison (`lib/storage/`), stocké via `Preferences.putBytes()` (NVS natif ESP32, wear-leveling géré par la bibliothèque) plutôt qu'une écriture EEPROM manuelle. Un octet de version incompatible (y compris la flash jamais écrite, lue comme `0xFF`) ou un checksum invalide fait rejeter la lecture explicitement — le même piège "flash vierge = 0xFF" qu'OXY-LD avait dû corriger après coup (auto-calibration en heures) est ici géré par construction, pas en correctif.
- **Choix de pin invalidé a posteriori** (LED sur une strapping pin, déplacée deux fois) → la table de brochage vit à un seul endroit ([HARDWARE.md](HARDWARE.md)), vérifiée contre la documentation strapping-pin de l'ESP32-S3 *avant* implémentation, pas après un premier essai raté.
- **Tests unitaires PlatformIO (Unity, environnement `native`)** pour toute la logique pure : calcul MOD, conversion tension→%O2, arrondi, compensation thermique, logique de calibration. Ces modules ne touchent pas au matériel — testables sans carte branchée.
- **Watchdog matériel** activé, pour repartir proprement d'un état cohérent en cas de blocage plutôt que de rester figé.
- **Détection de stabilité par critère de pente** (`stability`, non implémenté) : au lieu du seul seuil min/max sur fenêtre glissante d'OXY-LD (15 lectures, 0.1 %), critère de pente lissée (EMA) `|d(%O2)/dt| < ε` — le taux de variation du signal s'approche de zéro quand la réaction chimique de la cellule est terminée, ce qui colle mieux à la réalité physique qu'un simple écart max−min qui peut s'annuler par hasard sur du bruit. Deux durées cumulatives : `min_settle_ms` (plancher dur depuis le dernier changement de gaz/calibration, avant même d'envisager la stabilité) et `sustained_ms` (la pente doit rester sous ε en continu avant de déclarer stable, anti-rebond contre un creux de bruit isolé).
  - Valeurs de départ initiales (60 s / 5 s) revues à la baisse après retour d'expérience : un analyseur Nitrox du commerce stabilise typiquement en **5-15 s**. Nouvelles valeurs de départ : `min_settle_ms = 5000`, `sustained_ms = 3000` (~8 s dans le meilleur cas, cohérent avec le bas de la fourchette observée). `ε` et `α` (lissage EMA) inchangés faute de donnée pour les ajuster. **Toujours non calibré sur la vraie cellule** — le retour d'expérience porte sur un analyseur commercial, pas sur cette cellule galvanique dans ce boîtier précis (flux d'air actif ou diffusion passive : à déterminer).
  - **Retour UI attendu (non implémenté)** : la valeur O2 affichée doit **clignoter** tant que `stability.is_stable()` est faux, et devenir **fixe** dès qu'il passe vrai — signal visuel direct que la mesure est validée. C'est une responsabilité de `display` (qui lit l'état de `stability` en entrée), pas de `stability` elle-même, qui reste une logique pure sans notion de rendu ni de rythme de clignotement. Reste à trancher à l'implémentation de `display` : période de clignotement, et si le comportement doit aussi se refléter sur l'étiquette imprimée ou seulement à l'écran.
  - **Implémenté** (`lib/stability/`) : `StabilityTracker` — EMA de la pente `d(%O2)/dt` (pas de blending sur la toute première pente calculée, pour éviter un biais de démarrage artificiel vers "stable"), deux durées cumulatives (`min_settle_ms`, `sustained_ms`), `is_stable()` non verrouillé (redevient faux si la pente remonte), gestion explicite du rollover `millis()` (soustractions non signées uniquement) et des écarts temporels trop grands entre échantillons (`max_sample_gap_ms` → redémarrage silencieux de la baseline). 9 tests unitaires (`test/test_stability`), tous passés du premier coup — contrairement à `mod_calc` où le détail préalable n'avait pas suffi à éviter un bug float/double, ici la conception discutée avant l'écriture a tenu.

## 3. Précision de mesure

La chaîne de mesure O2 est le cœur du projet — elle mérite plus d'attention que le firmware périphérique :

- **Mesure différentielle A0−A1** (déjà validée sur OXY-LD, conservée) — rejet du bruit secteur 50 Hz.
- **Filtrage explicite et documenté** — **tranché** : médiane de 3 échantillons (`GlitchFilter`, `lib/o2_sensor/glitch_filter.h`), pas une moyenne glissante. Une moyenne lisserait la vraie dynamique du signal et fausserait le calcul de pente de `stability` (double lissage, décalage imprévisible) ; la médiane rejette un glitch ADC isolé sans retarder un vrai changement — `stability` reste seule responsable de juger si c'est stabilisé.
- **Compensation thermique** — structure implémentée (`apply_thermal_compensation()`, `lib/o2_sensor/conversion.h`), testée avec des valeurs connues. Le **coefficient lui-même reste à sourcer** : pas de valeur par défaut assumée dans le code, contrairement au "0.3%/°C" d'OXY-LD dont l'origine (fiche technique vs estimation) n'est pas vérifiable. Même principe que pour la table MOD FFESSM : pas de chiffre de sécurité sans source.
- **Suivi du vieillissement de cellule** : la tension à l'air à chaque calibration est historisée, pas seulement la dernière valeur — permet de détecter une dérive avant que la cellule tombe sous le seuil de remplacement (~7 mV), plutôt que de le découvrir à la calibration suivante.

### `o2_sensor` — partie pure implémentée

`voltage_to_o2_percent(v_measured_mv, v_calibration_mv, o2_reference_percent=20.9)` : conversion linéaire, `NAN` si non calibré (`v_calibration_mv ≤ 0`). Ne possède pas l'état de calibration — fourni par l'appelant (module `calibration`).

La lecture ADS1115 réelle (gain `GAIN_SIXTEEN`, `readADC_Differential_0_1()`) vit dans `lib/ads1115_reader/` — pas dans `lib/o2_sensor/` : PlatformIO compile tout le contenu d'un dossier `lib/` dès qu'un seul fichier est inclus, donc mélanger code pur et code dépendant d'`Adafruit_ADS1X15`/`Arduino.h` dans le même dossier casserait `pio test -e native`.

17 tests unitaires (`test/test_o2_sensor`), tous passés du premier coup.

### `ads1115_reader` — implémenté et **vérifié sur la vraie carte**

Signatures API confirmées en lisant les vraies bibliothèques après `pio run -e esp32s3` (`Adafruit_ADS1X15 @ 2.6.2`, dans `.pio/libdeps/esp32s3/`) plutôt qu'écrites de mémoire — `begin()`, `setGain(GAIN_SIXTEEN)`, `readADC_Differential_0_1()`, `computeVolts()` (retourne des volts, converti en mV ici).

**Flashé sur la carte réelle (COM6) et vérifié par lecture série** (nuit du 2026-08-09, ESP32-S3 QFN56 rev v0.2, PSRAM embarquée 2MB confirmée par `esptool` — cohérent avec N16R2) :
```
v_mv=8.141 fo2=20.92 o2=21 mod=56 slope=0.1709 stable=0 cal_v=8.133
v_mv=8.125 fo2=20.88 o2=21 mod=56 slope=-0.2678 stable=0 cal_v=8.133
```
Tension cellule ~8.13 mV (plausible, légèrement sous la fourchette nominale ~9-13 mV d'OXY-LD — pas alarmant, variabilité normale entre cellules), valeur qui **varie réellement** d'une lecture à l'autre (8.125–8.148 mV, pas une valeur figée). `mod=56` pour O2=21%/ppO2=1.4 correspond **exactement** à la valeur validée contre la table papier du club plus tôt dans cette session — première confirmation que toute la chaîne (ADS1115 → conversion → arrondi → table MOD) produit un résultat cohérent de bout en bout sur du vrai matériel, pas seulement en tests unitaires.

**Non vérifié** : je n'ai pas pu physiquement souffler/couvrir la cellule pour confirmer que la valeur réagit à un vrai changement de gaz (pas d'accès physique à la carte) — seule la variation de bruit normale est confirmée, pas la réponse à un stimulus.

### `display` — implémenté et **confirmé visuellement**, police/format repris d'OXY-LD

Signatures API vérifiées dans les vraies bibliothèques (`Adafruit ST7735 and ST7789 Library @ 1.11.0`, `Adafruit GFX Library @ 1.12.6`) : `Adafruit_ST7789(SPIClass*, cs, dc, rst)` (constructeur SPI matériel), `init(240,320)`, `invertDisplay(true)`, `setRotation(1)`, `sendCommand(ST77XX_MADCTL, &madctl, 1)` avec `ST77XX_MADCTL_MV` pour le miroir. `SPI.begin(sck=12, miso=-1, mosi=11, cs=10)` reprend la note de `HARDWARE.md`/OXY-LD sur le MISO non câblé.

**Confirmé par l'utilisateur au réveil** : écran lisible, O2/MOD/ppO2 dans le bon sens (orientation/MADCTL corrects) — avec un léger scintillement au premier flash.

**Scintillement diagnostiqué et corrigé** en reprenant la technique et le format d'affichage réels d'OXY-LD (`OXY-LD/src/main.cpp`, `displayRead()`) plutôt que la version improvisée de cette nuit :
- **Cause** : un `fillRect` (noir) suivi d'un redraw à *chaque* appel de `show_measurement()`, même quand rien n'a changé, produit un flash noir visible sur ce panel — même symptôme qu'OXY-LD avait documenté (qui a des environnements de diagnostic dédiés, `esp32s3-tfttest-wifi`/`nowifi`, dans son `platformio.ini`).
- **Correction** : chaque zone (chiffre O2, badge stabilité, bande MOD/ppO2) compare son contenu au dernier contenu dessiné (cache par instance dans `Display`) et saute le `fillRect`+redraw si identique — `force_redraw()` force un redraw complet après un changement de mode.
- **Police** : polices vectorielles `FreeSansBold9pt7b`/`FreeSansBold12pt7b`/`FreeSansBold24pt7b` (bundle `Fonts/` d'Adafruit GFX) au lieu de la police bitmap standard — confirmées présentes dans `.pio/libdeps/esp32s3/Adafruit GFX Library/Fonts/`.
- **Gabarit** : grand chiffre O2 centré (zone y 30–148), badge "OK"/"..." en haut à droite (ne clignote pas, contrairement au chiffre), bande basse "MOD X m" / "ppO2 X.X" — positions reprises des coordonnées exactes d'OXY-LD.
- Le clignotement du chiffre O2 pendant la stabilisation (exigence utilisateur antérieure, cf. §2 "Retour UI attendu") est **conservé** — absent d'OXY-LD, ajout spécifique à OXY-LD2. Combiné avec le cache anti-scintillement : le texte effectif (vide si invisible) est ce qui est comparé, donc le clignotement déclenche bien un redraw à chaque bascule sans revenir au scintillement continu.

**ppO2 par défaut changé de 1.4 à 1.6** (demande explicite) — `mod_lookup` avec `PPO2Setpoint::P16`, vérifié par log série (`o2=21 mod=66`, correspond exactement à la valeur validée contre la table du club).

**Correction taille/fond suite au retour utilisateur** ("bon sens, léger scintillement" après le premier flash de ce matin) :
- **Taille du grand chiffre O2** : la première version appelait `center_text()` avec un `setTextSize(1)` codé en dur, alors qu'OXY-LD affiche ce chiffre précis à `setTextSize(2)` sur la police déjà la plus grande (`FreeSansBold24pt7b`) pour remplir l'espace disponible. `center_text()` prend maintenant un paramètre `scale` (défaut 1), utilisé à 2 uniquement pour ce chiffre — `y=48` repris exactement de la position vérifiée d'OXY-LD (`numTopY` dans `OXY-LD/src/main.cpp`), pas une valeur inventée.
- **Fond noir** : posé explicitement dans `begin()` (`tft_.fillScreen(ST77XX_BLACK)` juste après le réglage MADCTL), pas seulement comme conséquence indirecte de `show_splash()`.

**Badge de stabilité agrandi** suite à un second retour ("texte dans le petit carré trop petit") : passé de `setTextSize(1)` (taille exacte d'OXY-LD) à `setTextSize(2)`, boîte élargie de 72×22 à 92×32 pour accueillir le texte plus grand. Écart assumé par rapport à la fidélité stricte au gabarit d'OXY-LD — priorité donnée au retour direct de l'utilisateur sur la lisibilité réelle de cet écran précis.

### `rtc_clock` — implémenté et **vérifié sur la carte**

RTC câblé en cours de session (annoncé par l'utilisateur, pas câblé la veille). `RtcClock` (`lib/rtc_clock/`) : wrapper fin autour de `RTC_PCF8563` (RTClib), même raison de séparation que `ads1115_reader`/`display` — module matériel, pas de logique pure à tester en `native`. `now()`/`lostPower()` de RTClib ne sont pas const-qualifiées (accès I2C) : `RtcClock::now()`/`lost_power()` non plus, par cohérence — piège évité avant compilation en vérifiant les signatures réelles plutôt que de deviner.

Horloge affichée en haut à gauche (`Display::show_clock()`, zone 0,0,90,26 reprise d'OXY-LD), même cache anti-scintillement que le reste (`prev_clock_text_`). `lost_power()` vérifié au démarrage (pile CR2032 absente/HS → heure non fiable) et loggé, sans action corrective possible pour l'instant (pas de bouton pour régler l'heure).

**Vérifié par log série** : `rtc_ok=1 clock=11:34` — RTC détecté à l'adresse I2C 0x51, heure lue plausible (correspond à l'heure réelle de la session). `RTC_PCF8563.cpp` déjà confirmé présent dans `RTClib @ 2.1.4` lors du premier `pio run -e esp32s3` de cette session — l'intégration n'a donc pas eu de surprise côté bibliothèque.

### `calibration` — implémenté

`CalibrationTracker` : tension à l'air + température **optionnelle** (sentinelle `NAN`, cohérent avec le reste du code — pas `std::optional`, portabilité ESP32/Arduino incertaine). Le DS18B20 étant explicitement optionnel dans HARDWARE.md, la température ne peut pas être un paramètre obligatoire. `has_temperature()` distingue "pas calibré" de "calibré sans sonde".

Historique en buffer circulaire à capacité fixe (10, pas de valeur validée — arbitraire mais raisonnable, ajustable) pour détecter une **dérive progressive** (`is_declining()`, compare la calibration la plus ancienne *encore en mémoire* à la plus récente) en plus du seuil dur (`is_cell_aging()`). Aucune des constantes de sécurité (plage de tension plausible, seuil de remplacement ~7 mV, seuil de dérive) n'a de valeur par défaut codée en dur — toutes passées en paramètre par l'appelant, même principe que pour le coefficient thermique et la table MOD.

Persistance flash **non incluse ici** — `calibrate()` ne fait qu'une gestion d'état en mémoire ; l'écriture réelle (NVS/EEPROM versionnée, cf. §2) est la responsabilité de `storage`, pas encore écrit.

11 tests unitaires (`test/test_calibration`), incluant un test dédié à l'éviction du buffer circulaire (vérifie que `is_declining()` compare bien contre le plus ancien point *encore présent*, pas le tout premier historique jamais enregistré).

### `storage` — partiellement implémenté

`serialize_calibration()`/`deserialize_calibration()` (`lib/storage/calibration_storage.h`) : format fixe 17 octets (version + `v_air_mv` + `temp_c` + `timestamp_ms` + checksum FNV-1a 32 bits). `temp_c` à `NAN` se sérialise/désérialise sans traitement spécial — copie brute des bits, pas d'arithmétique dessus.

**Limite assumée, prise sans redemander confirmation** (« fait au mieux ») : seul le **dernier** point de calibration est persisté, pas les 10 entrées d'historique que garde `CalibrationTracker` en mémoire. Conséquence concrète : `is_declining()` ne voit que les calibrations faites depuis le dernier redémarrage, pas la dérive sur plusieurs semaines/mois que la fonctionnalité est censée capter à terme (cf. §3, "Suivi du vieillissement de cellule"). Persister l'historique complet demanderait d'exposer l'état interne de `CalibrationTracker` (déjà committé) — repoussé plutôt que fait à moitié en silence.

L'écriture flash réelle (`Preferences.putBytes()`/`getBytes()`) n'est pas incluse — même raison que le driver ADS1115 : non vérifiable sans matériel, reportée au câblage.

6 tests unitaires (`test/test_storage`) : aller-retour avec/sans température, version incompatible, checksum corrompu, flash vierge (`0xFF`).

### `printer` — implémenté

`build_label_tspl()` (`lib/printer/label.h`) : génère la commande TSPL complète pour l'étiquette (cas simple sans badge RFID — `rfid_badge` n'existe pas encore). Prend `LabelData` (O2% déjà arrondi, MOD déjà calculée, ppO2, date/heure déjà formatées en chaîne — pas de dépendance RTC ici) et écrit dans un buffer fourni par l'appelant, avec détection explicite de dépassement (retourne 0 plutôt que de tronquer silencieusement).

**Non vérifié visuellement** : les positions/tailles de police TSPL sont un point de départ raisonnable repris du format documenté dans `OXY-LD/README.md`, jamais imprimées pour de vrai (imprimante TSC TH240 non câblée). À ajuster une fois câblée.

4 tests unitaires (`test/test_printer`) : présence des champs attendus, MOD invalide affiché `--` plutôt que `-1 m`, formatage ppO2, buffer trop petit.

### `led_status` — implémenté

`led_status_for(SystemState)` (`lib/led_status/led_status.h`) : table pure état→couleur/clignotement, pas d'appel matériel (ni NeoPixel, ni gestion de luminosité — détails de rendu hors scope). Couleurs reprises du schéma documenté dans `OXY-LD/WIRING.md`. Scope volontairement limité à 4 états déterminables par la boucle de mesure de ce soir (`Splash`, `Stabilizing`, `Stable`, `Error`) — pas d'états pour badge/impression/historique/réglage horaire, ces modules n'existent pas encore.

5 tests unitaires (`test/test_led_status`), incluant une vérification que les 4 états ont des couleurs deux-à-deux distinctes (sinon deux états seraient visuellement indiscernables sur la LED).

### `buttons` — implémenté

`ButtonTracker` (`lib/buttons/button.h`) : même schéma temporel que `StabilityTracker` — l'appelant pousse l'état brut (`digitalRead()`) à chaque lecture, le module décide. **Différence de conception assumée** : l'appui long est détecté *pendant* le maintien, au moment où le seuil est franchi (pas au relâchement) — feedback immédiat, et évite qu'un relâchement après appui long ne déclenche en plus un `ShortPress` fantôme.

**Lecture TTP223 réelle non incluse** — boutons non câblés actuellement. `ButtonTracker` est prêt à être branché sur un `digitalRead()` dès que le câblage existe.

6 tests unitaires (`test/test_buttons`) : cycles indépendants (le chrono d'un appui ne doit pas hériter du timing du précédent), non-répétition du `LongPress` tant que maintenu, rollover `millis()`.

### `src/main.cpp` — première intégration, **vérifiée en fonctionnement réel**

Boucle : `ads1115_reader` → `GlitchFilter` → `voltage_to_o2_percent` → `StabilityTracker` (horloge `millis()`, RTC pas câblé) → `mod_lookup` → `display`/`led_status`. ppO2 fixe à 1.4 (pas de bouton câblé). Pas de compensation thermique (DS18B20 pas câblé).

**Simplification assumée pour ce premier smoke-test, PAS la vraie fonctionnalité** : auto-calibration naïve sur la toute première lecture valide au démarrage (`calibration.calibrate(filtered_mv, now)` si `!is_calibrated()`), pour obtenir un %O2 exploitable sans bouton pour déclencher une vraie calibration. Contrairement à l'auto-calibration d'OXY-LD (après arrêt prolongé, avec vérification de stabilité), ceci calibre sur une lecture unique non vérifiée stable — à remplacer par un vrai déclenchement (bouton) dès que câblé.

**Bug trouvé et corrigé avant tout flash** : une coquille `# vrai declenchement...` (dièse au lieu de `//`) dans un commentaire aurait été interprétée comme une directive préprocesseur invalide — repérée et corrigée avant la première compilation.

**Vérifié par log série réel** (cf. `ads1115_reader` ci-dessus pour les valeurs) : le pipeline complet tourne sans planter, produit des valeurs plausibles et cohérentes avec la table MOD validée. Diagnostic série ajouté spécifiquement cette nuit (`Serial.printf` 1×/s dans `loop()`) pour ne pas se contenter d'une absence d'erreur comme preuve de fonctionnement.

---

## Ce qui n'est pas encore tranché

- Portée du firmware de test/diagnostic (OXY-LD garde des `.cpp` de diagnostic séparés dans `src/` — à reproduire ou remplacer par les tests unitaires `test/`).
- Valeurs finales de `ε`/`α`/`min_settle_ms`/`sustained_ms` pour `stability` — revues une fois sur retour d'expérience (analyseur du commerce), mais toujours pas mesurées sur la vraie cellule de ce projet.
- Coefficient de compensation thermique (`coefficient_percent_per_c`) — à sourcer depuis la fiche technique de la cellule ou une mesure empirique.
- Constantes `calibration` : plage de tension plausible à l'air, seuil de remplacement cellule, seuil de dérive, capacité de l'historique (10 par défaut, arbitraire) — aucune sourcée depuis une fiche technique vérifiée.
- Écriture flash réelle de `storage` (`Preferences.putBytes`/`getBytes`) — à écrire au câblage.
- Persistance de l'historique complet de calibration (pas seulement le dernier point) — nécessite d'étendre `CalibrationTracker`.
- Persistance de l'historique d'analyses et des réglages (ppO2 verrouillée, nom de station...) — pas encore couverte par `storage`.
- **Auto-calibration naïve de `main.cpp`** (première lecture = référence) à remplacer par un vrai déclenchement (bouton `ButtonTracker`, déjà écrit) dès que les TTP223 sont câblés.
- **Intégration DS18B20/boutons dans `main.cpp`** — hors scope pour l'instant, non câblés (RTC câblé et intégré depuis).
- **Réglage de l'heure RTC** — pas de bouton pour le faire ; `lost_power()` est vérifié et loggé mais aucune action corrective possible actuellement.
- Positions/tailles de police TSPL (`printer`) — jamais vérifiées visuellement (imprimante non câblée), point de départ à ajuster.
- **`-std=gnu++17` ajouté à `[env:esp32s3]` mais n'élimine pas le warning `inline constexpr` d'`mod_table.h`**, même après un rebuild complet (`rm -rf .pio/build/esp32s3`) — probablement un flag `-std=` par défaut de la plateforme `espressif32` placé après le nôtre sur la ligne de commande GCC (le dernier `-std=` gagne). Cosmétique (warning, pas erreur, aucun effet observé sur le fonctionnement), pas creusé davantage cette nuit.

Ces points se trancheront à l'implémentation de chaque module, pas dans ce document.
