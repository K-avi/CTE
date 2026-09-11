#include "test_common.h"

int run_test_ai(void) {
    unsigned int mseed = test_get_seed();
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

    srand(mseed ^ 0x555);
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
        srand(mseed + (unsigned)seed * 1337 + 42);
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

    for(unsigned int s = 0; s < 50; s++){
        reset_all_players(&game_3p_fuzz.players);
        srand(mseed + s * 101);
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

    srand(mseed ^ 0x7777);
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

    srand(mseed ^ 0x8888);
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

    // ---- T36 : Upper Bound Pruning (UBP) Multi-Models ----
    s_cte_pos ubp_pos;
    memset(&ubp_pos, 0, sizeof(ubp_pos));
    ubp_pos.nb_players = 2;
    ubp_pos.current_player = 0;
    ubp_pos.table_bb = (1ULL << 10) | (1ULL << 8); // Valet + 10
    ubp_pos.hand_counts[0] = 3;
    ubp_pos.hand_bb[0] = (1ULL << 12) | (1ULL << 0) | (1ULL << 1); // Roi, 2, 3
    ubp_pos.hand_counts[1] = 3;
    ubp_pos.hand_bb[1] = (1ULL << 4) | (1ULL << 5) | (1ULL << 6);
    ubp_pos.card_points[0] = 5;
    ubp_pos.card_points[1] = 8;
    ubp_pos.won_card_counts[0] = 12;
    ubp_pos.won_card_counts[1] = 18;

    int32_t ub_none = compute_upper_bound(&ubp_pos, 0, 2, UBP_NONE);
    int32_t ub_strict = compute_upper_bound(&ubp_pos, 0, 2, UBP_STRICT_ADMISSIBLE);
    int32_t ub_notablic = compute_upper_bound(&ubp_pos, 0, 2, UBP_NO_TABLIC);
    int32_t ub_heuristic = compute_upper_bound(&ubp_pos, 0, 2, UBP_TIGHT_HEURISTIC);
    int32_t lb_strict = compute_lower_bound(&ubp_pos, 0, 2, UBP_STRICT_ADMISSIBLE);

    assert(ub_none > ub_strict);
    assert(ub_strict >= ub_notablic);
    assert(ub_notablic >= ub_heuristic);
    assert(ub_strict >= lb_strict);

    struct s_cte_move_list ubp_moves;
    err = init_move_list(&ubp_moves, 8);
    assert(err == e_ok);
    err = pos_gen_moves(&ubp_moves, &ubp_pos);
    assert(err == e_ok);

    s_cte_search_config cfg_unpruned = {
        .max_depth = 2,
        .timeout_ms = 0,
        .ubp_model = UBP_NONE
    };
    uint16_t best_unpruned = search_best_move(&ubp_pos, &ubp_moves, &cfg_unpruned);

    s_cte_search_config cfg_strict = {
        .max_depth = 2,
        .timeout_ms = 0,
        .ubp_model = UBP_STRICT_ADMISSIBLE
    };
    uint16_t best_strict = search_best_move(&ubp_pos, &ubp_moves, &cfg_strict);

    // Strict admissible upper bound must find the identical move as full unpruned search
    assert(best_unpruned == best_strict);
    assert(cfg_strict.nodes_visited <= cfg_unpruned.nodes_visited);

    free_move_list(&ubp_moves);

    // ---- T37 : TUI Cheater Difficulty & UBP_NO_TABLIC Configuration ----
    s_cte_game game_diff;
    char *diff_names[2] = { "Cheater_Bot", "Greedy_Bot" };
    err = init_game(&game_diff, 2, diff_names, false);
    assert(err == e_ok);
    err = setup_round(&game_diff);
    assert(err == e_ok);

    struct s_cte_move_list test_moves;
    err = init_move_list(&test_moves, 16);
    assert(err == e_ok);
    err = gen_all_moves(&test_moves, game_diff.table_bb, &game_diff.players.players[0].hand);
    assert(err == e_ok);
    assert(test_moves.size > 0);

    s_cte_game_state st = {
        .table_bb = game_diff.table_bb,
        .players = &game_diff.players,
        .deck = &game_diff.deck,
        .current_player_id = 0,
        .is_team_mode = false,
    };

    // Test Cheater Easy (depth 2, UBP_NO_TABLIC)
    s_cte_search_config cfg_easy = {
        .max_depth = 2,
        .timeout_ms = 0,
        .ubp_model = UBP_NO_TABLIC
    };
    uint16_t move_easy = eval_cheater(&st, &test_moves, &cfg_easy);
    assert(move_easy < test_moves.size);
    assert(cfg_easy.nodes_visited > 0);
    assert(cfg_easy.depth_reached == 2);

    // Test Cheater Normal (depth 4, UBP_NO_TABLIC)
    s_cte_search_config cfg_normal = {
        .max_depth = 4,
        .timeout_ms = 0,
        .ubp_model = UBP_NO_TABLIC
    };
    uint16_t move_normal = eval_cheater(&st, &test_moves, &cfg_normal);
    assert(move_normal < test_moves.size);
    assert(cfg_normal.nodes_visited >= cfg_easy.nodes_visited);
    assert(cfg_normal.depth_reached == 4);

    // Test Cheater Master (depth 6, UBP_NO_TABLIC)
    s_cte_search_config cfg_master = {
        .max_depth = 6,
        .timeout_ms = 0,
        .ubp_model = UBP_NO_TABLIC
    };
    uint16_t move_master = eval_cheater(&st, &test_moves, &cfg_master);
    assert(move_master < test_moves.size);
    assert(cfg_master.nodes_visited >= cfg_normal.nodes_visited);
    assert(cfg_master.depth_reached == 6);

    // ---- T38 : Validation pos_evaluate_exact (Score terminal mathématique exact) ----
    s_cte_pos exact_pos;
    memset(&exact_pos, 0, sizeof(exact_pos));
    exact_pos.nb_players = 2;
    exact_pos.card_points[0] = 14;
    exact_pos.tablic_counts[0] = 1; // 14 + 2 = 16 pts
    exact_pos.won_card_counts[0] = 28; // >= 27: +3 pts (300 units)
    exact_pos.card_points[1] = 8;
    exact_pos.tablic_counts[1] = 0; // 8 pts
    exact_pos.won_card_counts[1] = 24;

    int32_t ex_score_p0 = pos_evaluate_exact(&exact_pos, 0);
    // Expected: (16 - 8) * 100 + 300 = +1100
    assert(ex_score_p0 == 1100);
    int32_t ex_score_p1 = pos_evaluate_exact(&exact_pos, 1);
    // Expected: (8 - 16) * 100 - 300 = -1100
    assert(ex_score_p1 == -1100);

    // Tie majority test (26-26 cards)
    exact_pos.won_card_counts[0] = 26;
    exact_pos.won_card_counts[1] = 26;
    assert(pos_evaluate_exact(&exact_pos, 0) == 800);

    // ---- T39 : Validation Solveur Donne 4 & Recherche Multi-Donnes ----
    s_cte_game game_deal4;
    char *d4_names[2] = { "P0", "P1" };
    err = init_game(&game_deal4, 2, d4_names, false);
    assert(err == e_ok);
    err = setup_round(&game_deal4);
    assert(err == e_ok);

    // Advance shoe to Deal 4 (cur_card = 40, deal 12 cards -> cur_card = 52)
    game_deal4.deck.cur_card = 40;
    err = deal_next_hand(&game_deal4);
    assert(err == e_ok);
    assert(game_deal4.deck.cur_card == 52);

    s_cte_pos pos_d4 = pos_from_game(&game_deal4);
    assert(pos_d4.deck != NULL);
    assert(pos_d4.cur_card == 52);

    struct s_cte_move_list d4_moves;
    err = init_move_list(&d4_moves, 16);
    assert(err == e_ok);
    err = pos_gen_moves(&d4_moves, &pos_d4);
    assert(err == e_ok);
    assert(d4_moves.size > 0);

    // Auto-solve Deal 4 to exact depth (12 plies)
    s_cte_search_config cfg_d4_solve = {
        .max_depth = 2, // base depth 2 will auto-expand to 12
        .timeout_ms = 0,
        .solve_deal4 = true,
        .ubp_model = UBP_STRICT_ADMISSIBLE,
    };
    uint16_t best_d4 = search_best_move(&pos_d4, &d4_moves, &cfg_d4_solve);
    assert(best_d4 < d4_moves.size);
    assert(cfg_d4_solve.depth_reached == 12);
    assert(cfg_d4_solve.nodes_visited > 0);

    // Test Multi-Deal lookahead from Deal 3 across deal boundary
    s_cte_game game_d3;
    char *d3_names[2] = { "P0", "P1" };
    err = init_game(&game_d3, 2, d3_names, false);
    assert(err == e_ok);
    err = setup_round(&game_d3);
    assert(err == e_ok);

    // Deal 3 starts at cur_card = 28 -> deal to cur_card = 40
    game_d3.deck.cur_card = 28;
    err = deal_next_hand(&game_d3);
    assert(err == e_ok);
    assert(game_d3.deck.cur_card == 40);

    s_cte_pos pos_d3 = pos_from_game(&game_d3);
    // Hands with 2 cards for P0 and 1 for P1 to test multi-deal transition with >= 2 moves
    pos_d3.hand_counts[0] = 2;
    pos_d3.hand_bb[0] = (1ULL << game_d3.deck.cards[28]) | (1ULL << game_d3.deck.cards[29]);
    pos_d3.hand_counts[1] = 1;
    pos_d3.hand_bb[1] = (1ULL << game_d3.deck.cards[34]);

    struct s_cte_move_list d3_moves;
    err = init_move_list(&d3_moves, 8);
    assert(err == e_ok);
    err = pos_gen_moves(&d3_moves, &pos_d3);
    assert(err == e_ok);
    assert(d3_moves.size >= 2);

    // Search with depth 4 and multi_deal = true: will cross from Deal 3 into Deal 4!
    s_cte_search_config cfg_cross_deal = {
        .max_depth = 4,
        .timeout_ms = 0,
        .multi_deal = true,
        .ubp_model = UBP_STRICT_ADMISSIBLE,
    };
    uint16_t best_cross = search_best_move(&pos_d3, &d3_moves, &cfg_cross_deal);
    assert(best_cross < d3_moves.size);
    assert(cfg_cross_deal.depth_reached == 4);

    free_move_list(&d3_moves);
    free_game(&game_d3);

    free_move_list(&d4_moves);
    free_game(&game_deal4);

    free_move_list(&test_moves);
    free_game(&game_diff);

    free_game(&game_ai);
    return 0;
}

#ifdef TEST_STANDALONE
int main(void) {
    test_get_seed();
    run_test_ai();
    printf("[PASS] test_ai standalone\n");
    return 0;
}
#endif
