# Analyse Critique du Backend Bitboard CTE

> Rapport d'audit technique — 2 sept 2026
> Périmètre : `backend_bitboard.c`, `bitboard_tables.{h,c}`, `move.c` (partition), `minmax.c` (search), `bench_backends.c`

---

## 1. Résumé des performances mesurées

Benchmarks sur 200 000 positions aléatoires réalistes (0–5 cartes table, 1–6 cartes main), `-O3 -march=native`, GCC :

| Backend                              | Temps / Pos | Débit (Mcoups/s) | Speedup vs Array |
|--------------------------------------|-------------|-------------------|------------------|
| Array (référence)                    | ~1500 ns    | ~3.2              | 1.00×            |
| Bitboard Dynamic (Carry-Rippler)     | ~5800 ns    | ~0.8              | 0.26× (régressé) |
| Bitboard 1D Pivot Tables             | ~680 ns     | ~7.0              | 2.2×             |
| Compact Dynamic (carry-rippler)      | ~6000 ns    | ~0.8              | 0.25× (régressé) |
| **Compact 1D Pivot Tables (1-Pass)** | **~550 ns** | **~8.6**          | **2.7×**         |

Compteurs perf (run complet) : IPC = 2.22, cache-miss = 17.7%, branch-miss = 56 M.

---

## 2. Points forts

### 2.1 Architecture & design

- **Abstraction backend propre** : `s_cte_engine_backend` permet de brancher n'importe quel moteur sans toucher au game loop. Bonne séparation des préoccupations.
- **`s_cte_pos` en 64 bytes** : snapshot compact qui tient dans une ligne L1. Choix structurant pour le minimax.
- **`s_cte_bitboard_move` = 16 bytes** : représentation zéro-allocation, pure stack. Idéal pour la recherche arborescente.
- **Pivot Inverted Index** : l'approche d'indexation 1D par (card_pivot, target_value) avec reachability mask est un design original et efficace. Le reachability check élimine des branches entières en 1 cycle (AND + test).

### 2.2 Qualités du code

- Pas de `malloc` dans le hot path du compact move generator.
- Tables pré-calculées alignées 64 bytes (`__attribute__((aligned(64)))`).
- Déroulement 4-way dans `collect_active_base_masks` — aide l'ILP.
- Validation croisée par fuzzing différentiel 3-voies (Array vs Dynamic vs Table).

---

## 3. Points faibles & problèmes identifiés

### 3.1 `is_exact_partition` — goulot d'étranglement O(2^n) avec malloc

C'est **le problème #1**. Cette fonction est appelée dans les chemins carry-rippler (dynamic et compact dynamic) pour chaque sous-ensemble de la table × chaque carte de la main.

```c
// move.c L106-107 — malloc à chaque appel
uint8_t *valid_base = malloc(num_masks * sizeof(uint8_t));
int8_t *memo = malloc(num_masks * sizeof(int8_t));
```

**Problèmes** :
- 2× `malloc` + 2× `free` par appel, dans une boucle potentiellement exponentielle.
- Allocation de `2^n` octets à chaque appel alors que n ≤ 16 max → le buffer pourrait être sur la stack.
- C'est la raison principale pour laquelle le Carry-Rippler est **4× plus lent que le backend Array** qu'il est censé remplacer.

**Impact** : explique directement le ratio 0.25× du compact dynamic.

### 3.2 `combine_disjoint_masks` — récursion + déduplication O(k²)

```c
// backend_bitboard.c L250-257 — scan linéaire pour dédupliquer
for(uint16_t k = 0; k < *out_count; k++){
    if(out_captures[k] == new_union){
        exists = true;
        break;
    }
}
```

