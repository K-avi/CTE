#ifndef __CTE_MOVE_H
#define __CTE_MOVE_H

#include "card.h"
#include "player.h"

// Dynamic array for captured cards in a single move
struct s_cte_darr {
    uint8_t size;
    uint8_t max;
    uint8_t *array;
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

// Move validation, generation & execution
t_cteerr is_legal(bool *ret, struct s_cte_move *move);
t_cteerr gen_card_moves(struct s_cte_move_list *moves, t_card card);
t_cteerr gen_all_moves(struct s_cte_move_list *moves, struct s_cte_hand *hand);
t_cteerr play_move(struct s_cte_move *move, struct s_cte_player_data *player, bool *captured);

// Move formatting & printing
void format_move(char *buf, size_t buf_size, const struct s_cte_move *move, e_cte_render_style style);
void print_move(struct s_cte_move *move);

#endif
