#include "front_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    e_cte_render_style style;
    bool               is_team_mode;
} s_cli_ui_ctx;

// Callback: start of round
static void cli_on_round_start(uint8_t round_nb, void *ui_ctx){
    (void)ui_ctx;
    printf("\n>>> Starting Round %u <<<\n", (unsigned)round_nb);
}

// Callback: turn start (display board & legal moves for human player)
static void cli_on_turn_start(const s_cte_game_state *state, const struct s_cte_move_list *moves, void *ui_ctx){
    const struct s_cte_player_data *cur_player = &state->players->players[state->current_player_id];
    if(!cur_player->is_human) return;

    s_cli_ui_ctx *ctx = (s_cli_ui_ctx*)ui_ctx;
    e_cte_render_style style = ctx ? ctx->style : CTE_RENDER_UNICODE;

    printf("\n=======================================================\n");
    printf(" [Round Status] (Deck: %u cards left)\n", (unsigned)(52 - deck.cur_card));
    for(uint8_t p = 0; p < state->players->size; p++){
        const struct s_cte_player_data *pl = &state->players->players[p];
        uint8_t pts = 0;
        for(uint8_t j = 0; j < pl->won_cards.size; j++){
            pts += get_points(pl->won_cards.array[j]);
        }
        printf("   * %-14s : %2u cards captured (%2u card pts, %u tablic)\n",
               pl->player_name,
               (unsigned)pl->won_cards.size,
               (unsigned)pts,
               (unsigned)pl->nb_tablic);
    }
    printf("-------------------------------------------------------\n");

    printf(" [Table (%u cards)] : ", (unsigned)state->table->nb_cards_on_table);
    if(state->table->nb_cards_on_table == 0){
        printf("(empty)\n");
    } else {
        for(uint8_t i = 0; i < state->table->nb_cards_on_table; i++){
            char card_str[16];
            format_card(card_str, sizeof(card_str), state->table->cards_on_table[i], style);
            printf("%s ", card_str);
        }
        printf("\n");
    }

    printf(" [%s's Hand (%u cards)] : ", cur_player->player_name, (unsigned)cur_player->hand.size);
    for(uint8_t i = 0; i < cur_player->hand.size; i++){
        char card_str[16];
        format_card(card_str, sizeof(card_str), cur_player->hand.array[i], style);
        printf("%s ", card_str);
    }
    printf("\n");

    printf(" Available moves (%u):\n", (unsigned)moves->size);
    for(uint16_t i = 0; i < moves->size; i++){
        char move_str[128];
        format_move(move_str, sizeof(move_str), &moves->moves[i], style);
        printf("   [%u] %s\n", (unsigned)i, move_str);
    }
}

// Callback: move played
static void cli_on_move_played(const struct s_cte_player_data *player, const struct s_cte_move *move, bool captured, void *ui_ctx){
    (void)captured;
    s_cli_ui_ctx *ctx = (s_cli_ui_ctx*)ui_ctx;
    e_cte_render_style style = ctx ? ctx->style : CTE_RENDER_UNICODE;

    char move_str[128];
    format_move(move_str, sizeof(move_str), move, style);
    printf(" [%s%s] played : %s\n",
           player ? player->player_name : "Player",
           (player && !player->is_human) ? " (AI)" : "",
           move_str);
}

// Callback: round end score breakdown
static void cli_on_round_end(const struct s_cte_players *players, const s_cte_round_score scores[], void *ui_ctx){
    s_cli_ui_ctx *ctx = (s_cli_ui_ctx*)ui_ctx;
    bool is_team = ctx ? ctx->is_team_mode : false;

    printf("\n-------------------------------------------------------\n");
    printf(" [Round Summary Breakdown]\n");
    for(uint8_t i = 0; i < players->size; i++){
        const struct s_cte_player_data *p = &players->players[i];
        printf("   * %-14s : %2u card pts + %u majority (%2u cards) + %u tablic = %2u pts\n",
               p->player_name,
               (unsigned)scores[i].card_points,
               (unsigned)scores[i].majority_bonus,
               (unsigned)p->won_cards.size,
               (unsigned)scores[i].tablic_points,
               (unsigned)scores[i].total);
    }

    if(is_team && players->size == 4){
        uint16_t team0_tot = (uint16_t)(scores[0].total + scores[2].total);
        uint16_t team1_tot = (uint16_t)(scores[1].total + scores[3].total);
        uint8_t team0_cards = (uint8_t)(players->players[0].won_cards.size + players->players[2].won_cards.size);
        uint8_t team1_cards = (uint8_t)(players->players[1].won_cards.size + players->players[3].won_cards.size);
        printf("   --- Team Totals ---\n");
        printf("   * Team 1 (P1+P3) : %2u cards -> Round Score: %2u pts\n", team0_cards, team0_tot);
        printf("   * Team 2 (P2+P4) : %2u cards -> Round Score: %2u pts\n", team1_cards, team1_tot);
    }
    printf("-------------------------------------------------------\n");
}

