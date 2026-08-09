# Architecture — OXY-LD2

Décisions structurantes pour la réécriture, en réponse directe aux limites observées sur OXY-LD. Trois axes, à poids égal : **architecture**, **fiabilité**, **précision**.

---

## 1. Architecture modulaire

OXY-LD concentre capteur, affichage, stockage, réseau, impression et RFID dans un seul `main.cpp` de 2510 lignes. OXY-LD2 découpe par responsabilité, chaque module vivant dans `lib/<nom>/` (convention PlatformIO — bibliothèque privée) avec une interface `.h` minimale et une seule responsabilité :

| Module (prévu) | Responsabilité |
|---|---|
| `o2_sensor` ✅ | Conversion tension → %O2, compensation thermique, filtrage — **implémenté**, voir ci-dessous |
| `ads1115_reader` ✅ | Lecture différentielle ADS1115 réelle — **implémenté et vérifié sur la carte**, voir ci-dessous |
| `calibration` ✅ | État de calibration + **méthode réelle (auto + manuelle) implémentée et vérifiée sur la carte**, voir ci-dessous |
| `stability` ✅ | Détection de stabilité par pente lissée, **sur la tension brute** (pas le %O2) depuis l'ajout de la calibration réelle — voir ci-dessous |
| `mod_calc` ✅ | MOD par table statique — **implémenté**, voir ci-dessous |
| `display` ✅ | Rendu écran TFT (ST7789) — **implémenté et confirmé visuellement**, voir ci-dessous. Police/format repris d'OXY-LD |
| `storage` ✅ | Sérialisation versionnée de la calibration + **écriture flash réelle (Preferences NVS) implémentée et vérifiée**, voir ci-dessous. Historique complet et réglages pas encore couverts |
| `rtc_clock` ✅ | Horodatage RTC PCF8563 — **implémenté et vérifié sur la carte**, voir ci-dessous |
| `rfid_badge` | Lecture badges PN532 (nom, licence FFESSM) |
| `printer` ✅ | Génération étiquette TSPL — **implémenté**, voir ci-dessous. Envoi UART reporté (imprimante non câblée) |
| `led_status` ✅ | Codes couleur LED d'état — **implémenté**, voir ci-dessous. Rendu NeoPixel réel non inclus |
| `buttons` ✅ | Détection appui court/long — **implémenté**, voir ci-dessous. Lecture TTP223 réelle non incluse (boutons non câblés) |
| `web_ui` ✅ | Point d'accès WiFi + serveur web, génération de pages — **implémenté**, voir ci-dessous. SSID confirmé actif, pages non testées via navigateur |

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

**Badge de stabilité — plusieurs itérations sur retour utilisateur** :
1. Agrandi (`FreeSansBold9pt7b` à `setTextSize(2)`, boîte 92×32) suite à "texte trop petit".
2. Ramené exactement au gabarit d'OXY-LD (`setTextSize(1)`, boîte 72×22) sur demande explicite — rejeté aussi, toujours "pas la bonne taille" **même en confirmant qu'il s'agissait bien du dernier flash**.
3. **Version actuelle** : `FreeSansBold12pt7b` nativement à `setTextSize(1)` (pas une police plus petite étirée), boîte élargie à 106×30. Hypothèse retenue après le refus de l'étape 1 malgré une taille objectivement plus grande : le problème n'était probablement pas la taille en elle-même mais le rendu pixelisé d'une police 9pt doublée par `setTextSize(2)`, plutôt qu'une police réellement plus grande avec son propre jeu de glyphes. Non encore confirmé par l'utilisateur au moment de ce commit.

**Fond blanc au lieu de noir** (retour utilisateur, persistant après deux tentatives de `fillScreen(BLACK)` explicite) : `invertDisplay(true)`, copié tel quel d'OXY-LD, est suspecté produire l'inverse sur ce panneau précis — les clones ST7789 varient d'un lot à l'autre, `HARDWARE.md` notait déjà ce réglage comme "à revalider sur le module réel" avant que la session ne le traite comme acquis. **Changé en `invertDisplay(false)` — confirmé corrigé par l'utilisateur** ("l'écran est bien noir").

