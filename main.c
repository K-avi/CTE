#include "cte.h"
#include "front_cli.h"
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

static void print_usage(const char *prog_name){
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("Tablić card game engine & interactive player.\n\n");
    printf("Options:\n");
    printf("  -n, --players <number>     Number of players: 2 (default), 3, or 4\n");
    printf("  -t, --team                 Enable 4-player 2v2 team mode (valid only with -n 4)\n");
    printf("  -a, --ai-type <list>       AI strategy or comma-separated list (e.g. greedy or greedy,cheater)\n");
    printf("                             Supported: random (default), dumb, greedy, cheater\n");
    printf("  -m, --mode <mode>          UI mode: cli (default), tui, gui\n");
    printf("  -s, --style <style>        Card render style: unicode (default), ascii\n");
    printf("  -g, --game <mode>          Game mode: h-vs-ai (default), h-vs-h, ai-vs-ai\n");
    printf("  -w, --winning-score <pts>  Target score to win the match (default: 101)\n");
    printf("  -c, --rounds <number>      Max number of rounds/deck cycles (default: 0 = unlimited)\n");
    printf("  -r, --seed <number>        RNG seed (default: system time)\n");
    printf("  -h, --help                 Display this help message and exit\n\n");
}

static bool parse_ai_strategy(const char *token, e_cli_ai_type *type_out){
    if(strcmp(token, "random") == 0){
        *type_out = AI_TYPE_RANDOM; return true;
    } else if(strcmp(token, "dumb") == 0){
        *type_out = AI_TYPE_DUMB; return true;
    } else if(strcmp(token, "greedy") == 0){
        *type_out = AI_TYPE_GREEDY; return true;
    } else if(strcmp(token, "cheater") == 0 || strcmp(token, "minimax") == 0){
        *type_out = AI_TYPE_CHEATER; return true;
    }
    return false;
}

int main(int argc, char **argv){
    e_ui_mode mode = UI_MODE_CLI;
    s_cte_cli_config cli_config = {
        .nb_players    = 2,
        .is_team_mode  = false,
        .game_type     = GAME_HUMAN_VS_AI,
        .ai_types      = { AI_TYPE_RANDOM, AI_TYPE_RANDOM, AI_TYPE_RANDOM, AI_TYPE_RANDOM },
        .nb_ai_types   = 0,
        .style         = CTE_RENDER_UNICODE,
        .winning_score = 101,
        .max_rounds    = 0,
    };
    unsigned int seed = (unsigned int)time(NULL);
    bool seed_specified = false;

    static struct option long_options[] = {
        {"players",       required_argument, 0, 'n'},
        {"team",          no_argument,       0, 't'},
        {"ai-type",       required_argument, 0, 'a'},
        {"ai",            required_argument, 0, 'a'},
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
    while((opt = getopt_long(argc, argv, "n:ta:m:s:g:w:c:r:h", long_options, &option_index)) != -1){
        switch(opt){
            case 'n': {
                long val = strtol(optarg, NULL, 10);
                if(val < 2 || val > 4){
                    fprintf(stderr, "Error: Invalid number of players '%s'. Must be 2, 3, or 4.\n", optarg);
                    return 1;
                }
                cli_config.nb_players = (uint8_t)val;
                break;
            }
            case 't':
                cli_config.is_team_mode = true;
                break;
            case 'a': {
                char *arg_copy = strdup(optarg);
                if(!arg_copy){
                    fprintf(stderr, "Error: Memory allocation failed.\n");
                    return 1;
                }
                char *saveptr = NULL;
                char *token = strtok_r(arg_copy, ",", &saveptr);
                cli_config.nb_ai_types = 0;
                while(token && cli_config.nb_ai_types < 4){
                    e_cli_ai_type type;
                    if(!parse_ai_strategy(token, &type)){
                        fprintf(stderr, "Error: Unknown AI strategy '%s'. Supported: random, dumb, greedy, cheater\n", token);
                        free(arg_copy);
                        return 1;
                    }
                    cli_config.ai_types[cli_config.nb_ai_types++] = type;
                    token = strtok_r(NULL, ",", &saveptr);
                }
                free(arg_copy);
                break;
            }
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
                    cli_config.style = CTE_RENDER_UNICODE;
                } else if(strcmp(optarg, "ascii") == 0){
                    cli_config.style = CTE_RENDER_ASCII;
                } else {
                    fprintf(stderr, "Error: Unknown style '%s'. Supported: unicode, ascii\n", optarg);
                    return 1;
                }
                break;
            case 'g':
                if(strcmp(optarg, "h-vs-ai") == 0){
                    cli_config.game_type = GAME_HUMAN_VS_AI;
                } else if(strcmp(optarg, "h-vs-h") == 0){
                    cli_config.game_type = GAME_HUMAN_VS_HUMAN;
                } else if(strcmp(optarg, "ai-vs-ai") == 0){
                    cli_config.game_type = GAME_AI_VS_AI;
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
                cli_config.winning_score = (uint16_t)val;
                break;
            }
            case 'c': {
                long val = strtol(optarg, NULL, 10);
                if(val <= 0 || val > 255){
                    fprintf(stderr, "Error: Invalid number of rounds '%s'. Must be between 1 and 255.\n", optarg);
                    return 1;
                }
                cli_config.max_rounds = (uint8_t)val;
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

    if(cli_config.is_team_mode && cli_config.nb_players != 4){
        fprintf(stderr, "Error: Team mode (-t / --team) is only valid with 4 players (-n 4).\n");
        return 1;
    }

    srand(seed);
    if(seed_specified){
        printf("[Info] Using specified RNG seed: %u\n", seed);
    }

    switch(mode){
        case UI_MODE_CLI:
            return run_cli_frontend(&cli_config);
        case UI_MODE_TUI:
            printf("[Info] TUI mode (ncurses) is scheduled in the roadmap and will be available in an upcoming release.\n");
            printf("[Info] Falling back to interactive CLI mode.\n\n");
            return run_cli_frontend(&cli_config);
        case UI_MODE_GUI:
            printf("[Info] GUI mode is scheduled in the roadmap and will be available in an upcoming release.\n");
            printf("[Info] Falling back to interactive CLI mode.\n\n");
            return run_cli_frontend(&cli_config);
    }

    return 0;
}