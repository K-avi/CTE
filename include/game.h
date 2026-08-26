#ifndef __CTE_GAME_H
#define __CTE_GAME_H

#include "card.h"
#include "player.h"
#include "move.h"
#include "eval.h"

// Round score summary for a player
typedef struct {
    uint8_t card_points;     // Points from captured cards
    uint8_t majority_bonus;  // +3 if >= 27 cards and strict majority, else 0
    uint8_t tablic_points;   // = nb_tablic
    uint8_t total;           // card_points + majority_bonus + tablic_points
} s_cte_round_score;

// Match management structure
struct s_cte_match {
    struct s_cte_players *players;
    uint16_t match_scores[4]; // Cumulative score per player
    uint16_t winning_score;   // Configurable (e.g. 51, 101, 201)
    uint8_t  round_nb;        // Current round index (0-indexed)
    uint8_t  max_rounds;      // Optional round limit (0 = unlimited / by score)
};

// UI Event Observer Callbacks
typedef struct s_cte_ui_callbacks {
    void (*on_round_start)(uint8_t round_nb, void *ui_ctx);
    void (*on_deal)(const struct s_cte_players *players, void *ui_ctx);
    void (*on_turn_start)(const s_cte_game_state *state, const struct s_cte_move_list *moves, void *ui_ctx);
    void (*on_move_played)(const struct s_cte_player_data *player, const struct s_cte_move *move, bool captured, void *ui_ctx);
    void (*on_round_end)(const struct s_cte_players *players, const s_cte_round_score scores[], void *ui_ctx);
    void (*on_match_end)(const struct s_cte_match *match, int8_t winner_id, void *ui_ctx);
} s_cte_ui_callbacks;

// Game dealing & lifecycle
t_cteerr setup_game(struct s_cte_players *players);
t_cteerr deal_next_hand(struct s_cte_players *players);
t_cteerr award_remaining_table_cards(struct s_cte_players *players, uint8_t last_captor_id);
t_cteerr run_round(struct s_cte_players *players, const s_cte_round_config *config);

// Scoring & match execution
t_cteerr compute_round_score(struct s_cte_players *players, s_cte_round_score scores[]);
t_cteerr init_match(struct s_cte_match *match, struct s_cte_players *players, uint16_t winning_score);
bool     match_is_over(const struct s_cte_match *match);
int8_t   match_winner(const struct s_cte_match *match);
t_cteerr run_match(struct s_cte_match *match, const s_cte_round_config *config);

#endif