**Rouge affiché bleu (et inversement)** : symptôme classique d'ordre de canaux couleur inversé (RGB vs BGR) sur les clones ST7789, distinct du problème de fond (`invertDisplay`) et de miroir (`MADCTL_MV`) déjà corrigés séparément — les trois défauts sont indépendants et peuvent coexister sur un même clone. Corrigé en ajoutant le bit BGR (`0x08`) au `MADCTL` déjà envoyé pour le miroir (`ST77XX_MADCTL_MV | 0x08`) — pas de constante nommée dans cette version de la bibliothèque (seul `ST77XX_MADCTL_RGB=0x00` existe), bit ajouté d'après la disposition standard documentée pour la famille ST77xx. Non revérifié visuellement par l'utilisateur au moment de ce commit.

**Clignotement ralenti** : fenêtre 500ms → 800ms sur retour utilisateur ("un peu plus lent").

**Oscillation rapide de la valeur affichée entre deux entiers adjacents** (ex. 21%/22%) : lissage cosmétique ajouté dans `main.cpp` (`display_fo2_smoothed`, EMA α=0.15), **parallèle** à `fo2_raw` qui continue d'alimenter `stability.push_sample()` sans modification (même principe que `GlitchFilter` : ne pas doublement lisser le signal qui sert au calcul de pente). Vérifié par log série : réduit le bruit mais **n'élimine pas l'oscillation** quand la valeur brute oscille véritablement autour d'une frontière entière (`fo2≈21.00`, bruit capteur réel) — `stable=0` reste correct dans ce cas, le système refuse à raison de déclarer stable une valeur qui n'a pas convergé. Un lissage plus agressif réduirait le clignotement au prix d'un temps de réponse plus lent - compromis à trancher si le comportement actuel gêne en pratique.

