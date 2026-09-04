#ifndef __CTE_ENGINE_H
#define __CTE_ENGINE_H

#include "card.h"
#include "player.h"
#include "move.h"

typedef enum {
    CTE_BACKEND_ARRAY    = 0, // Reference array backend (exact DP subset sum, tests only)
    CTE_BACKEND_BITBOARD = 1, // SWAR Rank Patterns bitboard backend (7.1 KB RAM, default)
} e_cte_backend_type;

// Backend interface contract: strictly Move Generation & Validation
typedef struct s_cte_engine_backend {
    e_cte_backend_type type;
    const char        *name;

    // Move generation & validation (polymorphic)
    t_cteerr (*is_legal)(bool *ret, uint64_t table_bb, const struct s_cte_move *move);
    t_cteerr (*gen_card_moves)(struct s_cte_move_list *moves, uint64_t table_bb, t_card card);
    t_cteerr (*gen_all_moves)(struct s_cte_move_list *moves, uint64_t table_bb, const struct s_cte_hand *hand);
} s_cte_engine_backend;

// Get available backend by type
const s_cte_engine_backend *cte_get_backend(e_cte_backend_type type);

#endif
