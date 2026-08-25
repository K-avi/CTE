#ifndef __CTE_CTE_H
#define __CTE_CTE_H

#include <stdint.h>

//------type to spit out error codes
typedef uint8_t t_cteerr;

enum cter_err_codes{ 
    e_ok, e_null, e_inval_val, e_alloc, e_realloc
};//idk


//------Various typedef n structs used in the project

typedef uint8_t t_card;
typedef uint8_t t_points;

//ur basic dynamic array
struct s_cte_darr{
    uint8_t size; 
    uint8_t max; 

    uint8_t *array;
};//maybe add an "empty slot or smtg" 

//a simple way to represent a move, the card played (from hand) and card(s) picked 
//from the table; might replace card_picked w a pointer to it 
struct s_cte_move{
    t_card card_played;
    struct s_cte_darr cards_picked; 
};

//actually It might be smarter to use fixed sized arrays, 
//one for each hand, one for the draw, one for the table
//actually actually, in order to support 4 player tablic it might not be such a good idea
//anyways a hand will never contain more than 6 cards so a darr seems silly
struct s_cte_hand{
    uint8_t size; 
    uint8_t array[6];
};

struct s_cte_won_cards{
    uint8_t size; 
    uint8_t array[52];
};

struct s_cte_player_data{
    uint8_t player_id;
    uint8_t nb_tablic;
    char *player_name;
    struct s_cte_hand hand;
    struct s_cte_won_cards won_cards; 
};

struct s_cte_players{
    uint8_t size; 
    struct s_cte_player_data *players;
};

struct s_cte_move_list{
    uint16_t size;
    uint16_t max;
    struct s_cte_move *moves;
};

// Global variables 
extern uint8_t __tab_points[52];//point of each card 
extern uint8_t values[13];//value of each card

enum e_colors{ clubs = 0, diamonds, hearts, spade };

// Utility macros
#define get_value(card) (values[(card)%13])
#define get_color(card) ((card)/13)
#define is_ace(card) (get_value(card) == 11)

#define get_points_var(value, color) (__tab_points[(color)*13 + (value-2)])
#define get_points(card) (__tab_points[card])

// Players and game management
t_cteerr init_players(struct s_cte_players *players, uint8_t nb_players, char **player_names);
void free_players(struct s_cte_players *players);
t_cteerr setup_game(struct s_cte_players *players);

// Move list memory management
t_cteerr init_move_list(struct s_cte_move_list *list, uint16_t initial_cap);
void free_move(struct s_cte_move *move);
void free_move_list(struct s_cte_move_list *list);

// Core game moves & rules
t_cteerr is_legal(bool *ret, struct s_cte_move *move);
t_cteerr gen_card_moves(struct s_cte_move_list *moves, t_card card);
t_cteerr gen_all_moves(struct s_cte_move_list *moves, struct s_cte_hand *hand);
t_cteerr play_move(struct s_cte_move *move, struct s_cte_player_data *player);

extern struct deck{
    uint8_t cur_card; 
    t_card cards[52];
}deck;

extern struct table {
    uint8_t nb_cards_on_table;
    t_card cards_on_table[52]; 
}table;

void print_hand(struct s_cte_hand *hand);
void print_table(void);
void print_card(uint8_t card);
void print_move(struct s_cte_move *move);

#endif

