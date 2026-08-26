#ifndef __CTE_EVAL_H
#define __CTE_EVAL_H

#include "card.h"
#include "player.h"
#include "move.h"

// Snapshot of game state passed to evaluators
typedef struct s_cte_game_state {
    const struct table         *table;
    const struct s_cte_players *players;
    uint8_t                     current_player_id;
} s_cte_game_state;

// Generic evaluator signature
typedef uint16_t (*t_evaluator)(
    const s_cte_game_state       *state,
    const struct s_cte_move_list *moves,
    void                         *ctx
);

// Forward declaration of UI callbacks
struct s_cte_ui_callbacks;

// Round configuration
typedef struct {
    uint8_t     first_player;     // Index of first player (0..nb_players-1)
    bool        is_team_mode;     // True for 4-player 2v2 team mode (P0+P2 vs P1+P3)
    t_evaluator evaluators[4];    // Evaluators per player slot
    void       *eval_contexts[4]; // Context pointers (NULL for eval_random)
    const struct s_cte_ui_callbacks *callbacks; // Optional UI event callbacks (NULL for headless)
    void       *ui_context;       // Context pointer passed to UI callbacks
} s_cte_round_config;

// Pure algorithmic evaluators (zero I/O)
uint16_t eval_random(const s_cte_game_state *state,
                     const struct s_cte_move_list *moves,
                     void *ctx);

#endif
