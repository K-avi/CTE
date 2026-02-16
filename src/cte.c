#include "cte.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#ifdef DEBUG
void print_table(void);
void print_hand(struct s_cte_hand *hand);
void print_move(struct s_cte_move *move);
#endif

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
uint8_t __tab_points[52] = {
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


#ifndef DEBUG
static t_cteerr is_legal(bool *ret, struct s_cte_move *move){
#else
t_cteerr is_legal(bool *ret, struct s_cte_move *move){
#endif
    /*
    Let's figure out the max number of cars that can be picked up in tablic
    Let's say I have a king in hand. 
    Let's say the table is filled w every other card. 

    I can pick up the three remaining kings. 
    Every queen + ace or Ace + 3
    every jack + 2

    every 10 + 4
    every 9 + 5 

    every 8 + 6
    the four sevens

    Which leaves the threes or queens on the table.

    for a total of 3 + 5*8 + 4 = 47 cards that can be picked up with a king.
    the sum of the values of the cards would be : 
    14*25 (wowie)
    ps: I don't think that's useful but hey I'll leave a note 4 the future
    */
    *ret = false; 
    uint8_t value = get_value(move->card_played);
    uint32_t sum = 0; 
    uint8_t nb_aces = 0; //used to substract

    struct s_cte_darr *cards_picked = &move->cards_picked;

    //sum the values of the cards picked up, and count the number of aces
    for(uint8_t i = 0 ; i < cards_picked->size; i++){
        uint8_t card_val = get_value(cards_picked->array[i]);
        if( card_val == 11) nb_aces++;
        sum += card_val;
    }
    if(nb_aces == 0) *ret = sum % value == 0;
    
    else{
        for(uint8_t i = 0 ; i <= nb_aces; i++){
            if((sum - i*10) % value == 0){
                *ret = true;
                break;
            }
        }
    }
    return e_ok;
}//tested; seems ok, more thorough testing needed


//to do this we want to avoid 
//playing the same move multiple times. 

//so we will build the lists of legal 
//move with increasing value of the card played.
//we will also begin by generating the moves with the most cards picked up
//and if we can split them, we will create smaller moves out of them.

/*
Hold up I think theres an nlogn solution to this.
first u sort 
actually maybe not.
bc I don't think theres a O(n) to check sums

I think I still need to sort the cards by value ....

No this is actually a knapsack ffs
*/

static bool is_in_move(struct s_cte_move *move, uint8_t card){
    for(uint8_t i = 0 ; i < move->cards_picked.size; i++){
        if(move->cards_picked.array[i] == card) return true;
    }
    return false;
}//not tested

#define is_ace(card) (get_value(card) == 11)

static bool intersects(struct s_cte_move *move, struct s_cte_move *other){
    for(uint8_t i = 0 ; i < move->cards_picked.size; i++){
        if(is_in_move(other, move->cards_picked.array[i])) return true;
    }
    
    return false;
}//not tested

#ifndef DEBUG
static void generate_combinations(uint8_t **combinations, 
#else 
void generate_combinations(uint8_t **combinations,
#endif

    uint8_t playable_cards[], uint8_t nb_playable_cards, uint8_t combination_size, 
    uint8_t max_value){
    /*
    @param combinations      : 2d array where the combinations will be stored
    @param playable_cards    : array of the cards that can be picked up (we assume value < value of card played) 
           (n in C(n, k))
    @param nb_playable_cards : number of playable cards (size of the playable_cards array 
                               dim2 of the combinations array)
    @param combination_size  : size of the combinations to generate 
                               (k in C(n, k))
    */
    if(combination_size == 1){
        for(uint8_t i = 0 ; i < nb_playable_cards; i++){
            combinations[i][0] = playable_cards[i];
        }
    }else if(combination_size == nb_playable_cards){
        for(uint8_t i = 0 ; i < nb_playable_cards; i++){
            combinations[0][i] = playable_cards[i];
        }
    }else{
        uint8_t indices[combination_size];
        for(uint8_t i = 0 ; i < combination_size; i++){
            indices[i] = i;
        }
        uint8_t comb_idx = 0;
        while(true){
            for(uint8_t i = 0 ; i < combination_size; i++){
                combinations[comb_idx][i] = playable_cards[indices[i]];
            }
            comb_idx++;
            //return;
            int i = combination_size - 1;
            while(i >= 0 && indices[i] == nb_playable_cards - combination_size + i){
                i--;
            }
            if(i < 0) break;
            indices[i]++;
            for(uint8_t j = i + 1; j < combination_size; j++){
                indices[j] = indices[j - 1] + 1;
            }
        }
    }
    
}//tested; seems ok; more thorough testing needed

static void filter_combinations(uint8_t **combinations_src, uint8_t combination_sizes_src, uint8_t nb_combinations_src,
                                uint8_t **combinations_dst, uint8_t *combination_sizes_dst, uint8_t *dst_idx_start,
                                uint8_t nb_combinations_dst, uint8_t value){
    //filter out the combinations that are not legal (sum of values of cards != value of card played)
    //we assume that the combinations are sorted by size (descending)
    uint8_t dst_idx = *dst_idx_start;
    for(uint8_t i = 0 ; i < nb_combinations_src; i++){
        if(dst_idx >= nb_combinations_dst) break;
        
        //calculate sum of values in combination + count aces
        uint32_t sum = 0; 
        uint8_t nb_aces = 0;
        for(uint8_t j = 0 ; j < combination_sizes_src; j++){
            sum += get_value(combinations_src[i][j]);
            if(is_ace(combinations_src[i][j])) nb_aces++;

        }

        if(nb_aces == 0){
            if(sum == value){
                memcpy(combinations_dst[dst_idx], combinations_src[i], sizeof(uint8_t) * combination_sizes_src);
                combination_sizes_dst[dst_idx] = combination_sizes_src;
                dst_idx++;
            }
        }else{
            for(uint8_t k = 0 ; k <= nb_aces; k++){
                if(sum - k*10 == value){
                    memcpy(combinations_dst[dst_idx], combinations_src[i], sizeof(uint8_t) * combination_sizes_src);
                    combination_sizes_dst[dst_idx] = combination_sizes_src;
                    dst_idx++;
                    break;
                }
            }
        }
    }
    *dst_idx_start = dst_idx;
}//not tested

#ifndef DEBUG
static t_cteerr gen_card_moves(struct s_cte_move  ** moves, t_card card){
#else
t_cteerr gen_card_moves(struct s_cte_move  ** moves, t_card card){
#endif
    
    if(!table.nb_cards_on_table){
        (*moves)->card_played = 53 ;
        (*moves)->cards_picked.size = 0; 
    }else{
        //generate all legal moves
        
        //store temporary moves to evaluate if they are ok
        //total number of moves is the sum of the number of combinations of cards
        //smh.

        //filter out the cards that are > to the card played

        uint8_t playable_cards[table.nb_cards_on_table];
        uint8_t nb_playable_cards = 0;
        uint8_t nb_aces = 0;
        for(uint8_t i = 0 ; i < table.nb_cards_on_table; i++){
            if(is_ace(table.cards_on_table[i])){
                nb_aces++;
                playable_cards[nb_playable_cards++] = table.cards_on_table[i];
            }else if(get_value(table.cards_on_table[i]) < get_value(card) ){
                playable_cards[nb_playable_cards++] = table.cards_on_table[i];
            }
        }
        
        uint8_t max_comb_size = 1 << nb_playable_cards;
        uint8_t **tmp_combinations = malloc(sizeof(uint8_t*) * max_comb_size);
        uint8_t **kept_combinations = malloc(sizeof(uint8_t*) * max_comb_size);

        for(uint8_t i = 0 ; i < max_comb_size; i++){
            #ifdef DEBUG
            tmp_combinations[i] = calloc(max_comb_size, sizeof(uint8_t));
            kept_combinations[i] = calloc(max_comb_size, sizeof(uint8_t));
            #else
            tmp_combinations[i] = malloc(sizeof(uint8_t) * nb_playable_cards);
            kept_combinations[i] = malloc(sizeof(uint8_t) * nb_playable_cards);

            #endif
        }           

        uint8_t *kept_combination_sizes = malloc(sizeof(uint8_t) * max_comb_size);
        uint8_t nb_kept_combinations = 0;

        uint8_t value = get_value(card);

        for(uint8_t size = 1 ; size <= nb_playable_cards; size++){
            //C(n, k) combinations of the playable cards
            uint16_t nb_combinations = 1;
            for(uint8_t i = 0 ; i < size; i++){
                nb_combinations = nb_combinations * (nb_playable_cards - i) / (i + 1);
            }   

            //generate combinations of size "size" of the playable cards
            generate_combinations(tmp_combinations, playable_cards, nb_playable_cards, size, value);
            //filter out the combinations that are not legal and store the legal ones in kept_combinations
            filter_combinations(tmp_combinations, size, nb_combinations, 
                                kept_combinations, kept_combination_sizes, 
                                &nb_kept_combinations, nb_combinations, value);
        }
        //free tmp combinations / kept combinations
        for(uint8_t i = 0 ; i < max_comb_size; i++){
            free(tmp_combinations[i]);
            free(kept_combinations[i]);

        }
        free(tmp_combinations);
        free(kept_combinations);
        free(kept_combination_sizes);

        print_table();
        print_move(*moves);
        //print cards played
        

        /*todo : 
        
        "fuse" unrelated combinations to generate more moves.
        write back into the moves array.
        */
    }
    return e_ok;
}//not tested
/*
static t_cteerr generate_all_moves(struct s_cte_move ** moves, struct s_cte_hand* hand){
    //generates all legal moves with a given hand and table

    return 0;
}


static uint8_t simple_evaluate(struct s_cte_move *move){
    //evaluate a move by counting the points of the cards picked up
    uint8_t points = 0; 
    for(uint8_t i = 0 ; i < move->cards_picked.size; i++){
        points += get_points(move->cards_picked.array[i]);
    }
    return points;
}*/


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

void print_move(struct s_cte_move *move){
    printf("Card played : %s of %s\n", value_str[get_value(move->card_played)-2], color_str[get_color(move->card_played)]);
    printf("Cards picked up : \n");
    for(uint8_t i = 0 ; i < move->cards_picked.size; i++){
        uint8_t card = move->cards_picked.array[i];
        uint8_t value = get_value(card);
        uint8_t color = get_color(card);

        printf("%s of %s\n", value_str[value-2], color_str[color]);
    }
    printf("\n");
}//tested; ok

t_cteerr print_won_cards();
//t_cteerr count_points();