#define _XOPEN_SOURCE_EXTENDED 1
#include "front_tui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include <locale.h>

#define PAIR_DEFAULT   1
#define PAIR_HEADER    2
#define PAIR_RED_CARD  3
#define PAIR_BLK_CARD  4
#define PAIR_SELECT    5
#define PAIR_TARGET    6
#define PAIR_BORDER    7
#define PAIR_ACCENT    8

typedef struct {
    e_cte_render_style style;
    bool               is_team_mode;
    char               last_log[256];
    uint8_t            cur_round;
    const struct s_cte_player_data *last_player;
    const struct s_cte_move        *last_move;
} s_tui_ui_ctx;

static void init_tui_colors(void){
    start_color();
    use_default_colors();

    init_pair(PAIR_DEFAULT,   COLOR_WHITE,   -1);
    init_pair(PAIR_HEADER,    COLOR_BLACK,   COLOR_CYAN);
    init_pair(PAIR_RED_CARD,  COLOR_RED,     -1);
    init_pair(PAIR_BLK_CARD,  COLOR_WHITE,   -1);
    init_pair(PAIR_SELECT,    COLOR_BLACK,   COLOR_YELLOW);
    init_pair(PAIR_TARGET,    COLOR_GREEN,   -1);
    init_pair(PAIR_BORDER,    COLOR_BLUE,    -1);
    init_pair(PAIR_ACCENT,    COLOR_YELLOW,  -1);
}