**Badge de calibration (bas-droit) + retour visuel de tentative — ajouté suite à retour utilisateur** : "quand j'appuie sur calibration je ne vois rien ... il serait bien que l'on voit qu'une calibration est lancée et avoir une indication calibré en rouge ou en vert en bas à droite de l'écran". Deux ajouts distincts dans `show_measurement()` :
- **Badge persistant "CALIBRE"/"NON CAL"** (zone 200,208,120,30, vert/rouge, même technique de cache anti-scintillement que le badge de stabilité) — reflète `calibration.is_calibrated()` en continu, y compris dans `show_error()` (l'état d'erreur "0%" ne suffisait pas à indiquer *pourquoi* : pas calibré vs cellule hors plage).
- **Message temporaire "CALIBRATION OK"/"CALIBRATION ECHEC"** (`Display::notify_calibration_attempt()`, remplace la bande MOD/ppO2 pendant 3s) déclenché depuis `main.cpp` uniquement pour une demande **manuelle** (bouton web `/plongee`), dans les trois issues possibles : succès, refus pour tension hors plage plausible, refus pour cellule pas encore stable (probablement le cas le plus fréquent, et celui qui expliquait le silence rapporté — la demande était déjà silencieusement ignorée avant cet ajout si la cellule n'était pas stable au moment du clic). L'auto-calibration reste silencieuse à l'écran (événement de fond, pas initié par un geste devant l'écran).
- La bande MOD/ppO2 a été réduite (hauteur 80→58, s'arrête à y=208) pour laisser la place au nouveau badge sans chevauchement des deux caches de contenu.
- **Vérifié par log série** (flash du 2026-08-09, ~14h14) : `Calibration effectuee : 8.023 mV (manuelle)` apparaît dans le flux au moment exact d'un clic réel sur `/plongee`, confirmant que la demande atteint bien `loop()` et déclenche le nouveau chemin de retour. **Rendu visuel réel du badge/message non confirmé** (pas d'accès visuel direct à l'écran depuis cette session) — à confirmer par l'utilisateur.

### `rtc_clock` — implémenté et **vérifié sur la carte**

RTC câblé en cours de session (annoncé par l'utilisateur, pas câblé la veille). `RtcClock` (`lib/rtc_clock/`) : wrapper fin autour de `RTC_PCF8563` (RTClib), même raison de séparation que `ads1115_reader`/`display` — module matériel, pas de logique pure à tester en `native`. `now()`/`lostPower()` de RTClib ne sont pas const-qualifiées (accès I2C) : `RtcClock::now()`/`lost_power()` non plus, par cohérence — piège évité avant compilation en vérifiant les signatures réelles plutôt que de deviner.

Horloge affichée en haut à gauche (`Display::show_clock()`, zone 0,0,90,26 reprise d'OXY-LD), même cache anti-scintillement que le reste (`prev_clock_text_`). `lost_power()` vérifié au démarrage (pile CR2032 absente/HS → heure non fiable) et loggé, sans action corrective possible pour l'instant (pas de bouton pour régler l'heure).

**Vérifié par log série** : `rtc_ok=1 clock=11:34` — RTC détecté à l'adresse I2C 0x51, heure lue plausible (correspond à l'heure réelle de la session). `RTC_PCF8563.cpp` déjà confirmé présent dans `RTClib @ 2.1.4` lors du premier `pio run -e esp32s3` de cette session — l'intégration n'a donc pas eu de surprise côté bibliothèque.

### `calibration` — implémenté

`CalibrationTracker` : tension à l'air + température **optionnelle** (sentinelle `NAN`, cohérent avec le reste du code — pas `std::optional`, portabilité ESP32/Arduino incertaine). Le DS18B20 étant explicitement optionnel dans HARDWARE.md, la température ne peut pas être un paramètre obligatoire. `has_temperature()` distingue "pas calibré" de "calibré sans sonde".

Historique en buffer circulaire à capacité fixe (10, pas de valeur validée — arbitraire mais raisonnable, ajustable) pour détecter une **dérive progressive** (`is_declining()`, compare la calibration la plus ancienne *encore en mémoire* à la plus récente) en plus du seuil dur (`is_cell_aging()`). Aucune des constantes de sécurité (plage de tension plausible, seuil de remplacement ~7 mV, seuil de dérive) n'a de valeur par défaut codée en dur — toutes passées en paramètre par l'appelant, même principe que pour le coefficient thermique et la table MOD.

11 tests unitaires (`test/test_calibration`), incluant un test dédié à l'éviction du buffer circulaire (vérifie que `is_declining()` compare bien contre le plus ancien point *encore présent*, pas le tout premier historique jamais enregistré).

### Méthode de calibration réelle — conçue avec l'utilisateur, implémentée et vérifiée

Demande explicite : calibration possible depuis l'interface web **et** automatique au démarrage, avec un moyen d'éviter qu'un redémarrage rapide (gaz résiduel dans le circuit) ne déclenche une calibration invalide. Recherche faite dans le code réel d'OXY-LD (`g_autoCalibHours`, défaut 24h, armé au boot si assez de temps écoulé depuis la dernière calibration RTC, déclenché seulement une fois stable) avant de concevoir la version OXY-LD2, plutôt que d'improviser.

**Trois garde-fous cumulatifs, combinant les deux propositions de l'utilisateur + une troisième ajoutée** :
1. **Temps de repos (idée utilisateur, validée par l'usage réel d'OXY-LD)** — 24h par défaut entre deux auto-calibrations. Protège du redémarrage rapide.
2. **Plage de tension plausible (idée utilisateur)** — déjà portée par `CalibrationTracker` (`v_air_min_mv`/`v_air_max_mv`), maintenant correctement branchée à la fois sur l'auto-calibration et la calibration manuelle. Protège du mauvais gaz resté branché même après une longue pause.
3. **Stabilité (ajout)** — ne calibrer que si `stability.is_stable()`, jamais sur une lecture instantanée.

**Asymétrie délibérée entre auto et manuel** (position de l'assistant, adoptée par l'utilisateur) : le temps de repos ne s'applique qu'à l'auto-calibration. Une calibration manuelle demandée par l'encadrant depuis `/plongee` n'est PAS soumise au repos — c'est une décision humaine explicite, pas un déclenchement automatique à protéger contre lui-même — mais reste soumise aux deux autres garde-fous (plage plausible, stabilité). Cohérent avec OXY-LD, dont le handler web `/calibrate` rejette immédiatement (pas de mise en file d'attente) si `!g_isStable` — même choix repris ici : la demande manuelle est un essai unique, silencieusement ignorée si non stable au moment du clic (texte affiché sur `/plongee` pour prévenir l'utilisateur), pas mise en attente comme l'auto-calibration.

**Prérequis technique découvert en concevant la fonctionnalité** : "temps depuis la dernière calibration" n'a de sens que si la calibration **survit à un redémarrage**. Ça a entraîné deux changements structurants, pas prévus au départ :
- **Écriture flash réelle** (`Preferences`, namespace `"oxyld2"`, clé `"cal"`) — chargée au boot, réutilise directement `serialize_calibration()`/`deserialize_calibration()` déjà écrits et testés hier soir. Si aucune calibration valide n'est trouvée en flash (première mise en service, ou données corrompues), **pas d'auto-armement** — la toute première calibration doit être manuelle (même comportement qu'OXY-LD : `g_autoCalibHours` n'agit que si `g_calibDateValid` est déjà vrai).
- **`calibration` passe sur l'horloge RTC** (`rtc.unix_time()`, secondes Unix) au lieu de `millis()`, qui repart à zéro à chaque redémarrage et rendrait "temps écoulé" toujours nul. `stability` reste sur `millis()` (pas besoin de survivre au reboot pour du court terme) — deux horloges différentes, choix délibéré et documenté dans le code, pas une incohérence.

**Effet de bord important, découvert en implémentant, pas anticipé au départ** : convertir une tension brute en %O2 nécessite déjà une calibration (`voltage_to_o2_percent`) — donc si `stability` avait continué à recevoir le %O2 calculé, il aurait été impossible de détecter la stabilité *avant* la toute première calibration (dépendance circulaire : pas de stabilité sans calibration, pas de calibration sans stabilité). **`stability.push_sample()` reçoit maintenant la tension brute (`filtered_mv`) directement**, pas le %O2 — %O2 = k·mV étant linéaire, la détection de pente donne le même résultat qualitatif (mêmes passages à zéro de la dérivée), seul le seuil ε change d'unité (mV/s au lieu de %O2/s). Ça rend aussi `stability` utilisable même quand rien n'est encore calibré, ce qui est correct : "le signal a-t-il fini de bouger" ne devrait jamais avoir dépendu d'une calibration potentiellement fausse.

**Seuil ε recalibré sur une vraie observation, en trois passes** :
1. Conversion naïve de l'ancien seuil %O2/s (0.02) vers mV/s par un facteur supposé → 0.01 mV/s, **jamais atteignable en pratique** (bruit ambiant réel observé entre 0.02 et 0.4 mV/s en continu).
2. Remonté à 0.5 mV/s, ancré sur le bruit réellement mesuré — `stable=1` atteint, mais **retour utilisateur : "la stabilisation ne tient pas, il doit y avoir de fluctuation"**, confirmé dans les logs (passe à `stable=1` quelques secondes puis retombe). Le seuil seul, collé au plafond de bruit observé, ne laissait pas assez de marge.
3. **Deux ajustements combinés** : `α` (lissage EMA de la pente) 0.3→0.1 — réduit la sensibilité au bruit instantané en lissant le signal lui-même, pas seulement en déplaçant le seuil — et seuil 0.5→0.6 mV/s pour une vraie marge. **`stable=1` confirmé tenir en continu sur 25 s d'observation directe**, sans un seul retour à faux.

Toujours pas vérifié pour discriminer un vrai changement de gaz du bruit ambiant — nécessiterait de souffler/couvrir la cellule physiquement, impossible à distance.

**Calibration manuelle confirmée fonctionnelle en conditions réelles** : `cal_v=8.063` observé dans les logs après ce correctif, alors que `auto_armed=0` (impossible que ce soit l'auto-calibration, jamais armée sur ce premier boot sans calibration préalable en flash) — signe qu'une calibration manuelle via le bouton `/plongee` a réussi, déclenchée par l'utilisateur pendant les tests. Première confirmation que le mécanisme complet (pas seulement la détection de stabilité) fonctionne de bout en bout sur la carte réelle.

**Bug trouvé suite au retour utilisateur ("j'ai toujours état non calibré")** : `handle_plongee()` construisait `DiveStatus` sans jamais copier `g_state.calibrated` — le champ restait à sa valeur par défaut (`false`) quelle que soit la vraie calibration, donc la page `/plongee` affichait "non calibré" en permanence alors que le badge de l'écran physique (qui lit `calibration.is_calibrated()` directement, pas via `g_state`) était correct depuis le début. Deux chemins de lecture distincts pour la même donnée, un seul avait été câblé. Corrigé (`d.calibrated = g_state.calibrated;`).

**Affichage de l'état de calibration sur `/plongee` allégé** (retour utilisateur : "j'aimerais que l'info calibré soit plus discrète") : suppression du paragraphe pleine taille ("Etat : calibré/non calibré") au profit d'un badge en ligne, plus petit et non gras (`.cal-status`, `.8rem`, `font-weight:normal`), accolé directement au bouton "Calibrer maintenant" plutôt qu'affiché comme une section à part entière.

**Vérifié par log série** : premier boot sans calibration flash → `"Aucune calibration valide en flash - calibration manuelle requise"` (pas d'auto-armement, conforme). Diagnostic ajouté spécifiquement pour rester visible même en état non calibré (`is_fo2_valid()` échoue tant que non calibré, ce qui coupait le log de diagnostic existant avant ce correctif — bug trouvé et corrigé dans la foulée). **Calibration manuelle confirmée déclenchée avec succès** par l'utilisateur via `/plongee` (cf. section seuil ε ci-dessus). **Toujours non vérifié** : l'auto-calibration au démarrage après le délai de repos (nécessiterait d'attendre 24h ou de manipuler l'horloge RTC).

**Temps de repos rendu configurable** (retour utilisateur : "il faudrait pouvoir changer le temps de recalibration automatique") — la durée de 24h était une `constexpr` codée en dur. Devenue `g_rest_seconds` (variable, valeur par défaut 24h identique), réglable via un formulaire sur `/plongee` (`POST /rest`, borné 1-500h côté serveur), persistée en flash (`Preferences`, clé `"rest_h"`, chargée au boot comme la calibration elle-même).

Changement structurant découvert en implémentant, pas anticipé : la décision d'armement (`auto_cal_armed`) n'était évaluée **qu'une seule fois au boot** dans `setup()`. Deux problèmes avec ça une fois le réglage rendu dynamique : (1) changer la valeur depuis le web n'aurait eu d'effet qu'au **prochain redémarrage**, pas immédiatement ; (2) un appareil qui tourne en continu sans jamais redémarrer ne se réarmerait **jamais**, même après des mois — bug latent indépendant de cette fonctionnalité, révélé en la construisant. Extrait dans `maybe_arm_auto_calibration()`, appelée une fois au boot (pour le log initial) **et** à chaque tour de `loop()` (coût négligeable : une comparaison entière, pas d'accès I2C) — journalise uniquement au moment où l'état bascule à armé, pas à chaque appel, pour ne pas noyer le log série (10-20 Hz).

**Vérifié par log série** au flash : `Temps de repos avant auto-calibration : 24 h` puis `Calibration recente (0 h) - pas d'auto-calibration avant 24 h (repos, modifiable via /plongee)` au boot juste après une calibration manuelle — comportement attendu. **Non vérifié** : qu'une modification effective depuis `/rest` est bien prise en compte sans redémarrage (nécessiterait d'envoyer la requête depuis un appareil connecté au point d'accès, pas fait depuis cette session).

### `storage` — écriture flash réelle implémentée

`serialize_calibration()`/`deserialize_calibration()` (`lib/storage/calibration_storage.h`) : format fixe 17 octets (version + `v_air_mv` + `temp_c` + `timestamp_ms` + checksum FNV-1a 32 bits). `temp_c` à `NAN` se sérialise/désérialise sans traitement spécial — copie brute des bits, pas d'arithmétique dessus.

**Écriture flash réelle** (`Preferences`, cf. section calibration ci-dessus) implémentée en réponse directe à la demande de calibration au démarrage — le report initial ("non vérifiable sans matériel") ne tenait plus une fois le besoin fonctionnel concret identifié.

**Limite toujours assumée** : seul le **dernier** point de calibration est persisté, pas les 10 entrées d'historique que garde `CalibrationTracker` en mémoire — `is_declining()` ne voit donc que les calibrations faites depuis le dernier redémarrage, pas la dérive sur plusieurs semaines/mois. Persister l'historique complet demanderait d'exposer l'état interne de `CalibrationTracker` (déjà committé et testé) — toujours repoussé, pas fait à moitié en silence.

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

**ppO2 n'est plus une constante** : `constexpr float kPpo2Setpoint` remplacé par `SharedState::ppo2_setpoint` (mutable, protégé par mutex), réglable depuis la page web `/plongee` — cf. `web_ui` ci-dessous.

### `web_ui` — implémenté, SSID confirmé actif

Demande explicite : configuration entièrement via serveur web accessible à l'encadrant, plusieurs pages (matériel / paramètres de plongée ppO2 / tables). OXY-LD a déjà exactement ce système (point d'accès WiFi + `ESPAsyncWebServer`, JSON pour le polling, mutex FreeRTOS pour l'accès concurrent à l'état partagé) — repris comme référence plutôt que réinventé.

**Découpage pur/matériel, même principe que les modules précédents** : `lib/web_ui/web_content.h` génère le HTML/JSON (chaînes `std::string`, aucune dépendance `WiFi`/`AsyncWebServer`) à partir de structs `HardwareStatus`/`DiveStatus` simples — testable en `native`. Le câblage réel (`WiFi.softAP`, routes `AsyncWebServer`, mutex) vit dans `src/main.cpp`, comme chez OXY-LD qui a la même contrainte d'accès à l'état partagé.

**Pages** (décidées avec l'utilisateur — page matériel = statut diagnostic seul pour l'instant, pas d'auth HTTP, le mot de passe WiFi suffit) :
- `/materiel` — ADS1115 détecté ?, RTC détecté ? + heure, dernière tension lue
- `/plongee` — sélecteur ppO2 (1.4/1.6, `POST /ppo2`) + mesure courante (O2%, MOD, stabilité)
- `/tables` — table MOD complète, **réutilise directement les données de `mod_table.h`** — pas de nouvelle source de vérité, la même table déjà validée contre la table du club

**Écart déliberé par rapport à OXY-LD, documenté et assumé** : OXY-LD appelle `rtc.now()` directement depuis un handler web (après libération du mutex d'état, mais l'accès I2C lui-même n'est pas protégé). Risque réel : `AsyncWebServer` exécute ses handlers sur sa propre tâche FreeRTOS, potentiellement concurrente avec `loop()` — deux accès I2C concurrents (un depuis `loop()`, un depuis un handler web) pourraient corrompre une transaction sur le bus. Ici, **seul `loop()` touche le matériel I2C** ; les handlers web ne lisent jamais `ads`/`rtc` directement, uniquement le `SharedState` mis à jour par `loop()` à chaque itération sous mutex.

**Vérifié** : le SSID `OXY-LD2` (WPA2, mot de passe `plongee24`) est confirmé actif par scan WiFi (`netsh wlan show networks`, sans rejoindre le réseau). Un PC connecté au point d'accès **charge correctement les pages**. Le firmware compile, flashe, et le pipeline de mesure continue de tourner normalement avec le mutex en place (`ppo2=1.6` visible dans le diagnostic série).

**Bug trouvé par l'utilisateur, en trois temps** : téléphone connecté au SSID `OXY-LD2` mais page inaccessible, alors qu'un PC y arrivait sans souci sur le même réseau. Un PC n'a généralement pas ce problème car son système d'exploitation ne fait pas ce type de vérification bloquante de connectivité avant d'autoriser la navigation manuelle.
1. Première hypothèse (insuffisante) : routes HTTP de contournement de la vérification de connectivité (`generate_204`, `hotspot-detect.html`, `connecttest.txt`, `ncsi.txt`) qu'OXY-LD utilise déjà pour le même symptôme. **N'a pas suffi** ("tj rien sur mon tel").
2. Deuxième hypothèse (insuffisante) : `DNSServer` répondant à tous les noms de domaine avec l'IP de la carte (`dns_server.start(53, "*", ...)`, `processNextRequest()` en tout début de `loop()`), nécessaire car la vérification de connectivité résout d'abord un nom de domaine par DNS avant la requête HTTP. **Toujours pas suffi** ("tj pas ça fonctionner, sur OXY-LD [ça marche]" — confirmation que le vrai OXY-LD fonctionne sur téléphone).
3. **Cause réelle, trouvée en comparant la séquence de démarrage AP ligne par ligne avec OXY-LD** : il manquait `WiFi.mode(WIFI_AP)` explicite, `WiFi.softAPConfig(ip, passerelle, masque)` avant `softAP()` (la passerelle DHCP fait partie de ce qu'un téléphone évalue pour juger si un réseau a "Internet"), les paramètres canal/visibilité/max-connexions de `softAP()`, et un `delay(200)` entre `softAP()` et le démarrage du DNS ("laisse le temps à l'interface AP d'être prête", commentaire OXY-LD conservé). Repris à l'identique. **Pas encore reconfirmé sur téléphone au moment de ce commit.**

8 tests unitaires (`test/test_web_ui`), incluant un piège de test découvert et corrigé : le CSS `.status-bad{...}`/`.status-ok{...}` étant toujours présent via `page_shell()`, chercher juste `"status-bad"` le trouve toujours — corrigé en cherchant `class="status-bad"` (usage réel), pas juste le nom de la classe.

**Refonte visuelle** (retour utilisateur : "un design des pages web plus pro") — `kStyle` réécrit entièrement : variables CSS pour une palette cohérente (fond quasi-noir à dominante bleue, accent cyan, teintes dédiées ok/attente/erreur), sections regroupées en cartes (`.card`), statuts affichés en pastilles colorées (`.pill`) plutôt qu'en texte gras brut, police système (`-apple-system`/`Segoe UI`/Roboto, pas de webfont externe — la carte n'a de toute façon pas d'accès réseau sortant utile ici), navigation en onglets avec l'onglet courant mis en évidence (comparaison du titre de page dans `page_shell()`). Aucune ressource externe, tout reste auto-suffisant dans le firmware comme avant. Le contenu textuel/les attributs vérifiés par les tests (`value="1.4" selected`, `status-ok">calibre`, `action="/calibrate"`, etc.) sont restés strictement identiques — seule l'habillage visuel autour a changé, les 8 tests existants passent sans modification de leurs assertions.

**Itérations successives sur le badge de calibration**, retour utilisateur à chaque étape : bug réel trouvé (`handle_plongee()` ne copiait jamais `g_state.calibrated`, page bloquée sur "non calibré" en permanence, cf. section `calibration` ci-dessus) → badge rendu discret sur demande (police réduite, `.cal-status`) → constat qu'il chevauchait le bouton "Calibrer maintenant" (span inline dans le même flux) → passé en `display:block` sur sa propre ligne → demande finale de reprendre exactement le même gabarit que la ligne "Stabilité" (`<p>Etat : <span class="status-ok/bad">...</span></p>`, classe `.cal-status` custom abandonnée). Chaque étape flashée et vérifiée par tests + log série avant la suivante.

**Temps de repos configurable ajouté sur `/plongee`** (`POST /rest`, cf. section calibration ci-dessus) : nouveau champ `DiveStatus.rest_hours`, formulaire numérique borné 1-500h, même style de carte que le reste de la page.

---

## Ce qui n'est pas encore tranché

- Portée du firmware de test/diagnostic (OXY-LD garde des `.cpp` de diagnostic séparés dans `src/` — à reproduire ou remplacer par les tests unitaires `test/`).
- Valeurs finales de `α`/`min_settle_ms`/`sustained_ms` pour `stability` — `ε` recalibré une fois sur du bruit réellement observé (0.5 mV/s), mais toujours pas vérifié pour distinguer un vrai changement de gaz du bruit ambiant (nécessite un stimulus physique réel).
- Coefficient de compensation thermique (`coefficient_percent_per_c`) — à sourcer depuis la fiche technique de la cellule ou une mesure empirique.
- Constantes `calibration` : plage de tension plausible à l'air (5-15 mV, non vérifiée), seuil de remplacement cellule, seuil de dérive, capacité de l'historique (10 par défaut, arbitraire) — aucune sourcée depuis une fiche technique vérifiée.
- Durée du temps de repos auto-calibration (24h) — reprise d'OXY-LD, jamais testée en conditions réelles sur ce projet (nécessiterait d'attendre 24h ou de manipuler l'horloge RTC).
- Persistance de l'historique complet de calibration (pas seulement le dernier point) — nécessite d'étendre `CalibrationTracker`.
- Persistance de l'historique d'analyses et des réglages (ppO2 verrouillée, nom de station...) — pas encore couverte par `storage`.
- **Auto-calibration au démarrage après le délai de repos, non vérifiée** — calibration manuelle confirmée fonctionnelle, mais l'auto (24h d'attente) pas encore observée en conditions réelles.
- **Intégration DS18B20/boutons dans `main.cpp`** — hors scope pour l'instant, non câblés (RTC câblé et intégré depuis).
- **Réglage de l'heure RTC** — pas de bouton pour le faire ; `lost_power()` est vérifié et loggé mais aucune action corrective possible actuellement.
- Positions/tailles de police TSPL (`printer`) — jamais vérifiées visuellement (imprimante non câblée), point de départ à ajuster.
- **`-std=gnu++17` ajouté à `[env:esp32s3]` mais n'élimine pas le warning `inline constexpr` d'`mod_table.h`**, même après un rebuild complet (`rm -rf .pio/build/esp32s3`) — probablement un flag `-std=` par défaut de la plateforme `espressif32` placé après le nôtre sur la ligne de commande GCC (le dernier `-std=` gagne). Cosmétique (warning, pas erreur, aucun effet observé sur le fonctionnement), pas creusé davantage cette nuit.

Ces points se trancheront à l'implémentation de chaque module, pas dans ce document.
