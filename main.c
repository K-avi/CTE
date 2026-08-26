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
    printf("  -m, --mode <mode>          UI mode: cli (default), tui, gui\n");
    printf("  -s, --style <style>        Card render style: unicode (default), ascii\n");
    printf("  -g, --game <mode>          Game mode: h-vs-ai (default), h-vs-h, ai-vs-ai\n");
    printf("  -w, --winning-score <pts>  Target score to win the match (default: 101)\n");
    printf("  -c, --rounds <number>      Max number of rounds/deck cycles (default: 0 = unlimited)\n");
    printf("  -r, --seed <number>        RNG seed (default: system time)\n");
    printf("  -h, --help                 Display this help message and exit\n\n");
}

int main(int argc, char **argv){
    e_ui_mode mode = UI_MODE_CLI;
    s_cte_cli_config cli_config = {
        .game_type     = GAME_HUMAN_VS_AI,
        .style         = CTE_RENDER_UNICODE,
        .winning_score = 101,
        .max_rounds    = 0,
    };
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