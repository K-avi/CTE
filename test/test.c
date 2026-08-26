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

    // ---- T7 : init_players — chemins d'erreur ----
    struct s_cte_players p_err;
    t_cteerr e7 = init_players(&p_err, 1, (char*[]){"Solo"});
    assert(e7 == e_inval_val);

    e7 = init_players(&p_err, 5, (char*[]){"A","B","C","D","E"});
    assert(e7 == e_inval_val);

    e7 = init_players(&p_err, 0, (char*[]){});
    assert(e7 == e_inval_val);

    t_cteerr err = setup_game(&players);
    assert(err == e_ok);

    assert(players.players[0].hand.size == 6);
    assert(players.players[1].hand.size == 6);

    //print_hand(&players.players[0].hand);
    //print_hand(&players.players[1].hand);

    assert(deck.cur_card == 16);
    assert(table.nb_cards_on_table == 4);

    // ---- T8 : setup_game — contenu individuel de la table ----
    assert(table.cards_on_table[0] == deck.cards[12]);
    assert(table.cards_on_table[1] == deck.cards[13]);
    assert(table.cards_on_table[2] == deck.cards[14]);
    assert(table.cards_on_table[3] == deck.cards[15]);

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

    // ---- T1 : is_legal — chemins d'erreur NULL ----
    struct s_cte_move dummy_move = {
        .card_played = 8,
        .cards_picked = { .size = 0, .max = 0, .array = NULL }
    };
    t_cteerr err_null = is_legal(NULL, &dummy_move);
    assert(err_null == e_null);

    bool dummy_legal = false;
    err_null = is_legal(&dummy_legal, NULL);
    assert(err_null == e_null);

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

    // ---- T4 : gen_card_moves — table vide ----
    table.nb_cards_on_table = 0;

    struct s_cte_move_list ml_empty;
    t_cteerr err_e = init_move_list(&ml_empty, 4);
    assert(err_e == e_ok);
    err_e = gen_card_moves(&ml_empty, 8); // 10 clubs
    assert(err_e == e_ok);
    assert(ml_empty.size == 1); // seul le drop
    assert(ml_empty.moves[0].cards_picked.size == 0); // c'est bien un drop
    assert(ml_empty.moves[0].card_played == 8);
    free_move_list(&ml_empty);

    // ---- T5 : gen_all_moves — table vide, main à 1 carte ----
    table.nb_cards_on_table = 0;

    struct s_cte_hand hand_one;
    hand_one.size = 1;
    hand_one.array[0] = 5; // 7 clubs

    struct s_cte_move_list ml_one;
    t_cteerr err_o = init_move_list(&ml_one, 4);
    assert(err_o == e_ok);
    err_o = gen_all_moves(&ml_one, &hand_one);
    assert(err_o == e_ok);
    assert(ml_one.size == 1);
    assert(ml_one.moves[0].card_played == 5);
    assert(ml_one.moves[0].cards_picked.size == 0);
    free_move_list(&ml_one);

    // ---- T6 : gen_card_moves — aucun doublon ----
    table.cards_on_table[0] = 21;
    table.cards_on_table[1] = 5;
    table.cards_on_table[2] = 1;
    table.cards_on_table[3] = 2;
    table.cards_on_table[4] = 4;
    table.nb_cards_on_table = 5;

    struct s_cte_move_list ml_nodup;
    err = init_move_list(&ml_nodup, 16);
    assert(err == e_ok);
    err = gen_card_moves(&ml_nodup, 8);
    assert(err == e_ok);

    for (uint16_t i = 0; i < ml_nodup.size; i++) {
        for (uint16_t j = i + 1; j < ml_nodup.size; j++) {
            uint64_t mask_i = 0, mask_j = 0;
            for (uint8_t k = 0; k < ml_nodup.moves[i].cards_picked.size; k++)
                mask_i |= (1ULL << ml_nodup.moves[i].cards_picked.array[k]);
            for (uint8_t k = 0; k < ml_nodup.moves[j].cards_picked.size; k++)
                mask_j |= (1ULL << ml_nodup.moves[j].cards_picked.array[k]);
            assert(!(ml_nodup.moves[i].card_played == ml_nodup.moves[j].card_played
                     && mask_i == mask_j));
        }
    }
    free_move_list(&ml_nodup);

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
    bool captured = true; // doit passer à false pour un drop
    err = play_move(&test_drop, &players.players[0], &captured);
    assert(err == e_ok);
    assert(captured == false);
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

    err = play_move(&test_tablic, &players.players[0], &captured);
    assert(err == e_ok);
    assert(captured == true);
    assert(players.players[0].hand.size == 0);
    assert(table.nb_cards_on_table == 0);
    assert(players.players[0].nb_tablic == 1);
    assert(players.players[0].won_cards.size == 2);
    free(test_tablic.cards_picked.array);

    // ---- T2 : play_move — capture multi-cartes non-Tablic ----
    table.nb_cards_on_table = 3;
    table.cards_on_table[0] = 5; // 7 clubs
    table.cards_on_table[1] = 1; // 3 clubs
    table.cards_on_table[2] = 0; // 2 clubs (reste sur la table)
    players.players[0].hand.size = 1;
    players.players[0].hand.array[0] = 8; // 10 clubs
    players.players[0].won_cards.size = 0;
    players.players[0].nb_tablic = 0;

    struct s_cte_move test_multi_cap;
    test_multi_cap.card_played = 8;
    test_multi_cap.cards_picked.size = 2;
    test_multi_cap.cards_picked.max = 2;
    test_multi_cap.cards_picked.array = malloc(2 * sizeof(uint8_t));
    test_multi_cap.cards_picked.array[0] = 5;
    test_multi_cap.cards_picked.array[1] = 1;

    captured = false;
    err = play_move(&test_multi_cap, &players.players[0], &captured);
    assert(err == e_ok);
    assert(captured == true);
    assert(players.players[0].hand.size == 0);
    assert(table.nb_cards_on_table == 1);
    assert(table.cards_on_table[0] == 0); // 2 clubs est resté
    assert(players.players[0].won_cards.size == 3); // 10 clubs + 7 clubs + 3 clubs
    assert(players.players[0].nb_tablic == 0);

    bool found_8 = false, found_5 = false, found_1 = false;
    for (int i = 0; i < players.players[0].won_cards.size; i++) {
        if (players.players[0].won_cards.array[i] == 8) found_8 = true;
        if (players.players[0].won_cards.array[i] == 5) found_5 = true;
        if (players.players[0].won_cards.array[i] == 1) found_1 = true;
    }
    assert(found_8 && found_5 && found_1);
    free(test_multi_cap.cards_picked.array);

    // ---- T3 : play_move — paramètre captured = NULL ----
    table.nb_cards_on_table = 1;
    table.cards_on_table[0] = 21; // 10 diamonds
    players.players[0].hand.size = 1;
    players.players[0].hand.array[0] = 8; // 10 clubs

    // Drop avec captured = NULL
    struct s_cte_move drop_null = {
        .card_played = 8,
        .cards_picked = { .size = 0, .max = 0, .array = NULL }
    };
    t_cteerr err_cn = play_move(&drop_null, &players.players[0], NULL);
    assert(err_cn == e_ok);
    assert(table.nb_cards_on_table == 2);

    // Capture avec captured = NULL
    players.players[0].hand.size = 1;
    players.players[0].hand.array[0] = 8; // 10 clubs
    struct s_cte_move cap_null;
    cap_null.card_played = 8;
    cap_null.cards_picked.size = 1;
    cap_null.cards_picked.max = 1;
    cap_null.cards_picked.array = malloc(sizeof(uint8_t));
    cap_null.cards_picked.array[0] = 21;

    uint8_t won_before = players.players[0].won_cards.size;
    t_cteerr err_cn2 = play_move(&cap_null, &players.players[0], NULL);
    assert(err_cn2 == e_ok);
    assert(players.players[0].won_cards.size == won_before + 2);
    free(cap_null.cards_picked.array);

    // ---- Tests award_remaining_table_cards ----
    // Cas 1 : table non vide -> attribuer au joueur 0
    table.nb_cards_on_table = 3;
    table.cards_on_table[0] = 5;
    table.cards_on_table[1] = 10;
    table.cards_on_table[2] = 15;
    players.players[0].won_cards.size = 0;
    err = award_remaining_table_cards(&players, 0);
    assert(err == e_ok);
    assert(table.nb_cards_on_table == 0);
    assert(players.players[0].won_cards.size == 3);
    assert(players.players[0].won_cards.array[0] == 5);
    assert(players.players[0].won_cards.array[1] == 10);
    assert(players.players[0].won_cards.array[2] == 15);

    // ---- T10 : award_remaining_table_cards — append sur won_cards non vide ----
    players.players[1].won_cards.size = 5;
    for (uint8_t i = 0; i < 5; i++) players.players[1].won_cards.array[i] = i;

    table.nb_cards_on_table = 2;
    table.cards_on_table[0] = 30;
    table.cards_on_table[1] = 31;

    t_cteerr err_t10 = award_remaining_table_cards(&players, 1);
    assert(err_t10 == e_ok);
    assert(table.nb_cards_on_table == 0);
    assert(players.players[1].won_cards.size == 7); // 5 + 2
    assert(players.players[1].won_cards.array[5] == 30);
    assert(players.players[1].won_cards.array[6] == 31);

    // Cas 2 : table vide -> aucun effet
    players.players[1].won_cards.size = 0;
    err = award_remaining_table_cards(&players, 1);
    assert(err == e_ok);
    assert(players.players[1].won_cards.size == 0); // inchangé

    // Cas 3 : id invalide
    table.nb_cards_on_table = 1;
    table.cards_on_table[0] = 7;
    err = award_remaining_table_cards(&players, 99);
    assert(err == e_inval_val);
    assert(table.nb_cards_on_table == 1); // table inchangée
    table.nb_cards_on_table = 0; // nettoyage

    // ---- Tests reset_player_round / reset_all_players ----
    // Après le tablic précédent, player[0] : won_cards.size=2, nb_tablic=1, hand.size=0
    // On simule un état non-vide pour être sûr de tester le reset.
    players.players[1].hand.size = 3;
    players.players[1].won_cards.size = 10;
    players.players[1].nb_tablic = 2;

    // reset d'un seul joueur
    reset_player_round(&players.players[0]);
    assert(players.players[0].hand.size == 0);
    assert(players.players[0].won_cards.size == 0);
    assert(players.players[0].nb_tablic == 0);
    // Le nom et l'id ne doivent pas être altérés
    assert(players.players[0].player_id == 0);
    assert(strcmp(players.players[0].player_name, "Alice") == 0);

    // reset de tous les joueurs
    reset_all_players(&players);
    assert(players.players[1].hand.size == 0);
    assert(players.players[1].won_cards.size == 0);
    assert(players.players[1].nb_tablic == 0);
    assert(players.players[1].player_id == 1);
    assert(strcmp(players.players[1].player_name, "Bob") == 0);

    // ---- Tests deal_next_hand ----
    // On repart d'une partie fraîchement initialisée pour avoir un état propre.
    // setup_game a déjà eu lieu plus haut. On réinitialise manuellement l'état du
    // paquet et des mains pour simuler un début de manche en bonne et due forme.
    // État après setup_game : deck.cur_card == 16, 12 cartes distribuées + 4 en table.

    // Simuler que les joueurs ont joué toutes leurs cartes (mains vides)
    reset_all_players(&players);

    // Capturer l'état de la table avant deal (T9)
    uint8_t table_nb_before = table.nb_cards_on_table;
    uint8_t table_snap[52];
    for (int i = 0; i < table.nb_cards_on_table; i++)
        table_snap[i] = table.cards_on_table[i];

    // Tour 2 : redistribution depuis deck.cur_card == 16
    uint8_t cur_before = deck.cur_card;
    err = deal_next_hand(&players);
    assert(err == e_ok);
    assert(players.players[0].hand.size == 6);
    assert(players.players[1].hand.size == 6);
    assert(deck.cur_card == cur_before + 12);
    // T9 : table inchangée après deal_next_hand
    assert(table.nb_cards_on_table == table_nb_before);
    for (int i = 0; i < table_nb_before; i++)
        assert(table.cards_on_table[i] == table_snap[i]);

    // Vérifier que les cartes données sont bien celles du paquet à partir de cur_before
    for(int i = 0; i < 6; i++){
        assert(players.players[0].hand.array[i] == deck.cards[cur_before + i]);
        assert(players.players[1].hand.array[i] == deck.cards[cur_before + i + 6]);
    }

    // Tour 3 : deuxième redistribution
    reset_all_players(&players);
    cur_before = deck.cur_card;
    err = deal_next_hand(&players);
    assert(err == e_ok);
    assert(deck.cur_card == cur_before + 12);

    // Tour 4 : troisième redistribution (dernière — paquet à 52-16 = 36 cartes, 3x12)
    reset_all_players(&players);
    cur_before = deck.cur_card;
    err = deal_next_hand(&players);
    assert(err == e_ok);
    assert(deck.cur_card == 52); // paquet épuisé

    // Refus quand le paquet est épuisé
    reset_all_players(&players);
    err = deal_next_hand(&players);
    assert(err == e_inval_val);

    // Vérifier qu'un deal échoué ne modifie pas les mains
    players.players[0].hand.size = 3; // simule un état non vide
    players.players[1].hand.size = 2;
    err = deal_next_hand(&players); // paquet vide -> refus
    assert(err == e_inval_val);
    assert(players.players[0].hand.size == 3); // inchangé
    assert(players.players[1].hand.size == 2); // inchangé

    // ---- Tests run_round ----
    // On utilise eval_random avec une seed fixe pour la reproductibilité.
    // On repart de joueurs frais (reset) pour avoir un état propre.
    reset_all_players(&players);

    s_cte_round_config config = {
        .first_player    = 0,
        .evaluators      = { eval_random, eval_random, NULL, NULL },
        .eval_contexts   = { NULL, NULL, NULL, NULL },
    };

    srand(12345);
    err = run_round(&players, &config);
    assert(err == e_ok);

    // Invariant 1 : le paquet est épuisé
    assert(deck.cur_card == 52);

    // Invariant 2 : la table est vide (les cartes résiduelles ont été attribuées)
    assert(table.nb_cards_on_table == 0);

    // Invariant 3 : les mains sont vides
    assert(players.players[0].hand.size == 0);
    assert(players.players[1].hand.size == 0);

    // Invariant 4 : conservation des cartes — toutes les 52 cartes sont
    // exactement une fois dans les won_cards des joueurs.
    uint8_t card_seen[52] = {0};
    for(int p = 0; p < players.size; p++){
        for(int j = 0; j < players.players[p].won_cards.size; j++){
            uint8_t c = players.players[p].won_cards.array[j];
            assert(c < 52);
            card_seen[c]++;
        }
    }
    uint16_t total_won = 0;
    for(int p = 0; p < players.size; p++)
        total_won += players.players[p].won_cards.size;
    assert(total_won == 52);
    for(int i = 0; i < 52; i++)
        assert(card_seen[i] == 1);

    // ---- T12 : total card_points après manche complète == 22 ----
    s_cte_round_score sc_total[2] = {0};
    t_cteerr err_t12 = compute_round_score(&players, sc_total, false);
    assert(err_t12 == e_ok);
    // Somme de __tab_points sur les 52 cartes = 6 (clubs) + 6 (diamonds) + 5 (hearts) + 5 (spades) = 22
    assert(sc_total[0].card_points + sc_total[1].card_points == 22);

    // Invariant 5 : evaluateur NULL retourne e_null
    s_cte_round_config bad_config = {
        .first_player  = 0,
        .evaluators    = { NULL, eval_random, NULL, NULL },
        .eval_contexts = { NULL, NULL, NULL, NULL },
    };
    reset_all_players(&players);
    err = run_round(&players, &bad_config);
    assert(err == e_null);

    // ---- Tests compute_round_score (B.1) ----

    // Cas 1 : majorité nette (player 0 a 30 cartes, player 1 a 22)
    // On assigne 30 cartes à player 0 et 22 à player 1 manuellement.
    // Les cartes 0..29 (clubs 2..King + diamonds 2..6) et 30..51 pour player 1.
    reset_all_players(&players);
    // Player 0 : cartes 0..29 (30 cartes) — dont carte 0 (2♣, 1pt), carte 8 (10♣, 1pt), etc.
    for(uint8_t i = 0; i < 30; i++) players.players[0].won_cards.array[i] = i;
    players.players[0].won_cards.size = 30;
    players.players[0].nb_tablic = 2;
    // Player 1 : cartes 30..51 (22 cartes)
    for(uint8_t i = 0; i < 22; i++) players.players[1].won_cards.array[i] = 30 + i;
    players.players[1].won_cards.size = 22;
    players.players[1].nb_tablic = 0;

    s_cte_round_score scores[2] = {0};
    err = compute_round_score(&players, scores, false);
    assert(err == e_ok);

    // Player 0 : bonus majorité +3, tablic +2
    assert(scores[0].majority_bonus == 3);
    assert(scores[0].tablic_points  == 2);
    // Player 1 : pas de bonus
    assert(scores[1].majority_bonus == 0);
    assert(scores[1].tablic_points  == 0);

    // Vérifier card_points via somme brute des __tab_points
    uint8_t expected_p0 = 0;
    for(uint8_t i = 0; i < 30; i++) expected_p0 += get_points(i);
    assert(scores[0].card_points == expected_p0);
    uint8_t expected_p1 = 0;
    for(uint8_t i = 30; i < 52; i++) expected_p1 += get_points(i);
    assert(scores[1].card_points == expected_p1);

    assert(scores[0].total == scores[0].card_points + 3 + 2);
    assert(scores[1].total == scores[1].card_points + 0 + 0);

    // Cas 2 : égalité 26/26 → aucun bonus de majorité
    reset_all_players(&players);
    for(uint8_t i = 0; i < 26; i++) players.players[0].won_cards.array[i] = i;
    players.players[0].won_cards.size = 26;
    for(uint8_t i = 0; i < 26; i++) players.players[1].won_cards.array[i] = 26 + i;
    players.players[1].won_cards.size = 26;

    s_cte_round_score scores2[2] = {0};
    err = compute_round_score(&players, scores2, false);
    assert(err == e_ok);
    assert(scores2[0].majority_bonus == 0);
    assert(scores2[1].majority_bonus == 0);

    // ---- T11 : compute_round_score — valeurs absolues ----
    reset_all_players(&players);
    players.players[0].won_cards.array[0] = 0;  // 2 clubs (1 pt)
    players.players[0].won_cards.array[1] = 8;  // 10 clubs (1 pt)
    players.players[0].won_cards.array[2] = 9;  // Ace clubs (1 pt)
    players.players[0].won_cards.array[3] = 21; // 10 diamonds (2 pts)
    players.players[0].won_cards.array[4] = 22; // Ace diamonds (1 pt)
    players.players[0].won_cards.size = 5;
    players.players[0].nb_tablic = 0;

    players.players[1].won_cards.array[0] = 1; // 3 clubs
    players.players[1].won_cards.array[1] = 2; // 4 clubs
    players.players[1].won_cards.array[2] = 3; // 5 clubs
    players.players[1].won_cards.array[3] = 4; // 6 clubs
    players.players[1].won_cards.array[4] = 5; // 7 clubs
    players.players[1].won_cards.array[5] = 6; // 8 clubs
    players.players[1].won_cards.array[6] = 7; // 9 clubs
    players.players[1].won_cards.size = 7;
    players.players[1].nb_tablic = 0;

    s_cte_round_score sc_abs[2] = {0};
    t_cteerr err_t11 = compute_round_score(&players, sc_abs, false);
    assert(err_t11 == e_ok);
    assert(sc_abs[0].card_points == 6);
    assert(sc_abs[0].majority_bonus == 0);
    assert(sc_abs[0].tablic_points == 0);
    assert(sc_abs[0].total == 6);

    assert(sc_abs[1].card_points == 0);
    assert(sc_abs[1].majority_bonus == 0);
    assert(sc_abs[1].tablic_points == 0);
    assert(sc_abs[1].total == 0);

    // ---- T16 : compute_round_score — 27 vs 25 (seuil exact) ----
    reset_all_players(&players);
    for (uint8_t i = 0; i < 27; i++) players.players[0].won_cards.array[i] = i;
    players.players[0].won_cards.size = 27;
    for (uint8_t i = 0; i < 25; i++) players.players[1].won_cards.array[i] = 27 + i;
    players.players[1].won_cards.size = 25;

    s_cte_round_score sc_27[2] = {0};
    t_cteerr err_t16 = compute_round_score(&players, sc_27, false);
    assert(err_t16 == e_ok);
    assert(sc_27[0].majority_bonus == 3);
    assert(sc_27[1].majority_bonus == 0);

    // ---- Tests B.2 : init_match, match_is_over, match_winner, run_match ----

    reset_all_players(&players);

    // ---- T14 : init_match — winning_score = 0 ----
    struct s_cte_match match_bad;
    t_cteerr err_t14 = init_match(&match_bad, &players, 0);
    assert(err_t14 == e_inval_val);

    struct s_cte_match match;
    err = init_match(&match, &players, 101);
    assert(err == e_ok);
    assert(match.winning_score == 101);
    assert(match.round_nb == 0);
    assert(match.match_scores[0] == 0);
    assert(match.match_scores[1] == 0);

    // match_is_over : faux au départ
    assert(!match_is_over(&match));
    // match_winner : -1 si pas terminé
    assert(match_winner(&match) == -1);

    // Simuler un score déjà au-dessus du seuil
    match.match_scores[0] = 101;
    assert(match_is_over(&match));
    int8_t winner = match_winner(&match);
    assert(winner == 0);
    match.match_scores[0] = 0; // reset

    // ---- T13 : match_winner — égalité / scores au-dessus du seuil ----
    match.match_scores[0] = 105;
    match.match_scores[1] = 110;
    assert(match_is_over(&match));
    assert(match_winner(&match) == 1);

    match.match_scores[0] = 105;
    match.match_scores[1] = 105;
    assert(match_is_over(&match));
    int8_t tie_winner = match_winner(&match);
    assert(tie_winner >= 0);

    match.match_scores[0] = 0;
    match.match_scores[1] = 0;

    // run_match : match complet jusqu'à 101 avec eval_random
    srand(99);
    err = run_match(&match, &config);
    assert(err == e_ok);
    assert(match_is_over(&match));
    assert(match_winner(&match) >= 0);
    assert(match.round_nb > 0);

    // Après run_match, vérifier que les mains sont vides (reset_all_players fait en interne)
    assert(players.players[0].hand.size == 0);
    assert(players.players[1].hand.size == 0);

    // ---- T15 : run_round — fuzz conservation cartes sur 20 seeds ----
    s_cte_round_config fuzz_config = {
        .first_player  = 0,
        .evaluators    = { eval_random, eval_random, NULL, NULL },
        .eval_contexts = { NULL, NULL, NULL, NULL },
    };

    for (int seed = 0; seed < 20; seed++) {
        srand((unsigned)seed * 1337 + 42);
        reset_all_players(&players);

        t_cteerr err_fuzz = run_round(&players, &fuzz_config);
        assert(err_fuzz == e_ok);

        // Invariant : deck épuisé
        assert(deck.cur_card == 52);
        // Invariant : table vide
        assert(table.nb_cards_on_table == 0);
        // Invariant : mains vides
        assert(players.players[0].hand.size == 0);
        assert(players.players[1].hand.size == 0);
        // Invariant : conservation — exactement 52 cartes dans won_cards
        uint8_t fuzz_seen[52] = {0};
        for (int p = 0; p < players.size; p++)
            for (int j = 0; j < players.players[p].won_cards.size; j++)
                fuzz_seen[players.players[p].won_cards.array[j]]++;
        uint16_t fuzz_total = 0;
        for (int p = 0; p < players.size; p++)
            fuzz_total += players.players[p].won_cards.size;
        assert(fuzz_total == 52);
        for (int i = 0; i < 52; i++)
            assert(fuzz_seen[i] == 1);
    }

    // ---- Tests format_card & format_move ----
    char c_buf[32];
    format_card(c_buf, sizeof(c_buf), 0, CTE_RENDER_UNICODE);
    assert(strcmp(c_buf, "2♣") == 0);
    format_card(c_buf, sizeof(c_buf), 0, CTE_RENDER_ASCII);
    assert(strcmp(c_buf, "2C") == 0);

    format_card(c_buf, sizeof(c_buf), 21, CTE_RENDER_UNICODE);
    assert(strcmp(c_buf, "10♦") == 0);
    format_card(c_buf, sizeof(c_buf), 21, CTE_RENDER_ASCII);
    assert(strcmp(c_buf, "10D") == 0);

    format_card(c_buf, sizeof(c_buf), 35, CTE_RENDER_UNICODE);
    assert(strcmp(c_buf, "A♥") == 0);
    format_card(c_buf, sizeof(c_buf), 35, CTE_RENDER_ASCII);
    assert(strcmp(c_buf, "AH") == 0);

    format_card(c_buf, sizeof(c_buf), 51, CTE_RENDER_UNICODE);
    assert(strcmp(c_buf, "K♠") == 0);
    format_card(c_buf, sizeof(c_buf), 51, CTE_RENDER_ASCII);
    assert(strcmp(c_buf, "KS") == 0);

    // Test format_move drop
    struct s_cte_move m_fmt_drop = { .card_played = 21, .cards_picked = { .size = 0, .max = 0, .array = NULL } };
    char m_buf[128];
    format_move(m_buf, sizeof(m_buf), &m_fmt_drop, CTE_RENDER_UNICODE);
    assert(strcmp(m_buf, "Drop 10♦") == 0);
    format_move(m_buf, sizeof(m_buf), &m_fmt_drop, CTE_RENDER_ASCII);
    assert(strcmp(m_buf, "Drop 10D") == 0);

    // Test format_move capture
    struct s_cte_move m_fmt_cap;
    m_fmt_cap.card_played = 8;
    m_fmt_cap.cards_picked.size = 2;
    m_fmt_cap.cards_picked.max = 2;
    m_fmt_cap.cards_picked.array = (uint8_t[]){ 5, 1 };
    format_move(m_buf, sizeof(m_buf), &m_fmt_cap, CTE_RENDER_UNICODE);
    assert(strcmp(m_buf, "Play 10♣ -> Take [ 7♣, 3♣ ]") == 0);
    format_move(m_buf, sizeof(m_buf), &m_fmt_cap, CTE_RENDER_ASCII);
    assert(strcmp(m_buf, "Play 10C -> Take [ 7C, 3C ]") == 0);

    // ---- T17 : Partie complète à 3 joueurs ----
    struct s_cte_players players_3p;
    char *names_3p[3] = { "Alice", "Bob", "Charlie" };
    err = init_players(&players_3p, 3, names_3p);
    assert(err == e_ok);
    assert(players_3p.size == 3);

    s_cte_round_config config_3p = {
        .first_player  = 0,
        .is_team_mode  = false,
        .evaluators    = { eval_random, eval_random, eval_random, NULL },
        .eval_contexts = { NULL, NULL, NULL, NULL },
    };

    srand(777);
    err = run_round(&players_3p, &config_3p);
    assert(err == e_ok);
    assert(deck.cur_card == 52);
    assert(table.nb_cards_on_table == 0);
    for(int p = 0; p < 3; p++){
        assert(players_3p.players[p].hand.size == 0);
    }
    uint8_t seen_3p[52] = {0};
    uint16_t total_won_3p = 0;
    for(int p = 0; p < 3; p++){
        total_won_3p += players_3p.players[p].won_cards.size;
        for(int j = 0; j < players_3p.players[p].won_cards.size; j++){
            seen_3p[players_3p.players[p].won_cards.array[j]]++;
        }
    }
    assert(total_won_3p == 52);
    for(int i = 0; i < 52; i++){
        assert(seen_3p[i] == 1);
    }
    s_cte_round_score scores_3p[3] = {0};
    err = compute_round_score(&players_3p, scores_3p, false);
    assert(err == e_ok);
    assert(scores_3p[0].card_points + scores_3p[1].card_points + scores_3p[2].card_points == 22);

    // Fuzzing 3 joueurs (20 seeds)
    for(int s = 0; s < 20; s++){
        reset_all_players(&players_3p);
        srand(1000 + s);
        err = run_round(&players_3p, &config_3p);
        assert(err == e_ok);
        uint8_t f_seen[52] = {0};
        uint16_t f_tot = 0;
        for(int p = 0; p < 3; p++){
            f_tot += players_3p.players[p].won_cards.size;
            for(int j = 0; j < players_3p.players[p].won_cards.size; j++){
                f_seen[players_3p.players[p].won_cards.array[j]]++;
            }
        }
        assert(f_tot == 52);
        for(int i = 0; i < 52; i++) assert(f_seen[i] == 1);
    }
    free_players(&players_3p);

    // ---- T18 : Partie complète à 4 joueurs (Individuel) ----
    struct s_cte_players players_4p;
    char *names_4p[4] = { "P1", "P2", "P3", "P4" };
    err = init_players(&players_4p, 4, names_4p);
    assert(err == e_ok);
    assert(players_4p.size == 4);

    s_cte_round_config config_4p = {
        .first_player  = 0,
        .is_team_mode  = false,
        .evaluators    = { eval_random, eval_random, eval_random, eval_random },
        .eval_contexts = { NULL, NULL, NULL, NULL },
    };

    srand(888);
    err = run_round(&players_4p, &config_4p);
    assert(err == e_ok);
    assert(deck.cur_card == 52);
    assert(table.nb_cards_on_table == 0);
    for(int p = 0; p < 4; p++){
        assert(players_4p.players[p].hand.size == 0);
    }
    uint8_t seen_4p[52] = {0};
    uint16_t total_won_4p = 0;
    for(int p = 0; p < 4; p++){
        total_won_4p += players_4p.players[p].won_cards.size;
        for(int j = 0; j < players_4p.players[p].won_cards.size; j++){
            seen_4p[players_4p.players[p].won_cards.array[j]]++;
        }
    }
    assert(total_won_4p == 52);
    for(int i = 0; i < 52; i++){
        assert(seen_4p[i] == 1);
    }
    s_cte_round_score scores_4p[4] = {0};
    err = compute_round_score(&players_4p, scores_4p, false);
    assert(err == e_ok);
    assert(scores_4p[0].card_points + scores_4p[1].card_points + scores_4p[2].card_points + scores_4p[3].card_points == 22);

    // Fuzzing 4 joueurs individuel (20 seeds)
    for(int s = 0; s < 20; s++){
        reset_all_players(&players_4p);
        srand(2000 + s);
        err = run_round(&players_4p, &config_4p);
        assert(err == e_ok);
        uint8_t f_seen[52] = {0};
        uint16_t f_tot = 0;
        for(int p = 0; p < 4; p++){
            f_tot += players_4p.players[p].won_cards.size;
            for(int j = 0; j < players_4p.players[p].won_cards.size; j++){
                f_seen[players_4p.players[p].won_cards.array[j]]++;
            }
        }
        assert(f_tot == 52);
        for(int i = 0; i < 52; i++) assert(f_seen[i] == 1);
    }

    // ---- T19 : Partie à 4 joueurs en Mode Équipe 2v2 ----
    s_cte_round_config config_team = {
        .first_player  = 0,
        .is_team_mode  = true,
        .evaluators    = { eval_random, eval_random, eval_random, eval_random },
        .eval_contexts = { NULL, NULL, NULL, NULL },
    };

    reset_all_players(&players_4p);
    srand(999);
    err = run_round(&players_4p, &config_team);
    assert(err == e_ok);

    s_cte_round_score scores_team[4] = {0};
    err = compute_round_score(&players_4p, scores_team, true);
    assert(err == e_ok);

    uint8_t team0_cards = (uint8_t)(players_4p.players[0].won_cards.size + players_4p.players[2].won_cards.size);
    uint8_t team1_cards = (uint8_t)(players_4p.players[1].won_cards.size + players_4p.players[3].won_cards.size);
    assert(team0_cards + team1_cards == 52);

    if(team0_cards >= 27 && team0_cards != team1_cards){
        assert(scores_team[0].majority_bonus == 3);
        assert(scores_team[1].majority_bonus == 0);
    } else if(team1_cards >= 27 && team0_cards != team1_cards){
        assert(scores_team[1].majority_bonus == 3);
        assert(scores_team[0].majority_bonus == 0);
    } else {
        assert(scores_team[0].majority_bonus == 0);
        assert(scores_team[1].majority_bonus == 0);
    }

    // Match 2v2
    reset_all_players(&players_4p);
    struct s_cte_match match_team;
    err = init_match(&match_team, &players_4p, 51);
    assert(err == e_ok);
    err = run_match(&match_team, &config_team);
    assert(err == e_ok);
    assert(match_is_over(&match_team));
    int8_t team_winner = match_winner(&match_team);
    assert(team_winner == 0 || team_winner == 1);

    // ---- T20 : Validation score_move & Évaluateurs IA (dumb, greedy, cheater/minimax) ----
    // 1. Test score_move
    struct s_cte_move m_drop = { .card_played = 0, .cards_picked = { .size = 0, .max = 0, .array = NULL } };
    s_cte_move_score sc_d = score_move(&m_drop, 4);
    assert(sc_d.card_points == 0 && sc_d.nb_cards == 0 && !sc_d.is_tablic && sc_d.total_points == 0);

    // Prise de 10♣ (carte 8, 1 pt) avec 10♦ (carte 21, 2 pts) sur table de 3 cartes
    uint8_t picked_arr[1] = { 8 };
    struct s_cte_move m_cap = { .card_played = 21, .cards_picked = { .size = 1, .max = 1, .array = picked_arr } };
    s_cte_move_score sc_c = score_move(&m_cap, 3);
    assert(sc_c.card_points == 3); // 2 + 1 = 3
    assert(sc_c.nb_cards == 2);
    assert(!sc_c.is_tablic);
    assert(sc_c.total_points == 3);

    // Prise Tablić (1 carte sur table, ramasse 1 carte)
    s_cte_move_score sc_tab = score_move(&m_cap, 1);
    assert(sc_tab.is_tablic);
    assert(sc_tab.total_points == 4); // 3 + 1 bonus tablic

    // 2. Partie Greedy vs Dumb
    struct s_cte_players players_ai;
    char *names_ai[2] = { "Greedy", "Dumb" };
    err = init_players(&players_ai, 2, names_ai);
    assert(err == e_ok);
    players_ai.players[0].evaluator = eval_greedy;
    players_ai.players[1].evaluator = eval_dumb;

    s_cte_round_config config_ai = {
        .first_player  = 0,
        .is_team_mode  = false,
        .evaluators    = { eval_greedy, eval_dumb, NULL, NULL },
        .eval_contexts = { NULL, NULL, NULL, NULL },
    };

    srand(555);
    err = run_round(&players_ai, &config_ai);
    assert(err == e_ok);
    assert(deck.cur_card == 52);
    assert(table.nb_cards_on_table == 0);
    assert(players_ai.players[0].won_cards.size + players_ai.players[1].won_cards.size == 52);

    s_cte_round_score sc_ai[2] = {0};
    err = compute_round_score(&players_ai, sc_ai, false);
    assert(err == e_ok);
    // Greedy doit écraser Dumb
    assert(sc_ai[0].total >= sc_ai[1].total);

    // 3. Partie Cheater (Minimax) vs Random
    reset_all_players(&players_ai);
    players_ai.players[0].evaluator = eval_cheater;
    players_ai.players[1].evaluator = eval_random;
    config_ai.evaluators[0] = eval_cheater;
    config_ai.evaluators[1] = eval_random;

    srand(666);
    err = run_round(&players_ai, &config_ai);
    assert(err == e_ok);
    assert(deck.cur_card == 52);
    assert(table.nb_cards_on_table == 0);
    assert(players_ai.players[0].won_cards.size + players_ai.players[1].won_cards.size == 52);

    s_cte_round_score sc_cheat[2] = {0};
    err = compute_round_score(&players_ai, sc_cheat, false);
    assert(err == e_ok);
    assert(sc_cheat[0].total >= sc_cheat[1].total);

    // 4. Fuzzing multiseed Cheater vs Greedy (10 seeds)
    for(unsigned int s = 100; s < 110; s++){
        reset_all_players(&players_ai);
        players_ai.players[0].evaluator = eval_cheater;
        players_ai.players[1].evaluator = eval_greedy;
        config_ai.evaluators[0] = eval_cheater;
        config_ai.evaluators[1] = eval_greedy;

        srand(s);
        err = run_round(&players_ai, &config_ai);
        assert(err == e_ok);
        assert(deck.cur_card == 52);
        assert(table.nb_cards_on_table == 0);
        assert(players_ai.players[0].won_cards.size + players_ai.players[1].won_cards.size == 52);
    }

    // ---- T21 : Invariant de Conservation Absolue des 22 Points de Cartes ----
    for(unsigned int s = 200; s < 250; s++){
        reset_all_players(&players_ai);
        players_ai.players[0].evaluator = eval_greedy;
        players_ai.players[1].evaluator = eval_cheater;
        config_ai.evaluators[0] = eval_greedy;
        config_ai.evaluators[1] = eval_cheater;

        srand(s);
        err = run_round(&players_ai, &config_ai);
        assert(err == e_ok);
        assert(players_ai.players[0].won_cards.size + players_ai.players[1].won_cards.size == 52);

        uint8_t total_card_pts = 0;
        for(uint8_t p = 0; p < players_ai.size; p++){
            for(uint8_t c = 0; c < players_ai.players[p].won_cards.size; c++){
                total_card_pts += get_points(players_ai.players[p].won_cards.array[c]);
            }
        }
        assert(total_card_pts == 22);
    }

    // ---- T22 : Cas Limites de Partitionnement Exact (Prise triple & Rejet strict) ----
    // 1. Prise triple : Table = { Dame (11), As (9), 8 (6), 6 (4), 9 (7), 5 (3) }
    // Carte jouée = Roi (12, val 14). (13+1=14, 8+6=14, 9+5=14)
    table.nb_cards_on_table = 6;
    table.cards_on_table[0] = 11; // Dame de trèfle (val 13)
    table.cards_on_table[1] = 9;  // As de trèfle (val 1 / 11)
    table.cards_on_table[2] = 6;  // 8 de trèfle (val 8)
    table.cards_on_table[3] = 4;  // 6 de trèfle (val 6)
    table.cards_on_table[4] = 7;  // 9 de trèfle (val 9)
    table.cards_on_table[5] = 3;  // 5 de trèfle (val 5)

    uint8_t triple_arr[6] = { 11, 9, 6, 4, 7, 3 };
    struct s_cte_move m_triple = {
        .card_played = 12, // Roi de trèfle (val 14)
        .cards_picked = { .size = 6, .max = 6, .array = triple_arr }
    };
    bool is_leg = false;
    err = is_legal(&is_leg, &m_triple);
    assert(err == e_ok);
    assert(is_leg == true);

    // 2. Rejet strict : Table = { 5 (carte 4), 5 (carte 17), 2 (carte 1) }, Jouée = 10 (carte 9)
    // Tenter de ramasser les 3 cartes {5, 5, 2} avec un 10 doit être rejeté (5+5=10 mais 2 résiduel non apparié)
    table.nb_cards_on_table = 3;
    table.cards_on_table[0] = 4;
    table.cards_on_table[1] = 17;
    table.cards_on_table[2] = 1;

    uint8_t invalid_arr[3] = { 4, 17, 1 };
    struct s_cte_move m_invalid = {
        .card_played = 9, // 10 de pique (val 10)
        .cards_picked = { .size = 3, .max = 3, .array = invalid_arr }
    };
    is_leg = true;
    err = is_legal(&is_leg, &m_invalid);
    assert(err == e_ok);
    assert(is_leg == false);

    // ---- T23 : Validation award_remaining_table_cards ----
    reset_all_players(&players_ai);
    table.nb_cards_on_table = 4;
    table.cards_on_table[0] = 0;
    table.cards_on_table[1] = 1;
    table.cards_on_table[2] = 2;
    table.cards_on_table[3] = 3;
    err = award_remaining_table_cards(&players_ai, 0);
    assert(err == e_ok);
    assert(table.nb_cards_on_table == 0);
    assert(players_ai.players[0].won_cards.size == 4);

    // ID de captor invalide (> players->size) renvoie e_inval_val
    err = award_remaining_table_cards(&players_ai, 99);
    assert(err == e_inval_val);

    // ---- T24 : Résolution Tactique Directe Minimax & Déterminisme ----
    s_cte_pos tactical_pos;
    memset(&tactical_pos, 0, sizeof(tactical_pos));
    tactical_pos.nb_players = 2;
    tactical_pos.current_player = 0;
    tactical_pos.table_count = 2;
    tactical_pos.table[0] = 11; // Dame de trèfle (val 13, 1 pt)
    tactical_pos.table[1] = 9;  // As de trèfle (val 1, 1 pt)
    tactical_pos.hand_counts[0] = 2;
    tactical_pos.hands[0][0] = 12; // Roi de trèfle (val 14, 1 pt) -> capture Dame+As = 3 pts + tablic !
    tactical_pos.hands[0][1] = 0;  // 2 de trèfle (val 2, 0 pt) -> défausse = 0 pt
    tactical_pos.hand_counts[1] = 2;
    tactical_pos.hands[1][0] = 2;
    tactical_pos.hands[1][1] = 3;

    struct s_cte_move_list tact_moves;
    err = init_move_list(&tact_moves, 8);
    assert(err == e_ok);
    err = pos_gen_moves(&tact_moves, &tactical_pos);
    assert(err == e_ok);

    s_cte_search_config search_cfg = { .max_depth = 2, .timeout_ms = 0 };
    uint16_t best_tact1 = search_best_move(&tactical_pos, &tact_moves, &search_cfg);
    uint16_t best_tact2 = search_best_move(&tactical_pos, &tact_moves, &search_cfg);
    assert(best_tact1 == best_tact2); // Déterminisme absolu

    // Le meilleur coup doit être la prise avec le Roi (card_played == 12 et cards_picked.size == 2)
    assert(tact_moves.moves[best_tact1].card_played == 12);
    assert(tact_moves.moves[best_tact1].cards_picked.size == 2);
    free_move_list(&tact_moves);

    // ---- T25 : Fuzzing Multi-Joueurs (3p et 4p 2v2) croisé avec les 4 IA (100 graines) ----
    // 1. Fuzzing 3 joueurs (50 graines)
    struct s_cte_players players_3p_fuzz;
    char *names_3p_fuzz[3] = { "Cheater", "Greedy", "Dumb" };
    err = init_players(&players_3p_fuzz, 3, names_3p_fuzz);
    assert(err == e_ok);
    players_3p_fuzz.players[0].evaluator = eval_cheater;
    players_3p_fuzz.players[1].evaluator = eval_greedy;
    players_3p_fuzz.players[2].evaluator = eval_dumb;

    s_cte_round_config config_3p_fuzz = {
        .first_player  = 0,
        .is_team_mode  = false,
        .evaluators    = { eval_cheater, eval_greedy, eval_dumb, NULL },
        .eval_contexts = { NULL, NULL, NULL, NULL },
    };

    for(unsigned int s = 300; s < 350; s++){
        reset_all_players(&players_3p_fuzz);
        srand(s);
        err = run_round(&players_3p_fuzz, &config_3p_fuzz);
        assert(err == e_ok);
        assert(deck.cur_card == 52);
        assert(table.nb_cards_on_table == 0);
        uint8_t total_c = players_3p_fuzz.players[0].won_cards.size +
                          players_3p_fuzz.players[1].won_cards.size +
                          players_3p_fuzz.players[2].won_cards.size;
        assert(total_c == 52);

        uint8_t pts = 0;
        for(uint8_t p = 0; p < 3; p++){
            for(uint8_t c = 0; c < players_3p_fuzz.players[p].won_cards.size; c++){
                pts += get_points(players_3p_fuzz.players[p].won_cards.array[c]);
            }
        }
        assert(pts == 22);
    }
    free_players(&players_3p_fuzz);

    // 2. Fuzzing 4 joueurs 2v2 par équipes (50 graines)
    struct s_cte_players players_4p_fuzz;
    char *names_4p_fuzz[4] = { "Cheater_T1", "Greedy_T2", "Cheater_T1", "Dumb_T2" };
    err = init_players(&players_4p_fuzz, 4, names_4p_fuzz);
    assert(err == e_ok);
    players_4p_fuzz.players[0].evaluator = eval_cheater;
    players_4p_fuzz.players[1].evaluator = eval_greedy;
    players_4p_fuzz.players[2].evaluator = eval_cheater;
    players_4p_fuzz.players[3].evaluator = eval_dumb;

    s_cte_round_config config_4p_fuzz = {
        .first_player  = 0,
        .is_team_mode  = true,
        .evaluators    = { eval_cheater, eval_greedy, eval_cheater, eval_dumb },
        .eval_contexts = { NULL, NULL, NULL, NULL },
    };

    for(unsigned int s = 400; s < 450; s++){
        reset_all_players(&players_4p_fuzz);
        srand(s);
        err = run_round(&players_4p_fuzz, &config_4p_fuzz);
        assert(err == e_ok);
        assert(deck.cur_card == 52);
        assert(table.nb_cards_on_table == 0);
        uint8_t total_c = players_4p_fuzz.players[0].won_cards.size +
                          players_4p_fuzz.players[1].won_cards.size +
                          players_4p_fuzz.players[2].won_cards.size +
                          players_4p_fuzz.players[3].won_cards.size;
        assert(total_c == 52);

        uint8_t pts = 0;
        for(uint8_t p = 0; p < 4; p++){
            for(uint8_t c = 0; c < players_4p_fuzz.players[p].won_cards.size; c++){
                pts += get_points(players_4p_fuzz.players[p].won_cards.array[c]);
            }
        }
        assert(pts == 22);
    }
    free_players(&players_4p_fuzz);

    free_players(&players_ai);

    free_players(&players_4p);
    free_players(&players);

    printf("All tests passed\n");

    return 0;
}