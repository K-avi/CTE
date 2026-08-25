#include "cte.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <getopt.h>

typedef enum {
    UI_MODE_CLI,
    UI_MODE_TUI,
    UI_MODE_GUI
} e_ui_mode;

typedef enum {
    GAME_HUMAN_VS_AI,
    GAME_HUMAN_VS_HUMAN,
    GAME_AI_VS_AI
} e_game_type;

static void print_usage(const char *prog_name){
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("Tablić card game engine & interactive player.\n\n");
    printf("Options:\n");
    printf("  -m, --mode <mode>          UI mode: cli (default), tui, gui\n");
    printf("  -s, --style <style>        Card render style: unicode (default), ascii\n");
    printf("  -g, --game <mode>          Game mode: h-vs-ai (default), h-vs-h, ai-vs-ai\n");
    printf("  -w, --winning-score <pts>  Target score to win the match (default: 101)\n");
    printf("  -c, --rounds <number>      Max number of rounds/deck cycles (default: 0 = unlimited)\n");
    printf("  -r, --seed <number>        RNG seed (default: system time)\n");
    printf("  -h, --help                 Display this help message and exit\n\n");
}

static int run_cli_match(e_game_type game_type, e_cte_render_style style, uint16_t winning_score, uint8_t max_rounds){
    struct s_cte_players players;
    char *names[2];

    switch(game_type){
        case GAME_HUMAN_VS_AI:
            names[0] = "Human";
            names[1] = "Bot";
            break;
        case GAME_HUMAN_VS_HUMAN:
            names[0] = "Player 1";
            names[1] = "Player 2";
            break;
        case GAME_AI_VS_AI:
            names[0] = "Bot 1";
            names[1] = "Bot 2";
            break;
    }

    t_cteerr err = init_players(&players, 2, names);
    if(err != e_ok){
        fprintf(stderr, "Error: Failed to initialize players (code: %u)\n", err);
        return 1;
    }

    struct s_cte_match match;
    err = init_match(&match, &players, winning_score);
    if(err != e_ok){
        fprintf(stderr, "Error: Failed to initialize match (code: %u)\n", err);
        free_players(&players);
        return 1;
    }
    match.max_rounds = max_rounds;

    s_cte_round_config round_config;
    round_config.first_player = 0;
    round_config.eval_contexts[0] = (void*)&style;
    round_config.eval_contexts[1] = (void*)&style;
    round_config.eval_contexts[2] = NULL;
    round_config.eval_contexts[3] = NULL;

    switch(game_type){
        case GAME_HUMAN_VS_AI:
            round_config.evaluators[0] = eval_human_cli;
            round_config.evaluators[1] = eval_ai_cli;
            break;
        case GAME_HUMAN_VS_HUMAN:
            round_config.evaluators[0] = eval_human_cli;
            round_config.evaluators[1] = eval_human_cli;
            break;
        case GAME_AI_VS_AI:
            round_config.evaluators[0] = eval_ai_cli;
            round_config.evaluators[1] = eval_ai_cli;
            break;
    }
    round_config.evaluators[2] = NULL;
    round_config.evaluators[3] = NULL;

    printf("=======================================================\n");
    printf("                   CTE - TABLIĆ CLI                    \n");
    printf("=======================================================\n");
    printf(" Match Target : %u points%s\n", (unsigned)winning_score, (max_rounds > 0) ? " (or max rounds reached)" : "");
    if(max_rounds > 0){
        printf(" Max Rounds   : %u deck cycle%s\n", (unsigned)max_rounds, (max_rounds > 1) ? "s" : "");
    }
    printf(" Player 1     : %s%s\n", names[0], (game_type == GAME_HUMAN_VS_AI || game_type == GAME_HUMAN_VS_HUMAN) ? " (Interactive)" : " (AI)");
    printf(" Player 2     : %s%s\n", names[1], (game_type == GAME_HUMAN_VS_HUMAN) ? " (Interactive)" : " (AI)");
    printf(" Render Style : %s\n", (style == CTE_RENDER_UNICODE) ? "Unicode" : "ASCII");
    printf("=======================================================\n");

    while(!match_is_over(&match)){
        printf("\n>>> Starting Round %u%s <<<\n",
               (unsigned)(match.round_nb + 1),
               (max_rounds > 0) ? "" : "");
        round_config.first_player = (uint8_t)(match.round_nb % 2); // alternate first player

        err = run_round(match.players, &round_config);
        if(err != e_ok){
            fprintf(stderr, "Error during round execution (code: %u)\n", err);
            free_players(&players);
            return 1;
        }

        s_cte_round_score scores[2] = {0};
        err = compute_round_score(match.players, scores);
        if(err != e_ok){
            fprintf(stderr, "Error during score computation (code: %u)\n", err);
            free_players(&players);
            return 1;
        }

        for(uint8_t i = 0; i < match.players->size; i++){
            match.match_scores[i] += scores[i].total;
        }
        match.round_nb++;

        printf("\n-------------------------------------------------------\n");
        printf(" [Round %u Breakdown]\n", (unsigned)match.round_nb);
        for(uint8_t i = 0; i < match.players->size; i++){
            struct s_cte_player_data *p = &match.players->players[i];
            printf("   * %-10s : %2u card pts + %u majority (%2u cards) + %u tablic = %2u pts  [Total: %3u]\n",
                   p->player_name,
                   (unsigned)scores[i].card_points,
                   (unsigned)scores[i].majority_bonus,
                   (unsigned)p->won_cards.size,
                   (unsigned)scores[i].tablic_points,
                   (unsigned)scores[i].total,
                   (unsigned)match.match_scores[i]);
        }
        printf("-------------------------------------------------------\n");

        reset_all_players(match.players);
    }

    int8_t winner = match_winner(&match);
    printf("\n=======================================================\n");
    if(winner >= 0 && winner < (int8_t)match.players->size){
        printf("  MATCH OVER! Winner: %s with %u points! (Rounds played: %u)\n",
               match.players->players[winner].player_name,
               (unsigned)match.match_scores[winner],
               (unsigned)match.round_nb);
    } else {
        printf("  MATCH OVER in a Draw! (Rounds played: %u)\n", (unsigned)match.round_nb);
    }
    printf("=======================================================\n");

    free_players(&players);
    return 0;
}

