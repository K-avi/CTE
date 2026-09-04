#ifndef __CTE_BACKEND_BITBOARD_H
#define __CTE_BACKEND_BITBOARD_H

#include "engine.h"

// Compact Bitboard move (pure registers / stack, 0 malloc)
typedef struct {
    t_card   card_played;
    uint64_t capture_mask; // 0 for drop, or 64-bit mask of captured cards
} s_cte_bitboard_move;

typedef struct {
    uint16_t size;
    s_cte_bitboard_move moves[1024];
} s_cte_bitboard_move_list;

// SWAR Rank Patterns bitboard move generation (7.1 KB RAM, 100% L1 cache resident)
void bitboard_gen_all_compact_moves_rank(s_cte_bitboard_move_list *out_list, uint64_t table_bb, const struct s_cte_hand *hand);
t_cteerr bitboard_gen_card_moves_rank(struct s_cte_move_list *moves, uint64_t table_bb, t_card card);
t_cteerr bitboard_gen_all_moves_rank(struct s_cte_move_list *moves, uint64_t table_bb, const struct s_cte_hand *hand);

// Registered backend
extern const s_cte_engine_backend g_backend_bitboard;

#endif
