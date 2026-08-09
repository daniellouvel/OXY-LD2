# test/

Tests unitaires PlatformIO (Unity, `pio test -e native`) pour la logique pure : calcul MOD, arrondi O2, compensation thermique, logique de calibration. Voir [ARCHITECTURE.md](../ARCHITECTURE.md#2-fiabilité). Les tests arrivent avec les modules qu'ils couvrent.

- `test_mod_calc/` — bornes de la table MOD, concordance avec la table papier du club, cohérence table/formule génératrice, arrondi O2.
- `test_stability/` — machine à états de stabilité : pente rapide/plate, anti-rebond sur bruit isolé, non-verrouillage après redérive, gap temporel, rollover `millis()`.
