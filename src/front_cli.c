#include "front_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


static void cli_on_round_start(uint8_t round_nb, void *ui_ctx){
    (void)ui_ctx;
    printf("\n>>> Starting Round %u <<<\n", (unsigned)round_nb);
}

static void cli_on_turn_start(const s_cte_game_state *state, const struct s_cte_move_list *moves, void *ui_ctx){
    const struct s_cte_player_data *cur_player = &state->players->players[state->current_player_id];
    if(!cur_player->is_human) return;

    s_cli_ui_ctx *ctx = (s_cli_ui_ctx*)ui_ctx;
    e_cte_render_style style = ctx ? ctx->style : CTE_RENDER_UNICODE;

    printf("\n=======================================================\n");
    printf(" [Round Status] (Deck: %u cards left)\n", (unsigned)(state->deck ? (52 - state->deck->cur_card) : 0));
    for(uint8_t p = 0; p < state->players->size; p++){
        const struct s_cte_player_data *pl = &state->players->players[p];
        uint8_t pts = 0;
        for(uint8_t j = 0; j < pl->won_cards.size; j++){
            pts += get_points(pl->won_cards.array[j]);
        }
        printf("   * %-18s : %2u cards captured (%2u card pts, %u tablic)\n",
               pl->player_name,
               (unsigned)pl->won_cards.size,
               (unsigned)pts,
               (unsigned)pl->nb_tablic);
    }
    printf("-------------------------------------------------------\n");

    uint8_t table_count = (uint8_t)__builtin_popcountll(state->table_bb);
    printf(" [Table (%u cards)] : ", (unsigned)table_count);
    if(table_count == 0){
        printf("(empty)\n");
    } else {
        uint64_t temp = state->table_bb;
        while(temp > 0){
            t_card c = (t_card)__builtin_ctzll(temp);
            char card_str[16];
            format_card(card_str, sizeof(card_str), c, style);
            printf("%s ", card_str);
            temp &= (temp - 1);
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

static void cli_on_move_played(const struct s_cte_player_data *player, const struct s_cte_move *move, bool captured, void *ui_ctx){
    (void)captured;
    s_cli_ui_ctx *ctx = (s_cli_ui_ctx*)ui_ctx;
    e_cte_render_style style = ctx ? ctx->style : CTE_RENDER_UNICODE;

    char move_str[128];
    format_move(move_str, sizeof(move_str), move, style);
    printf(" [%s] played : %s\n",
           player ? player->player_name : "Player",
           move_str);
}

static void cli_on_round_end(const struct s_cte_players *players, const s_cte_round_score scores[], void *ui_ctx){
    s_cli_ui_ctx *ctx = (s_cli_ui_ctx*)ui_ctx;
    bool is_team = ctx ? ctx->is_team_mode : false;

    printf("\n-------------------------------------------------------\n");
    printf(" [Round Summary Breakdown]\n");
    for(uint8_t i = 0; i < players->size; i++){
        const struct s_cte_player_data *p = &players->players[i];
        printf("   * %-18s : %2u card pts + %u majority (%2u cards) + %u tablic = %2u pts\n",
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

static void cli_on_match_end(const struct s_cte_match *match, int8_t winner_id, void *ui_ctx){
    (void)ui_ctx;
    printf("\n=======================================================\n");
    if(match->is_team_mode && match->game && match->game->players.size == 4){
        if(winner_id == 0){
            printf("  MATCH OVER! Winner: Team 1 (%s & %s) with %u points! (Rounds played: %u)\n",
                   match->game->players.players[0].player_name,
                   match->game->players.players[2].player_name,
                   (unsigned)match->match_scores[0],
                   (unsigned)match->round_nb);
        } else if(winner_id == 1){
            printf("  MATCH OVER! Winner: Team 2 (%s & %s) with %u points! (Rounds played: %u)\n",
                   match->game->players.players[1].player_name,
                   match->game->players.players[3].player_name,
                   (unsigned)match->match_scores[1],
                   (unsigned)match->round_nb);
        } else {
            printf("  MATCH OVER in a Draw! (Rounds played: %u)\n", (unsigned)match->round_nb);
        }
    } else if(match->game) {
        if(winner_id >= 0 && winner_id < (int8_t)match->game->players.size){
            printf("  MATCH OVER! Winner: %s with %u points! (Rounds played: %u)\n",
                   match->game->players.players[winner_id].player_name,
                   (unsigned)match->match_scores[winner_id],
                   (unsigned)match->round_nb);
        } else {
            printf("  MATCH OVER in a Draw! (Rounds played: %u)\n", (unsigned)match->round_nb);
        }
    }
    printf("=======================================================\n");
}

uint16_t cli_read_human_move(const s_cte_game_state *state,
                             const struct s_cte_move_list *moves,
                             void *ctx)
{
    (void)state;
    (void)ctx;
    if(!moves || moves->size == 0) return 0;

    for(;;){
        printf(" Enter move number (0 to %u, or 'q' to quit): ", (unsigned)(moves->size - 1));
        char line[64];
        if(!fgets(line, sizeof(line), stdin)){
            printf("\n[CTE] Input closed (EOF). Exiting...\n");
            exit(0);
        }

        size_t len = strlen(line);
        while(len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' ')){
            line[--len] = '\0';
        }

        if(strcmp(line, "q") == 0 || strcmp(line, "Q") == 0 ||
           strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0){
            printf("\n[CTE] Game aborted by user.\n");
            exit(0);
        }

        char *endptr = NULL;
        long val = strtol(line, &endptr, 10);
        if(endptr != line && *endptr == '\0' && val >= 0 && val < (long)moves->size){
            return (uint16_t)val;
        }
        printf(" [!] Invalid move number. Please choose between 0 and %u (or 'q' to quit).\n", (unsigned)(moves->size - 1));
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

const s_cte_ui_callbacks *cli_get_callbacks(void){
    return &g_cli_callbacks;
}

static t_evaluator get_evaluator(e_cli_ai_type type, const char **name_out){
    switch(type){
        case AI_TYPE_DUMB:
            if(name_out) *name_out = "Dumb";
            return eval_dumb;
        case AI_TYPE_GREEDY:
            if(name_out) *name_out = "Greedy";
            return eval_greedy;
        case AI_TYPE_CHEATER:
            if(name_out) *name_out = "Cheater";
            return eval_cheater;
        case AI_TYPE_RANDOM:
        default:
            if(name_out) *name_out = "Random";
            return eval_random;
    }
}

int run_cli_frontend(const s_cte_cli_config *config){
    if(!config) return 1;

    uint8_t nb_p = config->nb_players;
    if(nb_p < 2 || nb_p > 4) nb_p = 2;

    char names_buf[4][64];
    char *names[4];

    for(uint8_t i = 0; i < 4; i++){
        names[i] = names_buf[i];
    }

    t_evaluator slot_evaluators[4];
    bool slot_is_human[4];

    for(uint8_t i = 0; i < nb_p; i++){
        char base_name[48];
        if(config->game_type == GAME_HUMAN_VS_HUMAN){
            slot_is_human[i] = true;
            slot_evaluators[i] = cli_read_human_move;
            if(i == 0 && config->profile_name[0] != '\0'){
                snprintf(base_name, sizeof(base_name), "%.31s", config->profile_name);
            } else {
                snprintf(base_name, sizeof(base_name), "Player %u (Human)", (unsigned)(i + 1));
            }
        } else if(config->game_type == GAME_HUMAN_VS_AI){
            if(i == 0){
                slot_is_human[i] = true;
                slot_evaluators[i] = cli_read_human_move;
                if(config->profile_name[0] != '\0'){
                    snprintf(base_name, sizeof(base_name), "%.31s", config->profile_name);
                } else {
                    snprintf(base_name, sizeof(base_name), "Human (P1)");
                }
            } else {
                slot_is_human[i] = false;
                uint8_t ai_idx = (config->nb_ai_types > 1) ? (uint8_t)(i - 1) : 0;
                const char *strat_name = NULL;
                slot_evaluators[i] = get_evaluator(config->ai_types[ai_idx % 4], &strat_name);
                snprintf(base_name, sizeof(base_name), "Bot %u (%s)", (unsigned)i, strat_name);
            }
        } else { // AI vs AI
            slot_is_human[i] = false;
            uint8_t ai_idx = (config->nb_ai_types > 1) ? i : 0;
            const char *strat_name = NULL;
            slot_evaluators[i] = get_evaluator(config->ai_types[ai_idx % 4], &strat_name);
            snprintf(base_name, sizeof(base_name), "Bot %u (%s)", (unsigned)(i + 1), strat_name);
        }

        if(config->is_team_mode && nb_p == 4){
            uint8_t team_num = (uint8_t)((i % 2) + 1);
            snprintf(names[i], sizeof(names_buf[i]), "%s [T%u]", base_name, (unsigned)team_num);
        } else {
            snprintf(names[i], sizeof(names_buf[i]), "%s", base_name);
        }
    }

    s_cte_game game;
    t_cteerr err = init_game(&game, nb_p, names, config->is_team_mode);
    if(err != e_ok){
        fprintf(stderr, "Error: Failed to initialize game (code: %u)\n", err);
        return 1;
    }

    for(uint8_t i = 0; i < nb_p; i++){
        game.players.players[i].is_human = slot_is_human[i];
        game.players.players[i].evaluator = slot_evaluators[i];
    }

    struct s_cte_match match;
    err = init_match(&match, &game, config->winning_score);
    if(err != e_ok){
        fprintf(stderr, "Error: Failed to initialize match (code: %u)\n", err);
        free_game(&game);
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
        printf(" Player %u     : %s\n", (unsigned)(i+1), names[i]);
    }
    printf("=======================================================\n\n");

    err = run_match(&match, &round_config);
    if(err != e_ok){
        fprintf(stderr, "Error during match execution (code: %u)\n", err);
        free_game(&game);
        return 1;
    }

    if(err == e_ok && config->profile_name[0] != '\0' && config->game_type != GAME_AI_VS_AI){
        s_cte_profile_db db;
        if(init_profile_db(&db, NULL) == e_ok){
            s_cte_profile *p = find_or_create_profile(&db, config->profile_name);
            if(p){
                p->total_points += match.match_scores[0];
                p->total_tablics += match.match_tablics[0];
                p->matches_played++;

                int16_t opp_elo = CTE_DEFAULT_ELO;
                if(config->nb_players >= 2){
                    if(!game.players.players[1].is_human && config->nb_ai_types > 0){
                        opp_elo = cte_default_ai_elo(config->ai_types[0]);
                    } else {
                        s_cte_profile *opp_p = find_profile(&db, names[1]);
                        if(opp_p) opp_elo = opp_p->elo;
                    }
                }

                uint16_t s0 = match.match_scores[0];
                uint16_t s1 = (config->nb_players >= 2) ? match.match_scores[1] : 0;
                double score = 0.5;
                if(s0 > s1){
                    p->matches_won++;
                    score = 1.0;
                } else if(s1 > s0){
                    p->matches_lost++;
                    score = 0.0;
                } else {
                    p->matches_tied++;
                    score = 0.5;
                }

                int16_t delta = compute_elo_delta(p->elo, opp_elo, score, CTE_DEFAULT_K_FACTOR);
                int16_t old_elo = p->elo;
                p->elo += delta;
                if(p->elo < 100) p->elo = 100;
                p->last_played_at = (uint64_t)time(NULL);

                save_profiles(&db);
                printf("\n[Profile] '%s' updated: Elo %d -> %d (%+d), Matches: %u (W:%u L:%u D:%u)\n",
                       p->name, (int)old_elo, (int)p->elo, (int)delta,
                       (unsigned)p->matches_played, (unsigned)p->matches_won,
                       (unsigned)p->matches_lost, (unsigned)p->matches_tied);
            }
        }
    }

    free_game(&game);
    return 0;
}
