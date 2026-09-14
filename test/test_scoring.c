#include "test_common.h"

int run_test_scoring(void) {
    unsigned int mseed = test_get_seed();
    s_cte_game game;
    t_cteerr err = init_game(&game, 2, (char*[]){"Alice", "Bob"}, false);
    assert(err == e_ok);
    struct s_cte_players *players = &game.players;

    s_cte_round_config config = {
        .first_player    = 0,
        .evaluators      = { eval_random, eval_random, NULL, NULL },
        .eval_contexts   = { NULL, NULL, NULL, NULL },
    };

    srand(mseed ^ 0x12345);
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

    // ---- T21: Conservation of 22 trick points across 50 seeds ----
    s_cte_game game_ai;
    char *names_ai[2] = { "Greedy", "Cheater" };
    err = init_game(&game_ai, 2, names_ai, false);
    assert(err == e_ok);
    game_ai.players.players[0].evaluator = eval_greedy;
    game_ai.players.players[1].evaluator = eval_cheater;

    s_cte_round_config config_ai = {
        .first_player  = 0,
        .is_team_mode  = false,
        .evaluators    = { eval_greedy, eval_cheater, NULL, NULL },
        .eval_contexts = { NULL, NULL, NULL, NULL },
    };

    for(unsigned int s = 0; s < 50; s++){
        reset_all_players(&game_ai.players);
        srand(mseed + s * 23 + 200);
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
    free_game(&game_ai);
    free_game(&game);
    return 0;
}

#ifdef TEST_STANDALONE
int main(void) {
    test_get_seed();
    run_test_scoring();
    printf("[PASS] test_scoring standalone\n");
    return 0;
}
#endif

