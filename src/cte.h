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
    uint8_t array_52;
};

struct s_cte_player_data{
    uint8_t player_id;
    char *player_name;
    struct s_cte_hand hand;
    struct s_cte_won_cards; 
};

struct s_cte_players{
    uint8_t size; 
    struct s_cte_player_data *players;
};

//-------- Global variables 

extern uint8_t points[52];//point of each card 
extern uint8_t values[13];//value of each card

enum e_colors{ clubs = 0,diamonds, hearts, spade };

//actual game functions ig?

//get the points of a card from a value and it's color
#define get_points(value,color) ({int v = (value), c = (color) ; points[(v-2) * c] }) 

#define get_color(index) ({uint8_t v = (index); index/4;})
#define get_value(card) ({uint8_t c = (card); c/4 ;})

t_cteerr setup_game(struct s_cte_players *);
/*t_cteerr play_move();
t_cteerr print_hand(); 
t_cteerr print_table();*/

#endif
