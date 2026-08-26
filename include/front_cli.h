#ifndef __CTE_FRONT_CLI_H
#define __CTE_FRONT_CLI_H

#include "cte.h"


typedef struct {
    uint8_t            nb_players;    // 2, 3, or 4 (default: 2)
    bool               is_team_mode;  // 2v2 team mode (valid with nb_players == 4)
    e_cli_game_type    game_type;
    e_cli_ai_type      ai_types[4];   // Strategy for each AI player slot
    uint8_t            nb_ai_types;   // Number of explicitly specified AI types
    e_cte_render_style style;
    uint16_t           winning_score;
    uint8_t            max_rounds;
} s_cte_cli_config;

// Interactive input evaluator for human players
uint16_t cli_read_human_move(const s_cte_game_state *state,
                             const struct s_cte_move_list *moves,
                             void *ctx);

// Main CLI game launcher
int run_cli_frontend(const s_cte_cli_config *config);

#endif
