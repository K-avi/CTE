#include "cte.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

int main(){
    struct s_cte_players players;
    init_players(&players, 2, (char*[]){"Alice", "Bob"});

    assert(players.size == 2);
    assert(players.players[0].player_id == 0);
    assert(players.players[1].player_id == 1);
    assert(strcmp(players.players[0].player_name, "Alice") == 0);
    assert(strcmp(players.players[1].player_name, "Bob") == 0);

    t_cteerr err = setup_game(&players);
    assert(err == e_ok);

    assert(players.players[0].hand.size == 6);
    assert(players.players[1].hand.size == 6);

    //print_hand(&players.players[0].hand);
    //print_hand(&players.players[1].hand);

    assert(deck.cur_card == 16);
    assert(table.nb_cards_on_table == 4);

    //verify that the deck is shuffled (i.e at least one card is different from the original order)
    //init srand to a fixed seed
    srand(42);
    bool shuffled = false;
    for(int i = 0; i < 52; i++){
        if(deck.cards[i] != i){
            shuffled = true;
            break;
        }
    }
    assert(shuffled);

    //assert that all the cards are present once in the deck, players hands and table
    uint8_t card_present[52] = {0};
    for(int i = deck.cur_card; i < 52; i++){
        card_present[deck.cards[i]]++;
    }
    for(int i = 0; i < players.size; i++){
        for(int j = 0; j < players.players[i].hand.size; j++){
            card_present[players.players[i].hand.array[j]]++;
        }
    }
    for(int i = 0; i < table.nb_cards_on_table; i++){
        card_present[table.cards_on_table[i]]++;
    }
    for(int i = 0; i < 52; i++){
        assert(card_present[i] == 1);
    }


    //assert value and color macros
    for(int i = 0; i < 52; i++){    
        switch (i){
            case 0 ... 12:
                assert(get_color(i) == clubs);
                assert(get_value(i) == i + 2);
                break;
            case 13 ... 25:
                assert(get_color(i) == diamonds);
                assert(get_value(i) == i - 13 + 2);
                break;
            case 26 ... 38:
                assert(get_color(i) == hearts);
                assert(get_value(i) == i - 26 + 2);
                break;
            case 39 ... 51:
                assert(get_color(i) == spade);
                assert(get_value(i) == i - 39 + 2);
                break;
        } 

        //assert points macro
        uint8_t expected_points = 0;
        if(get_value(i) >= 10 && get_value(i) <= 14) expected_points = 1;
        if(get_color(i) == diamonds && get_value(i) == 11) expected_points = 2;
        if(get_color(i) == diamonds && get_value(i) == 2) expected_points = 1;
        assert(get_points_var(get_value(i), get_color(i)) == expected_points);
        assert(get_points(i) == expected_points);
    }
    
    //print_hand(&players.players[0].hand);
    //print_hand(&players.players[1].hand);
    //print_table();

    struct s_cte_move move_aces; 
    move_aces.card_played = 12; //king of clubs
    move_aces.cards_picked.size = 2;
    move_aces.cards_picked.max = 2;
    move_aces.cards_picked.array = malloc(sizeof(uint8_t) * 2);

    move_aces.cards_picked.array[0] = 11; //queen of clubs
    move_aces.cards_picked.array[1] = 9; //ace of clubs

    bool legal;

    err = is_legal(&legal, &move_aces);
    assert(err == e_ok);
    assert(legal);

    free(move_aces.cards_picked.array);
    
    struct s_cte_move move_single;
    move_single.card_played = 8; //10 of clubs
    move_single.cards_picked.size = 1;
    move_single.cards_picked.max = 1;
    move_single.cards_picked.array = malloc(sizeof(uint8_t) * 1);
    move_single.cards_picked.array[0] = 21; //10 of diamonds

    err = is_legal(&legal, &move_single);
    assert(err == e_ok);
    assert(legal);

    free(move_single.cards_picked.array);


    struct s_cte_move move_multi;

    move_multi.card_played = 12; //king of clubs
    move_multi.cards_picked.size = 4;
    move_multi.cards_picked.max = 4;
    move_multi.cards_picked.array = malloc(sizeof(uint8_t) * 4);
    move_multi.cards_picked.array[0] = 11; //queen of clubs
    move_multi.cards_picked.array[1] = 22; //ace of diamonds
    move_multi.cards_picked.array[2] = 13; //2 of diamonds
    move_multi.cards_picked.array[3] = 23; //jack of diamonds

    err = is_legal(&legal, &move_multi);
    assert(err == e_ok);
    assert(legal);

    free(move_multi.cards_picked.array);

    struct s_cte_move move_illegal;
    move_illegal.card_played = 8; //10 of clubs
    move_illegal.cards_picked.size = 2;
    move_illegal.cards_picked.max = 2;
    move_illegal.cards_picked.array = malloc(sizeof(uint8_t) * 2);
    move_illegal.cards_picked.array[0] = 21; //10 of diamonds
    move_illegal.cards_picked.array[1] = 22; //jack of diamonds

    err = is_legal(&legal, &move_illegal);
    assert(err == e_ok);
    assert(!legal);

    free(move_illegal.cards_picked.array);
    free_players(&players);

    //test generate_combinations function
    uint8_t playable_cards[] = {1, 2, 3, 4, 5};
    uint8_t nb_playable_cards = 5;
    uint8_t combination_size = 3;
    uint8_t **combinations; 
    combinations = malloc(sizeof(uint8_t*) * 10);
    for(int i = 0; i < 10; i++){
        combinations[i] = malloc(sizeof(uint8_t) * combination_size);
    }
    uint8_t combination_sizes[10];
    for(uint8_t i = 0 ; i < 10; i++){
        combination_sizes[i] = 3;
    }

    generate_combinations((uint8_t**)combinations, playable_cards, nb_playable_cards, combination_size, 10);
    
    //print combinations sizes
    /*printf("Combinations of size %d : \n", combination_size);
    for(uint8_t i = 0 ; i < 10; i++){
        printf("Combination %d : size %d \n", i, combination_sizes[i]);
        for(uint8_t j = 0 ; j < combination_sizes[i]; j++){
            printf("%d ", combinations[i][j]);
        }
        printf("\n");
    }*/

    for(int i = 0; i < 10; i++){
        free(combinations[i]);
    }
    free(combinations);

    struct s_cte_move * moves;
    moves = malloc(sizeof(struct s_cte_move));
    moves->card_played = 12; //king of clubs
    moves->cards_picked.size = 0;
    moves->cards_picked.max = 2;
    moves->cards_picked.array = malloc(sizeof(uint8_t) * 2);

    table.nb_cards_on_table = 4;

   // gen_card_moves(&moves, 12); //10 of clubs

    //play ace of spades
    moves->card_played = 48; //ace of spades
    gen_card_moves(&moves, 48);
    

    free(moves->cards_picked.array);
    free(moves);

    return 0;
}