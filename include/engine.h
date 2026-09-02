#ifndef __CTE_ENGINE_H
#define __CTE_ENGINE_H

#include "card.h"
#include "player.h"
#include "move.h"
#include "eval.h"
#include "minmax.h"
#include "game.h"

typedef enum {
    CTE_BACKEND_ARRAY          = 0, // Reference array backend (exact DP subset sum)
    CTE_BACKEND_BITBOARD       = 1, // Dynamic Bitboard backend (Carry-Rippler, 0 KB RAM)
    CTE_BACKEND_BITBOARD_TABLE = 2, // Lookup Table Bitboard backend (Precomputed masks, 225 KB RAM)
    CTE_BACKEND_BITBOARD_RANK  = 3, // Compact SWAR Rank Patterns backend (7.1 KB RAM, 100% L1 cache)
    CTE_BACKEND_SIMD           = 4  // SIMD batched vector backend (AVX2 / MIPPv2)
} e_cte_backend_type;

// Backend interface contract
typedef struct s_cte_engine_backend {
    e_cte_backend_type type;
    const char        *name;

    // Game lifecycle
    t_cteerr (*init_game)(s_cte_game *game, uint8_t nb_players, char *names[], bool is_team_mode);
    void     (*free_game)(s_cte_game *game);
    t_cteerr (*setup_round)(s_cte_game *game);
    t_cteerr (*deal_next_hand)(s_cte_game *game);
    t_cteerr (*award_remaining)(s_cte_game *game);
    t_cteerr (*run_round)(s_cte_game *game, const s_cte_round_config *config);

    // Move generation, validation & scoring
    t_cteerr (*is_legal)(bool *ret, uint64_t table_bb, const struct s_cte_move *move);
    t_cteerr (*gen_card_moves)(struct s_cte_move_list *moves, uint64_t table_bb, t_card card);
    t_cteerr (*gen_all_moves)(struct s_cte_move_list *moves, uint64_t table_bb, const struct s_cte_hand *hand);
    t_cteerr (*play_move)(uint64_t *table_bb, const struct s_cte_move *move, struct s_cte_player_data *player, bool *captured);
    s_cte_move_score (*score_move)(const struct s_cte_move *move, uint64_t table_bb);

    // Search snapshot conversion
    s_cte_pos (*to_pos)(const s_cte_game *game);
} s_cte_engine_backend;

// Get available backend by type
const s_cte_engine_backend *cte_get_backend(e_cte_backend_type type);

#endif
