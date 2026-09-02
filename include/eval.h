#ifndef __CTE_EVAL_H
#define __CTE_EVAL_H

#include "card.h"
#include "player.h"
#include "move.h"

typedef enum {
    GAME_HUMAN_VS_AI,
    GAME_HUMAN_VS_HUMAN,
    GAME_AI_VS_AI
} e_cli_game_type;

typedef enum {
    AI_TYPE_RANDOM,
    AI_TYPE_DUMB,
    AI_TYPE_GREEDY,
    AI_TYPE_CHEATER
} e_cli_ai_type;

// Snapshot of game state passed to evaluators
typedef struct s_cte_game_state {
    uint64_t                    table_bb;
    const struct s_cte_players *players;
    const struct deck          *deck;
    uint8_t                     current_player_id;
    bool                        is_team_mode;
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

uint16_t eval_dumb(const s_cte_game_state *state,
                   const struct s_cte_move_list *moves,
                   void *ctx);

uint16_t eval_greedy(const s_cte_game_state *state,
                     const struct s_cte_move_list *moves,
                     void *ctx);

uint16_t eval_cheater(const s_cte_game_state *state,
                      const struct s_cte_move_list *moves,
                      void *ctx);

#endif
