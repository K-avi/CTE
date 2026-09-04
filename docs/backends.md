# CTE Engine Backends Documentation

The **CTE** (*Card Table Engine*) features an abstract multi-backend architecture exposed through the [`s_cte_engine_backend`](../include/engine.h) interface. This architecture decouples the game orchestration layer, evaluation heuristics, tree search algorithms, and frontends (CLI, TUI) from low-level bitboard representations and move generation mechanics.

---

## 1. Overview of the Two Backends

| Backend | Identifier | RAM Footprint | Key Characteristics | Primary Role |
|---|:---:|:---:|---|---|
| **Array Backend** | `CTE_BACKEND_ARRAY` | $0\text{ KB}$ | Byte arrays, exact recursive backtracking (subset-sum partition) | **Reference Oracle (Testing & Verification)** |
| **Bitboard** | `CTE_BACKEND_BITBOARD` | $7.1\text{ KB}$ | 4-bit SWAR rank patterns, 100% L1-resident, 0 heap allocations | **Default Production Backend** |

---

## 2. Detailed Backend Architecture

### A. Array Reference Backend (`CTE_BACKEND_ARRAY`)
- **Source Files:** [`src/move.c`](../src/move.c), [`src/engine.c`](../src/engine.c)
- **Algorithm:**
  - Standard byte-array representation of table and hand cards.
  - Capture generation via recursive depth-first backtracking (`is_exact_partition`).
  - Mathematically straightforward, transparent, and easy to audit by inspection.
- **Role in CTE:**
  - Serves as the **ground truth mathematical oracle**.
  - Exclusively used in 2-way differential fuzzing to prove the bitboard engine emits 0 illegal moves and misses 0 valid partitions.

---

### B. Bitboard Backend (`CTE_BACKEND_BITBOARD`)
- **Source Files:** [`src/backend_bitboard.c`](../src/backend_bitboard.c), [`src/bitboard_rank_tables.c`](../src/bitboard_rank_tables.c), [`include/backend_bitboard.h`](../include/backend_bitboard.h), [`include/bitboard_rank_tables.h`](../include/bitboard_rank_tables.h)
- **Algorithm:**
  - Reentrant 64-bit mask representation of cards (`uint64_t table_bb`).
  - **Compact Rank Patterns:** Card ranks 2 through King (13 ranks) are packed into 4-bit nibbles with SWAR guard bits (`CTE_SWAR_GUARD_MASK`).
  - Valid subset rank combinations are detected through a single vector subtraction instruction (`table_swar - p->packed_swar`).
  - Suit expansion utilizes an ultra-compact lookup table ($7.1\text{ KB}$ total), guaranteeing 100% residency in the CPU L1 data cache across all modern architectures.
  - Fast multi-capture deduplication using a 256-bit Bloom filter allocated locally on the stack.
- **Available Generators:**
  - `bitboard_gen_all_moves_rank`: Polymorphic adapter implementing `s_cte_engine_backend` and populating a heap-backed `struct s_cte_move_list`.
  - `bitboard_gen_all_compact_moves_rank`: Ultra-fast 1-pass zero-heap generator emitting compact `s_cte_bitboard_move` pairs `(card_played, uint64_t capture_mask)` purely on stack registers. Used directly by the Minimax search tree.

---

## 3. Differential Validation & Fuzzing

The engine is continuously verified under **AddressSanitizer** (`-fsanitize=address`) and **UndefinedBehaviorSanitizer** (`-fsanitize=undefined`):

- **Functional Regression Suite (`make run-test`):**
  - Tests **T1 through T26**: Complete verification of Tablić game rules (dealing, turn progression, tricks scoring, majority bonus, tablić counting, and engine interface contracts).
- **2-Way Bitboard Differential Suite (`make run-test-bitboard`):**
  - **BT1:** Critical tactical captures (compound partitions, dual-value Aces at 1 or 11) verified between Array and Bitboard.
  - **BT2:** Exhaustive 2-way differential fuzzing across **10,000 randomized configurations** of tables and hands:
    - 0 false positives (`is_legal` asserted on every move).
    - Strict isomorphism of move sets (0 missed moves, 0 extraneous moves).
  - **BT3:** Differential validation of the 1-pass compact SWAR generator against the Array oracle (10,000 configurations).
  - **BT4:** 500 complete multi-player simulated rounds under the SWAR backend asserting strict conservation of all 52 cards and 22 trick points.

---

## 4. Empirical Performance Benchmarks

Measured on $200,000$ realistic game positions with `-O3 -funroll-loops -march=native` on an Intel Skylake core @ 2.2 GHz:

| Implementation | Time (s) | Throughput (Mmoves/s) | Latency / Pos | Cycles @ 2.2 GHz |
|---|:---:|:---:|:---:|:---:|
| **Array (Reference Oracle)** | $0.254\text{ s}$ | $3.75\text{ Mmoves/s}$ | $1269.7\text{ ns}$ | $2,793\text{ cycles}$ |
| **Bitboard SWAR (Vtable Move List)** | $0.061\text{ s}$ | $15.48\text{ Mmoves/s}$ | $307.3\text{ ns}$ | $676\text{ cycles}$ |
| **Bitboard Compact SWAR (100% L1)** | **$0.051\text{ s}$** | **$18.73\text{ Mmoves/s}$** | **$254.1\text{ ns}$** | **$559\text{ cycles}$** |

**Speedup:** The SWAR Rank Patterns engine achieves a **$5.00\times$ speedup** over the reference array engine with zero memory allocation.