static void print_colored_card(int y, int x, t_card card, e_cte_render_style style, bool highlighted){
    if(card >= 52) return;

    char card_str[16];
    format_card(card_str, sizeof(card_str), card, style);

    uint8_t suit = (uint8_t)(card / 13);
    bool is_red = (suit == 1 || suit == 2); // Diamonds or Hearts

    if(highlighted){
        attron(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
    } else {
        attron(COLOR_PAIR(is_red ? PAIR_RED_CARD : PAIR_BLK_CARD) | A_BOLD);
    }

    mvprintw(y, x, "[%s]", card_str);

    if(highlighted){
        attroff(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
    } else {
        attroff(COLOR_PAIR(is_red ? PAIR_RED_CARD : PAIR_BLK_CARD) | A_BOLD);
    }
}

static void render_tui_board(const s_cte_game_state *state,
                            const struct s_cte_move_list *moves,
                            uint16_t selected_move_idx,
                            s_tui_ui_ctx *ctx)
{
    erase();
    e_cte_render_style style = ctx ? ctx->style : CTE_RENDER_UNICODE;
    int max_y = 0, max_x = 0;
    getmaxyx(stdscr, max_y, max_x);
    (void)max_y;

    // 1. Header Bar
    attron(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    for(int x = 0; x < max_x; x++) mvaddch(0, x, ' ');
    mvprintw(0, 2, " CTE - TABLIC TUI ");
    mvprintw(0, 22, "| Round: %u", (unsigned)(ctx ? ctx->cur_round : 1));
    mvprintw(0, 36, "| Deck: %2u cards left", (unsigned)(state->deck ? (52 - state->deck->cur_card) : 0));
    mvprintw(0, 62, "| Mode: %u Players%s",
             state->players->size,
             state->players->size == 4 && ctx && ctx->is_team_mode ? " (2v2)" : "");
    attroff(COLOR_PAIR(PAIR_HEADER) | A_BOLD);

    // 2. Players Summary Box (Top)
    attron(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);
    mvprintw(2, 2, "=== PLAYERS & SCORES ===");
    attroff(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);

    for(uint8_t p = 0; p < state->players->size; p++){
        const struct s_cte_player_data *pl = &state->players->players[p];
        uint8_t pts = 0;
        for(uint8_t j = 0; j < pl->won_cards.size; j++){
            pts += get_points(pl->won_cards.array[j]);
        }
        bool is_active = (p == state->current_player_id);
        if(is_active) attron(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
        mvprintw(3 + p, 4, "%c %-20s : %2u cards (%2u pts, %u tablic)",
                 is_active ? '>' : ' ',
                 pl->player_name,
                 (unsigned)pl->won_cards.size,
                 (unsigned)pts,
                 (unsigned)pl->nb_tablic);
        if(is_active) attroff(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
    }

    int table_y = 4 + state->players->size;

    // 3. Table Box
    attron(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);
    mvprintw(table_y, 2, "=== TABLE (%u cards) ===", (unsigned)state->table->nb_cards_on_table);
    attroff(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);

    // Find which table cards are targeted by currently selected move
    const struct s_cte_move *sel_move = (moves && selected_move_idx < moves->size) ? &moves->moves[selected_move_idx] : NULL;

    if(state->table->nb_cards_on_table == 0){
        mvprintw(table_y + 1, 4, "(empty table)");
    } else {
        int cx = 4;
        for(uint8_t i = 0; i < state->table->nb_cards_on_table; i++){
            t_card c = state->table->cards_on_table[i];
            bool is_picked = false;
            if(sel_move){
                for(uint8_t k = 0; k < sel_move->cards_picked.size; k++){
                    if(sel_move->cards_picked.array[k] == c){
                        is_picked = true;
                        break;
                    }
                }
            }
            print_colored_card(table_y + 1, cx, c, style, is_picked);
            cx += 8;
        }
    }

    // 4. Human Hand Box
    int hand_y = table_y + 3;
    const struct s_cte_player_data *cur_pl = &state->players->players[state->current_player_id];
    attron(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);
    mvprintw(hand_y, 2, "=== %s'S HAND (%u cards) ===", cur_pl->player_name, (unsigned)cur_pl->hand.size);
    attroff(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);

    int hx = 4;
    for(uint8_t i = 0; i < cur_pl->hand.size; i++){
        t_card c = cur_pl->hand.array[i];
        bool is_played = (sel_move && sel_move->card_played == c);
        print_colored_card(hand_y + 1, hx, c, style, is_played);
        hx += 8;
    }

    // 5. Legal Moves Selector
    int moves_y = hand_y + 3;
    attron(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);
    mvprintw(moves_y, 2, "=== AVAILABLE MOVES (Navigation: [UP/DOWN] - Play: [ENTER] - Quit: [q]) ===");
    attroff(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);

    if(moves && moves->size > 0){
        int display_max = 8;
        int start_idx = 0;
        if(selected_move_idx >= (uint16_t)display_max){
            start_idx = selected_move_idx - display_max + 1;
        }

        for(int i = 0; i < display_max && (start_idx + i) < moves->size; i++){
            uint16_t idx = (uint16_t)(start_idx + i);
            char move_str[128];
            format_move(move_str, sizeof(move_str), &moves->moves[idx], style);

            s_cte_move_score sc = score_move(&moves->moves[idx], state->table->nb_cards_on_table);
            char score_info[64] = "";
            if(moves->moves[idx].cards_picked.size > 0){
                snprintf(score_info, sizeof(score_info), " -> +%u pt%s (%u cards)%s",
                         (unsigned)sc.total_points, (sc.total_points > 1) ? "s" : "",
                         (unsigned)sc.nb_cards, sc.is_tablic ? " [TABLIC!]" : "");
            } else {
                snprintf(score_info, sizeof(score_info), " -> Drop (0 pt)");
            }

            bool is_cur = (idx == selected_move_idx);
            if(is_cur){
                attron(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
                mvprintw(moves_y + 1 + i, 4, " > [%u] %s%s ", (unsigned)idx, move_str, score_info);
                attroff(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
            } else {
                mvprintw(moves_y + 1 + i, 4, "   [%u] %s%s", (unsigned)idx, move_str, score_info);
            }
        }
    }

    // 6. Footer / Journal Bar
    int footer_y = moves_y + 10;
    attron(COLOR_PAIR(PAIR_BORDER) | A_BOLD);
    for(int x = 0; x < max_x; x++) mvaddch(footer_y, x, '-');
    attroff(COLOR_PAIR(PAIR_BORDER) | A_BOLD);

    mvprintw(footer_y + 1, 2, "Last Action : %s", (ctx && ctx->last_log[0]) ? ctx->last_log : "Game started.");

    refresh();
}

static uint16_t tui_read_human_move(const s_cte_game_state *state,
                                    const struct s_cte_move_list *moves,
                                    void *ctx)
{
    if(!moves || moves->size == 0) return 0;
    s_tui_ui_ctx *ui_ctx = (s_tui_ui_ctx*)ctx;

    uint16_t selected = 0;

    for(;;){
        render_tui_board(state, moves, selected, ui_ctx);

        int ch = getch();
        if(ch == KEY_UP || ch == 'k' || ch == 'K'){
            if(selected > 0) selected--;
        } else if(ch == KEY_DOWN || ch == 'j' || ch == 'J'){
            if(selected + 1 < moves->size) selected++;
        } else if(ch == KEY_HOME){
            selected = 0;
        } else if(ch == KEY_END){
            selected = (uint16_t)(moves->size - 1);
        } else if(ch >= '0' && ch <= '9'){
            uint16_t digit = (uint16_t)(ch - '0');
            if(digit < moves->size) selected = digit;
        } else if(ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' '){
            return selected;
        } else if(ch == 'q' || ch == 'Q' || ch == 27){
            endwin();
            printf("\n[CTE] Game exited by user.\n");
            exit(0);
        }
    }
}

static void tui_on_round_start(uint8_t round_nb, void *ui_ctx){
    s_tui_ui_ctx *ctx = (s_tui_ui_ctx*)ui_ctx;
    if(ctx){
        ctx->cur_round = round_nb;
        snprintf(ctx->last_log, sizeof(ctx->last_log), "Starting Round %u", (unsigned)round_nb);
    }
}

static void tui_on_move_played(const struct s_cte_player_data *player, const struct s_cte_move *move, bool captured, void *ui_ctx){
    (void)captured;
    s_tui_ui_ctx *ctx = (s_tui_ui_ctx*)ui_ctx;
    if(!ctx) return;

    char move_str[128];
    format_move(move_str, sizeof(move_str), move, ctx->style);
    snprintf(ctx->last_log, sizeof(ctx->last_log), "[%s] played : %s",
             player ? player->player_name : "Player", move_str);

    // If bot move, short pause to see bot play
    if(player && !player->is_human){
        napms(200);
    }
}

static void tui_on_round_end(const struct s_cte_players *players, const s_cte_round_score scores[], void *ui_ctx){
    s_tui_ui_ctx *ctx = (s_tui_ui_ctx*)ui_ctx;
    erase();

    attron(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    mvprintw(2, 4, " ================= ROUND %u SUMMARY ================= ", (unsigned)(ctx ? ctx->cur_round : 1));
    attroff(COLOR_PAIR(PAIR_HEADER) | A_BOLD);

    for(uint8_t i = 0; i < players->size; i++){
        const struct s_cte_player_data *p = &players->players[i];
        mvprintw(4 + i, 6, "* %-18s : %2u card pts + %u majority (%2u cards) + %u tablic = %2u pts",
                 p->player_name,
                 (unsigned)scores[i].card_points,
                 (unsigned)scores[i].majority_bonus,
                 (unsigned)p->won_cards.size,
                 (unsigned)scores[i].tablic_points,
                 (unsigned)scores[i].total);
    }

    if(ctx && ctx->is_team_mode && players->size == 4){
        uint16_t t0 = (uint16_t)(scores[0].total + scores[2].total);
        uint16_t t1 = (uint16_t)(scores[1].total + scores[3].total);
        mvprintw(10, 6, "--- Team Totals ---");
        mvprintw(11, 6, "* Team 1 (P1+P3) : Round Score: %2u pts", t0);
        mvprintw(12, 6, "* Team 2 (P2+P4) : Round Score: %2u pts", t1);
    }

    attron(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
    mvprintw(15, 6, " [Press any key to continue...] ");
    attroff(COLOR_PAIR(PAIR_SELECT) | A_BOLD);

    refresh();
    getch();
}

static void tui_on_match_end(const struct s_cte_match *match, int8_t winner_id, void *ui_ctx){
    (void)ui_ctx;
    erase();

    attron(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    mvprintw(2, 4, " ================= MATCH FINISHED ================= ");
    attroff(COLOR_PAIR(PAIR_HEADER) | A_BOLD);

    if(match->is_team_mode && match->game && match->game->players.size == 4){
        if(winner_id == 0){
            mvprintw(5, 6, "WINNER: Team 1 (%s & %s) with %u points! (Rounds: %u)",
                     match->game->players.players[0].player_name,
                     match->game->players.players[2].player_name,
                     (unsigned)match->match_scores[0],
                     (unsigned)match->round_nb);
        } else if(winner_id == 1){
            mvprintw(5, 6, "WINNER: Team 2 (%s & %s) with %u points! (Rounds: %u)",
                     match->game->players.players[1].player_name,
                     match->game->players.players[3].player_name,
                     (unsigned)match->match_scores[1],
                     (unsigned)match->round_nb);
        } else {
            mvprintw(5, 6, "Match finished in a DRAW! (Rounds: %u)", (unsigned)match->round_nb);
        }
    } else if(match->game) {
        if(winner_id >= 0 && winner_id < (int8_t)match->game->players.size){
            mvprintw(5, 6, "WINNER: %s with %u points! (Rounds: %u)",
                     match->game->players.players[winner_id].player_name,
                     (unsigned)match->match_scores[winner_id],
                     (unsigned)match->round_nb);
        } else {
            mvprintw(5, 6, "Match finished in a DRAW! (Rounds: %u)", (unsigned)match->round_nb);
        }
    }

    attron(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
    mvprintw(9, 6, " [Press any key to exit TUI...] ");
    attroff(COLOR_PAIR(PAIR_SELECT) | A_BOLD);

    refresh();
    getch();
}

static const s_cte_ui_callbacks g_tui_callbacks = {
    .on_round_start = tui_on_round_start,
    .on_deal        = NULL,
    .on_turn_start  = NULL,
    .on_move_played = tui_on_move_played,
    .on_round_end   = tui_on_round_end,
    .on_match_end   = tui_on_match_end,
};

static t_evaluator tui_get_evaluator(e_cli_ai_type type, const char **name_out){
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

int run_tui_frontend(const s_cte_tui_config *config){
    if(!config) return 1;

    uint8_t nb_p = config->nb_players;
    if(nb_p < 2 || nb_p > 4) nb_p = 2;

    char names_buf[4][64];
    char *names[4];

    for(uint8_t i = 0; i < 4; i++){
        names[i] = names_buf[i];
    }

    s_tui_ui_ctx ui_ctx = {
        .style = config->style,
        .is_team_mode = config->is_team_mode,
        .last_log = "Match started.",
        .cur_round = 1,
        .last_player = NULL,
        .last_move = NULL,
    };

    t_evaluator slot_evaluators[4];
    bool slot_is_human[4];

    for(uint8_t i = 0; i < nb_p; i++){
        char base_name[48];
        if(config->game_type == GAME_HUMAN_VS_HUMAN){
            slot_is_human[i] = true;
            slot_evaluators[i] = tui_read_human_move;
            snprintf(base_name, sizeof(base_name), "Player %u (Human)", (unsigned)(i + 1));
        } else if(config->game_type == GAME_HUMAN_VS_AI){
            if(i == 0){
                slot_is_human[i] = true;
                slot_evaluators[i] = tui_read_human_move;
                snprintf(base_name, sizeof(base_name), "Human (P1)");
            } else {
                slot_is_human[i] = false;
                uint8_t ai_idx = (config->nb_ai_types > 1) ? (uint8_t)(i - 1) : 0;
                const char *strat_name = NULL;
                slot_evaluators[i] = tui_get_evaluator(config->ai_types[ai_idx % 4], &strat_name);
                snprintf(base_name, sizeof(base_name), "Bot %u (%s)", (unsigned)i, strat_name);
            }
        } else { // AI vs AI
            slot_is_human[i] = false;
            uint8_t ai_idx = (config->nb_ai_types > 1) ? i : 0;
            const char *strat_name = NULL;
            slot_evaluators[i] = tui_get_evaluator(config->ai_types[ai_idx % 4], &strat_name);
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
        game.players.players[i].eval_context = &ui_ctx;
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

    s_cte_round_config round_config = {
        .first_player  = 0,
        .is_team_mode  = config->is_team_mode,
        .evaluators    = { NULL, NULL, NULL, NULL },
        .eval_contexts = { NULL, NULL, NULL, NULL },
        .callbacks     = &g_tui_callbacks,
        .ui_context    = &ui_ctx,
    };

    // Initialize ncurses with UTF-8 locale
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    init_tui_colors();

    err = run_match(&match, &round_config);

    // Terminate ncurses
    endwin();

    if(err != e_ok){
        fprintf(stderr, "Error during TUI match execution (code: %u)\n", err);
        free_game(&game);
        return 1;
    }

    free_game(&game);
    return 0;
}
