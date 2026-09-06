#include "test_common.h"

int run_test_ai(void) {
    t_cteerr err;

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

    // ---- T15 : run_round fuzzing (20 seeds) ----
    s_cte_round_config config_rand = {
        .first_player  = 0,
        .evaluators    = { eval_random, eval_random, NULL, NULL },
        .eval_contexts = { NULL, NULL, NULL, NULL },
    };
    for (int seed = 0; seed < 20; seed++) {
        srand((unsigned)seed * 1337 + 42);
        reset_all_players(&game_ai.players);
        err = run_round(&game_ai, &config_rand);
        assert(err == e_ok);
        assert(game_ai.deck.cur_card == 52);
        assert(game_ai.table_bb == 0);
        assert(game_ai.players.players[0].won_cards.size + game_ai.players.players[1].won_cards.size == 52);
    }

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
    assert(backend->is_legal != NULL);
    assert(backend->gen_card_moves != NULL);
    assert(backend->gen_all_moves != NULL);

    s_cte_game backend_game;
    char *backend_names[2] = { "B_P1", "B_P2" };
    err = init_game(&backend_game, 2, backend_names, false);
    assert(err == e_ok);
    err = cte_set_backend(&backend_game, CTE_BACKEND_ARRAY);
    assert(err == e_ok);
    assert(backend_game.backend == backend);

    s_cte_round_config backend_cfg = {
        .first_player  = 0,
        .is_team_mode  = false,
        .evaluators    = { eval_greedy, eval_random, NULL, NULL },
        .eval_contexts = { NULL, NULL, NULL, NULL },
    };

    srand(7777);
    err = run_round(&backend_game, &backend_cfg);
    assert(err == e_ok);
    assert(backend_game.deck.cur_card == 52);
    assert(backend_game.table_bb == 0);
    assert(backend_game.players.players[0].won_cards.size + backend_game.players.players[1].won_cards.size == 52);

    s_cte_pos backend_pos = pos_from_game(&backend_game);
    assert(backend_pos.nb_players == 2);

    free_game(&backend_game);

    // Also validate CTE_BACKEND_BITBOARD contract & round execution
    const s_cte_engine_backend *backend_bb = cte_get_backend(CTE_BACKEND_BITBOARD);
    assert(backend_bb != NULL);
    assert(backend_bb->type == CTE_BACKEND_BITBOARD);
    assert(backend_bb->is_legal != NULL);
    assert(backend_bb->gen_card_moves != NULL);
    assert(backend_bb->gen_all_moves != NULL);

    s_cte_game rnk_game;
    char *rnk_names[2] = { "R_P1", "R_P2" };
    err = init_game(&rnk_game, 2, rnk_names, false);
    assert(err == e_ok);
    assert(rnk_game.backend == backend_bb);

    srand(8888);
    err = run_round(&rnk_game, &backend_cfg);
    assert(err == e_ok);
    assert(rnk_game.deck.cur_card == 52);
    assert(rnk_game.table_bb == 0);
    assert(rnk_game.players.players[0].won_cards.size + rnk_game.players.players[1].won_cards.size == 52);

    s_cte_pos rnk_pos = pos_from_game(&rnk_game);
    assert(rnk_pos.nb_players == 2);
    free_game(&rnk_game);

    // ---- T34 (part 1) : Validation de cte_get_evaluator ----
    const char *name = NULL;
    assert(cte_get_evaluator(AI_TYPE_RANDOM, &name) == eval_random);
    assert(name != NULL && strcmp(name, "Random") == 0);

    name = NULL;
    assert(cte_get_evaluator(AI_TYPE_DUMB, &name) == eval_dumb);
    assert(name != NULL && strcmp(name, "Dumb") == 0);

    name = NULL;
    assert(cte_get_evaluator(AI_TYPE_GREEDY, &name) == eval_greedy);
    assert(name != NULL && strcmp(name, "Greedy") == 0);

    name = NULL;
    assert(cte_get_evaluator(AI_TYPE_CHEATER, &name) == eval_cheater);
    assert(name != NULL && strcmp(name, "Cheater") == 0);

    // Fallback default
    name = NULL;
    assert(cte_get_evaluator((e_cte_ai_type)99, &name) == eval_random);
    assert(name != NULL && strcmp(name, "Random") == 0);

    // Support name_out == NULL
    assert(cte_get_evaluator(AI_TYPE_GREEDY, NULL) == eval_greedy);

    // ---- T35 : Iterative Deepening & Search Tree Metadata ----
    s_cte_pos iddfs_pos;
    memset(&iddfs_pos, 0, sizeof(iddfs_pos));
    iddfs_pos.nb_players = 2;
    iddfs_pos.current_player = 0;
    iddfs_pos.table_bb = (1ULL << 11) | (1ULL << 9); // Dame + As
    iddfs_pos.hand_counts[0] = 2;
    iddfs_pos.hand_bb[0] = (1ULL << 12) | (1ULL << 0); // Roi + 2
    iddfs_pos.hand_counts[1] = 2;
    iddfs_pos.hand_bb[1] = (1ULL << 2) | (1ULL << 3);

    struct s_cte_move_list iddfs_moves;
    err = init_move_list(&iddfs_moves, 8);
    assert(err == e_ok);
    err = pos_gen_moves(&iddfs_moves, &iddfs_pos);
    assert(err == e_ok);

    s_cte_search_config iddfs_cfg = {
        .max_depth = 2,
        .timeout_ms = 0
    };
    uint16_t best_iddfs = search_best_move(&iddfs_pos, &iddfs_moves, &iddfs_cfg);
    assert(iddfs_cfg.depth_reached == 2);
    assert(iddfs_cfg.num_candidates == iddfs_moves.size);
    assert(iddfs_cfg.nodes_visited > 0);
    assert(iddfs_cfg.candidates[0].score >= iddfs_cfg.candidates[1].score);
    assert(iddfs_cfg.candidates[0].depth_completed == 2);
    assert(iddfs_moves.moves[best_iddfs].card_played == 12);

    // Timeout behavior with high depth
    s_cte_search_config timeout_cfg = {
        .max_depth = 12,
        .timeout_ms = 5 // 5ms budget
    };
    uint16_t timeout_best = search_best_move(&iddfs_pos, &iddfs_moves, &timeout_cfg);
    assert(timeout_cfg.depth_reached >= 1);
    assert(timeout_cfg.num_candidates == iddfs_moves.size);
    assert(timeout_best < iddfs_moves.size);

    free_move_list(&iddfs_moves);

    free_game(&game_ai);
    return 0;
}

#ifdef TEST_STANDALONE
int main(void) {
    run_test_ai();
    printf("[PASS] test_ai standalone\n");
    return 0;
}
#endif
