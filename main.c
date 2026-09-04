#include "cte.h"
#include "front_cli.h"
#include "front_tui.h"
#include "tournament.h"
#include "profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <locale.h>
#include <unistd.h>

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
    printf("  -T, --tournament <type>    Run tournament: round-robin or cup\n");
    printf("  -p, --profile <name>       Active player profile for tracking statistics and Elo\n");
    printf("  -L, --leaderboard          Display player Elo leaderboard and exit\n");
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

static t_evaluator eval_from_ai_type(e_cli_ai_type type){
    switch(type){
        case AI_TYPE_DUMB:    return eval_dumb;
        case AI_TYPE_GREEDY:  return eval_greedy;
        case AI_TYPE_CHEATER: return eval_cheater;
        case AI_TYPE_RANDOM:
        default:              return eval_random;
    }
}

int main(int argc, char **argv){
    setlocale(LC_ALL, "");

    struct timespec ts;
    unsigned int seed;
    if(clock_gettime(CLOCK_REALTIME, &ts) == 0){
        seed = (unsigned int)(ts.tv_nsec ^ ts.tv_sec);
    } else {
        seed = (unsigned int)time(NULL);
    }
    srand(seed);

    // If launched with zero arguments on an interactive terminal, open TUI menu
    if(argc == 1 && isatty(STDIN_FILENO)){
        return run_tui_main_menu();
    }

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
    bool seed_specified = false;

    bool is_tournament = false;
    char tournament_type_str[32] = "round-robin";
    bool show_leaderboard = false;
    char profile_name[32] = {0};
    bool profile_specified = false;

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
        {"tournament",    required_argument, 0, 'T'},
        {"profile",       required_argument, 0, 'p'},
        {"leaderboard",   no_argument,       0, 'L'},
        {"help",          no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    while((opt = getopt_long(argc, argv, "n:ta:m:s:g:w:c:r:T:p:Lh", long_options, &option_index)) != -1){
        switch(opt){
            case 'n': {
                long val = strtol(optarg, NULL, 10);
                if(val < 2 || val > 16){
                    fprintf(stderr, "Error: Invalid number of players '%s'. Must be between 2 and 16.\n", optarg);
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
            case 'T':
                is_tournament = true;
                snprintf(tournament_type_str, sizeof(tournament_type_str), "%s", optarg);
                break;
            case 'p':
                profile_specified = true;
                snprintf(profile_name, sizeof(profile_name), "%s", optarg);
                break;
            case 'L':
                show_leaderboard = true;
                break;
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

    if(show_leaderboard){
        s_cte_profile_db db;
        init_profile_db(&db, NULL);
        sort_profiles_by_elo(&db);
        print_leaderboard(&db);
        return 0;
    }

    if(!is_tournament && cli_config.is_team_mode && cli_config.nb_players != 4){
        fprintf(stderr, "Error: Team mode (-t / --team) is only valid with 4 players (-n 4).\n");
        return 1;
    }

    if(seed_specified){
        srand(seed);
        printf("[Info] Using specified RNG seed: %u\n", seed);
    }

    // CLI Tournament execution
    if(is_tournament){
        s_cte_tournament t;
        e_cte_tournament_type t_type = TOURNAMENT_ROUND_ROBIN;
        if(strcmp(tournament_type_str, "cup") == 0 || strcmp(tournament_type_str, "knockout") == 0){
            t_type = TOURNAMENT_KNOCKOUT;
        }

        uint8_t nb_part = cli_config.nb_players;
        if(nb_part < 2 || nb_part > CTE_MAX_TOURNAMENT_PLAYERS){
            nb_part = 4;
        }
        if(t_type == TOURNAMENT_KNOCKOUT && (nb_part & (nb_part - 1)) != 0){
            nb_part = 4; // Default to 4 if not power of 2
        }

        s_cte_tournament_config t_cfg = {
            .type            = t_type,
            .nb_participants = nb_part,
            .winning_score   = cli_config.winning_score,
            .max_rounds      = cli_config.max_rounds,
            .silent          = true,
            .style           = cli_config.style,
        };

        const char *bot_names[8] = {
            "Bot_Greedy", "Bot_Cheater", "Bot_Random", "Bot_Dumb",
            "Bot_Greedy2", "Bot_Cheater2", "Bot_Random2", "Bot_Dumb2"
        };
        t_evaluator bot_evals[4] = { eval_greedy, eval_cheater, eval_random, eval_dumb };

        for(uint8_t i = 0; i < nb_part; i++){
            if(i == 0 && profile_specified && cli_config.game_type != GAME_AI_VS_AI){
                snprintf(t_cfg.participants[i].name, sizeof(t_cfg.participants[i].name), "%s", profile_name);
                t_cfg.participants[i].is_human = true;
                t_cfg.participants[i].evaluator = cli_read_human_move;
            } else {
                snprintf(t_cfg.participants[i].name, sizeof(t_cfg.participants[i].name), "%s", bot_names[i % 8]);
                t_cfg.participants[i].is_human = false;
                t_cfg.participants[i].evaluator = (cli_config.nb_ai_types > 0)
                    ? eval_from_ai_type(cli_config.ai_types[i % cli_config.nb_ai_types])
                    : bot_evals[i % 4];
            }
        }

        t_cteerr err = init_tournament(&t, &t_cfg);
        if(err != e_ok){
            fprintf(stderr, "Error: Failed to initialize tournament (code: %u)\n", err);
            return 1;
        }

        printf("Launching CTE Tournament (%s, %u participants)...\n",
               (t_type == TOURNAMENT_ROUND_ROBIN) ? "Round Robin" : "Knockout Cup",
               (unsigned)nb_part);

        err = run_tournament(&t);
        if(err != e_ok){
            fprintf(stderr, "Error: Tournament execution failed (code: %u)\n", err);
            free_tournament(&t);
            return 1;
        }

        print_tournament_standings(&t, cli_config.style);

        // Update profile stats if active
        if(profile_specified){
            s_cte_profile_db db;
            if(init_profile_db(&db, NULL) == e_ok){
                s_cte_profile *p = find_or_create_profile(&db, profile_name);
                if(p){
                    p->total_points += t.config.participants[0].total_points;
                    p->total_tablics += t.config.participants[0].total_tablics;
                    for(uint16_t m = 0; m < t.nb_matches; m++){
                        s_cte_tournament_match *match = &t.matches[m];
                        if(match->p1_idx == 0 || match->p2_idx == 0){
                            p->matches_played++;
                            if(match->winner_idx == -1){
                                p->matches_tied++;
                            } else if((match->p1_idx == 0 && match->winner_idx == 0) ||
                                      (match->p2_idx == 0 && match->winner_idx == 1)){
                                p->matches_won++;
                                p->elo += 16;
                            } else {
                                p->matches_lost++;
                                if(p->elo > 116) p->elo -= 16;
                            }
                        }
                    }
                    save_profiles(&db);
                }
            }
        }

        free_tournament(&t);
        return 0;
    }

    // Regular game execution
    switch(mode){
        case UI_MODE_CLI:
            return run_cli_frontend(&cli_config);
        case UI_MODE_TUI: {
            s_cte_tui_config tui_config = {
                .nb_players    = cli_config.nb_players,
                .is_team_mode  = cli_config.is_team_mode,
                .game_type     = cli_config.game_type,
                .nb_ai_types   = cli_config.nb_ai_types,
                .style         = cli_config.style,
                .winning_score = cli_config.winning_score,
                .max_rounds    = cli_config.max_rounds,
            };
            for(int i = 0; i < 4; i++) tui_config.ai_types[i] = cli_config.ai_types[i];
            return run_tui_frontend(&tui_config);
        }
        case UI_MODE_GUI:
            printf("[Info] Graphical GUI mode is scheduled in the roadmap for an upcoming release.\n");
            printf("[Info] Falling back to interactive CLI mode.\n\n");
            return run_cli_frontend(&cli_config);
    }

    return 0;
}