#ifndef __CTE_MINMAX_H
#define __CTE_MINMAX_H

#include "card.h"
#include "player.h"
#include "move.h"
#include "eval.h"

// Compact game snapshot (fits in a 64-byte L1 cache line: exactly 60 bytes)
typedef struct {
    uint64_t table_bb;          // 52-bit mask of cards on the table
    uint64_t hand_bb[4];        // 52-bit mask of cards in each player's hand
    uint8_t  hand_counts[4];    // Number of cards in each player's hand
    uint8_t  won_card_counts[4];// Number of won cards per player
    uint8_t  card_points[4];    // Cumulative card points per player
    uint8_t  tablic_counts[4];  // Cumulative tablic count per player
    uint8_t  current_player;    // Active player (0..nb_players-1)
    int8_t   last_captor;       // Last capturing player (-1 if none)
    uint8_t  nb_players;        // 2, 3, or 4
    bool     is_team_mode;      // True for 4-player 2v2 team mode
} s_cte_pos;

#define CTE_MAX_ROOT_CANDIDATES 256

typedef struct {
    uint16_t move_idx;         // Index into root moves list
    int32_t  score;            // Evaluation score at last completed depth
    uint8_t  depth_completed;  // Depth at which this move was fully evaluated
    uint64_t nodes_spent;      // Nodes evaluated searching this branch
    bool     refuted;          // True if pruned by upper bound or proven unviable
} s_cte_root_candidate;

typedef enum {
    UBP_NONE = 0,               // No upper bound pruning (standard alpha-beta)
    UBP_STRICT_ADMISSIBLE = 1,  // 100% mathematically sound upper bound
    UBP_NO_TABLIC = 2,          // Disregards future tablics
    UBP_TIGHT_HEURISTIC = 3     // Aggressive heuristic upper bound
} e_cte_ubp_model;

// Forward declaration for eval function pointer
struct s_cte_pos_fwd;

typedef struct {
    uint8_t         max_depth;         // Search depth in plies (e.g. 2 for 1 turn lookahead, 4, 6...)
    uint32_t        timeout_ms;        // Max time in ms for iterative deepening (0 = fixed depth)
    e_cte_ubp_model ubp_model;         // Upper Bound Pruning model to use
    int32_t       (*eval_fn)(const s_cte_pos*, uint8_t); // Custom eval function (NULL = pos_evaluate)
    uint64_t        nodes_visited;     // Total search tree nodes visited
    uint64_t        ubp_cutoffs;       // Number of branches pruned by UBP
    uint8_t         depth_reached;     // Max depth fully completed across all candidates
    uint16_t        num_candidates;    // Number of root candidates recorded
    s_cte_root_candidate candidates[CTE_MAX_ROOT_CANDIDATES];
} s_cte_search_config;


// Convert s_cte_game_state to compact s_cte_pos
s_cte_pos pos_from_state(const s_cte_game_state *state);

// Apply a move to a position snapshot (pure, returns new position on stack)
s_cte_pos pos_apply_move(const s_cte_pos *pos, const struct s_cte_move *move);
s_cte_pos pos_apply_bitboard_move(const s_cte_pos *pos, t_card card_played, uint64_t capture_mask);

// Generate legal moves for current player in pos
t_cteerr pos_gen_moves(struct s_cte_move_list *moves, const s_cte_pos *pos);

// Upper Bound Pruning (UBP) bounds
int32_t compute_upper_bound(const s_cte_pos *pos, uint8_t root_player, uint8_t depth, e_cte_ubp_model model);
int32_t compute_lower_bound(const s_cte_pos *pos, uint8_t root_player, uint8_t depth, e_cte_ubp_model model);

// Static evaluation function (from perspective of root_player)
int32_t pos_evaluate(const s_cte_pos *pos, uint8_t root_player);

// Frozen baseline evaluation (pre-optimization) for A/B benchmarking
int32_t pos_evaluate_v0(const s_cte_pos *pos, uint8_t root_player);

// Alpha-Beta / Minimax search with Iterative Deepening
uint16_t search_best_move(const s_cte_pos *pos,
                          const struct s_cte_move_list *moves,
                          s_cte_search_config *config);

#endif
