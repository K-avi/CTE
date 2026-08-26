#ifndef __CTE_MOVE_H
#define __CTE_MOVE_H

#include "card.h"
#include "player.h"

// Static array for captured cards in a single move (0 heap allocation, cache L1 friendly)
struct s_cte_darr {
    uint8_t size;
    uint8_t array[16];
};

// Represents a move (card played from hand, and optional captured cards from table)
struct s_cte_move {
    t_card card_played;
    struct s_cte_darr cards_picked;
};

// Dynamic list of legal moves
struct s_cte_move_list {
    uint16_t size;
    uint16_t max;
    struct s_cte_move *moves;
};

// Memory management
t_cteerr init_move_list(struct s_cte_move_list *list, uint16_t initial_cap);
void free_move(struct s_cte_move *move);
void free_move_list(struct s_cte_move_list *list);

// Move validation, generation & execution (reentrant, takes const struct table*)
bool is_exact_partition(const uint8_t *cards, uint8_t n, uint8_t target_val);
t_cteerr is_legal(bool *ret, const struct table *table, const struct s_cte_move *move);
t_cteerr gen_card_moves(struct s_cte_move_list *moves, const struct table *table, t_card card);
t_cteerr gen_all_moves(struct s_cte_move_list *moves, const struct table *table, const struct s_cte_hand *hand);
t_cteerr play_move(struct table *table, const struct s_cte_move *move, struct s_cte_player_data *player, bool *captured);

// Move scoring (pure evaluation, decoupled from state update)
typedef struct {
    uint8_t card_points;   // Points from captured cards (card_played + cards_picked), 0 if drop
    uint8_t nb_cards;      // Number of captured cards (1 + cards_picked.size), 0 if drop
    bool    is_tablic;     // True if table is completely cleared
    uint8_t total_points;  // card_points + (is_tablic ? 1 : 0)
} s_cte_move_score;

s_cte_move_score score_move(const struct s_cte_move *move, uint8_t nb_cards_on_table);

// Move formatting & printing
void format_move(char *buf, size_t buf_size, const struct s_cte_move *move, e_cte_render_style style);
void print_move(const struct s_cte_move *move);

#endif
