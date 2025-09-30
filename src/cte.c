#include "cte.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>


/* order : 
* 2, 3, 4, 5, 6, 7, 8, 9, 10, ACE, JACK, QUEEN, KING
* Clubs, Diamonds, Hearts, Spade
*/

//Maybe useless, array with the existing values of each card. 
//This doesn't handle the ace bc ACE will be dealt with during calculation
uint8_t values[13] = {
    2,3,4,5,6,7,8,9,10,11,12,13,14
};

char* printable_vals[13] = {
    "2", "3", "4", "5", "6", "7", "8", "9", "10", 
    "A", "J", "Q", "K"
};//maybe useless


//Array of the value (in points) of each card in the game
uint8_t points[52] = {
    0, 0, 0,0, 0, 0, 0, 0, 1, 1, 1, 1, 1, //clubs
   1, 0, 0,0, 0, 0, 0, 0, 1, 2, 1, 1, 1,//diamonds
   0, 0, 0,0, 0, 0, 0, 0, 1, 1, 1, 1, 1, //hearts
   0, 0, 0,0, 0, 0, 0, 0, 1, 1, 1, 1, 1, //spades
}; 

struct deck{
    uint8_t cur_card; 
    t_card cards[52];
} deck = {
    0,
    {
    0,1,2,3,4,5,6,7,8,9,10,
    11,12,13,14,15,16,17,18,19,
    20,21,22,23,24,25,26,27,28,29,
    30,31,32,33,34,35,36,37,38,39,
    41,42,43,44,45,46,47,48,49,
    50,51,52
    }
};
#define DECKSIZE 52

//theoreticallly, there could be up to 52 cards on the table 
//the global table where cards are put on
struct table {
    uint8_t nb_cards_on_table;
    t_card cards_on_table[52]; 
}table = {0,{0}};


//this is the order of distribution of the cards for a game
static void shuffle_deck(){
    //simple fisher-yates shuffle
    uint8_t cur = 0; 

    for(uint8_t i = DECKSIZE - 1 ; i > 0; i-- ){
        int idx = rand()%(i);
        uint8_t tmp = deck.cards[i];
        deck.cards[i] = deck.cards[idx]; 
        deck.cards[idx] = tmp;
    }
}//not tested

static t_cteerr is_legal(bool *ret, struct s_cte_move *move){

    *ret = false; 
    uint8_t value = get_value(move->card_played);
    uint32_t sum = 0; 
    uint8_t nb_aces; //used to substract

    struct s_cte_darr *cards_picked = &move->cards_picked;

    for(uint8_t i = 0 ; i < cards_picked->size; i++){
        uint8_t card_val = get_value(cards_picked->array[i]);
        if( card_val == 11) nb_aces++;
        sum += card_val;
    }
    if(nb_aces == 0) *ret = sum == value;

    for(uint8_t i = 0 ; i < nb_aces; i++){
        sum -= 10; 
        if((*ret=sum==value)) break; //if true break   
    }//*ret will be set to false if nothing is found

    return e_ok;
}//not tested

t_cteerr setup_game(struct s_cte_players *players){
    
    shuffle_deck();

    if(players->size == 2){
        for(int i = 0 ; i < 6; i++){//distribute 6 cards to each player 
            players->players[0].hand.array[i] = deck.cards[i];
            players->players[1].hand.array[i] = deck.cards[i+6];
        }
        players->players[0].hand.size = 6;
        players->players[1].hand.size = 3;
    }else if (players->size == 4) {
        //not implemented yet
    }else{
        return e_inval_val;
    }

    //put 4 cards on the table
    for(int i = 0; i < 4; i++){
        table.cards_on_table[i] = deck.cards[i+12];
    }
    table.nb_cards_on_table = 4;
    deck.cur_card = 16;
    
    return e_ok;
}//not tested

t_cteerr play_move(struct s_cte_move *move){
    return e_ok;
}
t_cteerr print_hand(); 
t_cteerr print_table();


static t_cteerr gen_card_moves(struct s_cte_move  ** moves, t_card card);

static t_cteerr generate_all_moves(struct s_cte_move ** moves, struct s_cte_hand* hand){
    /*generates all legal moves with a given hand and table*/

    return 0;
}

static t_cteerr evaluate();