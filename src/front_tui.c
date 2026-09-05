#define _XOPEN_SOURCE_EXTENDED 1
#include "front_tui.h"
#include "tournament.h"
#include "profile.h"
#include "eval.h"
#include <curses.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PAIR_DEFAULT 1
#define PAIR_HEADER 2
#define PAIR_RED_CARD 3
#define PAIR_BLK_CARD 4
#define PAIR_SELECT 5
#define PAIR_TARGET 6
#define PAIR_BORDER 7
#define PAIR_ACCENT 8
#define PAIR_ALERT 9

typedef struct {
  e_cte_render_style style;
  bool is_team_mode;
  char last_log[256];
  uint8_t cur_round;
  const struct s_cte_player_data *last_player;
  const struct s_cte_move *last_move;
} s_tui_ui_ctx;

typedef struct {
  bool          is_human;
  e_cli_ai_type ai_type;
  char          name[32];
} s_tui_participant_slot;

#define TUI_MAX_PARTICIPANTS 16

static void init_tui_colors(void) {
  start_color();
  use_default_colors();

  init_pair(PAIR_DEFAULT, COLOR_WHITE, -1);
  init_pair(PAIR_HEADER, COLOR_BLACK, COLOR_CYAN);
  init_pair(PAIR_RED_CARD, COLOR_RED, -1);
  init_pair(PAIR_BLK_CARD, COLOR_WHITE, -1);
  init_pair(PAIR_SELECT, COLOR_BLACK, COLOR_YELLOW);
  init_pair(PAIR_TARGET, COLOR_GREEN, -1);
  init_pair(PAIR_BORDER, COLOR_BLUE, -1);
  init_pair(PAIR_ACCENT, COLOR_YELLOW, -1);
  init_pair(PAIR_ALERT, COLOR_RED, -1);
}

