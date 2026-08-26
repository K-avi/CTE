#ifndef __CTE_PLAYER_H
#define __CTE_PLAYER_H

#include "card.h"

struct s_cte_hand {
    uint8_t size;
    uint8_t array[6];
};

struct s_cte_won_cards {
    uint8_t size;
    uint8_t array[52];
};

// Forward declaration of evaluator parameters
struct s_cte_game_state;
struct s_cte_move_list;

struct s_cte_player_data {
    uint8_t player_id;
    uint8_t team_id; // Team identifier (0 or 1 in 2v2 mode, or equal to player_id in individual mode)
    uint8_t nb_tablic;
    char *player_name;
    struct s_cte_hand hand;
    struct s_cte_won_cards won_cards;

    // Player strategy controller
    uint16_t (*evaluator)(const struct s_cte_game_state *state, const struct s_cte_move_list *moves, void *ctx);
    void *eval_context;
    bool is_human;
};

struct s_cte_players {
    uint8_t size;
    struct s_cte_player_data *players;
};

// Player lifecycle & round resets
t_cteerr init_players(struct s_cte_players *players, uint8_t nb_players, char **player_names);
void free_players(struct s_cte_players *players);
void reset_player_round(struct s_cte_player_data *player);
void reset_all_players(struct s_cte_players *players);
void print_hand(struct s_cte_hand *hand);

#endif