int main(int argc, char **argv){
    e_ui_mode mode = UI_MODE_CLI;
    e_cte_render_style style = CTE_RENDER_UNICODE;
    e_game_type game = GAME_HUMAN_VS_AI;
    uint16_t winning_score = 101;
    uint8_t max_rounds = 0;
    unsigned int seed = (unsigned int)time(NULL);
    bool seed_specified = false;

    static struct option long_options[] = {
        {"mode",          required_argument, 0, 'm'},
        {"style",         required_argument, 0, 's'},
        {"game",          required_argument, 0, 'g'},
        {"winning-score", required_argument, 0, 'w'},
        {"rounds",        required_argument, 0, 'c'},
        {"cycles",        required_argument, 0, 'c'},
        {"seed",          required_argument, 0, 'r'},
        {"help",          no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    while((opt = getopt_long(argc, argv, "m:s:g:w:c:r:h", long_options, &option_index)) != -1){
        switch(opt){
            case 'm':
                if(strcmp(optarg, "cli") == 0){
                    mode = UI_MODE_CLI;
                } else if(strcmp(optarg, "tui") == 0){
                    mode = UI_MODE_TUI;
                } else if(strcmp(optarg, "gui") == 0){
                    mode = UI_MODE_GUI;
                } else {
                    fprintf(stderr, "Error: Unknown mode '%s'. Supported: cli, tui, gui\n", optarg);
                    return 1;
                }
                break;
            case 's':
                if(strcmp(optarg, "unicode") == 0){
                    style = CTE_RENDER_UNICODE;
                } else if(strcmp(optarg, "ascii") == 0){
                    style = CTE_RENDER_ASCII;
                } else {
                    fprintf(stderr, "Error: Unknown style '%s'. Supported: unicode, ascii\n", optarg);
                    return 1;
                }
                break;
            case 'g':
                if(strcmp(optarg, "h-vs-ai") == 0){
                    game = GAME_HUMAN_VS_AI;
                } else if(strcmp(optarg, "h-vs-h") == 0){
                    game = GAME_HUMAN_VS_HUMAN;
                } else if(strcmp(optarg, "ai-vs-ai") == 0){
                    game = GAME_AI_VS_AI;
                } else {
                    fprintf(stderr, "Error: Unknown game mode '%s'. Supported: h-vs-ai, h-vs-h, ai-vs-ai\n", optarg);
                    return 1;
                }
                break;
            case 'w': {
                long val = strtol(optarg, NULL, 10);
                if(val <= 0 || val > 1000){
                    fprintf(stderr, "Error: Invalid winning score '%s'. Must be between 1 and 1000.\n", optarg);
                    return 1;
                }
                winning_score = (uint16_t)val;
                break;
            }
            case 'c': {
                long val = strtol(optarg, NULL, 10);
                if(val <= 0 || val > 255){
                    fprintf(stderr, "Error: Invalid number of rounds '%s'. Must be between 1 and 255.\n", optarg);
                    return 1;
                }
                max_rounds = (uint8_t)val;
                break;
            }
            case 'r': {
                long val = strtol(optarg, NULL, 10);
                seed = (unsigned int)val;
                seed_specified = true;
                break;
            }
            case 'h':
                print_usage(argv[0]);
                return 0;
            case '?':
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                return 1;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    srand(seed);
    if(seed_specified){
        printf("[Info] Using specified RNG seed: %u\n", seed);
    }

    switch(mode){
        case UI_MODE_CLI:
            return run_cli_match(game, style, winning_score, max_rounds);
        case UI_MODE_TUI:
            printf("[Info] TUI mode (ncurses) is scheduled in the roadmap and will be available in an upcoming release.\n");
            printf("[Info] Falling back to interactive CLI mode.\n\n");
            return run_cli_match(game, style, winning_score, max_rounds);
        case UI_MODE_GUI:
            printf("[Info] GUI mode is scheduled in the roadmap and will be available in an upcoming release.\n");
            printf("[Info] Falling back to interactive CLI mode.\n\n");
            return run_cli_match(game, style, winning_score, max_rounds);
    }

    return 0;
}