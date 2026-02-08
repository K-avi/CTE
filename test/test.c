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
    
    print_hand(&players.players[0].hand);
    print_hand(&players.players[1].hand);
    print_table();

    free_players(&players);
    return 0;
}