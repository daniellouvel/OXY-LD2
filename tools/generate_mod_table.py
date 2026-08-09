#!/usr/bin/env python3
"""Genere les tables statiques MOD (lib/mod_calc/mod_table.cpp).

Methode : MOD(m) = floor(((ppO2 / (fO2/100)) - 1) * 10), troncature vers le bas
(jamais vers le haut - direction conservatrice, cf. ARCHITECTURE.md).

Une tolerance de 1e-9 est ajoutee avant la troncature pour compenser
l'imprecision de representation binaire des float (ex: 1.4/0.28 ne vaut pas
exactement 5.0, ce qui fait basculer un resultat exact comme 40.0 a 39 sans
cette correction). La tolerance est bien plus petite que l'ecart minimum
entre deux valeurs MOD atteignables pour un O2% entier : elle ne peut jamais
faire remonter une vraie valeur non-entiere au palier superieur.

Valide le 2026-08-09 : les 78 valeurs ppO2=1.6 (O2 22-99%) concordent
exactement avec la table papier utilisee par le club (F. Gentili /
sommeildesepaves.com - photo dans table.png, archive dans
docs/mod_table_review.html). Pas de reference independante pour ppO2=1.4
ni pour les lignes 21%/100% (absentes de cette table papier).

Usage : python3 tools/generate_mod_table.py
Affiche les deux tableaux C++ a coller dans lib/mod_calc/mod_table.cpp.
"""

import math

O2_MIN = 21
O2_MAX = 100
EPS = 1e-9


def mod_floor(fo2_percent: int, ppo2: float) -> int:
    mod_exact = ((ppo2 / (fo2_percent / 100.0)) - 1) * 10
    return math.floor(mod_exact + EPS)


def render_table(name: str, ppo2: float) -> str:
    values = [mod_floor(o2, ppo2) for o2 in range(O2_MIN, O2_MAX + 1)]
    rows = []
    for i in range(0, len(values), 10):
        chunk = values[i : i + 10]
        rows.append(
            "    " + ", ".join(f"{v:2d}" for v in chunk) + ","
            f"  // O2 {O2_MIN + i}-{O2_MIN + i + len(chunk) - 1}%"
        )
    body = "\n".join(rows)
    return f"const int8_t {name}[{len(values)}] = {{\n{body}\n}};"


if __name__ == "__main__":
    print(render_table("kModTable_PPO2_1_4", 1.4))
    print()
    print(render_table("kModTable_PPO2_1_6", 1.6))
