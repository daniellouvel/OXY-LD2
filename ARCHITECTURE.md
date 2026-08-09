# Architecture — OXY-LD2

Décisions structurantes pour la réécriture, en réponse directe aux limites observées sur OXY-LD. Trois axes, à poids égal : **architecture**, **fiabilité**, **précision**.

---

## 1. Architecture modulaire

OXY-LD concentre capteur, affichage, stockage, réseau, impression et RFID dans un seul `main.cpp` de 2510 lignes. OXY-LD2 découpe par responsabilité, chaque module vivant dans `lib/<nom>/` (convention PlatformIO — bibliothèque privée) avec une interface `.h` minimale et une seule responsabilité :

| Module (prévu) | Responsabilité |
|---|---|
| `o2_sensor` | Lecture ADS1115 différentielle, conversion tension → %O2, filtrage |
| `calibration` | État de calibration (air, cellule), persistance, détection de vieillissement |
| `stability` | Détection de stabilité (critère de pente `\|ΔV/Δt\| < ε`, cf. §2) |
| `mod_calc` ✅ | MOD par table statique — **implémenté**, voir ci-dessous |
| `display` | Rendu écran TFT (ST7789) — valeur O2 **clignotante tant que `stability.is_stable()` est faux, fixe une fois vrai** (cf. §2) |
| `storage` | Persistance (calibration, historique, réglages) |
| `rtc_clock` | Horodatage RTC |
| `rfid_badge` | Lecture badges PN532 (nom, licence FFESSM) |
| `printer` | Génération étiquette TSPL, envoi UART |
| `led_status` | Codes couleur LED d'état |
| `buttons` | Lecture boutons TTP223, détection appui court/long |
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

- **Corruption EEPROM par décalage de champs** (fixée en urgence sur OXY-LD en déplaçant des champs à une adresse ≥1300) → sur OXY-LD2, la persistance passe par un stockage structuré avec **versionnage de schéma explicite** (un champ `version` en tête de struct, migration ou reset propre si la version ne correspond pas), au lieu d'offsets bruts codés en dur.
- **Choix de pin invalidé a posteriori** (LED sur une strapping pin, déplacée deux fois) → la table de brochage vit à un seul endroit ([HARDWARE.md](HARDWARE.md)), vérifiée contre la documentation strapping-pin de l'ESP32-S3 *avant* implémentation, pas après un premier essai raté.
- **Tests unitaires PlatformIO (Unity, environnement `native`)** pour toute la logique pure : calcul MOD, conversion tension→%O2, arrondi, compensation thermique, logique de calibration. Ces modules ne touchent pas au matériel — testables sans carte branchée.
- **Watchdog matériel** activé, pour repartir proprement d'un état cohérent en cas de blocage plutôt que de rester figé.
- **Détection de stabilité par critère de pente** (`stability`, non implémenté) : au lieu du seul seuil min/max sur fenêtre glissante d'OXY-LD (15 lectures, 0.1 %), critère de pente lissée (EMA) `|d(%O2)/dt| < ε` — le taux de variation du signal s'approche de zéro quand la réaction chimique de la cellule est terminée, ce qui colle mieux à la réalité physique qu'un simple écart max−min qui peut s'annuler par hasard sur du bruit. Deux durées cumulatives : `min_settle_ms` (plancher dur depuis le dernier changement de gaz/calibration, avant même d'envisager la stabilité) et `sustained_ms` (la pente doit rester sous ε en continu avant de déclarer stable, anti-rebond contre un creux de bruit isolé).
  - Valeurs de départ initiales (60 s / 5 s) revues à la baisse après retour d'expérience : un analyseur Nitrox du commerce stabilise typiquement en **5-15 s**. Nouvelles valeurs de départ : `min_settle_ms = 5000`, `sustained_ms = 3000` (~8 s dans le meilleur cas, cohérent avec le bas de la fourchette observée). `ε` et `α` (lissage EMA) inchangés faute de donnée pour les ajuster. **Toujours non calibré sur la vraie cellule** — le retour d'expérience porte sur un analyseur commercial, pas sur cette cellule galvanique dans ce boîtier précis (flux d'air actif ou diffusion passive : à déterminer).
  - **Retour UI attendu (non implémenté)** : la valeur O2 affichée doit **clignoter** tant que `stability.is_stable()` est faux, et devenir **fixe** dès qu'il passe vrai — signal visuel direct que la mesure est validée. C'est une responsabilité de `display` (qui lit l'état de `stability` en entrée), pas de `stability` elle-même, qui reste une logique pure sans notion de rendu ni de rythme de clignotement. Reste à trancher à l'implémentation de `display` : période de clignotement, et si le comportement doit aussi se refléter sur l'étiquette imprimée ou seulement à l'écran.

## 3. Précision de mesure

La chaîne de mesure O2 est le cœur du projet — elle mérite plus d'attention que le firmware périphérique :

- **Mesure différentielle A0−A1** (déjà validée sur OXY-LD, conservée) — rejet du bruit secteur 50 Hz.
- **Filtrage explicite et documenté** (moyenne glissante ou médiane sur N échantillons — à trancher en implémentant `o2_sensor`, avec le compromis stabilité/réactivité écrit en commentaire).
- **Compensation thermique** via DS18B20 quantifiée et testée (le module `mod_calc`/`calibration` doit pouvoir être testé avec des valeurs de température connues et un résultat attendu).
- **Suivi du vieillissement de cellule** : la tension à l'air à chaque calibration est historisée, pas seulement la dernière valeur — permet de détecter une dérive avant que la cellule tombe sous le seuil de remplacement (~7 mV), plutôt que de le découvrir à la calibration suivante.

---

## Ce qui n'est pas encore tranché

- Filtrage exact du signal ADS1115 (moyenne vs médiane, taille de fenêtre).
- Format de persistance (Preferences/NVS natif ESP32 vs structure EEPROM versionnée maison).
- Portée du firmware de test/diagnostic (OXY-LD garde des `.cpp` de diagnostic séparés dans `src/` — à reproduire ou remplacer par les tests unitaires `test/`).
- Valeurs finales de `ε`/`α`/`min_settle_ms`/`sustained_ms` pour `stability` — revues une fois sur retour d'expérience (analyseur du commerce), mais toujours pas mesurées sur la vraie cellule de ce projet.

Ces points se trancheront à l'implémentation de chaque module, pas dans ce document.
