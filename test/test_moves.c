#include "test_common.h"

int run_test_moves(void) {
    uint64_t table;
    t_cteerr err;

    // Assert value and color macros
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

        uint8_t expected_points = 0;
        if(get_value(i) >= 10 && get_value(i) <= 14) expected_points = 1;
        if(get_color(i) == diamonds && get_value(i) == 10) expected_points = 2;
        if(get_color(i) == clubs && get_value(i) == 2) expected_points = 1;
        assert(get_points_var(get_value(i), get_color(i)) == expected_points);
        assert(get_points(i) == expected_points);
    }

    // -------------------------------------------------------------
    // Test is_legal: Valid single capture, multi-sum, and drop
    // -------------------------------------------------------------
    bool legal = false;
    struct s_cte_move move_drop = { .card_played = 8, .cards_picked = { .size = 0, .array = {0} } };
    err = is_legal(&legal, 0, &move_drop);
    assert(err == e_ok && legal);

    struct s_cte_move move_single = { .card_played = 8, .cards_picked = { .size = 1, .array = {21} } }; // 10 of diamonds
    err = is_legal(&legal, 0, &move_single);
    assert(err == e_ok && legal);

    struct s_cte_move move_sum = { .card_played = 8, .cards_picked = { .size = 2, .array = {5, 1} } }; // 7 clubs, 3 clubs
    err = is_legal(&legal, 0, &move_sum);
    assert(err == e_ok && legal);

    struct s_cte_move move_multi_sum = { .card_played = 8, .cards_picked = { .size = 5, .array = { 21, 5, 1, 2, 4 } } };
    err = is_legal(&legal, 0, &move_multi_sum);
    assert(err == e_ok && legal);

    // False partitions
    struct s_cte_move move_fp1 = { .card_played = 8, .cards_picked = { .size = 3, .array = { 3, 3, 0 } } };
    err = is_legal(&legal, 0, &move_fp1);
    assert(err == e_ok && !legal);

    struct s_cte_move move_fp2 = { .card_played = 8, .cards_picked = { .size = 1, .array = { 0 } } };
    err = is_legal(&legal, 0, &move_fp2);
    assert(err == e_ok && !legal);

    struct s_cte_move move_fp3 = { .card_played = 8, .cards_picked = { .size = 2, .array = { 21, 0 } } };
    err = is_legal(&legal, 0, &move_fp3);
    assert(err == e_ok && !legal);

    // Error handling
    struct s_cte_move dummy_move;
    dummy_move.card_played = 0;
    dummy_move.cards_picked.size = 0;
    t_cteerr err_null = is_legal(NULL, 0, &dummy_move);
    assert(err_null == e_null);

    bool dummy_legal;
    err_null = is_legal(&dummy_legal, 0, NULL);
    assert(err_null == e_null);

    // Tests with Ace (1 or 11)
    struct s_cte_move move_ace1 = { .card_played = 9, .cards_picked = { .size = 1, .array = { 22 } } }; // Ace diamonds
    err = is_legal(&legal, 0, &move_ace1);
    assert(err == e_ok && legal);

    struct s_cte_move move_ace11 = { .card_played = 9, .cards_picked = { .size = 2, .array = { 5, 2 } } }; // 7 clubs, 4 clubs -> 11
    err = is_legal(&legal, 0, &move_ace11);
    assert(err == e_ok && legal);

    struct s_cte_move move_ace_as_1 = { .card_played = 10, .cards_picked = { .size = 2, .array = { 9, 21 } } }; // Ace (1), 10 -> 11 != 12
    err = is_legal(&legal, 0, &move_ace_as_1);
    assert(err == e_ok && !legal);

    struct s_cte_move move_two_aces = { .card_played = 0, .cards_picked = { .size = 2, .array = { 9, 22 } } }; // Ace (1), Ace (1) -> 2
    err = is_legal(&legal, 0, &move_two_aces);
    assert(err == e_ok && legal);

    struct s_cte_move move_jack_ace = { .card_played = 10, .cards_picked = { .size = 2, .array = { 9, 22 } } }; // Ace (11), Ace (1) -> 12
    err = is_legal(&legal, 0, &move_jack_ace);
    assert(err == e_ok && legal);

    struct s_cte_move move_jack_illegal = { .card_played = 10, .cards_picked = { .size = 3, .array = { 9, 22, 0 } } };
    err = is_legal(&legal, 0, &move_jack_illegal);
    assert(err == e_ok && !legal);

    // Multi-sum Ace tests
    struct s_cte_move move_king_disjoint_ace1 = { .card_played = 12, .cards_picked = { .size = 4, .array = { 21, 2, 9, 11 } } };
    err = is_legal(&legal, 0, &move_king_disjoint_ace1);
    assert(err == e_ok && legal);

    struct s_cte_move move_queen_disjoint_ace11 = { .card_played = 11, .cards_picked = { .size = 4, .array = { 9, 0, 21, 1 } } };
    err = is_legal(&legal, 0, &move_queen_disjoint_ace11);
    assert(err == e_ok && legal);

    struct s_cte_move move_ace_takes_ace_plus_ten = { .card_played = 48, .cards_picked = { .size = 2, .array = { 9, 21 } } };
    err = is_legal(&legal, 0, &move_ace_takes_ace_plus_ten);
    assert(err == e_ok && legal);

    // -------------------------------------------------------------
    // Test Move Generation: gen_card_moves
    // -------------------------------------------------------------
    table = (1ULL << 21) | (1ULL << 5) | (1ULL << 1) | (1ULL << 2) | (1ULL << 4);
    // 21 (10D), 5 (7C), 1 (3C), 2 (4C), 4 (6C)

    struct s_cte_move_list move_list;
    err = init_move_list(&move_list, 16);
    assert(err == e_ok);

    err = gen_card_moves(&move_list, table, 8); // 10 of clubs
    assert(err == e_ok);
    assert(move_list.size == 8);

    for(uint16_t m = 0; m < move_list.size; m++){
        bool is_valid = false;
        err = is_legal(&is_valid, table, &move_list.moves[m]);
        assert(err == e_ok);
        assert(is_valid);
    }
    free_move_list(&move_list);

    // King on {10, 4, Ace, Queen}
    table = (1ULL << 21) | (1ULL << 2) | (1ULL << 9) | (1ULL << 11);

    err = init_move_list(&move_list, 8);
    assert(err == e_ok);
    err = gen_card_moves(&move_list, table, 12); // King of clubs (val 14)
    assert(err == e_ok);
    assert(move_list.size == 4);
    for(uint16_t m = 0; m < move_list.size; m++){
        bool is_valid = false;
        err = is_legal(&is_valid, table, &move_list.moves[m]);
        assert(err == e_ok && is_valid);
    }
    free_move_list(&move_list);

    // Queen on {Ace, 2, 10, 3}
    table = (1ULL << 9) | (1ULL << 0) | (1ULL << 21) | (1ULL << 1);

    err = init_move_list(&move_list, 8);
    assert(err == e_ok);
    err = gen_card_moves(&move_list, table, 11); // Queen of clubs (val 13)
    assert(err == e_ok);
    assert(move_list.size == 5);
    for(uint16_t m = 0; m < move_list.size; m++){
        bool is_valid = false;
        err = is_legal(&is_valid, table, &move_list.moves[m]);
        assert(err == e_ok && is_valid);
    }
    free_move_list(&move_list);

    // Ace on {Ace, 10}
    table = (1ULL << 9) | (1ULL << 21);

    err = init_move_list(&move_list, 8);
    assert(err == e_ok);
    err = gen_card_moves(&move_list, table, 48); // Ace of spades
    assert(err == e_ok);
    assert(move_list.size == 3);
    for(uint16_t m = 0; m < move_list.size; m++){
        bool is_valid = false;
        err = is_legal(&is_valid, table, &move_list.moves[m]);
        assert(err == e_ok && is_valid);
    }
    free_move_list(&move_list);

    // -------------------------------------------------------------
    // Test gen_all_moves
    // -------------------------------------------------------------
    table = (1ULL << 21) | (1ULL << 5) | (1ULL << 1) | (1ULL << 2) | (1ULL << 4);

    struct s_cte_hand test_hand;
    test_hand.size = 2;
    test_hand.array[0] = 8;  // 10 of clubs
    test_hand.array[1] = 12; // King of clubs

    struct s_cte_move_list all_moves;
    err = init_move_list(&all_moves, 16);
    assert(err == e_ok);
    err = gen_all_moves(&all_moves, table, &test_hand);
    assert(err == e_ok);
    assert(all_moves.size == 11);
    for(uint16_t m = 0; m < all_moves.size; m++){
        bool is_valid = false;
        err = is_legal(&is_valid, table, &all_moves.moves[m]);
        assert(err == e_ok);
        assert(is_valid);
    }
    free_move_list(&all_moves);

    // T4: Table vide
    table = 0;
    struct s_cte_move_list ml_empty;
    err = init_move_list(&ml_empty, 4);
    assert(err == e_ok);
    err = gen_card_moves(&ml_empty, table, 8);
    assert(err == e_ok);
    assert(ml_empty.size == 1);
    assert(ml_empty.moves[0].cards_picked.size == 0);
    assert(ml_empty.moves[0].card_played == 8);
    free_move_list(&ml_empty);

    // T5: Table vide, hand 1 carte
    struct s_cte_hand hand_one;
    hand_one.size = 1;
    hand_one.array[0] = 5; // 7 clubs
    struct s_cte_move_list ml_one;
    err = init_move_list(&ml_one, 4);
    assert(err == e_ok);
    err = gen_all_moves(&ml_one, table, &hand_one);
    assert(err == e_ok);
    assert(ml_one.size == 1);
    assert(ml_one.moves[0].card_played == 5);
    assert(ml_one.moves[0].cards_picked.size == 0);
    free_move_list(&ml_one);

    // T6: gen_card_moves aucun doublon
    table = (1ULL << 21) | (1ULL << 5) | (1ULL << 1) | (1ULL << 2) | (1ULL << 4);

    struct s_cte_move_list ml_nodup;
    err = init_move_list(&ml_nodup, 16);
    assert(err == e_ok);
    err = gen_card_moves(&ml_nodup, table, 8);
    assert(err == e_ok);
    for(uint16_t i = 0; i < ml_nodup.size; i++){
        for(uint16_t j = i + 1; j < ml_nodup.size; j++){
            struct s_cte_move *m1 = &ml_nodup.moves[i];
            struct s_cte_move *m2 = &ml_nodup.moves[j];
            if(m1->cards_picked.size == m2->cards_picked.size){
                uint32_t mask1 = 0, mask2 = 0;
                for(uint8_t k = 0; k < m1->cards_picked.size; k++) mask1 |= (1u << m1->cards_picked.array[k]);
                for(uint8_t k = 0; k < m2->cards_picked.size; k++) mask2 |= (1u << m2->cards_picked.array[k]);
                assert(mask1 != mask2);
            }
        }
    }
    free_move_list(&ml_nodup);

    // -------------------------------------------------------------
    // Test play_move
    // -------------------------------------------------------------
    table = (1ULL << 21) | (1ULL << 5) | (1ULL << 1) | (1ULL << 2) | (1ULL << 4);

    struct s_cte_player_data test_player;
    memset(&test_player, 0, sizeof(test_player));
    test_player.hand.size = 1;
    test_player.hand.array[0] = 8; // 10 of clubs

    bool captured = false;
    err = play_move(&table, &move_multi_sum, &test_player, &captured);
    assert(err == e_ok);
    assert(captured == true);
    assert(test_player.hand.size == 0);
    assert(table == 0);
    assert(test_player.nb_tablic == 1);
    assert(test_player.won_cards.size == 6);

    // T1: Drop move execution
    table = 0;
    test_player.hand.size = 1;
    test_player.hand.array[0] = 8;
    test_player.won_cards.size = 0;
    test_player.nb_tablic = 0;
    captured = true;
    err = play_move(&table, &move_drop, &test_player, &captured);
    assert(err == e_ok);
    assert(captured == false);
    assert(test_player.hand.size == 0);
    assert(table == (1ULL << 8));
    assert(test_player.won_cards.size == 0);
    assert(test_player.nb_tablic == 0);

    // T2: play_move carte absente de la main
    struct s_cte_move move_absent = { .card_played = 50, .cards_picked = { .size = 0, .array = {0} } };
    test_player.hand.size = 1;
    test_player.hand.array[0] = 8;
    err = play_move(&table, &move_absent, &test_player, &captured);
    assert(err == e_inval_val);
    assert(test_player.hand.size == 1);

    // T3: play_move carte ciblée absente de la table
    table = (1ULL << 21);
    test_player.hand.size = 1;
    test_player.hand.array[0] = 8;
    struct s_cte_move move_bad_target = { .card_played = 8, .cards_picked = { .size = 1, .array = { 5 } } };
    err = play_move(&table, &move_bad_target, &test_player, &captured);
    assert(err == e_inval_val);

    // ---- Tests format_card & format_move ----
    char c_buf[32];
    format_card(c_buf, sizeof(c_buf), 0, CTE_RENDER_UNICODE);
    assert(strcmp(c_buf, "2♣") == 0);
    format_card(c_buf, sizeof(c_buf), 21, CTE_RENDER_UNICODE);
    assert(strcmp(c_buf, "10♦") == 0);
    format_card(c_buf, sizeof(c_buf), 35, CTE_RENDER_UNICODE);
    assert(strcmp(c_buf, "A♥") == 0);
    format_card(c_buf, sizeof(c_buf), 51, CTE_RENDER_UNICODE);
    assert(strcmp(c_buf, "K♠") == 0);

    // ---- T22 : Prise triple & Rejet strict ----
    table = (1ULL << 11) | (1ULL << 9) | (1ULL << 6) | (1ULL << 4) | (1ULL << 7) | (1ULL << 3);

    struct s_cte_move m_triple = {
        .card_played = 12, // Roi (14)
        .cards_picked = { .size = 6, .array = { 11, 9, 6, 4, 7, 3 } }
    };
    bool is_leg = false;
    err = is_legal(&is_leg, table, &m_triple);
    assert(err == e_ok && is_leg);

    table = (1ULL << 4) | (1ULL << 17) | (1ULL << 1);
    struct s_cte_move m_invalid = {
        .card_played = 9,
        .cards_picked = { .size = 3, .array = { 4, 17, 1 } }
    };
    is_leg = true;
    err = is_legal(&is_leg, table, &m_invalid);
    assert(err == e_ok && !is_leg);

    return 0;
}

#ifdef TEST_STANDALONE
int main(void) {
    test_get_seed();
    run_test_moves();
    printf("[PASS] test_moves standalone\n");
    return 0;
}
#endif
