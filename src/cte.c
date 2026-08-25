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
void print_card(uint8_t card);
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
   1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, //clubs (2 of clubs = 1, 10..King,Ace = 1)
   0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 1, 1, //diamonds (10 of diamonds = 2, Ace,Jack,Queen,King = 1)
   0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, //hearts (10..King,Ace = 1)
   0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, //spades (10..King,Ace = 1)
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
};//nb : values start at 2 and ace is between 10 and jack.
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

t_cteerr init_move_list(struct s_cte_move_list *list, uint16_t initial_cap){
    if(!list) return e_null;
    list->size = 0;
    list->max = (initial_cap > 0) ? initial_cap : 16;
    list->moves = malloc(sizeof(struct s_cte_move) * list->max);
    if(!list->moves) return e_alloc;
    return e_ok;
}

static t_cteerr move_list_push(struct s_cte_move_list *list, const struct s_cte_move *move){
    if(!list) return e_null;
    if(list->size >= list->max){
        uint16_t new_max = list->max ? (uint16_t)(list->max * 2) : 16;
        struct s_cte_move *new_moves = realloc(list->moves, sizeof(struct s_cte_move) * new_max);
        if(!new_moves) return e_realloc;
        list->moves = new_moves;
        list->max = new_max;
    }
    list->moves[list->size++] = *move;
    return e_ok;
}

void free_move(struct s_cte_move *move){
    if(move && move->cards_picked.array){
        free(move->cards_picked.array);
        move->cards_picked.array = NULL;
        move->cards_picked.size = 0;
        move->cards_picked.max = 0;
    }
}

void free_move_list(struct s_cte_move_list *list){
    if(!list) return;
    if(list->moves){
        for(uint16_t i = 0 ; i < list->size; i++){
            free_move(&list->moves[i]);
        }
        free(list->moves);
        list->moves = NULL;
    }
    list->size = 0;
    list->max = 0;
}

t_cteerr play_move(struct s_cte_move *move, struct s_cte_player_data *player){
    if(!move || !player) return e_null;

    // Remove card played from hand
    for(uint8_t i = 0 ; i < player->hand.size; i++){
        if(move->card_played == player->hand.array[i]){
            player->hand.array[i] = player->hand.array[player->hand.size - 1];
            player->hand.size--;
            break;
        }
    }

    // If no cards were picked, the card is dropped onto the table
    if(move->cards_picked.size == 0){
        if(table.nb_cards_on_table < 52){
            table.cards_on_table[table.nb_cards_on_table++] = move->card_played;
        }
        return e_ok;
    }

    // Remove picked cards from table
    for(uint8_t i = 0 ; i < move->cards_picked.size; i++){
        for(uint8_t j = 0; j < table.nb_cards_on_table; j++){
            if(table.cards_on_table[j] == move->cards_picked.array[i]){
                table.cards_on_table[j] = table.cards_on_table[table.nb_cards_on_table - 1];
                table.nb_cards_on_table--;
                break;
            }
        }
    }

    // Score a Tablic if table was emptied
    if(table.nb_cards_on_table == 0) player->nb_tablic++;

    // Add cards to player's won cards pile
    player->won_cards.array[player->won_cards.size++] = move->card_played;
    for(uint8_t i = 0; i < move->cards_picked.size; i++){
        player->won_cards.array[player->won_cards.size++] = move->cards_picked.array[i]; 
    }

    return e_ok;
}

/*********************** EXACT PARTITION & MOVE VALIDATION (DP) ********************/

static bool subset_sums_to(const uint8_t *cards, uint32_t mask, uint8_t n, uint8_t target_val){
    uint32_t sum_max = 0;
    uint8_t nb_aces = 0;
    for(uint8_t i = 0; i < n; i++){
        if(mask & (1u << i)){
            uint8_t v = get_value(cards[i]);
            if(v == 11){
                nb_aces++;
            }
            sum_max += v;
        }
    }
    // Each ace counted as 1 (instead of 11) reduces the sum by 10
    if(sum_max >= target_val && ((sum_max - target_val) % 10 == 0)){
        uint32_t k = (sum_max - target_val) / 10;
        if(k <= nb_aces){
            return true;
        }
    }
    return false;
}

static bool can_partition_rec(uint32_t mask, const uint8_t *valid_base, int8_t *memo){
    if(mask == 0) return true;
    if(memo[mask] != -1) return (bool)memo[mask];

    uint32_t lsb = mask & (-mask); // lowest set bit to avoid permutations
    for(uint32_t sub = mask; sub > 0; sub = (sub - 1) & mask){
        if((sub & lsb) && valid_base[sub]){
            if(can_partition_rec(mask ^ sub, valid_base, memo)){
                memo[mask] = 1;
                return true;
            }
        }
    }
    memo[mask] = 0;
    return false;
}

static bool is_exact_partition(const uint8_t *cards, uint8_t n, uint8_t target_val){
    if(n == 0) return false;
    if(n > 20) return false;

    uint32_t num_masks = 1u << n;
    uint8_t *valid_base = malloc(num_masks * sizeof(uint8_t));
    int8_t *memo = malloc(num_masks * sizeof(int8_t));
    if(!valid_base || !memo){
        free(valid_base);
        free(memo);
        return false;
    }

    for(uint32_t m = 0; m < num_masks; m++){
        valid_base[m] = (m > 0) ? (uint8_t)subset_sums_to(cards, m, n, target_val) : 0;
        memo[m] = -1;
    }

    bool result = can_partition_rec(num_masks - 1, valid_base, memo);

    free(valid_base);
    free(memo);
    return result;
}

