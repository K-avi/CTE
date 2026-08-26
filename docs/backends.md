# Documentation des Backends du Moteur CTE (Card Table Engine)

Le moteur **CTE** dispose d'une architecture multi-backend abstraite via l'interface [`s_cte_engine_backend`](file:///home/ivan/Files/projets/cte/include/engine.h). Cette architecture permet d'isoler l'IA (MinMax, Alpha-Beta, MCTS) de la représentation mémoire et de la génération des coups.

---

## 1. Vue d'Ensemble & Comparatif des Backends

| Backend | Identifiant | Empreinte RAM | Débit Réel (`-O3`) | Cycles CPU (@ 2.2 GHz) | Rôle Principal |
|---|:---:|:---:|:---:|:---:|---|
| **Array Backend** | `CTE_BACKEND_ARRAY` | $0 \text{ Ko}$ | **3.09 Mcoups/s** | $3\,384 \text{ cycles}$ | **Oracle de Référence (Ground Truth)** |
| **Bitboard Dynamic** | `CTE_BACKEND_BITBOARD` | $0 \text{ Ko}$ | **0.80 Mcoups/s** | $13\,088 \text{ cycles}$ | **Embarqué / Mémoire Nulle** |
| **Bitboard 1D Tables** | `CTE_BACKEND_BITBOARD_TABLE` | $225 \text{ Ko}$ | **8.80 Mcoups/s** | **1\,190 cycles** | **Haute Performance (Défaut Moteur)** |

---

## 2. Description Détaillée des Backends

### A. Backend Array (`CTE_BACKEND_ARRAY`)
- **Fichiers :** [`src/move.c`](file:///home/ivan/Files/projets/cte/src/move.c), [`src/game.c`](file:///home/ivan/Files/projets/cte/src/game.c)
- **Principe :** 
  - Représentation classique des cartes sous forme de tableaux d'octets (`struct table`, `cards_on_table[16]`).
  - Génération des prises par partitionnement arithmétique et backtracking récursif (`is_exact_partition`).
  - Structure de coup à zéro allocation dynamique sur le tas (`uint8_t array[16]` direct).
- **Rôle dans le projet :**
  - Sert de **modèle de référence mathématique (Oracle)**.
  - Tous les backends optimisés sont continuellement validés contre ce backend par fuzzing différentiel exhaustif sur des dizaines de milliers de positions aléatoires (tests `BT1` à `BT7`).

---

### B. Backend Bitboard Dynamique (`CTE_BACKEND_BITBOARD`)
- **Fichiers :** [`src/backend_bitboard.c`](file:///home/ivan/Files/projets/cte/src/backend_bitboard.c), [`include/backend_bitboard.h`](file:///home/ivan/Files/projets/cte/include/backend_bitboard.h)
- **Principe :**
  - Représentation de la table et des mains sur des entiers non-signés 64-bits (`uint64_t table_bb`).
  - Énumération exhaustive des sous-ensembles par l'algorithme matériel **Carry-Rippler** :
    $$\text{sub} = (\text{sub} - 1) \ \& \ \text{table\_bb}$$
  - Ne nécessite **aucune table de précalcul en mémoire** ($0 \text{ Ko}$ en `.rodata` et RAM).
- **Générateurs disponibles :**
  - `bitboard_gen_all_moves_dynamic` : Génère les coups au format générique `struct s_cte_move_list`.
  - `bitboard_gen_all_compact_moves` : Version compacte émettant directement `(card_played, uint64_t capture_mask)` 100% sur la pile.

---

### C. Backend Bitboard 1D Pivot Tables (`CTE_BACKEND_BITBOARD_TABLE`)
- **Fichiers :** [`src/backend_bitboard.c`](file:///home/ivan/Files/projets/cte/src/backend_bitboard.c), [`src/bitboard_tables.c`](file:///home/ivan/Files/projets/cte/src/bitboard_tables.c), [`include/bitboard_tables.h`](file:///home/ivan/Files/projets/cte/include/bitboard_tables.h)
- **Générateur hors-ligne :** [`tools/bitboard_gen.py`](file:///home/ivan/Files/projets/cte/tools/bitboard_gen.py)
- **Principe & Structure :**
  - Contient $28\,855$ sous-masques précalculés représentant l'intégralité des combinaisons valides de Tablić pour les 52 cartes et les 15 valeurs cibles.
  - Stocké sous forme de tableau plat 1D continu (`g_pivot_subset_masks[28855]`) aligné à 64 octets.
  - Indexation 1D instantanée en mémoire L1 :
    $$\text{Index}(c, v) = c \times 15 + v$$
    avec tables de métadonnées `g_pivot_offsets[780]` et `g_pivot_counts[780]` (seulement $3 \text{ Ko}$).
- **Optimisations intégrées :**
  1. **Fast-Rejection Global en 1 Instruction CPU :** Filtre en 1 cycle `(table_bb & global_reach) == 0` si aucune carte en main ne peut capturer sur la table.
  2. **Parcours 1-Passe Global :** La table n'est scannée qu'une seule fois pour toute la main en ne ciblant que les valeurs de cartes actives dans `val_mask`.
  3. **Déroulage 4-Voies :** Tests d'inclusion `(table_bb & m) == m` déroulés par paquets de 4 masques.
- **Générateurs disponibles :**
  - `bitboard_gen_all_moves_table` : Adaptateur pour l'interface générique `s_cte_engine_backend` (7.22 Mcoups/s).
  - `bitboard_gen_all_compact_moves_table` : Générateur ultra-rapide 1-passe compact (8.80 Mcoups/s, 540 ns / position).

---

## 3. État des Tests et Validation Différentielle

Le projet dispose d'une suite de tests complète sous **AddressSanitizer** et **UndefinedBehaviorSanitizer** :

- **Tests Fonctionnels de Base (`make run-test`) :**
  - Tests **T1 à T26** : validation de la logique de jeu, minmax, distribution, calcul de score.
- **Tests Différentiels Bitboard (`make run-test-bitboard`) :**
  - **BT1 :** Intégrité mathématique des 28 855 masques précalculés et des masques de joignabilité.
  - **BT2 :** Prises tactiques critiques (prises triples, quadruples, As multiples) vérifiées sur les 3 backends.
  - **BT3 & BT4 :** Fuzzing différentiel 3-voies sur **10 000 configurations aléatoires** (134 966 coups comparés, **0 divergence**).
  - **BT5 :** Déroulement de 500 parties complètes multi-joueurs.
  - **BT6 :** Fuzzing différentiel du générateur compact dynamique (10 000 configurations).
  - **BT7 :** Fuzzing différentiel du générateur compact 1-passe Tables 1D (10 000 configurations).

---

## 4. Analyse des Profilings Matériels (`perf`) et Pistes Futures

Le profilage matériel par compteurs d'instructions et cycles sur microarchitecture Intel Skylake montre :
1. **Goulot des Tables 1D :** Sur une table moyenne (2 à 4 cartes), la table ne possède que 4 à 16 sous-ensembles réels. Le parcours séquentiel des ~200 masques théoriques précalculés dans `.rodata` génère des lectures mémoires inutiles pour les masques absents de la table.
2. **Piste d'optimisation directe :** L'évaluation bitwise directe des $2^N$ sous-masques réels via la LUT de somme 13-bits $8 \text{ Ko}$ (`g_suit_sum`) permet de passer sous les **180 cycles par position** (> 50 Mcoups/s) tout en restant 100% portable en C standard.
