# Architecture — OXY-LD2

Décisions structurantes pour la réécriture, en réponse directe aux limites observées sur OXY-LD. Trois axes, à poids égal : **architecture**, **fiabilité**, **précision**.

---

## 1. Architecture modulaire

OXY-LD concentre capteur, affichage, stockage, réseau, impression et RFID dans un seul `main.cpp` de 2510 lignes. OXY-LD2 découpe par responsabilité, chaque module vivant dans `lib/<nom>/` (convention PlatformIO — bibliothèque privée) avec une interface `.h` minimale et une seule responsabilité :

| Module (prévu) | Responsabilité |
|---|---|
| `o2_sensor` | Lecture ADS1115 différentielle, conversion tension → %O2, filtrage |
| `calibration` | État de calibration (air, cellule), persistance, détection de vieillissement |
| `mod_calc` | Calcul MOD — logique pure, sans dépendance matérielle |
| `display` | Rendu écran TFT (ST7789) |
| `storage` | Persistance (calibration, historique, réglages) |
| `rtc_clock` | Horodatage RTC |
| `rfid_badge` | Lecture badges PN532 (nom, licence FFESSM) |
| `printer` | Génération étiquette TSPL, envoi UART |
| `led_status` | Codes couleur LED d'état |
| `buttons` | Lecture boutons TTP223, détection appui court/long |
| `web_ui` | Point d'accès WiFi + serveur web |

Chaque module ne connaît que son propre matériel — `o2_sensor` ignore l'existence de l'écran, `display` ignore l'ADS1115. Le fichier `src/main.cpp` orchestre, sans logique métier propre.

**Ce qui est pur (sans dépendance Arduino/matériel) doit le rester** : `mod_calc`, la logique d'arrondi, le calcul de compensation thermique. C'est ce qui rend possible le point 2.

## 2. Fiabilité

Bugs déjà rencontrés sur OXY-LD à traiter structurellement plutôt qu'au cas par cas :

- **Corruption EEPROM par décalage de champs** (fixée en urgence sur OXY-LD en déplaçant des champs à une adresse ≥1300) → sur OXY-LD2, la persistance passe par un stockage structuré avec **versionnage de schéma explicite** (un champ `version` en tête de struct, migration ou reset propre si la version ne correspond pas), au lieu d'offsets bruts codés en dur.
- **Choix de pin invalidé a posteriori** (LED sur une strapping pin, déplacée deux fois) → la table de brochage vit à un seul endroit ([HARDWARE.md](HARDWARE.md)), vérifiée contre la documentation strapping-pin de l'ESP32-S3 *avant* implémentation, pas après un premier essai raté.
- **Tests unitaires PlatformIO (Unity, environnement `native`)** pour toute la logique pure : calcul MOD, conversion tension→%O2, arrondi, compensation thermique, logique de calibration. Ces modules ne touchent pas au matériel — testables sans carte branchée.
- **Watchdog matériel** activé, pour repartir proprement d'un état cohérent en cas de blocage plutôt que de rester figé.

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

Ces points se trancheront à l'implémentation de chaque module, pas dans ce document.
