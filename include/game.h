#ifndef __CTE_GAME_H
#define __CTE_GAME_H

#include "card.h"
#include "player.h"
#include "move.h"
#include "eval.h"
#include "minmax.h"
#include "engine.h"

// Reentrant game structure containing all state
typedef struct s_cte_game {
    const s_cte_engine_backend *backend;           // Active backend (defaults to SWAR bitboard)
    struct deck                       deck;               // Sabot de 52 cartes
    uint64_t                          table_bb;           // 64-bit mask des cartes actives sur la table
    struct s_cte_players              players;            // Joueurs, mains et cartes remportées
    int8_t                            last_captor_id;     // Dernier joueur ayant fait une prise (-1 si aucun)
    uint8_t                           current_player_id;  // Joueur actif (0..players.size-1)
    bool                              is_team_mode;       // Mode 2v2 par équipes (4 joueurs)
} s_cte_game;

// Round score summary for a player
typedef struct {
    uint8_t card_points;     // Points from captured cards
    uint8_t majority_bonus;  // +3 if >= 27 cards and strict majority, else 0
    uint8_t tablic_points;   // = nb_tablic
    uint8_t total;           // card_points + majority_bonus + tablic_points
} s_cte_round_score;

// Match management structure
struct s_cte_match {
    s_cte_game *game;                 // Reference to reentrant game state
    uint16_t    match_scores[4];      // Cumulative score per player (or per team)
    uint16_t    winning_score;        // Configurable (e.g. 51, 101, 201)
    uint8_t     round_nb;             // Current round index (0-indexed)
    uint8_t     max_rounds;           // Optional round limit (0 = unlimited)
    bool        is_team_mode;         // True for 4-player 2v2 team mode
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

// Game lifecycle & dealing
t_cteerr init_game(s_cte_game *game, uint8_t nb_players, char *names[], bool is_team_mode);
void     free_game(s_cte_game *game);
t_cteerr cte_set_backend(s_cte_game *game, e_cte_backend_type type);
s_cte_pos pos_from_game(const s_cte_game *game);
t_cteerr setup_round(s_cte_game *game);
t_cteerr deal_next_hand(s_cte_game *game);
t_cteerr award_remaining_table_cards(s_cte_game *game);
t_cteerr run_round(s_cte_game *game, const s_cte_round_config *config);

// Scoring & match execution
t_cteerr compute_round_score(struct s_cte_players *players, s_cte_round_score scores[], bool is_team_mode);
t_cteerr init_match(struct s_cte_match *match, s_cte_game *game, uint16_t winning_score);
bool     match_is_over(const struct s_cte_match *match);
int8_t   match_winner(const struct s_cte_match *match);
t_cteerr run_match(struct s_cte_match *match, const s_cte_round_config *config);

#endif
