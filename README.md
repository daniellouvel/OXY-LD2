# OXY-LD2

**Analyseur fixe d'oxygène Nitrox — réécriture visant la fiabilité et la précision.**

Ce projet reprend le matériel documenté dans [OXY-LD](../OXY-LD) (station de paillasse ESP32-S3, alimentation secteur 220V AC) mais repart d'une base de code neuve. Objectif : un firmware modulaire, robuste et précis — pas une évolution incrémentale du `main.cpp` existant.

> **Statut : initialisation.** Le squelette du projet est en place, le firmware reste à écrire module par module.

---

## Pourquoi une réécriture

OXY-LD (2510 lignes dans un seul `main.cpp`) a accumulé, au fil de son développement, des classes de bugs révélatrices de limites structurelles : corruption EEPROM par décalage de champs, choix de pin invalidé a posteriori (strapping pin), incohérences de driver d'écran découvertes en cours de route. Rien de disqualifiant individuellement, mais le signe qu'une base plus modulaire et plus défensive vaut le coût de la réécriture.

Voir [ARCHITECTURE.md](ARCHITECTURE.md) pour les décisions structurantes.

## Matériel

Repris à l'identique d'OXY-LD. Voir [HARDWARE.md](HARDWARE.md) pour le BOM et le câblage de référence (dérivés de `OXY-LD/WIRING.md` et du schéma `Schematic_Analyseur_2026-08-06.pdf`), avec les points à reconfirmer sur le matériel réel avant implémentation.

## Structure du projet

```
OXY-LD2/
├── src/
│   └── main.cpp          Point d'entrée firmware
├── lib/                  Modules privés (un par responsabilité, à créer au fil du développement)
├── test/                 Tests unitaires PlatformIO (logique pure : calcul MOD, calibration, arrondi...)
├── platformio.ini         Configuration PlatformIO (ESP32-S3-WROOM-1 N16R2)
├── ARCHITECTURE.md        Décisions structurantes (modularité, fiabilité, précision)
├── HARDWARE.md            BOM + câblage de référence (repris d'OXY-LD)
└── README.md              Ce fichier
```

## Avertissements

> **⚠️ SÉCURITÉ PLONGÉE** : Ce projet est un outil d'aide à la préparation, **pas un équipement de sécurité certifié**. Toujours vérifier l'analyse avec un analyseur commercial avant une plongée.

## Licence

MIT.

## Auteur

Daniel Louvel — [@daniellouvel](https://github.com/daniellouvel)
