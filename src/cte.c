#include "cte.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/* order : 
* 2, 3, 4, 5, 6, 7, 8, 9, 10, ACE, JACK, QUEEN, KING
* Clubs, Diamonds, Hearts, Spade
*/

//Maybe useless, array with the existing values of each card. 
//This doesn't handle the ace bc ACE will be dealt with during calculation
uint8_t values[13] = {
    2,3,4,5,6,7,8,9,10,11,12,13,14
};

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
    0,1,2,3,4,5,6,7,8,9,10,11,12, //clubs
    13,14,15,16,17,18,19,20,21,22,23,24,25,//diamonds
    26,27,28,29,30,31,32,33,34,35,36,37,38, //hearts
    39,40,41,42,43,44,45,46,47,48,49,50,51 //spades
    }
};
#define DECKSIZE 52

//theoreticallly, there could be up to 52 cards on the table 
//the global table where cards are put on
struct table {
    uint8_t nb_cards_on_table;
    t_card cards_on_table[52]; 
}table = {0,{0}};
/******************************PLAYER DATA STUFF**************************************************/

static t_cteerr init_player_data(struct s_cte_player_data *player, uint8_t player_id, char *player_name){
    player->player_id = player_id;
    player->nb_tablic = 0;
    player->player_name = strndup(player_name, 20);
    if(!player->player_name) return e_alloc;
    player->hand.size = 0;
    player->won_cards.size = 0;
    return e_ok;
}//tested; ok

t_cteerr init_players(struct s_cte_players *players, uint8_t nb_players, char **player_names){
    if(nb_players < 2 || nb_players > 4) return e_inval_val;

    players->size = nb_players;
    players->players = malloc(sizeof(struct s_cte_player_data) * nb_players);
    if(!players->players) return e_alloc;

    for(uint8_t i = 0 ; i < nb_players; i++){
        t_cteerr err = init_player_data(&players->players[i], i, player_names[i]);
        if(err != e_ok) return err;
    }
    return e_ok;
}//tested; ok

static void free_player_data(struct s_cte_player_data *player){
    free(player->player_name);
}//tested; ok

void free_players(struct s_cte_players *players){
    for(uint8_t i = 0 ; i < players->size; i++){
        free_player_data(&players->players[i]);
    }
    free(players->players);
}//tested; ok


/***********************DECK STUFF************************************************/

//this is the order of distribution of the cards for a game
static void shuffle_deck(void){
    //simple fisher-yates shuffle

    for(uint8_t i = DECKSIZE - 1 ; i > 0; i-- ){
        int idx = rand()%(i);
        uint8_t tmp = deck.cards[i];
        deck.cards[i] = deck.cards[idx]; 
        deck.cards[idx] = tmp;
    }
}//tested; ok

//currently we will support 
//a simplified version of the game. 
//You can not pick up multiple cards that do unrelated 
//sums.
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

    //handle aces
    for(uint8_t i = 0 ; i < nb_aces; i++){
        sum -= 10; 
        if((*ret=sum==value)) break; //if true break   
    }//*ret will be set to false if nothing is found

    //ig the multiple hands think could be done by 
    //checking if the sum of the card picked can be equal 
    //to the value of the cards times something.
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
        players->players[1].hand.size = 6;
    }else if (players->size == 4) {
        return e_inval_val;
    }else if (players->size == 3) {
        return e_inval_val;
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
}//tested; ok

t_cteerr play_move(struct s_cte_move *move, struct s_cte_player_data *player){
    //assume the move is legal
    //put cards in won cards

    //remove card from hand
    for(uint8_t i = 0 ; i < player->hand.size; i++){
        if(move->card_played == player->hand.array[i]){
            player->hand.array[i] = player->hand.array[player->hand.size - 1];
            player->hand.size--;
            break;
        }
    }

    //remove cards from table, 
    for(uint8_t i = 0 ; i < move->cards_picked.size; i++){
        for(uint8_t j = 0; j < table.nb_cards_on_table; j++){
            if(table.cards_on_table[j] == move->cards_picked.array[i]){
                table.cards_on_table[j] = table.cards_on_table[table.nb_cards_on_table - 1];
                table.nb_cards_on_table--;
                break;
            }
        }
    }
    //score a point if table was emptied
    if(table.nb_cards_on_table == 0 ) player->nb_tablic++;

    //add cards to won cards;
    player->won_cards.array[player->won_cards.size++] = move->card_played;
    for(uint8_t i = 0; i < move->cards_picked.size; i++){
        player->won_cards.array[player->won_cards.size++] = move->cards_picked.array[i]; 
    }
    move->cards_picked.size = 0;

    return e_ok;
}//not tested

/*
//nothing done here
static t_cteerr gen_card_moves(struct s_cte_move  ** moves, t_card card){
    
    if(!table.nb_cards_on_table){
        (*moves)->card_played = 53 ;
        (*moves)->cards_picked.size = 0; 
    }else{
        //generate all legal moves
        
        //store temporary moves to evaluate if they are ok
        uint8_t candidate_moves[table.nb_cards_on_table][table.nb_cards_on_table];
        uint8_t tab_size[table.nb_cards_on_table]; //counters for each move size
        uint8_t nb_moves = 0; //counter for number of candidate moves

        uint8_t value = get_value(card);

        //initialize moves
        for(uint8_t i = 0 ; i < table.nb_cards_on_table; i++){
            if(get_value(table.cards_on_table[i]) < value){
                candidate_moves[i][0] = table.cards_on_table[i];
                nb_moves++;
            }
        }

        //initialize move size of non excluded moves
        for(uint8_t i = 0 ; i < nb_moves; i++){
            tab_size[i] = 1;
        }

        
       /// a problem I might encounter is that some moves might be repeated. If I pick up 
        //a queen, and ace with a king, i could also pick up the ace and queen. 

       // I also have to consider that I can play multiple "moves" if cards are unrelated.
    }
    return e_ok;
}*/
/*
static t_cteerr generate_all_moves(struct s_cte_move ** moves, struct s_cte_hand* hand){
    //generates all legal moves with a given hand and table

    return 0;
}*/

/*
t_cteerr simple_evaluate();
*/
//all of those should be really straight forward tbh


static char * value_str[] = {
    "2", "3", "4", "5", "6", "7", "8", "9", "10", "ACE", "JACK", "QUEEN", "KING"
};
static char * color_str[] = {
    "Clubs", "Diamonds", "Hearts", "Spades"
};

void print_hand(struct s_cte_hand *hand){
    printf("Cards in hand : \n");
    for(uint8_t i = 0 ; i < hand->size; i++){
        uint8_t card = hand->array[i];
        uint8_t value = get_value(card);

        printf("%s of %s\n", value_str[value-2], color_str[get_color(card)]);
    }
    printf("\n");
}//tested; ok

void print_table(){
    printf("Cards on table : \n");
    for(uint8_t i = 0 ; i < table.nb_cards_on_table; i++){
        uint8_t card = table.cards_on_table[i];
        uint8_t value = get_value(card);
        uint8_t color = get_color(card);

        printf("%s of %s\n", value_str[value-2], color_str[color]);
    }
    printf("\n");
}//tested; ok

t_cteerr print_won_cards();
//t_cteerr count_points();