// Callback: match end
static void cli_on_match_end(const struct s_cte_match *match, int8_t winner_id, void *ui_ctx){
    (void)ui_ctx;
    printf("\n=======================================================\n");
    if(match->is_team_mode && match->players->size == 4){
        if(winner_id == 0){
            printf("  MATCH OVER! Winner: Team 1 (%s & %s) with %u points! (Rounds played: %u)\n",
                   match->players->players[0].player_name,
                   match->players->players[2].player_name,
                   (unsigned)match->match_scores[0],
                   (unsigned)match->round_nb);
        } else if(winner_id == 1){
            printf("  MATCH OVER! Winner: Team 2 (%s & %s) with %u points! (Rounds played: %u)\n",
                   match->players->players[1].player_name,
                   match->players->players[3].player_name,
                   (unsigned)match->match_scores[1],
                   (unsigned)match->round_nb);
        } else {
            printf("  MATCH OVER in a Draw! (Rounds played: %u)\n", (unsigned)match->round_nb);
        }
    } else {
        if(winner_id >= 0 && winner_id < (int8_t)match->players->size){
            printf("  MATCH OVER! Winner: %s with %u points! (Rounds played: %u)\n",
                   match->players->players[winner_id].player_name,
                   (unsigned)match->match_scores[winner_id],
                   (unsigned)match->round_nb);
        } else {
            printf("  MATCH OVER in a Draw! (Rounds played: %u)\n", (unsigned)match->round_nb);
        }
    }
    printf("=======================================================\n");
}

// Human pure input evaluator
uint16_t cli_read_human_move(const s_cte_game_state *state,
                             const struct s_cte_move_list *moves,
                             void *ctx)
{
    (void)state;
    (void)ctx;
    if(!moves || moves->size == 0) return 0;

    for(;;){
        printf(" Select move [0-%u]: ", (unsigned)(moves->size - 1));
        fflush(stdout);

        char input_buf[64];
        if(!fgets(input_buf, sizeof(input_buf), stdin)){
            printf("\n");
            return 0;
        }

        char *endptr = NULL;
        long val = strtol(input_buf, &endptr, 10);
        if(endptr != input_buf && val >= 0 && val < (long)moves->size){
            return (uint16_t)val;
        }
        printf(" Invalid input. Please enter a valid number between 0 and %u.\n", (unsigned)(moves->size - 1));
    }
}

static const s_cte_ui_callbacks g_cli_callbacks = {
    .on_round_start = cli_on_round_start,
    .on_deal        = NULL,
    .on_turn_start  = cli_on_turn_start,
    .on_move_played = cli_on_move_played,
    .on_round_end   = cli_on_round_end,
    .on_match_end   = cli_on_match_end,
};

