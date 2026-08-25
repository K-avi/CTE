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
        if(i <= 12){
            assert(get_color(i) == clubs);
            assert(get_value(i) == i + 2);
        } else if(i <= 25){
            assert(get_color(i) == diamonds);
            assert(get_value(i) == i - 13 + 2);
        } else if(i <= 38){
            assert(get_color(i) == hearts);
            assert(get_value(i) == i - 26 + 2);
        } else {
            assert(get_color(i) == spade);
            assert(get_value(i) == i - 39 + 2);
        }

        //assert points macro (standard Tablic rules: 10 diamonds = 2, 2 clubs = 1, all other 10..King/Ace = 1)
        uint8_t expected_points = 0;
        if(get_value(i) >= 10 && get_value(i) <= 14) expected_points = 1;
        if(get_color(i) == diamonds && get_value(i) == 10) expected_points = 2;
        if(get_color(i) == clubs && get_value(i) == 2) expected_points = 1;
        assert(get_points_var(get_value(i), get_color(i)) == expected_points);
        assert(get_points(i) == expected_points);
    }
    
    //print_hand(&players.players[0].hand);
    //print_hand(&players.players[1].hand);
    //print_table();

    // -------------------------------------------------------------
    // Test is_legal: Valid single capture, multi-sum, and drop
    // -------------------------------------------------------------
    bool legal = false;
    struct s_cte_move move_drop = { .card_played = 8, .cards_picked = { .size = 0, .max = 0, .array = NULL } };
    err = is_legal(&legal, &move_drop);
    assert(err == e_ok && legal);

    struct s_cte_move move_single;
    move_single.card_played = 8; // 10 of clubs
    move_single.cards_picked.size = 1;
    move_single.cards_picked.max = 1;
    move_single.cards_picked.array = (uint8_t[]){21}; // 10 of diamonds
    err = is_legal(&legal, &move_single);
    assert(err == e_ok && legal);

    struct s_cte_move move_sum;
    move_sum.card_played = 8; // 10 of clubs
    move_sum.cards_picked.size = 2;
    move_sum.cards_picked.max = 2;
    move_sum.cards_picked.array = (uint8_t[]){5, 1}; // 7 of clubs (val 7), 3 of clubs (val 3)
    err = is_legal(&legal, &move_sum);
    assert(err == e_ok && legal);

    struct s_cte_move move_multi_sum;
    move_multi_sum.card_played = 8; // 10 of clubs
    move_multi_sum.cards_picked.size = 5;
    move_multi_sum.cards_picked.max = 5;
    move_multi_sum.cards_picked.array = (uint8_t[]){
        21, // 10 of diamonds (val 10)
        5,  // 7 of clubs (val 7)
        1,  // 3 of clubs (val 3)
        2,  // 4 of clubs (val 4)
        4   // 6 of clubs (val 6)
    };
    err = is_legal(&legal, &move_multi_sum);
    assert(err == e_ok && legal);

    // -------------------------------------------------------------
    // Test is_legal: False positive cases (rejection of non-partitionable sums)
    // -------------------------------------------------------------
    // False positive 1: 10 played, picking {7, 7, 6} -> Sum = 20 (20 % 10 == 0), but not partitionable into 10s!
    struct s_cte_move move_fp1;
    move_fp1.card_played = 8; // 10 of clubs
    move_fp1.cards_picked.size = 3;
    move_fp1.cards_picked.max = 3;
    move_fp1.cards_picked.array = (uint8_t[]){5, 18, 4}; // 7 clubs, 7 diamonds, 6 clubs
    err = is_legal(&legal, &move_fp1);
    assert(err == e_ok && !legal);

    // False positive 2: 8 played, picking {5, 5, 6} -> Sum = 16 (16 % 8 == 0), not partitionable into 8s!
    struct s_cte_move move_fp2;
    move_fp2.card_played = 6; // 8 of clubs
    move_fp2.cards_picked.size = 3;
    move_fp2.cards_picked.max = 3;
    move_fp2.cards_picked.array = (uint8_t[]){3, 16, 4}; // 5 clubs, 5 diamonds, 6 clubs
    err = is_legal(&legal, &move_fp2);
    assert(err == e_ok && !legal);

    // False positive 3: 7 played, picking {9, 5} -> Sum = 14 (14 % 7 == 0), not partitionable into 7s!
    struct s_cte_move move_fp3;
    move_fp3.card_played = 5; // 7 of clubs
    move_fp3.cards_picked.size = 2;
    move_fp3.cards_picked.max = 2;
    move_fp3.cards_picked.array = (uint8_t[]){7, 3}; // 9 clubs, 5 clubs
    err = is_legal(&legal, &move_fp3);
    assert(err == e_ok && !legal);

    // -------------------------------------------------------------
    // Test is_legal: Ace edge cases (1 vs 11, multiple Aces)
    // -------------------------------------------------------------
    // Ace played taking another Ace as 1
    struct s_cte_move move_ace1;
    move_ace1.card_played = 9; // Ace of clubs
    move_ace1.cards_picked.size = 1;
    move_ace1.cards_picked.max = 1;
    move_ace1.cards_picked.array = (uint8_t[]){22}; // Ace of diamonds
    err = is_legal(&legal, &move_ace1);
    assert(err == e_ok && legal);

    // Ace played taking 5 + 6 as 11
    struct s_cte_move move_ace11;
    move_ace11.card_played = 9; // Ace of clubs
    move_ace11.cards_picked.size = 2;
    move_ace11.cards_picked.max = 2;
    move_ace11.cards_picked.array = (uint8_t[]){3, 4}; // 5 clubs, 6 clubs
    err = is_legal(&legal, &move_ace11);
    assert(err == e_ok && legal);

    // Ace on table counted as 1: 10 takes 9 + Ace(1)
    struct s_cte_move move_ace_as_1;
    move_ace_as_1.card_played = 8; // 10 of clubs
    move_ace_as_1.cards_picked.size = 2;
    move_ace_as_1.cards_picked.max = 2;
    move_ace_as_1.cards_picked.array = (uint8_t[]){7, 22}; // 9 clubs, Ace diamonds (counted as 1)
    err = is_legal(&legal, &move_ace_as_1);
    assert(err == e_ok && legal);

    // 2 takes two Aces on table (1 + 1 = 2)
    struct s_cte_move move_two_aces;
    move_two_aces.card_played = 0; // 2 of clubs
    move_two_aces.cards_picked.size = 2;
    move_two_aces.cards_picked.max = 2;
    move_two_aces.cards_picked.array = (uint8_t[]){9, 22}; // Ace clubs, Ace diamonds
    err = is_legal(&legal, &move_two_aces);
    assert(err == e_ok && legal);

    // Jack (12) takes Ace(1) + 2 + 9 = 12
    struct s_cte_move move_jack_ace;
    move_jack_ace.card_played = 10; // Jack of clubs (val 12)
    move_jack_ace.cards_picked.size = 3;
    move_jack_ace.cards_picked.max = 3;
    move_jack_ace.cards_picked.array = (uint8_t[]){9, 0, 7}; // Ace clubs (1), 2 clubs (2), 9 clubs (9)
    err = is_legal(&legal, &move_jack_ace);
    assert(err == e_ok && legal);

    // Jack (12) trying to take Ace + 2 + 8 (Sum with Ace=1 is 11, with Ace=11 is 21 -> illegal)
    struct s_cte_move move_jack_illegal;
    move_jack_illegal.card_played = 10; // Jack of clubs (val 12)
    move_jack_illegal.cards_picked.size = 3;
    move_jack_illegal.cards_picked.max = 3;
    move_jack_illegal.cards_picked.array = (uint8_t[]){9, 0, 6}; // Ace clubs, 2 clubs, 8 clubs
    err = is_legal(&legal, &move_jack_illegal);
    assert(err == e_ok && !legal);

    // -------------------------------------------------------------
    // Advanced difficult tests for Ace captures (requested)
    // -------------------------------------------------------------

    // Test 1: Disjoint sums with Ace counted as 1 in one sum
    // King (val 14) takes (10 + 4 = 14) and (Ace(1) + Queen(13) = 14)
    struct s_cte_move move_king_disjoint_ace1;
    move_king_disjoint_ace1.card_played = 12; // King of clubs (val 14)
    move_king_disjoint_ace1.cards_picked.size = 4;
    move_king_disjoint_ace1.cards_picked.max = 4;
    move_king_disjoint_ace1.cards_picked.array = (uint8_t[]){
        21, // 10 of diamonds (val 10)
        2,  // 4 of clubs (val 4)
        9,  // Ace of clubs (val 11 -> counted as 1)
        11  // Queen of clubs (val 13)
    };
    err = is_legal(&legal, &move_king_disjoint_ace1);
    assert(err == e_ok && legal);

    // Test 2: Disjoint sums with Ace counted as 11 in one sum
    // Queen (val 13) takes (Ace(11) + 2 = 13) and (10 + 3 = 13)
    struct s_cte_move move_queen_disjoint_ace11;
    move_queen_disjoint_ace11.card_played = 11; // Queen of clubs (val 13)
    move_queen_disjoint_ace11.cards_picked.size = 4;
    move_queen_disjoint_ace11.cards_picked.max = 4;
    move_queen_disjoint_ace11.cards_picked.array = (uint8_t[]){
        9,  // Ace of clubs (val 11 -> counted as 11)
        0,  // 2 of clubs (val 2)
        21, // 10 of diamonds (val 10)
        1   // 3 of clubs (val 3)
    };
    err = is_legal(&legal, &move_queen_disjoint_ace11);
    assert(err == e_ok && legal);

    // Test 3: Ace(1) + 10 taken with an Ace played as 11
    // Ace of spades (played as 11) takes (Ace of clubs(1) + 10 of diamonds(10) = 11)
    struct s_cte_move move_ace_takes_ace_plus_ten;
    move_ace_takes_ace_plus_ten.card_played = 48; // Ace of spades (val 11)
    move_ace_takes_ace_plus_ten.cards_picked.size = 2;
    move_ace_takes_ace_plus_ten.cards_picked.max = 2;
    move_ace_takes_ace_plus_ten.cards_picked.array = (uint8_t[]){
        9,  // Ace of clubs (val 11 -> counted as 1)
        21  // 10 of diamonds (val 10)
    };
    err = is_legal(&legal, &move_ace_takes_ace_plus_ten);
    assert(err == e_ok && legal);


    // -------------------------------------------------------------
    // Test Move Generation: gen_card_moves
    // -------------------------------------------------------------
    table.cards_on_table[0] = 21; // 10 of diamonds (val 10)
    table.cards_on_table[1] = 5;  // 7 of clubs (val 7)
    table.cards_on_table[2] = 1;  // 3 of clubs (val 3)
    table.cards_on_table[3] = 2;  // 4 of clubs (val 4)
    table.cards_on_table[4] = 4;  // 6 of clubs (val 6)
    table.nb_cards_on_table = 5;

    struct s_cte_move_list move_list;
    err = init_move_list(&move_list, 16);
    assert(err == e_ok);

    // Play 10 of clubs (val 10)
    err = gen_card_moves(&move_list, 8);
    assert(err == e_ok);

    // 3 base subsets summing to 10: {10}, {7,3}, {4,6}
    // Expected combinations:
    // Drop (1), size 1 (3), size 2 (3), size 3 (1) -> 8 moves total
    assert(move_list.size == 8);

    // Verify all generated moves are certified legal by is_legal
    for(uint16_t m = 0; m < move_list.size; m++){
        bool is_valid = false;
        err = is_legal(&is_valid, &move_list.moves[m]);
        assert(err == e_ok);
        assert(is_valid);
    }
    free_move_list(&move_list);

    // Test gen_card_moves for Test 1: King on {10, 4, Ace, Queen}
    table.cards_on_table[0] = 21; // 10 diamonds
    table.cards_on_table[1] = 2;  // 4 clubs
    table.cards_on_table[2] = 9;  // Ace clubs
    table.cards_on_table[3] = 11; // Queen clubs
    table.nb_cards_on_table = 4;

    err = init_move_list(&move_list, 8);
    assert(err == e_ok);
    err = gen_card_moves(&move_list, 12); // King of clubs (val 14)
    assert(err == e_ok);
    // Expected: Drop, {10,4}, {Ace(1),Queen}, {10,4,Ace(1),Queen} -> 4 moves
    assert(move_list.size == 4);
    for(uint16_t m = 0; m < move_list.size; m++){
        bool is_valid = false;
        err = is_legal(&is_valid, &move_list.moves[m]);
        assert(err == e_ok && is_valid);
    }
    free_move_list(&move_list);

    // Test gen_card_moves for Test 2: Queen on {Ace, 2, 10, 3}
    table.cards_on_table[0] = 9;  // Ace clubs
    table.cards_on_table[1] = 0;  // 2 clubs
    table.cards_on_table[2] = 21; // 10 diamonds
    table.cards_on_table[3] = 1;  // 3 clubs
    table.nb_cards_on_table = 4;

    err = init_move_list(&move_list, 8);
    assert(err == e_ok);
    err = gen_card_moves(&move_list, 11); // Queen of clubs (val 13)
    assert(err == e_ok);
    // Expected: Drop, {Ace(11),2}, {10,3}, {Ace(1),2,10}, {Ace(11),2, 10,3} -> 5 moves
    assert(move_list.size == 5);
    for(uint16_t m = 0; m < move_list.size; m++){
        bool is_valid = false;
        err = is_legal(&is_valid, &move_list.moves[m]);
        assert(err == e_ok && is_valid);
    }
    free_move_list(&move_list);

    // Test gen_card_moves for Test 3: Ace on {Ace, 10}
    table.cards_on_table[0] = 9;  // Ace clubs
    table.cards_on_table[1] = 21; // 10 diamonds
    table.nb_cards_on_table = 2;

    err = init_move_list(&move_list, 8);
    assert(err == e_ok);
    err = gen_card_moves(&move_list, 48); // Ace of spades (val 11 / 1)
    assert(err == e_ok);
    // Expected: Drop, {Ace} (val 1 or 11), {Ace(1),10} (val 11) -> 3 moves
    assert(move_list.size == 3);
    for(uint16_t m = 0; m < move_list.size; m++){
        bool is_valid = false;
        err = is_legal(&is_valid, &move_list.moves[m]);
        assert(err == e_ok && is_valid);
    }
    free_move_list(&move_list);


    // -------------------------------------------------------------
    // Test gen_all_moves
    // -------------------------------------------------------------
    table.cards_on_table[0] = 21; // 10 of diamonds (val 10)
    table.cards_on_table[1] = 5;  // 7 of clubs (val 7)
    table.cards_on_table[2] = 1;  // 3 of clubs (val 3)
    table.cards_on_table[3] = 2;  // 4 of clubs (val 4)
    table.cards_on_table[4] = 4;  // 6 of clubs (val 6)
    table.nb_cards_on_table = 5;

    struct s_cte_hand test_hand;
    test_hand.size = 2;
    test_hand.array[0] = 8;  // 10 of clubs (8 moves)
    test_hand.array[1] = 12; // King of clubs (val 14 -> drop, {10,4}, {7,3,4} -> 3 moves)

    struct s_cte_move_list all_moves;
    err = init_move_list(&all_moves, 16);
    assert(err == e_ok);
    err = gen_all_moves(&all_moves, &test_hand);
    assert(err == e_ok);
    // 8 moves for 10 of clubs + 3 moves for King of clubs (drop, {10,4}, {7,3,4}) = 11 moves
    assert(all_moves.size == 11);
    for(uint16_t m = 0; m < all_moves.size; m++){
        bool is_valid = false;
        err = is_legal(&is_valid, &all_moves.moves[m]);
        assert(err == e_ok);
        assert(is_valid);
    }
    free_move_list(&all_moves);

    // -------------------------------------------------------------
    // Test play_move: Drop, Capture & Tablic
    // -------------------------------------------------------------
    // 1. Drop move
    table.nb_cards_on_table = 2;
    table.cards_on_table[0] = 0;
    table.cards_on_table[1] = 1;
    players.players[0].hand.size = 1;
    players.players[0].hand.array[0] = 8;
    players.players[0].won_cards.size = 0;
    players.players[0].nb_tablic = 0;

    struct s_cte_move test_drop = { .card_played = 8, .cards_picked = { .size = 0, .max = 0, .array = NULL } };
    err = play_move(&test_drop, &players.players[0]);
    assert(err == e_ok);
    assert(players.players[0].hand.size == 0);
    assert(table.nb_cards_on_table == 3);
    assert(table.cards_on_table[2] == 8);

    // 2. Capture and Tablic
    table.nb_cards_on_table = 1;
    table.cards_on_table[0] = 21; // 10 diamonds
    players.players[0].hand.size = 1;
    players.players[0].hand.array[0] = 8; // 10 clubs
    players.players[0].won_cards.size = 0;
    players.players[0].nb_tablic = 0;

    struct s_cte_move test_tablic;
    test_tablic.card_played = 8;
    test_tablic.cards_picked.size = 1;
    test_tablic.cards_picked.max = 1;
    test_tablic.cards_picked.array = malloc(sizeof(uint8_t));
    test_tablic.cards_picked.array[0] = 21;

    err = play_move(&test_tablic, &players.players[0]);
    assert(err == e_ok);
    assert(players.players[0].hand.size == 0);
    assert(table.nb_cards_on_table == 0);
    assert(players.players[0].nb_tablic == 1);
    assert(players.players[0].won_cards.size == 2);
    free(test_tablic.cards_picked.array);

    free_players(&players);

    return 0;
}