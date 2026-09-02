#include "cte.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

// Static instances for isolated unit tests
static uint64_t table;
static struct deck deck;

int main(){
    s_cte_game game;
    t_cteerr err = init_game(&game, 2, (char*[]){"Alice", "Bob"}, false);
    assert(err == e_ok);

    struct s_cte_players *players = &game.players;

    assert(players->size == 2);
    assert(players->players[0].player_id == 0);
    assert(players->players[1].player_id == 1);
    assert(strcmp(players->players[0].player_name, "Alice") == 0);
    assert(strcmp(players->players[1].player_name, "Bob") == 0);

    // ---- T7 : init_players — chemins d'erreur ----
    struct s_cte_players p_err;
    t_cteerr e7 = init_players(&p_err, 1, (char*[]){"Solo"});
    assert(e7 == e_inval_val);

    e7 = init_players(&p_err, 5, (char*[]){"A","B","C","D","E"});
    assert(e7 == e_inval_val);

    e7 = init_players(&p_err, 0, (char*[]){});
    assert(e7 == e_inval_val);

    err = setup_round(&game);
    assert(err == e_ok);

    assert(players->players[0].hand.size == 6);
    assert(players->players[1].hand.size == 6);

    assert(game.deck.cur_card == 16);
    assert(__builtin_popcountll(game.table_bb) == 4);

    // ---- T8 : setup_round — contenu individuel de la table ----
    for(int i = 0; i < 4; i++){
        assert((game.table_bb & (1ULL << game.deck.cards[12 + i])) != 0);
    }

    // Verify deck shuffle
    srand(42);
    init_deck(&deck);
    shuffle_deck(&deck);
    bool shuffled = false;
    for(int i = 0; i < 52; i++){
        if(deck.cards[i] != i){
            shuffled = true;
            break;
        }
    }
    assert(shuffled);

    // Assert that all cards are present once across deck, players hands and table
    uint8_t card_present[52] = {0};
    for(int i = game.deck.cur_card; i < 52; i++){
        card_present[game.deck.cards[i]]++;
    }
    for(int i = 0; i < players->size; i++){
        for(int j = 0; j < players->players[i].hand.size; j++){
            card_present[players->players[i].hand.array[j]]++;
        }
    }
    uint64_t temp_bb = game.table_bb;
    while(temp_bb > 0){
        t_card c = (t_card)__builtin_ctzll(temp_bb);
        card_present[c]++;
        temp_bb &= (temp_bb - 1);
    }
    for(int i = 0; i < 52; i++){
        assert(card_present[i] == 1);
    }

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

    // ---- Tests dealing cycle ----
    err = setup_round(&game);
    assert(err == e_ok);
    reset_all_players(players);

    uint8_t cur_before = game.deck.cur_card;
    err = deal_next_hand(&game);
    assert(err == e_ok);
    assert(players->players[0].hand.size == 6);
    assert(players->players[1].hand.size == 6);
    assert(game.deck.cur_card == cur_before + 12);

    reset_all_players(players);
    cur_before = game.deck.cur_card;
    err = deal_next_hand(&game);
    assert(err == e_ok);
    assert(game.deck.cur_card == cur_before + 12);

    reset_all_players(players);
    cur_before = game.deck.cur_card;
    err = deal_next_hand(&game);
    assert(err == e_ok);
    assert(game.deck.cur_card == 52); // Exhausted

    reset_all_players(players);
    err = deal_next_hand(&game);
    assert(err == e_inval_val);

    // ---- Tests run_round ----
    reset_all_players(players);
    s_cte_round_config config = {
        .first_player    = 0,
        .evaluators      = { eval_random, eval_random, NULL, NULL },
        .eval_contexts   = { NULL, NULL, NULL, NULL },
    };

    srand(12345);
    err = run_round(&game, &config);
    assert(err == e_ok);
    assert(game.deck.cur_card == 52);
    assert(game.table_bb == 0);
    assert(players->players[0].hand.size == 0);
    assert(players->players[1].hand.size == 0);
    assert(players->players[0].won_cards.size + players->players[1].won_cards.size == 52);

    // ---- T12 : total card_points == 22 ----
    s_cte_round_score sc_total[2] = {0};
    err = compute_round_score(players, sc_total, false);
    assert(err == e_ok);
    assert(sc_total[0].card_points + sc_total[1].card_points == 22);

    // ---- Tests compute_round_score ----
    reset_all_players(players);
    for(uint8_t i = 0; i < 30; i++) players->players[0].won_cards.array[i] = i;
    players->players[0].won_cards.size = 30;
    players->players[0].nb_tablic = 2;
    for(uint8_t i = 0; i < 22; i++) players->players[1].won_cards.array[i] = 30 + i;
    players->players[1].won_cards.size = 22;
    players->players[1].nb_tablic = 0;

    s_cte_round_score scores[2] = {0};
    err = compute_round_score(players, scores, false);
    assert(err == e_ok);
    assert(scores[0].majority_bonus == 3);
    assert(scores[0].tablic_points  == 2);
    assert(scores[1].majority_bonus == 0);
    assert(scores[1].tablic_points  == 0);

    // ---- Tests match ----
    reset_all_players(players);
    struct s_cte_match match;
    err = init_match(&match, &game, 101);
    assert(err == e_ok);
    assert(match.winning_score == 101);

    srand(99);
    err = run_match(&match, &config);
    assert(err == e_ok);
    assert(match_is_over(&match));
    assert(match_winner(&match) >= 0);

    // ---- T15 : run_round fuzzing (20 seeds) ----
    for (int seed = 0; seed < 20; seed++) {
        srand((unsigned)seed * 1337 + 42);
        reset_all_players(players);
        err = run_round(&game, &config);
        assert(err == e_ok);
        assert(game.deck.cur_card == 52);
        assert(game.table_bb == 0);
        assert(players->players[0].won_cards.size + players->players[1].won_cards.size == 52);
    }

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

    // ---- T17 : Partie complète à 3 joueurs ----
    s_cte_game game_3p;
    char *names_3p[3] = { "Alice", "Bob", "Charlie" };
    err = init_game(&game_3p, 3, names_3p, false);
    assert(err == e_ok);

    s_cte_round_config config_3p = {
        .first_player  = 0,
        .is_team_mode  = false,
        .evaluators    = { eval_random, eval_random, eval_random, NULL },
        .eval_contexts = { NULL, NULL, NULL, NULL },
    };

    srand(777);
    err = run_round(&game_3p, &config_3p);
    assert(err == e_ok);
    assert(game_3p.deck.cur_card == 52);
    assert(game_3p.table_bb == 0);
    assert(game_3p.players.players[0].won_cards.size +
           game_3p.players.players[1].won_cards.size +
           game_3p.players.players[2].won_cards.size == 52);

    free_game(&game_3p);

    // ---- T18 : Partie complète à 4 joueurs (Individuel) ----
    s_cte_game game_4p;
    char *names_4p[4] = { "P1", "P2", "P3", "P4" };
    err = init_game(&game_4p, 4, names_4p, false);
    assert(err == e_ok);

    s_cte_round_config config_4p = {
        .first_player  = 0,
        .is_team_mode  = false,
        .evaluators    = { eval_random, eval_random, eval_random, eval_random },
        .eval_contexts = { NULL, NULL, NULL, NULL },
    };

    srand(888);
    err = run_round(&game_4p, &config_4p);
    assert(err == e_ok);
    assert(game_4p.deck.cur_card == 52);
    assert(game_4p.table_bb == 0);
    assert(game_4p.players.players[0].won_cards.size +
           game_4p.players.players[1].won_cards.size +
           game_4p.players.players[2].won_cards.size +
           game_4p.players.players[3].won_cards.size == 52);

    // ---- T19 : Partie à 4 joueurs en Mode Équipe 2v2 ----
    s_cte_round_config config_team = {
        .first_player  = 0,
        .is_team_mode  = true,
        .evaluators    = { eval_random, eval_random, eval_random, eval_random },
        .eval_contexts = { NULL, NULL, NULL, NULL },
    };

    reset_all_players(&game_4p.players);
    srand(999);
    err = run_round(&game_4p, &config_team);
    assert(err == e_ok);

    s_cte_round_score scores_team[4] = {0};
    err = compute_round_score(&game_4p.players, scores_team, true);
    assert(err == e_ok);

    struct s_cte_match match_team;
    err = init_match(&match_team, &game_4p, 51);
    assert(err == e_ok);
    err = run_match(&match_team, &config_team);
    assert(err == e_ok);
    assert(match_is_over(&match_team));

    free_game(&game_4p);

    // ---- T20 : Validation score_move & Évaluateurs IA ----
    s_cte_game game_ai;
    char *names_ai[2] = { "Greedy", "Dumb" };
    err = init_game(&game_ai, 2, names_ai, false);
    assert(err == e_ok);
    game_ai.players.players[0].evaluator = eval_greedy;
    game_ai.players.players[1].evaluator = eval_dumb;

    s_cte_round_config config_ai = {
        .first_player  = 0,
        .is_team_mode  = false,
        .evaluators    = { eval_greedy, eval_dumb, NULL, NULL },
        .eval_contexts = { NULL, NULL, NULL, NULL },
    };

    srand(555);
    err = run_round(&game_ai, &config_ai);
    assert(err == e_ok);
    assert(game_ai.deck.cur_card == 52);
    assert(game_ai.table_bb == 0);
    assert(game_ai.players.players[0].won_cards.size + game_ai.players.players[1].won_cards.size == 52);

    // ---- T21 : Conservation des 22 points sur 50 seeds ----
    game_ai.players.players[0].evaluator = eval_greedy;
    game_ai.players.players[1].evaluator = eval_cheater;
    config_ai.evaluators[0] = eval_greedy;
    config_ai.evaluators[1] = eval_cheater;

    for(unsigned int s = 200; s < 250; s++){
        reset_all_players(&game_ai.players);
        srand(s);
        err = run_round(&game_ai, &config_ai);
        assert(err == e_ok);

        uint8_t total_card_pts = 0;
        for(uint8_t p = 0; p < game_ai.players.size; p++){
            for(uint8_t c = 0; c < game_ai.players.players[p].won_cards.size; c++){
                total_card_pts += get_points(game_ai.players.players[p].won_cards.array[c]);
            }
        }
        assert(total_card_pts == 22);
    }

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

    // ---- T23 : award_remaining_table_cards ----
    reset_all_players(&game_ai.players);
    game_ai.table_bb = (1ULL << 0) | (1ULL << 1) | (1ULL << 2) | (1ULL << 3);
    game_ai.last_captor_id = 0;
    err = award_remaining_table_cards(&game_ai);
    assert(err == e_ok);
    assert(game_ai.table_bb == 0);
    assert(game_ai.players.players[0].won_cards.size == 4);

    game_ai.last_captor_id = 99; // Invalide
    err = award_remaining_table_cards(&game_ai);
    assert(err == e_inval_val);

    // ---- T24 : Minimax Tactical Resolution & Déterminisme ----
    s_cte_pos tactical_pos;
    memset(&tactical_pos, 0, sizeof(tactical_pos));
    tactical_pos.nb_players = 2;
    tactical_pos.current_player = 0;
    tactical_pos.table_bb = (1ULL << 11) | (1ULL << 9); // Dame (11) + As (9)
    tactical_pos.hand_counts[0] = 2;
    tactical_pos.hand_bb[0] = (1ULL << 12) | (1ULL << 0); // Roi (12) + 2 (0)
    tactical_pos.hand_counts[1] = 2;
    tactical_pos.hand_bb[1] = (1ULL << 2) | (1ULL << 3);

    struct s_cte_move_list tact_moves;
    err = init_move_list(&tact_moves, 8);
    assert(err == e_ok);
    err = pos_gen_moves(&tact_moves, &tactical_pos);
    assert(err == e_ok);

    s_cte_search_config search_cfg = { .max_depth = 2, .timeout_ms = 0 };
    uint16_t best_tact1 = search_best_move(&tactical_pos, &tact_moves, &search_cfg);
    uint16_t best_tact2 = search_best_move(&tactical_pos, &tact_moves, &search_cfg);
    assert(best_tact1 == best_tact2);
    assert(tact_moves.moves[best_tact1].card_played == 12);
    assert(tact_moves.moves[best_tact1].cards_picked.size == 2);
    free_move_list(&tact_moves);

    // ---- T25 : Fuzzing Multi-Joueurs (3p et 4p 2v2) croisé avec les 4 IA ----
    s_cte_game game_3p_fuzz;
    char *names_3p_fuzz[3] = { "Cheater", "Greedy", "Dumb" };
    err = init_game(&game_3p_fuzz, 3, names_3p_fuzz, false);
    assert(err == e_ok);
    game_3p_fuzz.players.players[0].evaluator = eval_cheater;
    game_3p_fuzz.players.players[1].evaluator = eval_greedy;
    game_3p_fuzz.players.players[2].evaluator = eval_dumb;

    s_cte_round_config config_3p_fuzz = {
        .first_player  = 0,
        .is_team_mode  = false,
        .evaluators    = { eval_cheater, eval_greedy, eval_dumb, NULL },
        .eval_contexts = { NULL, NULL, NULL, NULL },
    };

    for(unsigned int s = 300; s < 350; s++){
        reset_all_players(&game_3p_fuzz.players);
        srand(s);
        err = run_round(&game_3p_fuzz, &config_3p_fuzz);
        assert(err == e_ok);
        assert(game_3p_fuzz.deck.cur_card == 52);
        assert(game_3p_fuzz.table_bb == 0);
        uint8_t total_c = game_3p_fuzz.players.players[0].won_cards.size +
                          game_3p_fuzz.players.players[1].won_cards.size +
                          game_3p_fuzz.players.players[2].won_cards.size;
        assert(total_c == 52);
    }
    free_game(&game_3p_fuzz);

    // ---- T26 : Validation Couche d'Abstraction Backend (s_cte_engine_backend) ----
    const s_cte_engine_backend *backend = cte_get_backend(CTE_BACKEND_ARRAY);
    assert(backend != NULL);
    assert(backend->type == CTE_BACKEND_ARRAY);
    assert(backend->init_game != NULL);
    assert(backend->setup_round != NULL);
    assert(backend->deal_next_hand != NULL);
    assert(backend->award_remaining != NULL);
    assert(backend->run_round != NULL);
    assert(backend->is_legal != NULL);
    assert(backend->gen_all_moves != NULL);
    assert(backend->play_move != NULL);
    assert(backend->score_move != NULL);
    assert(backend->to_pos != NULL);

    s_cte_game backend_game;
    char *backend_names[2] = { "B_P1", "B_P2" };
    err = backend->init_game(&backend_game, 2, backend_names, false);
    assert(err == e_ok);

    s_cte_round_config backend_cfg = {
        .first_player  = 0,
        .is_team_mode  = false,
        .evaluators    = { eval_greedy, eval_random, NULL, NULL },
        .eval_contexts = { NULL, NULL, NULL, NULL },
    };

    srand(7777);
    err = backend->run_round(&backend_game, &backend_cfg);
    assert(err == e_ok);
    assert(backend_game.deck.cur_card == 52);
    assert(backend_game.table_bb == 0);
    assert(backend_game.players.players[0].won_cards.size + backend_game.players.players[1].won_cards.size == 52);

    s_cte_pos backend_pos = backend->to_pos(&backend_game);
    assert(backend_pos.nb_players == 2);

    backend->free_game(&backend_game);

    // Also validate CTE_BACKEND_BITBOARD_RANK contract & round execution
    const s_cte_engine_backend *backend_rnk = cte_get_backend(CTE_BACKEND_BITBOARD_RANK);
    assert(backend_rnk != NULL);
    assert(backend_rnk->type == CTE_BACKEND_BITBOARD_RANK);
    assert(backend_rnk->gen_all_moves != NULL);
    assert(backend_rnk->play_move != NULL);

    s_cte_game rnk_game;
    char *rnk_names[2] = { "R_P1", "R_P2" };
    err = backend_rnk->init_game(&rnk_game, 2, rnk_names, false);
    assert(err == e_ok);

    srand(8888);
    err = backend_rnk->run_round(&rnk_game, &backend_cfg);
    assert(err == e_ok);
    assert(rnk_game.deck.cur_card == 52);
    assert(rnk_game.table_bb == 0);
    assert(rnk_game.players.players[0].won_cards.size + rnk_game.players.players[1].won_cards.size == 52);

    s_cte_pos rnk_pos = backend_rnk->to_pos(&rnk_game);
    assert(rnk_pos.nb_players == 2);
    backend_rnk->free_game(&rnk_game);

    free_game(&game_ai);
    free_game(&game);

    printf("All tests passed\n");
    return 0;
}