int run_cli_frontend(const s_cte_cli_config *config){
    if(!config) return 1;

    uint8_t nb_p = config->nb_players;
    if(nb_p < 2 || nb_p > 4) nb_p = 2;

    struct s_cte_players players;
    char *names[4];

    if(nb_p == 2){
        if(config->game_type == GAME_HUMAN_VS_AI){
            names[0] = "Human"; names[1] = "Bot";
        } else if(config->game_type == GAME_HUMAN_VS_HUMAN){
            names[0] = "Player 1"; names[1] = "Player 2";
        } else {
            names[0] = "Bot 1"; names[1] = "Bot 2";
        }
    } else if(nb_p == 3){
        if(config->game_type == GAME_HUMAN_VS_AI){
            names[0] = "Human"; names[1] = "Bot 1"; names[2] = "Bot 2";
        } else if(config->game_type == GAME_HUMAN_VS_HUMAN){
            names[0] = "Player 1"; names[1] = "Player 2"; names[2] = "Player 3";
        } else {
            names[0] = "Bot 1"; names[1] = "Bot 2"; names[2] = "Bot 3";
        }
    } else { // 4 players
        if(config->is_team_mode){
            if(config->game_type == GAME_HUMAN_VS_AI){
                names[0] = "Human (T1)"; names[1] = "Bot 1 (T2)"; names[2] = "Bot 2 (T1)"; names[3] = "Bot 3 (T2)";
            } else if(config->game_type == GAME_HUMAN_VS_HUMAN){
                names[0] = "P1 (T1)"; names[1] = "P2 (T2)"; names[2] = "P3 (T1)"; names[3] = "P4 (T2)";
            } else {
                names[0] = "Bot 1 (T1)"; names[1] = "Bot 2 (T2)"; names[2] = "Bot 3 (T1)"; names[3] = "Bot 4 (T2)";
            }
        } else {
            if(config->game_type == GAME_HUMAN_VS_AI){
                names[0] = "Human"; names[1] = "Bot 1"; names[2] = "Bot 2"; names[3] = "Bot 3";
            } else if(config->game_type == GAME_HUMAN_VS_HUMAN){
                names[0] = "Player 1"; names[1] = "Player 2"; names[2] = "Player 3"; names[3] = "Player 4";
            } else {
                names[0] = "Bot 1"; names[1] = "Bot 2"; names[2] = "Bot 3"; names[3] = "Bot 4";
            }
        }
    }

    t_cteerr err = init_players(&players, nb_p, names);
    if(err != e_ok){
        fprintf(stderr, "Error: Failed to initialize players (code: %u)\n", err);
        return 1;
    }

    // Configure player controller evaluators
    for(uint8_t i = 0; i < nb_p; i++){
        if(config->game_type == GAME_HUMAN_VS_HUMAN){
            players.players[i].is_human = true;
            players.players[i].evaluator = cli_read_human_move;
        } else if(config->game_type == GAME_HUMAN_VS_AI){
            if(i == 0){
                players.players[i].is_human = true;
                players.players[i].evaluator = cli_read_human_move;
            } else {
                players.players[i].is_human = false;
                players.players[i].evaluator = eval_random;
            }
        } else { // AI vs AI
            players.players[i].is_human = false;
            players.players[i].evaluator = eval_random;
        }
    }

    struct s_cte_match match;
    err = init_match(&match, &players, config->winning_score);
    if(err != e_ok){
        fprintf(stderr, "Error: Failed to initialize match (code: %u)\n", err);
        free_players(&players);
        return 1;
    }
    match.max_rounds = config->max_rounds;
    match.is_team_mode = config->is_team_mode;

    s_cli_ui_ctx ui_ctx = {
        .style = config->style,
        .is_team_mode = config->is_team_mode,
    };

    s_cte_round_config round_config = {
        .first_player  = 0,
        .is_team_mode  = config->is_team_mode,
        .evaluators    = { NULL, NULL, NULL, NULL },
        .eval_contexts = { NULL, NULL, NULL, NULL },
        .callbacks     = &g_cli_callbacks,
        .ui_context    = &ui_ctx,
    };

    printf("=======================================================\n");
    printf("                   CTE - TABLIĆ CLI                    \n");
    printf("=======================================================\n");
    printf(" Mode         : %u Players%s\n", (unsigned)nb_p, config->is_team_mode ? " (2v2 Teams)" : " (Individual)");
    printf(" Match Target : %u points%s\n", (unsigned)config->winning_score, (config->max_rounds > 0) ? " (or max rounds reached)" : "");
    if(config->max_rounds > 0){
        printf(" Max Rounds   : %u deck cycle%s\n", (unsigned)config->max_rounds, (config->max_rounds > 1) ? "s" : "");
    }
    for(uint8_t i = 0; i < nb_p; i++){
        printf(" Player %u     : %s%s\n", (unsigned)(i+1), names[i], players.players[i].is_human ? " (Interactive)" : " (AI)");
    }
    printf(" Render Style : %s\n", (config->style == CTE_RENDER_UNICODE) ? "Unicode" : "ASCII");
    printf("=======================================================\n");

    err = run_match(&match, &round_config);
    if(err != e_ok){
        fprintf(stderr, "Error during match execution (code: %u)\n", err);
        free_players(&players);
        return 1;
    }

    free_players(&players);
    return 0;
}