t_cteerr is_legal(bool *ret, struct s_cte_move *move){
    if(!ret || !move) return e_null;
    *ret = false;

    // Laying down a card on the table (taking 0 cards) is always legal
    if(move->cards_picked.size == 0){
        *ret = true;
        return e_ok;
    }

    uint8_t n = move->cards_picked.size;
    const uint8_t *cards = move->cards_picked.array;

    if(is_ace(move->card_played)){
        // An Ace played can represent either value 11 or value 1
        if(is_exact_partition(cards, n, 11) || is_exact_partition(cards, n, 1)){
            *ret = true;
        }
    } else {
        uint8_t target_val = get_value(move->card_played);
        if(is_exact_partition(cards, n, target_val)){
            *ret = true;
        }
    }

    return e_ok;
}

/*********************** MOVE GENERATION ******************************************/

static void find_valid_capture_masks(const uint8_t *table_cards, uint8_t n, uint8_t target_val, uint8_t *is_valid_capture){
    if(n == 0 || n > 20) return;
    uint32_t num_masks = 1u << n;
    uint8_t *valid_base = malloc(num_masks * sizeof(uint8_t));
    int8_t *memo = malloc(num_masks * sizeof(int8_t));
    if(!valid_base || !memo){
        free(valid_base);
        free(memo);
        return;
    }

    for(uint32_t m = 0; m < num_masks; m++){
        valid_base[m] = (m > 0) ? (uint8_t)subset_sums_to(table_cards, m, n, target_val) : 0;
        memo[m] = -1;
    }

    for(uint32_t m = 1; m < num_masks; m++){
        if(can_partition_rec(m, valid_base, memo)){
            is_valid_capture[m] = 1;
        }
    }

    free(valid_base);
    free(memo);
}

t_cteerr gen_card_moves(struct s_cte_move_list *moves, t_card card){
    if(!moves) return e_null;
    if(!moves->moves && moves->max == 0){
        t_cteerr err = init_move_list(moves, 16);
        if(err != e_ok) return err;
    }

    // 1. Always add the drop move (cards_picked.size = 0)
    struct s_cte_move drop_move;
    drop_move.card_played = card;
    drop_move.cards_picked.size = 0;
    drop_move.cards_picked.max = 0;
    drop_move.cards_picked.array = NULL;
    t_cteerr err = move_list_push(moves, &drop_move);
    if(err != e_ok) return err;

    uint8_t n = table.nb_cards_on_table;
    if(n == 0) return e_ok;
    if(n > 20) n = 20;

    uint32_t num_masks = 1u << n;
    uint8_t *is_valid_capture = calloc(num_masks, sizeof(uint8_t));
    if(!is_valid_capture) return e_alloc;

    if(is_ace(card)){
        find_valid_capture_masks(table.cards_on_table, n, 11, is_valid_capture);
        find_valid_capture_masks(table.cards_on_table, n, 1, is_valid_capture);
    } else {
        uint8_t val = get_value(card);
        find_valid_capture_masks(table.cards_on_table, n, val, is_valid_capture);
    }

    for(uint32_t m = 1; m < num_masks; m++){
        if(is_valid_capture[m]){
            uint8_t count = 0;
            for(uint8_t i = 0; i < n; i++){
                if(m & (1u << i)) count++;
            }

            struct s_cte_move move;
            move.card_played = card;
            move.cards_picked.size = count;
            move.cards_picked.max = count;
            move.cards_picked.array = malloc(sizeof(uint8_t) * count);
            if(!move.cards_picked.array){
                free(is_valid_capture);
                return e_alloc;
            }

            uint8_t idx = 0;
            for(uint8_t i = 0; i < n; i++){
                if(m & (1u << i)){
                    move.cards_picked.array[idx++] = table.cards_on_table[i];
                }
            }

            err = move_list_push(moves, &move);
            if(err != e_ok){
                free(move.cards_picked.array);
                free(is_valid_capture);
                return err;
            }
        }
    }

    free(is_valid_capture);
    return e_ok;
}

t_cteerr gen_all_moves(struct s_cte_move_list *moves, struct s_cte_hand *hand){
    if(!moves || !hand) return e_null;
    if(!moves->moves && moves->max == 0){
        t_cteerr err = init_move_list(moves, 32);
        if(err != e_ok) return err;
    }
    for(uint8_t i = 0 ; i < hand->size; i++){
        t_cteerr err = gen_card_moves(moves, hand->array[i]);
        if(err != e_ok) return err;
    }
    return e_ok;
}



static const char * const value_str[] = {
    "2", "3", "4", "5", "6", "7", "8", "9", "10", "ACE", "JACK", "QUEEN", "KING"
};
static const char * const color_str[] = {
    "Clubs", "Diamonds", "Hearts", "Spades"
};


void print_card(uint8_t card){
    uint8_t value = get_value(card);
    uint8_t color = get_color(card);

    printf("%s of %s\n", value_str[value-2], color_str[color]);
}//tested; ok

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