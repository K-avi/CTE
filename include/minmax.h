#ifndef __CTE_MINMAX_H
#define __CTE_MINMAX_H

#include "card.h"
#include "player.h"
#include "move.h"
#include "eval.h"

// Compact game snapshot (fits in a 64-byte L1 cache line)
typedef struct {
    uint8_t table_count;
    t_card  table[20];
    uint8_t hand_counts[4];
    t_card  hands[4][6];
    uint8_t won_card_counts[4];
    uint8_t card_points[4];
    uint8_t tablic_counts[4];
    uint8_t current_player;
    int8_t  last_captor;
    uint8_t nb_players;
    bool    is_team_mode;
} s_cte_pos;

typedef struct {
    uint8_t  max_depth;    // Search depth in plies (e.g. 2 for 1 turn lookahead, 4, 6...)
    uint32_t timeout_ms;   // Max time in ms for iterative deepening (0 = fixed depth)
} s_cte_search_config;

// Convert s_cte_game_state to compact s_cte_pos
s_cte_pos pos_from_state(const s_cte_game_state *state);

// Apply a move to a position snapshot (pure, returns new position on stack)
s_cte_pos pos_apply_move(const s_cte_pos *pos, const struct s_cte_move *move);

// Generate legal moves for current player in pos
t_cteerr pos_gen_moves(struct s_cte_move_list *moves, const s_cte_pos *pos);

// Static evaluation function (from perspective of root_player)
int32_t pos_evaluate(const s_cte_pos *pos, uint8_t root_player);

// Alpha-Beta / Minimax search with Iterative Deepening
uint16_t search_best_move(const s_cte_pos *pos,
                          const struct s_cte_move_list *moves,
                          const s_cte_search_config *config);

#endif
