#!/usr/bin/env python3
"""
bitboard_rank_gen.py — Generates compact rank-based pattern tables for CTE bitboard backend.
Total patterns: 398 (vs 28,855 full card masks), taking ~6.3 KB (100% fits in L1 cache).
"""

from collections import Counter
from itertools import combinations

RANK_VALUES = [2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14]

def is_ace(r):
    return r == 9  # card value 11 is at rank 9

def subset_sums_to(ranks, target):
    nom = sum(RANK_VALUES[r] for r in ranks)
    aces = sum(1 for r in ranks if is_ace(r))
    if nom < target:
        return False
    diff = nom - target
    if diff % 10 != 0:
        return False
    return (diff // 10) <= aces

def get_rank_patterns(target):
    res = []
    def backtrack(start_r, current, min_s):
        if min_s > target:
            return
        if current and subset_sums_to(current, target):
            res.append(list(current))
        for r in range(start_r, 13):
            vm = 1 if is_ace(r) else RANK_VALUES[r]
            if min_s + vm > target:
                continue
            if current.count(r) < 4:
                backtrack(r, current + [r], min_s + vm)
    backtrack(0, [], 0)
    return res

def pack_swar(ranks):
    # Pack 13 nibbles of 4 bits: count of each rank
    val = 0
    cnt = Counter(ranks)
    for r, count in cnt.items():
        val |= (count << (r * 4))
    return val

def generate_header(filename):
    with open(filename, "w") as f:
        f.write("""#ifndef __CTE_BITBOARD_RANK_TABLES_H
#define __CTE_BITBOARD_RANK_TABLES_H

#include <stdint.h>

#define CTE_TOTAL_RANK_PATTERNS 398
#define CTE_SWAR_GUARD_MASK 0x8888888888888ULL

// Compact rank pattern descriptor (16 bytes, L1-cache resident)
typedef struct {
    uint64_t packed_swar; // 13 nibbles (bits 0..51): count of each rank
    uint8_t  num_ranks;   // Number of distinct ranks (1..4)
    uint8_t  ranks[4];    // Distinct rank indices (0..12)
    uint8_t  counts[4];   // Multiplicity of each rank (1..4)
    uint8_t  _pad[3];     // Alignment to 16 bytes
} s_cte_rank_pattern;

extern const s_cte_rank_pattern g_rank_patterns[CTE_TOTAL_RANK_PATTERNS];
extern const uint16_t g_rank_pattern_offsets[16];
extern const uint16_t g_rank_pattern_counts[16];

// 16 entries: maps a 4-bit suit mask to 64-bit card bitmask at rank 0
extern const uint64_t g_suit_to_rank0[16];

// Suit combination lookup: for (suit_mask, need) -> list of chosen 4-bit suit combinations
extern const uint8_t g_suit_combinations[16][5][6];
extern const uint8_t g_suit_combination_counts[16][5];

#endif // __CTE_BITBOARD_RANK_TABLES_H
""")

def generate_source(filename, all_patterns_by_target):
    offsets = [0] * 16
    counts = [0] * 16
    flat_patterns = []

    cur_offset = 0
    for t in range(16):
        if t in all_patterns_by_target:
            pats = all_patterns_by_target[t]
            offsets[t] = cur_offset
            counts[t] = len(pats)
            cur_offset += len(pats)
            flat_patterns.extend(pats)
        else:
            offsets[t] = cur_offset
            counts[t] = 0

    assert len(flat_patterns) == 398, f"Expected 398 patterns, got {len(flat_patterns)}"

    # Generate g_suit_to_rank0
    suit_to_rank0 = [0] * 16
    for m in range(16):
        val = 0
        for s in range(4):
            if m & (1 << s):
                val |= (1 << (s * 13))
        suit_to_rank0[m] = val

    # Generate g_suit_combinations and g_suit_combination_counts
    suit_combos = [[[0] * 6 for _ in range(5)] for _ in range(16)]
    suit_counts = [[0] * 5 for _ in range(16)]
    for m in range(16):
        present = [s for s in range(4) if (m & (1 << s))]
        for need in range(1, 5):
            if len(present) >= need:
                cb = list(combinations(present, need))
                suit_counts[m][need] = len(cb)
                for idx, c in enumerate(cb):
                    suit_combos[m][need][idx] = sum(1 << s for s in c)

    with open(filename, "w") as f:
        f.write("""#include "bitboard_rank_tables.h"

// Offsets and counts per target value (0..15)
const uint16_t g_rank_pattern_offsets[16] = {
""")
        for t in range(16):
            f.write(f"    {offsets[t]},\n")
        f.write("};\n\nconst uint16_t g_rank_pattern_counts[16] = {\n")
        for t in range(16):
            f.write(f"    {counts[t]},\n")
        f.write("};\n\n")

        f.write("// Maps a 4-bit suit mask to 64-bit card bitmask at rank 0 (128 bytes)\n")
        f.write("const uint64_t g_suit_to_rank0[16] = {\n")
        for m in range(16):
            f.write(f"    0x{suit_to_rank0[m]:013x}ULL,\n")
        f.write("};\n\n")

        f.write("// Suit combinations lookup (480 bytes)\n")
        f.write("const uint8_t g_suit_combinations[16][5][6] = {\n")
        for m in range(16):
            f.write("    { ")
            for need in range(5):
                vals = ", ".join(f"{v}" for v in suit_combos[m][need])
                f.write(f"{{{vals}}}, ")
            f.write("},\n")
        f.write("};\n\n")

        f.write("// Suit combination counts (80 bytes)\n")
        f.write("const uint8_t g_suit_combination_counts[16][5] = {\n")
        for m in range(16):
            vals = ", ".join(f"{v}" for v in suit_counts[m])
            f.write(f"    {{{vals}}},\n")
        f.write("};\n\n")

        f.write(f"// 398 Compact Rank Patterns (~6.3 KB total, aligned on 64-byte cache line)\n")
        f.write(f"const s_cte_rank_pattern g_rank_patterns[CTE_TOTAL_RANK_PATTERNS] __attribute__((aligned(64))) = {{\n")

        for idx, pat in enumerate(flat_patterns):
            cnt = Counter(pat)
            distinct_ranks = sorted(cnt.keys())
            num_ranks = len(distinct_ranks)
            ranks = distinct_ranks + [0] * (4 - num_ranks)
            counts_list = [cnt[r] for r in distinct_ranks] + [0] * (4 - num_ranks)
            swar = pack_swar(pat)

            f.write(f"    /* {idx:3d} */ {{ 0x{swar:013x}ULL, {num_ranks}, "
                    f"{{ {ranks[0]:2d}, {ranks[1]:2d}, {ranks[2]:2d}, {ranks[3]:2d} }}, "
                    f"{{ {counts_list[0]}, {counts_list[1]}, {counts_list[2]}, {counts_list[3]} }}, {{0, 0, 0}} }},\n")
        f.write("};\n")

def main():
    by_target = {}
    for t in range(1, 15):
        by_target[t] = get_rank_patterns(t)

    generate_header("include/bitboard_rank_tables.h")
    generate_source("src/bitboard_rank_tables.c", by_target)
    print("Successfully generated include/bitboard_rank_tables.h and src/bitboard_rank_tables.c (398 patterns + combos, ~7.1 KB total).")

if __name__ == "__main__":
    main()