static void print_colored_card(int y, int x, t_card card,
                               e_cte_render_style style, bool highlighted) {
  if (card >= 52)
    return;

  char card_str[16];
  format_card(card_str, sizeof(card_str), card, style);

  uint8_t suit = (uint8_t)(card / 13);
  bool is_red = (suit == 1 || suit == 2); // Diamonds or Hearts

  if (highlighted) {
    attron(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
  } else {
    attron(COLOR_PAIR(is_red ? PAIR_RED_CARD : PAIR_BLK_CARD) | A_BOLD);
  }

  mvprintw(y, x, "[%s]", card_str);

  if (highlighted) {
    attroff(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
  } else {
    attroff(COLOR_PAIR(is_red ? PAIR_RED_CARD : PAIR_BLK_CARD) | A_BOLD);
  }
}

static void render_tui_board(const s_cte_game_state *state,
                             const struct s_cte_move_list *moves,
                             uint16_t selected_move_idx, s_tui_ui_ctx *ctx) {
  erase();
  e_cte_render_style style = ctx ? ctx->style : CTE_RENDER_UNICODE;
  int max_y = 0, max_x = 0;
  getmaxyx(stdscr, max_y, max_x);
  (void)max_y;

  // 1. Header Bar
  attron(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
  for (int x = 0; x < max_x; x++)
    mvaddch(0, x, ' ');
  mvprintw(0, 2, " CTE - TABLIC TUI ");
  mvprintw(0, 22, "| Round: %u", (unsigned)(ctx ? ctx->cur_round : 1));
  mvprintw(0, 36, "| Deck: %2u cards left",
           (unsigned)(state->deck ? (52 - state->deck->cur_card) : 0));
  mvprintw(0, 62, "| Mode: %u Players%s", state->players->size,
           state->players->size == 4 && ctx && ctx->is_team_mode ? " (2v2)"
                                                                 : "");
  attroff(COLOR_PAIR(PAIR_HEADER) | A_BOLD);

  // 2. Players Summary Box (Top)
  attron(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);
  mvprintw(2, 2, "=== PLAYERS & SCORES ===");
  attroff(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);

  for (uint8_t p = 0; p < state->players->size; p++) {
    const struct s_cte_player_data *pl = &state->players->players[p];
    uint8_t pts = 0;
    for (uint8_t j = 0; j < pl->won_cards.size; j++) {
      pts += get_points(pl->won_cards.array[j]);
    }
    bool is_active = (p == state->current_player_id);
    if (is_active)
      attron(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
    mvprintw(3 + p, 4, "%c %-20s : %2u cards (%2u pts, %u tablic)",
             is_active ? '>' : ' ', pl->player_name,
             (unsigned)pl->won_cards.size, (unsigned)pts,
             (unsigned)pl->nb_tablic);
    if (is_active)
      attroff(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
  }

  int table_y = 4 + state->players->size;

  // 3. Table Box
  uint8_t table_count = (uint8_t)__builtin_popcountll(state->table_bb);
  attron(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);
  mvprintw(table_y, 2, "=== TABLE (%u cards) ===", (unsigned)table_count);
  attroff(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);

  // Find which table cards are targeted by currently selected move
  const struct s_cte_move *sel_move = (moves && selected_move_idx < moves->size)
                                          ? &moves->moves[selected_move_idx]
                                          : NULL;

  if (table_count == 0) {
    mvprintw(table_y + 1, 4, "(empty table)");
  } else {
    int cx = 4;
    uint64_t temp = state->table_bb;
    while (temp > 0) {
      t_card c = (t_card)__builtin_ctzll(temp);
      bool is_picked = false;
      if (sel_move) {
        for (uint8_t k = 0; k < sel_move->cards_picked.size; k++) {
          if (sel_move->cards_picked.array[k] == c) {
            is_picked = true;
            break;
          }
        }
      }
      print_colored_card(table_y + 1, cx, c, style, is_picked);
      cx += 8;
      temp &= (temp - 1);
    }
  }

  // 4. Human Hand Box
  int hand_y = table_y + 3;
  const struct s_cte_player_data *cur_pl =
      &state->players->players[state->current_player_id];
  attron(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);
  mvprintw(hand_y, 2, "=== %s'S HAND (%u cards) ===", cur_pl->player_name,
           (unsigned)cur_pl->hand.size);
  attroff(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);

  int hx = 4;
  for (uint8_t i = 0; i < cur_pl->hand.size; i++) {
    t_card c = cur_pl->hand.array[i];
    bool is_played = (sel_move && sel_move->card_played == c);
    print_colored_card(hand_y + 1, hx, c, style, is_played);
    hx += 8;
  }

  // 5. Legal Moves Selector
  int moves_y = hand_y + 3;
  attron(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);
  mvprintw(moves_y, 2,
           "=== AVAILABLE MOVES (Navigation: [UP/DOWN] - Play: [ENTER] - Quit: "
           "[q]) ===");
  attroff(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);

  if (moves && moves->size > 0) {
    int display_max = 8;
    int start_idx = 0;
    if (selected_move_idx >= (uint16_t)display_max) {
      start_idx = selected_move_idx - display_max + 1;
    }

    for (int i = 0; i < display_max && (start_idx + i) < moves->size; i++) {
      uint16_t idx = (uint16_t)(start_idx + i);
      char move_str[128];
      format_move(move_str, sizeof(move_str), &moves->moves[idx], style);

      s_cte_move_score sc = score_move(&moves->moves[idx], state->table_bb);
      char score_info[64] = "";
      if (moves->moves[idx].cards_picked.size > 0) {
        snprintf(score_info, sizeof(score_info), " -> +%u pt%s (%u cards)%s",
                 (unsigned)sc.total_points, (sc.total_points > 1) ? "s" : "",
                 (unsigned)sc.nb_cards, sc.is_tablic ? " [TABLIC!]" : "");
      } else {
        snprintf(score_info, sizeof(score_info), " -> Drop (0 pt)");
      }

      bool is_cur = (idx == selected_move_idx);
      if (is_cur) {
        attron(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
        mvprintw(moves_y + 1 + i, 4, " > [%u] %s%s ", (unsigned)idx, move_str,
                 score_info);
        attroff(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
      } else {
        mvprintw(moves_y + 1 + i, 4, "   [%u] %s%s", (unsigned)idx, move_str,
                 score_info);
      }
    }
  }

  // 6. Footer / Journal Bar
  int footer_y = moves_y + 10;
  attron(COLOR_PAIR(PAIR_BORDER) | A_BOLD);
  for (int x = 0; x < max_x; x++)
    mvaddch(footer_y, x, '-');
  attroff(COLOR_PAIR(PAIR_BORDER) | A_BOLD);

  mvprintw(footer_y + 1, 2, "Last Action : %s",
           (ctx && ctx->last_log[0]) ? ctx->last_log : "Game started.");

  refresh();
}

static uint16_t tui_read_human_move(const s_cte_game_state *state,
                                    const struct s_cte_move_list *moves,
                                    void *ctx) {
  if (!moves || moves->size == 0)
    return 0;
  s_tui_ui_ctx *ui_ctx = (s_tui_ui_ctx *)ctx;

  uint16_t selected = 0;

  for (;;) {
    render_tui_board(state, moves, selected, ui_ctx);

    int ch = getch();
    if (ch == KEY_UP || ch == 'k' || ch == 'K') {
      if (selected > 0)
        selected--;
    } else if (ch == KEY_DOWN || ch == 'j' || ch == 'J') {
      if (selected + 1 < moves->size)
        selected++;
    } else if (ch == KEY_HOME) {
      selected = 0;
    } else if (ch == KEY_END) {
      selected = (uint16_t)(moves->size - 1);
    } else if (ch >= '0' && ch <= '9') {
      uint16_t digit = (uint16_t)(ch - '0');
      if (digit < moves->size)
        selected = digit;
    } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' ') {
      return selected;
    } else if (ch == 'q' || ch == 'Q' || ch == 27) {
      endwin();
      printf("\n[CTE] Game exited by user.\n");
      exit(0);
    }
  }
}

static void tui_on_round_start(uint8_t round_nb, void *ui_ctx) {
  s_tui_ui_ctx *ctx = (s_tui_ui_ctx *)ui_ctx;
  if (ctx) {
    ctx->cur_round = round_nb;
    snprintf(ctx->last_log, sizeof(ctx->last_log), "Starting Round %u",
             (unsigned)round_nb);
  }
}

static void tui_on_move_played(const struct s_cte_player_data *player,
                               const struct s_cte_move *move, bool captured,
                               void *ui_ctx) {
  (void)captured;
  s_tui_ui_ctx *ctx = (s_tui_ui_ctx *)ui_ctx;
  if (!ctx)
    return;

  char move_str[128];
  format_move(move_str, sizeof(move_str), move, ctx->style);
  snprintf(ctx->last_log, sizeof(ctx->last_log), "[%s] played : %s",
           player ? player->player_name : "Player", move_str);

  // If bot move, short pause to see bot play
  if (player && !player->is_human) {
    napms(200);
  }
}

static void tui_on_round_end(const struct s_cte_players *players,
                             const s_cte_round_score scores[], void *ui_ctx) {
  s_tui_ui_ctx *ctx = (s_tui_ui_ctx *)ui_ctx;
  erase();

  attron(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
  mvprintw(2, 4, " ================= ROUND %u SUMMARY ================= ",
           (unsigned)(ctx ? ctx->cur_round : 1));
  attroff(COLOR_PAIR(PAIR_HEADER) | A_BOLD);

  for (uint8_t i = 0; i < players->size; i++) {
    const struct s_cte_player_data *p = &players->players[i];
    mvprintw(4 + i, 6,
             "* %-18s : %2u card pts + %u majority (%2u cards) + %u tablic = "
             "%2u pts",
             p->player_name, (unsigned)scores[i].card_points,
             (unsigned)scores[i].majority_bonus, (unsigned)p->won_cards.size,
             (unsigned)scores[i].tablic_points, (unsigned)scores[i].total);
  }

  if (ctx && ctx->is_team_mode && players->size == 4) {
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

static void tui_on_match_end(const struct s_cte_match *match, int8_t winner_id,
                             void *ui_ctx) {
  (void)ui_ctx;
  erase();

  attron(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
  mvprintw(2, 4, " ================= MATCH FINISHED ================= ");
  attroff(COLOR_PAIR(PAIR_HEADER) | A_BOLD);

  if (match->is_team_mode && match->game && match->game->players.size == 4) {
    if (winner_id == 0) {
      mvprintw(5, 6, "WINNER: Team 1 (%s & %s) with %u points! (Rounds: %u)",
               match->game->players.players[0].player_name,
               match->game->players.players[2].player_name,
               (unsigned)match->match_scores[0], (unsigned)match->round_nb);
    } else if (winner_id == 1) {
      mvprintw(5, 6, "WINNER: Team 2 (%s & %s) with %u points! (Rounds: %u)",
               match->game->players.players[1].player_name,
               match->game->players.players[3].player_name,
               (unsigned)match->match_scores[1], (unsigned)match->round_nb);
    } else {
      mvprintw(5, 6, "Match finished in a DRAW! (Rounds: %u)",
               (unsigned)match->round_nb);
    }
  } else if (match->game) {
    if (winner_id >= 0 && winner_id < (int8_t)match->game->players.size) {
      mvprintw(5, 6, "WINNER: %s with %u points! (Rounds: %u)",
               match->game->players.players[winner_id].player_name,
               (unsigned)match->match_scores[winner_id],
               (unsigned)match->round_nb);
    } else {
      mvprintw(5, 6, "Match finished in a DRAW! (Rounds: %u)",
               (unsigned)match->round_nb);
    }
  }

  attron(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
  mvprintw(9, 6, " [Press any key to continue...] ");
  attroff(COLOR_PAIR(PAIR_SELECT) | A_BOLD);

  refresh();
  getch();
}

static const s_cte_ui_callbacks g_tui_callbacks = {
    .on_round_start = tui_on_round_start,
    .on_deal = NULL,
    .on_turn_start = NULL,
    .on_move_played = tui_on_move_played,
    .on_round_end = tui_on_round_end,
    .on_match_end = tui_on_match_end,
};

static t_evaluator tui_get_evaluator(e_cli_ai_type type,
                                     const char **name_out) {
  switch (type) {
  case AI_TYPE_DUMB:
    if (name_out)
      *name_out = "Dumb";
    return eval_dumb;
  case AI_TYPE_GREEDY:
    if (name_out)
      *name_out = "Greedy";
    return eval_greedy;
  case AI_TYPE_CHEATER:
    if (name_out)
      *name_out = "Cheater";
    return eval_cheater;
  case AI_TYPE_RANDOM:
  default:
    if (name_out)
      *name_out = "Random";
    return eval_random;
  }
}

int run_tui_frontend(const s_cte_tui_config *config) {
  if (!config)
    return 1;

  uint8_t nb_p = config->nb_players;
  if (nb_p < 2 || nb_p > 4) {
    fprintf(stderr, "Error: TUI requires 2 to 4 players (got %u)\n",
            (unsigned)nb_p);
    return 1;
  }

  if (config->is_team_mode && nb_p != 4) {
    fprintf(stderr, "Error: Team mode requires exactly 4 players.\n");
    return 1;
  }

  s_tui_ui_ctx ui_ctx = {
      .style = config->style,
      .is_team_mode = config->is_team_mode,
      .last_log = "Game started",
      .cur_round = 1,
      .last_player = NULL,
      .last_move = NULL,
  };

  char names_buf[4][32];
  char *names[4];
  bool slot_is_human[4];
  t_evaluator slot_evaluators[4];

  for (uint8_t i = 0; i < nb_p; i++) {
    names[i] = names_buf[i];
    char base_name[24];

    if (config->game_type == GAME_HUMAN_VS_HUMAN) {
      slot_is_human[i] = true;
      slot_evaluators[i] = tui_read_human_move;
      if (i == 0 && config->profile_name[0] != '\0') {
        snprintf(base_name, sizeof(base_name), "%.23s", config->profile_name);
      } else {
        snprintf(base_name, sizeof(base_name), "Player %u (Human)",
                 (unsigned)(i + 1));
      }
    } else if (config->game_type == GAME_HUMAN_VS_AI) {
      if (i == 0) {
        slot_is_human[i] = true;
        slot_evaluators[i] = tui_read_human_move;
        if (config->profile_name[0] != '\0') {
          snprintf(base_name, sizeof(base_name), "%.23s", config->profile_name);
        } else {
          snprintf(base_name, sizeof(base_name), "Human (P1)");
        }
      } else {
        slot_is_human[i] = false;
        uint8_t ai_idx = (config->nb_ai_types > 1) ? (uint8_t)(i - 1) : 0;
        const char *strat_name = NULL;
        slot_evaluators[i] =
            tui_get_evaluator(config->ai_types[ai_idx % 4], &strat_name);
        snprintf(base_name, sizeof(base_name), "Bot %u (%s)", (unsigned)i,
                 strat_name);
      }
    } else { // AI vs AI
      slot_is_human[i] = false;
      uint8_t ai_idx = (config->nb_ai_types > 1) ? i : 0;
      const char *strat_name = NULL;
      slot_evaluators[i] =
          tui_get_evaluator(config->ai_types[ai_idx % 4], &strat_name);
      snprintf(base_name, sizeof(base_name), "Bot %u (%s)", (unsigned)(i + 1),
               strat_name);
    }

    if (config->is_team_mode && nb_p == 4) {
      uint8_t team_num = (uint8_t)((i % 2) + 1);
      snprintf(names[i], sizeof(names_buf[i]), "%s [T%u]", base_name,
               (unsigned)team_num);
    } else {
      snprintf(names[i], sizeof(names_buf[i]), "%s", base_name);
    }
  }

  s_cte_game game;
  t_cteerr err = init_game(&game, nb_p, names, config->is_team_mode);
  if (err != e_ok) {
    fprintf(stderr, "Error: Failed to initialize game (code: %u)\n", err);
    return 1;
  }

  for (uint8_t i = 0; i < nb_p; i++) {
    game.players.players[i].is_human = slot_is_human[i];
    game.players.players[i].evaluator = slot_evaluators[i];
    game.players.players[i].eval_context = &ui_ctx;
  }

  struct s_cte_match match;
  err = init_match(&match, &game, config->winning_score);
  if (err != e_ok) {
    fprintf(stderr, "Error: Failed to initialize match (code: %u)\n", err);
    free_game(&game);
    return 1;
  }
  match.max_rounds = config->max_rounds;
  match.is_team_mode = config->is_team_mode;

  s_cte_round_config round_config = {
      .first_player = 0,
      .is_team_mode = config->is_team_mode,
      .evaluators = {NULL, NULL, NULL, NULL},
      .eval_contexts = {NULL, NULL, NULL, NULL},
      .callbacks = &g_tui_callbacks,
      .ui_context = &ui_ctx,
  };

  // Initialize ncurses with UTF-8 locale if not already active
  bool local_curses = false;
  if (!stdscr || isendwin()) {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    init_tui_colors();
    local_curses = true;
  }

  err = run_match(&match, &round_config);

  if (local_curses) {
    endwin();
  }

  if (err == e_ok && config->profile_name[0] != '\0' && config->game_type != GAME_AI_VS_AI) {
    s_cte_profile_db db;
    if (init_profile_db(&db, NULL) == e_ok) {
      s_cte_profile *p = find_or_create_profile(&db, config->profile_name);
      if (p) {
        p->total_points += match.match_scores[0];
        p->total_tablics += match.match_tablics[0];
        p->matches_played++;

        int16_t opp_elo = CTE_DEFAULT_ELO;
        if (config->nb_players >= 2) {
          if (!game.players.players[1].is_human && config->nb_ai_types > 0) {
            opp_elo = cte_default_ai_elo(config->ai_types[0]);
          } else {
            s_cte_profile *opp_p = find_profile(&db, names[1]);
            if (opp_p) opp_elo = opp_p->elo;
          }
        }

        uint16_t s0 = match.match_scores[0];
        uint16_t s1 = (config->nb_players >= 2) ? match.match_scores[1] : 0;
        double score = 0.5;
        if (s0 > s1) {
          p->matches_won++;
          score = 1.0;
        } else if (s1 > s0) {
          p->matches_lost++;
          score = 0.0;
        } else {
          p->matches_tied++;
          score = 0.5;
        }

        p->elo += compute_elo_delta(p->elo, opp_elo, score, CTE_DEFAULT_K_FACTOR);
        if (p->elo < 100) p->elo = 100;
        p->last_played_at = (uint64_t)time(NULL);
        save_profiles(&db);
      }
    }
  }

  if (err != e_ok) {
    fprintf(stderr, "Error during TUI match execution (code: %u)\n", err);
    free_game(&game);
    return 1;
  }

  free_game(&game);
  return 0;
}

// -----------------------------------------------------------------------------
// Interactive TUI Main Menu & Subscreens
// -----------------------------------------------------------------------------

static void tui_draw_box(int y, int x, int h, int w, const char *title) {
  attron(COLOR_PAIR(PAIR_BORDER));
  mvaddch(y, x, ACS_ULCORNER);
  mvaddch(y, x + w - 1, ACS_URCORNER);
  mvaddch(y + h - 1, x, ACS_LLCORNER);
  mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);
  for (int j = 1; j < w - 1; j++) {
    mvaddch(y, x + j, ACS_HLINE);
    mvaddch(y + h - 1, x + j, ACS_HLINE);
  }
  for (int i = 1; i < h - 1; i++) {
    mvaddch(y + i, x, ACS_VLINE);
    mvaddch(y + i, x + w - 1, ACS_VLINE);
  }
  attroff(COLOR_PAIR(PAIR_BORDER));

  if (title && title[0]) {
    attron(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    mvprintw(y, x + 2, " %s ", title);
    attroff(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
  }
}

static void tui_menu_quick_match(void) {
  uint8_t players = 2;
  e_cli_game_type gtype = GAME_HUMAN_VS_AI;
  bool team_mode = false;
  e_cli_ai_type ai_strat = AI_TYPE_GREEDY;
  uint16_t win_score = 101;
  e_cte_render_style style = CTE_RENDER_UNICODE;
  char profile_name[32] = {0};

  s_cte_profile_db db;
  bool has_db = (init_profile_db(&db, NULL) == e_ok);
  int cur_profile_idx = -1;

  int selected = 0;
  const int total_items = 9;

  for (;;) {
    erase();
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    (void)max_y;

    int box_w = 64;
    int box_h = 22;
    int start_x = (max_x > box_w) ? (max_x - box_w) / 2 : 1;
    int start_y = 2;

    tui_draw_box(start_y, start_x, box_h, box_w, "QUICK MATCH SETUP");

    const char *gtype_str = (gtype == GAME_HUMAN_VS_AI)      ? "Human vs AI"
                            : (gtype == GAME_HUMAN_VS_HUMAN) ? "Human vs Human"
                                                             : "AI vs AI";
    const char *ai_str = (ai_strat == AI_TYPE_GREEDY)    ? "Greedy"
                         : (ai_strat == AI_TYPE_CHEATER) ? "Cheater (Minimax)"
                         : (ai_strat == AI_TYPE_RANDOM)  ? "Random"
                                                         : "Dumb";
    const char *style_str = (style == CTE_RENDER_UNICODE) ? "Unicode" : "ASCII";

    char items[9][64];
    snprintf(items[0], sizeof(items[0]), "1. Number of Players : < %u >",
             (unsigned)players);
    snprintf(items[1], sizeof(items[1]), "2. Game Mode         : < %s >",
             gtype_str);
    snprintf(items[2], sizeof(items[2]), "3. Team Mode (2v2)   : < %s >",
             team_mode ? "ON" : "OFF");
    snprintf(items[3], sizeof(items[3]), "4. AI Strategy       : < %s >",
             ai_str);
    snprintf(items[4], sizeof(items[4]), "5. Target Score      : < %u points >",
             (unsigned)win_score);
    snprintf(items[5], sizeof(items[5]), "6. Card Render Style : < %s >",
             style_str);
    snprintf(items[6], sizeof(items[6]), "7. Player Profile    : < %s >",
             profile_name[0] ? profile_name : "None");
    snprintf(items[7], sizeof(items[7]), "[ START MATCH ]");
    snprintf(items[8], sizeof(items[8]), "[ Back to Main Menu ]");

    for (int i = 0; i < total_items; i++) {
      int row_y = start_y + 3 + (i >= 7 ? i + 1 : i);
      if (i == selected) {
        attron(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
        mvprintw(row_y, start_x + 4, " ->  %-48s ", items[i]);
        attroff(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
      } else {
        mvprintw(row_y, start_x + 4, "     %-48s ", items[i]);
      }
    }

    attron(COLOR_PAIR(PAIR_ACCENT));
    mvprintw(start_y + box_h - 2, start_x + 3,
             "[↑/↓] Navigate  [←/→/Space] Change  [Enter] Select  [Q] Back");
    attroff(COLOR_PAIR(PAIR_ACCENT));

    refresh();
    int ch = getch();

    if (ch == KEY_UP || ch == 'k' || ch == 'K') {
      if (selected > 0)
        selected--;
    } else if (ch == KEY_DOWN || ch == 'j' || ch == 'J') {
      if (selected < total_items - 1)
        selected++;
    } else if (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ') {
      if (selected == 0) {
        players = (players == 2) ? 3 : (players == 3 ? 4 : 2);
        if (players != 4)
          team_mode = false;
      } else if (selected == 1) {
        gtype = (gtype == GAME_HUMAN_VS_AI)      ? GAME_HUMAN_VS_HUMAN
                : (gtype == GAME_HUMAN_VS_HUMAN) ? GAME_AI_VS_AI
                                                 : GAME_HUMAN_VS_AI;
      } else if (selected == 2) {
        if (players == 4)
          team_mode = !team_mode;
      } else if (selected == 3) {
        ai_strat = (ai_strat == AI_TYPE_GREEDY)    ? AI_TYPE_CHEATER
                   : (ai_strat == AI_TYPE_CHEATER) ? AI_TYPE_RANDOM
                   : (ai_strat == AI_TYPE_RANDOM)  ? AI_TYPE_DUMB
                                                   : AI_TYPE_GREEDY;
      } else if (selected == 4) {
        win_score = (win_score == 101) ? 51 : (win_score == 51 ? 25 : 101);
      } else if (selected == 5) {
        style = (style == CTE_RENDER_UNICODE) ? CTE_RENDER_ASCII
                                              : CTE_RENDER_UNICODE;
      } else if (selected == 6) {
        // Cycle profiles
        if (has_db && db.count > 0) {
          cur_profile_idx++;
          if (cur_profile_idx >= db.count) {
            cur_profile_idx = -1;
            profile_name[0] = '\0';
          } else {
            snprintf(profile_name, sizeof(profile_name), "%.31s", db.profiles[cur_profile_idx].name);
          }
        }
      }
    } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
      if (selected == 6) {
        echo();
        curs_set(1);
        char new_name[32] = {0};
        mvprintw(start_y + box_h - 2, start_x + 3,
                 "Enter profile name (max 20 chars):                         ");
        move(start_y + box_h - 2, start_x + 38);
        getnstr(new_name, 20);
        noecho();
        curs_set(0);
        if (strlen(new_name) > 0) {
          snprintf(profile_name, sizeof(profile_name), "%.31s", new_name);
          if (has_db) {
            find_or_create_profile(&db, profile_name);
            save_profiles(&db);
          }
        }
      } else if (selected == 7) {
        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
          srand((unsigned int)(ts.tv_nsec ^ ts.tv_sec));
        }

        s_cte_tui_config cfg = {
            .nb_players = players,
            .is_team_mode = team_mode,
            .game_type = gtype,
            .nb_ai_types = 1,
            .ai_types = {ai_strat, ai_strat, ai_strat, ai_strat},
            .style = style,
            .winning_score = win_score,
            .max_rounds = 0,
        };
        if (profile_name[0] != '\0') {
          snprintf(cfg.profile_name, sizeof(cfg.profile_name), "%.31s", profile_name);
        }
        run_tui_frontend(&cfg);
        if (has_db) {
          load_profiles(&db);
        }
      } else if (selected == 8) {
        return;
      }
    } else if (ch == 'q' || ch == 'Q' || ch == 27) {
      return;
    }
  }
}

static void tui_edit_participants(s_tui_participant_slot *slots, uint8_t *nb_slots) {
  int cur = 0;
  char notice[64] = {0};

  for (;;) {
    erase();
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    (void)max_y;

    int box_w = 70;
    int box_h = 7 + *nb_slots;
    if (box_h < 15) box_h = 15;
    int start_x = (max_x > box_w) ? (max_x - box_w) / 2 : 1;
    int start_y = 2;

    char title[64];
    snprintf(title, sizeof(title), "TOURNAMENT PARTICIPANTS (%u / %u max)",
             (unsigned)*nb_slots, TUI_MAX_PARTICIPANTS);
    tui_draw_box(start_y, start_x, box_h, box_w, title);

    for (uint8_t i = 0; i < *nb_slots; i++) {
      int row_y = start_y + 2 + i;
      char line_buf[96];
      if (slots[i].is_human) {
        snprintf(line_buf, sizeof(line_buf), "#%u [Human]  : %-18.18s (Human Player)",
                 (unsigned)(i + 1), slots[i].name);
      } else {
        const char *strat = (slots[i].ai_type == AI_TYPE_CHEATER) ? "Cheater"
                          : (slots[i].ai_type == AI_TYPE_GREEDY)  ? "Greedy"
                          : (slots[i].ai_type == AI_TYPE_RANDOM)  ? "Random"
                                                                  : "Dumb";
        snprintf(line_buf, sizeof(line_buf), "#%u [AI]     : %-18.18s [%-7s] (Elo: %d)",
                 (unsigned)(i + 1), slots[i].name, strat,
                 (int)cte_default_ai_elo(slots[i].ai_type));
      }

      if (i == cur) {
        attron(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
        mvprintw(row_y, start_x + 3, " -> %-58s ", line_buf);
        attroff(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
      } else {
        mvprintw(row_y, start_x + 3, "    %-58s ", line_buf);
      }
    }

    if (notice[0]) {
      attron(COLOR_PAIR(PAIR_ALERT) | A_BOLD);
      mvprintw(start_y + box_h - 4, start_x + 3, "%-62s", notice);
      attroff(COLOR_PAIR(PAIR_ALERT) | A_BOLD);
    }

    attron(COLOR_PAIR(PAIR_ACCENT));
    mvprintw(start_y + box_h - 3, start_x + 3,
             "[A] Add   [D] Delete   [Enter] Edit Name   [←/→/Space] Change Type");
    mvprintw(start_y + box_h - 2, start_x + 3,
             "[↑/↓] Navigate         [Q / Esc] Finish & Return");
    attroff(COLOR_PAIR(PAIR_ACCENT));

    refresh();
    int ch = getch();
    notice[0] = '\0';

    if (ch == KEY_UP || ch == 'k' || ch == 'K') {
      if (cur > 0) cur--;
    } else if (ch == KEY_DOWN || ch == 'j' || ch == 'J') {
      if (cur < *nb_slots - 1) cur++;
    } else if (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ') {
      // Cycle: Human -> Random -> Dumb -> Greedy -> Cheater -> Human
      if (slots[cur].is_human) {
        slots[cur].is_human = false;
        slots[cur].ai_type = AI_TYPE_RANDOM;
        snprintf(slots[cur].name, sizeof(slots[cur].name), "Bot_Random_%u", (unsigned)(cur + 1));
      } else if (slots[cur].ai_type == AI_TYPE_RANDOM) {
        slots[cur].ai_type = AI_TYPE_DUMB;
        snprintf(slots[cur].name, sizeof(slots[cur].name), "Bot_Dumb_%u", (unsigned)(cur + 1));
      } else if (slots[cur].ai_type == AI_TYPE_DUMB) {
        slots[cur].ai_type = AI_TYPE_GREEDY;
        snprintf(slots[cur].name, sizeof(slots[cur].name), "Bot_Greedy_%u", (unsigned)(cur + 1));
      } else if (slots[cur].ai_type == AI_TYPE_GREEDY) {
        slots[cur].ai_type = AI_TYPE_CHEATER;
        snprintf(slots[cur].name, sizeof(slots[cur].name), "Bot_Cheater_%u", (unsigned)(cur + 1));
      } else {
        // From Cheater: check if another human already exists
        bool already_has_human = false;
        for (uint8_t j = 0; j < *nb_slots; j++) {
          if (j != cur && slots[j].is_human) {
            already_has_human = true;
            break;
          }
        }
        if (already_has_human) {
          slots[cur].ai_type = AI_TYPE_RANDOM;
          snprintf(slots[cur].name, sizeof(slots[cur].name), "Bot_Random_%u", (unsigned)(cur + 1));
          snprintf(notice, sizeof(notice), "Notice: Maximum 1 human player allowed per tournament.");
        } else {
          slots[cur].is_human = true;
          snprintf(slots[cur].name, sizeof(slots[cur].name), "Player");
        }
      }
    } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == 'e' || ch == 'E') {
      echo();
      curs_set(1);
      char new_name[32] = {0};
      mvprintw(start_y + box_h - 4, start_x + 3,
               "Enter name for #%u (max 20 chars):                         ", (unsigned)(cur + 1));
      move(start_y + box_h - 4, start_x + 38);
      getnstr(new_name, 20);
      noecho();
      curs_set(0);
      if (strlen(new_name) > 0) {
        snprintf(slots[cur].name, sizeof(slots[cur].name), "%.31s", new_name);
      }
    } else if (ch == 'a' || ch == 'A') {
      if (*nb_slots < TUI_MAX_PARTICIPANTS) {
        slots[*nb_slots].is_human = false;
        slots[*nb_slots].ai_type = AI_TYPE_RANDOM;
        snprintf(slots[*nb_slots].name, sizeof(slots[*nb_slots].name), "Bot_Random_%u", (unsigned)(*nb_slots + 1));
        (*nb_slots)++;
        cur = *nb_slots - 1;
      } else {
        snprintf(notice, sizeof(notice), "Maximum participants reached (%u).", TUI_MAX_PARTICIPANTS);
      }
    } else if (ch == 'd' || ch == 'D') {
      if (*nb_slots > 2) {
        for (uint8_t j = cur; j < *nb_slots - 1; j++) {
          slots[j] = slots[j + 1];
        }
        (*nb_slots)--;
        if (cur >= *nb_slots) cur = *nb_slots - 1;
      } else {
        snprintf(notice, sizeof(notice), "Tournament requires at least 2 participants.");
      }
    } else if (ch == 'q' || ch == 'Q' || ch == 27) {
      return;
    }
  }
}

static void tui_menu_tournament(void) {
  e_cte_tournament_type type = TOURNAMENT_ROUND_ROBIN;
  s_tui_participant_slot slots[TUI_MAX_PARTICIPANTS] = {
      { false, AI_TYPE_CHEATER, "Bot_Cheater" },
      { false, AI_TYPE_GREEDY,  "Bot_Greedy"  },
      { false, AI_TYPE_RANDOM,  "Bot_Random"  },
      { false, AI_TYPE_DUMB,    "Bot_Dumb"    },
  };
  uint8_t nb_slots = 4;
  uint16_t win_score = 25;
  bool persist_ai = false;

  int selected = 0;
  const int total_items = 6;

  for (;;) {
    erase();
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    (void)max_y;

    int box_w = 66;
    int box_h = 18;
    int start_x = (max_x > box_w) ? (max_x - box_w) / 2 : 1;
    int start_y = 3;

    tui_draw_box(start_y, start_x, box_h, box_w, "TOURNAMENT ARENA");

    bool ko_invalid = (type == TOURNAMENT_KNOCKOUT && (nb_slots & (nb_slots - 1)) != 0);

    char items[6][64];
    snprintf(items[0], sizeof(items[0]), "1. Format            : < %s >",
             (type == TOURNAMENT_ROUND_ROBIN) ? "Round Robin (Championship)"
                                              : "Knockout Cup (Elimination)");
    snprintf(items[1], sizeof(items[1]),
             "2. Participants      : < %u configured >   [Enter to edit]%s",
             (unsigned)nb_slots,
             ko_invalid ? " (!)" : "");
    snprintf(items[2], sizeof(items[2]), "3. Match Target      : < %u points >",
             (unsigned)win_score);
    snprintf(items[3], sizeof(items[3]), "4. Persist AI Stats  : < %s >",
             persist_ai ? "YES (saved to leaderboard)" : "NO (transient bots)");
    snprintf(items[4], sizeof(items[4]), "[ LAUNCH TOURNAMENT ]");
    snprintf(items[5], sizeof(items[5]), "[ Back to Main Menu ]");

    for (int i = 0; i < total_items; i++) {
      int row_y = start_y + 3 + (i >= 4 ? i + 1 : i);
      if (i == selected) {
        attron(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
        mvprintw(row_y, start_x + 3, " ->  %-58s ", items[i]);
        attroff(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
      } else {
        mvprintw(row_y, start_x + 3, "     %-58s ", items[i]);
      }
    }

    if (ko_invalid) {
      attron(COLOR_PAIR(PAIR_ALERT) | A_BOLD);
      mvprintw(start_y + 11, start_x + 3,
               " (!) Knockout requires power-of-2 participants (2, 4, 8, 16)");
      attroff(COLOR_PAIR(PAIR_ALERT) | A_BOLD);
    }

    attron(COLOR_PAIR(PAIR_ACCENT));
    mvprintw(start_y + box_h - 2, start_x + 3,
             "[↑/↓] Navigate  [←/→/Space] Change  [Enter] Select  [Q] Back");
    attroff(COLOR_PAIR(PAIR_ACCENT));

    refresh();
    int ch = getch();

    if (ch == KEY_UP || ch == 'k' || ch == 'K') {
      if (selected > 0)
        selected--;
    } else if (ch == KEY_DOWN || ch == 'j' || ch == 'J') {
      if (selected < total_items - 1)
        selected++;
    } else if (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ') {
      if (selected == 0) {
        type = (type == TOURNAMENT_ROUND_ROBIN) ? TOURNAMENT_KNOCKOUT
                                                : TOURNAMENT_ROUND_ROBIN;
      } else if (selected == 1) {
        tui_edit_participants(slots, &nb_slots);
      } else if (selected == 2) {
        win_score = (win_score == 25) ? 51 : (win_score == 51 ? 101 : 25);
      } else if (selected == 3) {
        persist_ai = !persist_ai;
      }
    } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
      if (selected == 1) {
        tui_edit_participants(slots, &nb_slots);
      } else if (selected == 3) {
        persist_ai = !persist_ai;
      } else if (selected == 4) {
        if (type == TOURNAMENT_KNOCKOUT && (nb_slots & (nb_slots - 1)) != 0) {
          erase();
          mvprintw(3, 4, "Error: Knockout format requires power-of-2 participants (2, 4, 8, 16).");
          mvprintw(4, 4, "Current participants: %u. Please edit participants or change format.", (unsigned)nb_slots);
          mvprintw(6, 4, "Press any key to return...");
          refresh();
          getch();
          continue;
        }

        s_tui_ui_ctx ui_ctx;
        memset(&ui_ctx, 0, sizeof(ui_ctx));

        s_cte_profile_db profile_db;
        bool has_profile_db = (init_profile_db(&profile_db, NULL) == e_ok);

        s_cte_tournament t;
        s_cte_tournament_config cfg = {
            .type            = type,
            .nb_participants = nb_slots,
            .winning_score   = win_score,
            .max_rounds      = 0,
            .silent          = true,
            .callbacks       = &g_tui_callbacks,
            .ui_context      = &ui_ctx,
            .profile_db      = has_profile_db ? &profile_db : NULL,
            .persist_ai      = persist_ai,
        };

        for (uint8_t i = 0; i < nb_slots; i++) {
          snprintf(cfg.participants[i].name, sizeof(cfg.participants[i].name), "%.31s", slots[i].name);
          cfg.participants[i].is_human = slots[i].is_human;
          cfg.participants[i].ai_type  = slots[i].ai_type;
          if (slots[i].is_human) {
            cfg.participants[i].evaluator = tui_read_human_move;
            cfg.participants[i].eval_context = &ui_ctx;
          } else {
            const char *dummy = NULL;
            cfg.participants[i].evaluator = tui_get_evaluator(slots[i].ai_type, &dummy);
            cfg.participants[i].eval_context = NULL;
          }
        }

        if (init_tournament(&t, &cfg) == e_ok) {
          struct timespec ts;
          if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
            srand((unsigned int)(ts.tv_nsec ^ ts.tv_sec));
          }

          erase();
          mvprintw(1, 2, "Running tournament... Please wait...");
          refresh();
          run_tournament(&t);
          if (has_profile_db) {
            sync_tournament_profiles(&t);
          }

          erase();
          getmaxyx(stdscr, max_y, max_x);
          int max_avail_h = (max_y > 4) ? max_y - 2 : 10;
          int res_h = 10 + nb_slots;
          if (res_h > max_avail_h) res_h = max_avail_h;

          int res_w = (max_x >= 80) ? 78 : (max_x > 4 ? max_x - 2 : 78);
          int res_x = (max_x > res_w) ? (max_x - res_w) / 2 : 1;
          tui_draw_box(1, res_x, res_h, res_w,
                       (type == TOURNAMENT_ROUND_ROBIN)
                           ? "ROUND ROBIN RESULTS"
                           : "KNOCKOUT CUP RESULTS");

          if (t.champion_idx >= 0 &&
              t.champion_idx < t.config.nb_participants) {
            attron(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
            mvprintw(3, res_x + 4, " >>> TOURNAMENT CHAMPION: %s <<< ",
                     t.config.participants[t.champion_idx].name);
            attroff(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
          }

          attron(A_BOLD);
          mvprintw(5, res_x + 3,
                   "Rank | %-16s | Elo Bef | Elo Aft | Delta | Won | Lost | Pts   | Win%%",
                   "Participant");
          attroff(A_BOLD);
          mvprintw(
              6, res_x + 3,
              "-----|------------------|---------|---------|-------|-----|------|-------|------");

          for (uint8_t r = 0; r < t.config.nb_participants; r++) {
            if (7 + r >= res_h - 1) {
              mvprintw(7 + r, res_x + 3, "... (%u more participants omitted) ...",
                       (unsigned)(t.config.nb_participants - r));
              break;
            }
            uint8_t idx = t.standings[r];
            const s_cte_tournament_participant *p = &t.config.participants[idx];
            double wr =
                (p->matches_played > 0)
                    ? ((double)p->matches_won / p->matches_played) * 100.0
                    : 0.0;
            int16_t delta = p->elo_current - p->elo_start;
            mvprintw(7 + r, res_x + 3,
                     " %2u  | %-16s |  %5d  |  %5d  | %+5d | %3u |  %3u | %5u | %4.0f%%",
                     (unsigned)(r + 1), p->name, (int)p->elo_start,
                     (int)p->elo_current, (int)delta, (unsigned)p->matches_won,
                     (unsigned)p->matches_lost, (unsigned)p->total_points, wr);
          }

          attron(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);
          mvprintw(res_h, res_x + 4,
                   " [Press any key to return to tournament menu...] ");
          attroff(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);

          refresh();
          getch();
          free_tournament(&t);
        }
      } else if (selected == 5) {
        return;
      }
    } else if (ch == 'q' || ch == 'Q' || ch == 27) {
      return;
    }
  }
}

static void tui_menu_leaderboard(void) {
  s_cte_profile_db db;
  init_profile_db(&db, NULL);

  for (;;) {
    erase();
    sort_profiles_by_elo(&db);

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    (void)max_y;

    int box_w = 76;
    int box_h = 18;
    int start_x = (max_x > box_w) ? (max_x - box_w) / 2 : 1;
    int start_y = 2;

    tui_draw_box(start_y, start_x, box_h, box_w,
                 "PLAYER PROFILES & ELO LEADERBOARD");

    attron(A_BOLD);
    mvprintw(start_y + 2, start_x + 3,
             "Rank | %-18s |  Elo  | Won | Lost | Tied |  Win%% | Tablics",
             "Player Name");
    attroff(A_BOLD);
    mvprintw(start_y + 3, start_x + 3,
             "-----|--------------------|-------|-----|------|------|-------|--"
             "------");

    if (db.count == 0) {
      mvprintw(start_y + 5, start_x + 8, "No profiles found in database.");
      mvprintw(start_y + 6, start_x + 8, "Press [A] to create a new profile.");
    } else {
      uint16_t show_count = (db.count > 10) ? 10 : db.count;
      for (uint16_t i = 0; i < show_count; i++) {
        const s_cte_profile *p = &db.profiles[i];
        double wr = (p->matches_played > 0)
                        ? ((double)p->matches_won / p->matches_played) * 100.0
                        : 0.0;
        mvprintw(start_y + 4 + i, start_x + 3,
                 " %2u  | %-18s | %5d | %3u |  %3u |  %3u | %4.0f%% |  %4u",
                 (unsigned)(i + 1), p->name, (int)p->elo,
                 (unsigned)p->matches_won, (unsigned)p->matches_lost,
                 (unsigned)p->matches_tied, wr, (unsigned)p->total_tablics);
      }
    }

    attron(COLOR_PAIR(PAIR_ACCENT));
    mvprintw(start_y + box_h - 2, start_x + 3,
             "[A] Add New Profile    [Q / Esc / Enter] Back to Menu");
    attroff(COLOR_PAIR(PAIR_ACCENT));

    refresh();
    int ch = getch();

    if (ch == 'a' || ch == 'A') {
      echo();
      curs_set(1);
      char new_name[32] = {0};
      mvprintw(start_y + box_h - 2, start_x + 3,
               "Enter profile name (max 20 chars):                         ");
      move(start_y + box_h - 2, start_x + 38);
      getnstr(new_name, 20);
      noecho();
      curs_set(0);

      if (strlen(new_name) > 0) {
        find_or_create_profile(&db, new_name);
        save_profiles(&db);
      }
    } else if (ch == 'q' || ch == 'Q' || ch == 27 || ch == '\n' || ch == '\r' ||
               ch == KEY_ENTER) {
      return;
    }
  }
}

static void tui_menu_rules(void) {
  erase();
  int max_y, max_x;
  getmaxyx(stdscr, max_y, max_x);
  (void)max_y;

  int box_w = 78;
  int box_h = 24;
  int start_x = (max_x > box_w) ? (max_x - box_w) / 2 : 1;
  int start_y = 1;

  tui_draw_box(start_y, start_x, box_h, box_w, "RULES OF TABLIĆ");

  mvprintw(start_y + 2, start_x + 3, "OBJECTIVE:");
  mvprintw(start_y + 3, start_x + 5,
           "Be the first player (or team) to reach the winning score (default "
           "101).");

  mvprintw(start_y + 5, start_x + 3, "CARD VALUES:");
  mvprintw(start_y + 6, start_x + 5, "• 2 to 10 : Face value (e.g. 7 = 7)");
  mvprintw(start_y + 7, start_x + 5,
           "• Ace     : 1 or 11 (adaptable to combinations)");
  mvprintw(start_y + 8, start_x + 5,
           "• Jack    : 12   • Queen : 13   • King : 14");

  mvprintw(start_y + 10, start_x + 3, "GAMEPLAY & CAPTURES:");
  mvprintw(start_y + 11, start_x + 5, "• Play 1 card per turn from your hand.");
  mvprintw(start_y + 12, start_x + 5,
           "• Capture any table card with the same value, OR any combination");
  mvprintw(start_y + 13, start_x + 5,
           "  of table cards that sum up to your played card.");
  mvprintw(start_y + 14, start_x + 5,
           "• Multiple distinct combinations can be captured simultaneously!");
  mvprintw(start_y + 15, start_x + 5,
           "• If no capture is possible, the card is dropped onto the table.");

  mvprintw(start_y + 17, start_x + 3, "TABLIĆ & SCORING:");
  mvprintw(start_y + 18, start_x + 5,
           "• TABLIĆ: Clearing all cards on the table awards +1 bonus point!");
  mvprintw(start_y + 19, start_x + 5,
           "• Most cards won at end of round: +3 points (split if tied).");
  mvprintw(
      start_y + 20, start_x + 5,
      "• Each 10, Jack, Queen, King, Ace: 1 point. 10 of Diamonds: 2 points.");
  mvprintw(start_y + 21, start_x + 5,
           "• 2 of Clubs: 1 point. Total base round card points: 25 points.");

  attron(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
  mvprintw(start_y + box_h - 2, start_x + 16,
           " [Press any key to return to main menu...] ");
  attroff(COLOR_PAIR(PAIR_SELECT) | A_BOLD);

  refresh();
  getch();
}

int run_tui_main_menu(void) {
  setlocale(LC_ALL, "");

  struct timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
    srand((unsigned int)(ts.tv_nsec ^ ts.tv_sec));
  } else {
    srand((unsigned int)time(NULL));
  }

  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);
  init_tui_colors();

  int selected = 0;
  const int total_items = 5;
  const char *menu_labels[5] = {
      "1. Quick Match           (Partie Rapide)",
      "2. Tournament Arena      (Championnats/Coupes)",
      "3. Leaderboard & Profiles(Classements/Elo)",
      "4. Rules & Instructions  (Règles du Tablić)",
      "5. Quit CTE              (Quitter)"};

  for (;;) {
    erase();
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    (void)max_y;

    int box_w = 64;
    int box_h = 24;
    int start_x = (max_x > box_w) ? (max_x - box_w) / 2 : 1;
    int start_y = 1;

    tui_draw_box(start_y, start_x, box_h, box_w, "CTE — TABLIĆ ENGINE v1.0");

    // ASCII Art Banner from logo.txt
    attron(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);
    mvprintw(start_y + 2, start_x + 25, "┏━━━┓━━━━┓━━━┓");
    mvprintw(start_y + 3, start_x + 25, "┃┏━┓┃┏┓┏┓┃┏━━┛");
    mvprintw(start_y + 4, start_x + 25, "┃┃━┗┛┛┃┃┗┛┗━━┓");
    mvprintw(start_y + 5, start_x + 25, "┃┃━┏┓━┃┃━━┏━━┛");
    mvprintw(start_y + 6, start_x + 25, "┃┗━┛┃┏┛┗┓━┗━━┓");
    mvprintw(start_y + 7, start_x + 25, "┗━━━┛┗━━┛━━━━┛");
    mvprintw(start_y + 8, start_x + 25, "━━━━━━━━━━━━━━");
    attroff(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);

    mvprintw(start_y + 10, start_x + 21, "Tablić Card Game Suite");

    for (int i = 0; i < total_items; i++) {
      int row_y = start_y + 13 + i;
      if (i == selected) {
        attron(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
        mvprintw(row_y, start_x + 8, "  -->  %-40s  ", menu_labels[i]);
        attroff(COLOR_PAIR(PAIR_SELECT) | A_BOLD);
      } else {
        mvprintw(row_y, start_x + 8, "       %-40s  ", menu_labels[i]);
      }
    }

    attron(COLOR_PAIR(PAIR_ACCENT));
    mvprintw(start_y + box_h - 2, start_x + 8,
             "[↑/↓] Navigate   [Enter/1-5] Select   [Q] Quit");
    attroff(COLOR_PAIR(PAIR_ACCENT));

    refresh();
    int ch = getch();

    if (ch == KEY_UP || ch == 'k' || ch == 'K') {
      if (selected > 0)
        selected--;
    } else if (ch == KEY_DOWN || ch == 'j' || ch == 'J') {
      if (selected < total_items - 1)
        selected++;
    } else if (ch >= '1' && ch <= '5') {
      selected = ch - '1';
      ch = '\n'; // Trigger selection
    } else if (ch == 'q' || ch == 'Q' || ch == 27) {
      break;
    }

    if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
      if (selected == 0) {
        tui_menu_quick_match();
      } else if (selected == 1) {
        tui_menu_tournament();
      } else if (selected == 2) {
        tui_menu_leaderboard();
      } else if (selected == 3) {
        tui_menu_rules();
      } else if (selected == 4) {
        break;
      }
    }
  }

  endwin();
  return 0;
}
