#ifndef __CTE_BITBOARD_RANK_TABLES_H
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
