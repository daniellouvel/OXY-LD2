# OXY-LD2

**Analyseur fixe d'oxygène Nitrox — réécriture visant la fiabilité et la précision.**

Ce projet reprend le matériel documenté dans [OXY-LD](../OXY-LD) (station de paillasse ESP32-S3, alimentation secteur 220V AC) mais repart d'une base de code neuve. Objectif : un firmware modulaire, robuste et précis — pas une évolution incrémentale du `main.cpp` existant.

> **Statut : en cours.** Cinq modules implémentés et testés (`mod_calc`, `stability`, `o2_sensor` — partie pure, `calibration`, `storage` — sérialisation calibration seule). Le reste du firmware reste à écrire module par module.

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
├── lib/                  Modules privés (un par responsabilité)
│   ├── mod_calc/          MOD par table statique + valeur O2 canonique
│   ├── stability/         Détection de stabilité (pente lissée EMA)
│   ├── o2_sensor/         Conversion tension→%O2, compensation thermique, filtre anti-glitch
│   ├── calibration/       État de calibration (air, température optionnelle), vieillissement cellule
│   └── storage/           Sérialisation versionnée de la calibration (checksum, détection flash vierge)
├── test/                 Tests unitaires PlatformIO (pio test -e native)
│   ├── test_mod_calc/
│   ├── test_stability/
│   ├── test_o2_sensor/
│   ├── test_calibration/
│   └── test_storage/
├── tools/
│   └── generate_mod_table.py   Génère les tables de lib/mod_calc/mod_table.cpp
├── docs/
│   └── mod_table_review.html   Revue et validation de la table MOD (vs table du club)
├── platformio.ini         Configuration PlatformIO (ESP32-S3-WROOM-1 N16R2)
├── ARCHITECTURE.md        Décisions structurantes (modularité, fiabilité, précision)
├── HARDWARE.md            BOM + câblage de référence (repris d'OXY-LD)
└── README.md              Ce fichier
```

## Tests

```
pio test -e native
```

Nécessite un compilateur hôte (g++/gcc) — pas installé par défaut sur Windows. Si absent : `scoop install gcc` (pas de droits admin requis).

## Avertissements

> **⚠️ SÉCURITÉ PLONGÉE** : Ce projet est un outil d'aide à la préparation, **pas un équipement de sécurité certifié**. Toujours vérifier l'analyse avec un analyseur commercial avant une plongée.

## Licence

MIT.

## Auteur

Daniel Louvel — [@daniellouvel](https://github.com/daniellouvel)
