#ifndef __CTE_FRONT_TUI_H
#define __CTE_FRONT_TUI_H

#include "cte.h"
#include "front_cli.h"

typedef struct {
    uint8_t            nb_players;    // 2, 3, or 4
    bool               is_team_mode;  // 2v2 team mode (valid with nb_players == 4)
    e_cli_game_type    game_type;
    e_cli_ai_type      ai_types[4];
    uint8_t            nb_ai_types;
    e_cte_render_style style;
    uint16_t           winning_score;
    uint8_t            max_rounds;
} s_cte_tui_config;

// Main TUI game runner
int run_tui_frontend(const s_cte_tui_config *config);

// Interactive TUI Main Menu
int run_tui_main_menu(void);

#endif
