# Documentation des Backends du Moteur CTE (Card Table Engine)

Le moteur **CTE** dispose d'une architecture multi-backend abstraite via l'interface [`s_cte_engine_backend`](file:///home/ivan/Files/projets/cte/include/engine.h). Cette architecture isole le moteur de recherche et les frontends (CLI, TUI, GUI) de la représentation mémoire interne et des algorithmes de génération des coups.

---

## 1. Vue d'Ensemble des 2 Backends

| Backend | Identifiant | Empreinte RAM | Caractéristiques Clés | Rôle Principal |
|---|:---:|:---:|---|---|
| **Array Backend** | `CTE_BACKEND_ARRAY` | $0 \text{ Ko}$ | Tableaux d'octets classiques, backtracking récursif exact | **Oracle de Référence (Tests uniquement)** |
| **Bitboard SWAR** | `CTE_BACKEND_BITBOARD` | $7.1 \text{ Ko}$ | Motifs de rangs SWAR 4-bits, 100% L1-résident, 0 heap | **Backend de Production par Défaut** |

---

## 2. Description Détaillée des Backends

### A. Backend Array (`CTE_BACKEND_ARRAY`)
- **Fichiers :** [`src/move.c`](file:///home/ivan/Files/projets/cte/src/move.c), [`src/engine.c`](file:///home/ivan/Files/projets/cte/src/engine.c)
- **Principe :**
  - Représentation directe des cartes sous forme de tableaux d'octets (`struct table`, `cards_on_table[16]`).
  - Génération des prises par partitionnement arithmétique et backtracking récursif (`is_exact_partition`).
  - Implémentation mathématiquement transparente et triviale à vérifier manuellement.
- **Rôle dans le projet :**
  - Sert d'**oracle mathématique de référence (Ground Truth)**.
  - Réservé aux tests différentiels pour garantir l'absence totale de faux positifs ou de coups omis dans le moteur bitboard.

---

### B. Backend Bitboard SWAR (`CTE_BACKEND_BITBOARD`)
- **Fichiers :** [`src/backend_bitboard.c`](file:///home/ivan/Files/projets/cte/src/backend_bitboard.c), [`src/bitboard_rank_tables.c`](file:///home/ivan/Files/projets/cte/src/bitboard_rank_tables.c), [`include/backend_bitboard.h`](file:///home/ivan/Files/projets/cte/include/backend_bitboard.h), [`include/bitboard_rank_tables.h`](file:///home/ivan/Files/projets/cte/include/bitboard_rank_tables.h)
- **Principe :**
  - Représentation de l'état du jeu sous forme de masques 64-bits (`uint64_t table_bb`).
  - **Motifs de rangs compacts (SWAR) :** Les 13 rangs de cartes (As à Roi) sont encodés sur des champs de 4 bits avec bit de garde SWAR (`CTE_SWAR_GUARD_MASK`).
  - La détection de sous-ensembles compatibles s'effectue par une soustraction vectorielle SWAR en 1 seule instruction CPU.
  - L'expansion des couleurs s'appuie sur une petite table de lookup LUT ultra-compacte ($7.1 \text{ Ko}$ au total), restant 100% résidente dans le cache L1 de tous les processeurs modernes.
  - Déduplication rapide des prises multiples via filtre de Bloom 256 bits local sur la pile.
- **Générateurs disponibles :**
  - `bitboard_gen_all_moves_rank` : Adaptateur pour l'interface générique `s_cte_engine_backend` émettant une `struct s_cte_move_list`.
  - `bitboard_gen_all_compact_moves_rank` : Générateur ultra-performant 1-passe émettant des `s_cte_bitboard_move` compacts `(card_played, uint64_t capture_mask)` sans aucune allocation heap (100% pile/registres). Utilisé directement par le module d'évaluation minmax.

---

## 3. Validation Différentielle et Fuzzing

Le moteur est validé en continu sous **AddressSanitizer** (`-fsanitize=address`) et **UndefinedBehaviorSanitizer** (`-fsanitize=undefined`) :

- **Tests Fonctionnels Généraux (`make run-test`) :**
  - Tests **T1 à T26** : validation de la logique complète du jeu de Tablić (distribution, alternance des tours, décompte des points de levées, majorité, tablićs, contrats des interfaces backend).
- **Tests Différentiels Bitboard 2-Voies (`make run-test-bitboard`) :**
  - **BT1 :** Scénarios tactiques critiques (prises composées, As à valeur 1 ou 11) vérifiés entre Array et SWAR.
  - **BT2 :** Fuzzing différentiel exhaustif sur **10 000 configurations aléatoires** de tables et de mains :
    - Zéro faux positif (`is_legal` validé sur chaque coup).
    - Isomorphisme parfait des listes de coups (0 coup omis, 0 coup en trop).
  - **BT3 :** Validation différentielle du générateur compact SWAR contre l'oracle Array (10 000 configurations).
  - **BT4 :** Simulation de 500 manches complètes multi-joueurs sous le backend SWAR avec vérification de la conservation stricte des 52 cartes et des 22 points.
