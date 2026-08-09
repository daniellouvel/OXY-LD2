# lib/

Bibliothèques privées PlatformIO, une par responsabilité matérielle ou fonctionnelle (voir la table dans [ARCHITECTURE.md](../ARCHITECTURE.md)). Les modules sont créés au fil de l'implémentation, pas d'avance.

- `mod_calc/` — MOD par table statique (O2 21-100%, ppO2 1.4/1.6) + valeur O2 canonique (arrondi entier vers le haut). Premier module implémenté, testé en `native`.