- La déduplication est un scan linéaire O(k) à chaque insertion → O(k²) global.
- La récursion a une profondeur potentielle de `num_active` (jusqu'à 64) → stack pressure non négligeable.
- Pas de borne théorique documentée sur le nombre de combinaisons.

### 3.3 Stack pressure dans `bitboard_gen_all_compact_moves_table`

```c
uint64_t active_by_val[15][64];  // 7680 bytes
uint64_t cap_by_val[15][256];    // 30720 bytes
```

**~38 KB sur la stack** par appel. Ce n'est pas un bug, mais :
- Empêche un usage récursif (dans minimax par ex.)
- Risque de stack overflow sur des systèmes embarqués ou des threads avec stack réduite.
- Pollution du cache L1 (typiquement 32-48 KB).

### 3.4 Conversion adapter table→bitboard répétée

Les 4 fonctions `bitboard_adapter_gen_*` reconstruisent le `table_bb` à partir du `struct table` à chaque appel :

```c
for(uint8_t i = 0; i < table->nb_cards_on_table; i++){
    t_card c = table->cards_on_table[i];
    if(c < 52) table_bb |= (1ULL << c);
}
```

Dans le minimax (`pos_gen_moves`), c'est encore pire : le bitboard est **décompressé** en `struct table`, passé au backend, qui le **recompresse** en `uint64_t`. Round-trip inutile.

### 3.5 `compute_table_max_sum` — recalcul linéaire

La somme de la table est recalculée à chaque appel à `bitboard_gen_card_moves_dynamic` via un scan de bits. En minimax, cette somme pourrait être maintenue incrémentalement dans `s_cte_pos`.

### 3.6 `bitboard_tables.c` — 1.8 MB de données statiques

28 855 masques × 8 bytes = ~225 KB de données utiles, mais le fichier source fait 1.8 MB (commentaires, formatting). Ce n'est pas un problème de perf runtime, mais :
- Augmente le temps de compilation (~3-4s pour ce seul fichier).
- Un format binaire embarqué (`.bin` + `xxd -i` ou `mmap`) réduirait la compilation.

---

## 4. Pistes d'amélioration sérieuses

### 4.1 [CRITIQUE] Éliminer malloc dans `is_exact_partition`

Puisque `n ≤ 16` (et même `n ≤ 20` dans le guard), les buffers tiennent sur la stack :

```c
bool is_exact_partition(const uint8_t *cards, uint8_t n, uint8_t target_val){
    if(n == 0 || n > 16) return false;
    uint32_t num_masks = 1u << n;  // max 65536

    uint8_t valid_base[65536];  // 64 KB stack — OK si n ≤ 16
    int8_t  memo[65536];
    // ... reste identique, sans malloc/free
}
```

Ou mieux : réduire la borne à `n ≤ 12` (réaliste pour le Tablic) → 4 KB stack, 0 malloc.

**Gain attendu** : le Dynamic Carry-Rippler devrait passer de 0.25× à ~1.5-2× vs Array.

### 4.2 [IMPORTANT] Chemin direct bitboard dans le minimax

`pos_gen_moves` devrait appeler directement `bitboard_gen_all_compact_moves_table` au lieu de décompresser en `struct table` puis recompresser :

```c
t_cteerr pos_gen_moves(struct s_cte_move_list *moves, const s_cte_pos *pos){
    struct s_cte_hand hand;
    hand.size = 0;
    uint64_t h = pos->hand_bb[pos->current_player];
    while(h){ hand.array[hand.size++] = __builtin_ctzll(h); h &= h - 1; }

    s_cte_bitboard_move_list cpt;
    bitboard_gen_all_compact_moves_table(&cpt, pos->table_bb, &hand);
    // convertir cpt → moves
}
```

**Gain attendu** : supprime le round-trip bitboard→array→bitboard, améliore la localité.

### 4.3 [IMPORTANT] `pos_apply_move` natif bitboard

Actuellement `pos_apply_move` itère sur `cards_picked.array[]` pour retirer les bits un par un. Si le move contient directement le `capture_mask` (format `s_cte_bitboard_move`), l'application devient :

```c
next.table_bb &= ~capture_mask;  // 1 instruction
```

Cela nécessite de propager le `s_cte_bitboard_move` dans le search au lieu du `s_cte_move`. Impact important pour le minimax profond.

### 4.4 [MOYEN] Déduplication par bitset dans `combine_disjoint_masks`

Remplacer le scan linéaire par un hash set simple ou, si le nombre de captures est borné, utiliser un tableau de bits indexé par un hash de la mask :

```c
// Hash rapide pour dédup — 64-bit → 12-bit
#define DEDUP_SHIFT 12
#define DEDUP_SIZE  (1u << DEDUP_SHIFT)
uint64_t dedup_table[DEDUP_SIZE]; // ou bitmap
```

Alternative plus simple : trier les masques et dédupliquer en O(n log n).

### 4.5 [MOYEN] Maintien incrémental de `table_sum` dans `s_cte_pos`

Ajouter un champ `uint8_t table_sum` dans `s_cte_pos` (il reste 4 bytes libres dans la ligne de cache de 64 bytes) et le maintenir dans `pos_apply_move`. Élimine `compute_table_max_sum`.

### 4.6 [MINEUR] Réduire la stack pressure de `bitboard_gen_all_compact_moves_table`

Les tableaux `active_by_val[15][64]` et `cap_by_val[15][256]` sont indexés par les 15 valeurs possibles, mais seules les valeurs présentes en main sont utilisées (max 6). Allouer dynamiquement sur la stack uniquement pour les valeurs utiles via un VLA ou réduire les dimensions.

### 4.7 [OPTIONNEL] GNU Vector Extensions pour le match loop

Le prototype `proto_simd_gnu.c` montre une piste intéressante mais incomplète : il ne gère pas la partition exacte (seulement la somme directe). Pour un gain réel, vectoriser le test `(table_bb & mask) == mask` dans `collect_active_base_masks` avec `v4di` (4× uint64 par vecteur) serait le candidat le plus pertinent. Le gain dépend du nombre de masques testés par bucket pivot — probablement 1.5-2× sur cette boucle spécifique.

---

## 5. Priorisation recommandée

| Priorité | Action                                     | Effort | Gain estimé      |
|----------|--------------------------------------------|--------|-------------------|
| P0       | Stack-alloc dans `is_exact_partition`      | 15 min | 4-6× sur Dynamic  |
| P1       | Chemin direct bitboard dans `pos_gen_moves`| 30 min | 10-30% sur search |
| P1       | `pos_apply_move` natif capture_mask        | 1h     | 20-40% sur search |
| P2       | Dédup hash dans `combine_disjoint_masks`   | 30 min | variable          |
| P2       | `table_sum` incrémental dans `s_cte_pos`   | 15 min | marginal          |
| P3       | Réduire stack de `gen_all_compact_table`    | 30 min | portabilité       |
| P3       | GNU vec ext pour mask matching             | 2-3h   | 1.5-2× localisé   |

---

## 6. Conclusion

Le backend Compact 1D Pivot Tables est le meilleur chemin actuel (2.7× vs Array, ~550 ns/pos, ~1200 cycles). Le design de l'index inversé par pivot est solide.

Le frein principal n'est pas dans le bitboard lui-même mais dans les fonctions partagées (`is_exact_partition` avec ses malloc, le round-trip struct table dans le minimax). Les gains les plus significatifs viendront de l'intégration plus profonde du format bitboard dans le search — en propageant le `capture_mask` natif jusqu'à `pos_apply_move` et en éliminant les conversions intermédiaires.

Le Carry-Rippler Dynamic reste utile comme fallback 0-RAM / référence de validation, mais ne devrait pas être utilisé en production tant que `is_exact_partition` alloue sur le heap.
