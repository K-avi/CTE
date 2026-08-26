#ifndef __CTE_FRONT_CLI_H
#define __CTE_FRONT_CLI_H

#include "cte.h"

typedef enum {
    GAME_HUMAN_VS_AI,
    GAME_HUMAN_VS_HUMAN,
    GAME_AI_VS_AI
} e_cli_game_type;

typedef struct {
    e_cli_game_type    game_type;
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
