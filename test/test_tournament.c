#include "test_common.h"

int run_test_tournament(void) {
    t_cteerr err;

    // ---- T27 : Tournament Engine (Round Robin & Knockout) ----
    {
        // 1. Error handling on init
        s_cte_tournament t_err;
        s_cte_tournament_config cfg_err = {0};
        assert(init_tournament(NULL, &cfg_err) == e_null);
        assert(init_tournament(&t_err, NULL) == e_null);

        cfg_err.nb_participants = 1;
        assert(init_tournament(&t_err, &cfg_err) == e_inval_val);

        cfg_err.nb_participants = 17;
        assert(init_tournament(&t_err, &cfg_err) == e_inval_val);

        cfg_err.type = TOURNAMENT_KNOCKOUT;
        cfg_err.nb_participants = 3; // Not a power of 2
        assert(init_tournament(&t_err, &cfg_err) == e_inval_val);

        cfg_err.nb_participants = 6; // Not a power of 2
        assert(init_tournament(&t_err, &cfg_err) == e_inval_val);

        // 2. Round-Robin Tournament (4 AI participants)
        s_cte_tournament t_rr;
        s_cte_tournament_config cfg_rr = {
            .type = TOURNAMENT_ROUND_ROBIN,
            .nb_participants = 4,
            .winning_score = 15,
            .max_rounds = 1,
            .silent = true,
            .participants = {
                { .name = "Bot_Greedy",  .evaluator = eval_greedy,  .is_human = false, .ai_type = AI_TYPE_GREEDY },
                { .name = "Bot_Cheater", .evaluator = eval_cheater, .is_human = false, .ai_type = AI_TYPE_CHEATER },
                { .name = "Bot_Random",  .evaluator = eval_random,  .is_human = false, .ai_type = AI_TYPE_RANDOM },
                { .name = "Bot_Dumb",    .evaluator = eval_dumb,    .is_human = false, .ai_type = AI_TYPE_DUMB },
            },
        };

        err = init_tournament(&t_rr, &cfg_rr);
        assert(err == e_ok);
        assert(t_rr.config.nb_participants == 4);
        assert(t_rr.config.participants[0].elo_start == 1000);
        assert(t_rr.config.participants[1].elo_start == 1150);
        assert(t_rr.config.participants[2].elo_start == 300);
        assert(t_rr.config.participants[3].elo_start == 0);

        err = run_tournament(&t_rr);
        assert(err == e_ok);
        // In 4-player round robin: 4 * 3 / 2 = 6 matches
        assert(t_rr.nb_matches == 6);
        for(int i = 0; i < 4; i++){
            assert(t_rr.config.participants[i].matches_played == 3);
            assert(t_rr.config.participants[i].matches_won +
                   t_rr.config.participants[i].matches_lost +
                   t_rr.config.participants[i].matches_tied == 3);
        }
        assert(t_rr.champion_idx >= 0 && t_rr.champion_idx < 4);
        assert(t_rr.standings[0] == (uint8_t)t_rr.champion_idx);

        // Verify standings print doesn't crash
        print_tournament_standings(&t_rr, CTE_RENDER_ASCII);
        free_tournament(&t_rr);

        // 3. Knockout Tournament (4 AI participants)
        s_cte_tournament t_ko;
        s_cte_tournament_config cfg_ko = {
            .type = TOURNAMENT_KNOCKOUT,
            .nb_participants = 4,
            .winning_score = 15,
            .max_rounds = 1,
            .silent = true,
            .participants = {
                { .name = "Cup_Greedy",  .evaluator = eval_greedy,  .is_human = false, .ai_type = AI_TYPE_GREEDY },
                { .name = "Cup_Cheater", .evaluator = eval_cheater, .is_human = false, .ai_type = AI_TYPE_CHEATER },
                { .name = "Cup_Random",  .evaluator = eval_random,  .is_human = false, .ai_type = AI_TYPE_RANDOM },
                { .name = "Cup_Dumb",    .evaluator = eval_dumb,    .is_human = false, .ai_type = AI_TYPE_DUMB },
            },
        };

        err = init_tournament(&t_ko, &cfg_ko);
        assert(err == e_ok);

        err = run_tournament(&t_ko);
        assert(err == e_ok);
        // In 4-player knockout: 2 semifinals + 1 final = 3 matches
        assert(t_ko.nb_matches == 3);
        assert(t_ko.champion_idx >= 0 && t_ko.champion_idx < 4);
        // Champion must have won exactly 2 matches
        assert(t_ko.config.participants[t_ko.champion_idx].matches_won == 2);

        print_tournament_standings(&t_ko, CTE_RENDER_ASCII);
        free_tournament(&t_ko);
    }

    // ---- T28 : Player Profiles & Elo Rating System ----
    {
        const char *test_db_path = "/tmp/cte_test_profiles.dat";
        unlink(test_db_path);

        s_cte_profile_db db;
        err = init_profile_db(&db, test_db_path);
        assert(err == e_ok);
        assert(db.count == 0);

        // find non-existent
        assert(find_profile(&db, "Alice") == NULL);

        // create Alice & Bob
        s_cte_profile *p_alice = find_or_create_profile(&db, "Alice");
        assert(p_alice != NULL);
        assert(db.count == 1);
        assert(p_alice->elo == CTE_DEFAULT_ELO);
        assert(p_alice->matches_played == 0);
        assert(strcmp(p_alice->name, "Alice") == 0);

        s_cte_profile *p_bob = find_or_create_profile(&db, "Bob");
        assert(p_bob != NULL);
        assert(db.count == 2);
        assert(p_bob->elo == CTE_DEFAULT_ELO);

        // case-insensitive lookup
        s_cte_profile *p_alice_lookup = find_or_create_profile(&db, "alice");
        assert(p_alice_lookup == p_alice);
        assert(db.count == 2);

        // Match 1: Alice beats Bob (k_factor = 32)
        // Expected: delta = +16 for Alice, -16 for Bob
        update_match_elo(p_alice, p_bob, 0, 32);
        assert(p_alice->elo == 1216);
        assert(p_bob->elo == 1184);
        assert(p_alice->matches_played == 1);
        assert(p_alice->matches_won == 1);
        assert(p_alice->matches_lost == 0);
        assert(p_bob->matches_played == 1);
        assert(p_bob->matches_won == 0);
        assert(p_bob->matches_lost == 1);

        // Match 2: Tie between Alice (1216) and Bob (1184)
        update_match_elo(p_alice, p_bob, -1, 32);
        assert(p_alice->matches_played == 2);
        assert(p_alice->matches_tied == 1);
        assert(p_bob->matches_played == 2);
        assert(p_bob->matches_tied == 1);
        // Alice had advantage, so tie slightly reduces Alice and boosts Bob
        assert(p_alice->elo < 1216);
        assert(p_bob->elo > 1184);

        // Persistence test
        err = save_profiles(&db);
        assert(err == e_ok);

        s_cte_profile_db db_reload;
        err = init_profile_db(&db_reload, test_db_path);
        assert(err == e_ok);
        assert(db_reload.count == 2);

        s_cte_profile *r_alice = find_profile(&db_reload, "Alice");
        s_cte_profile *r_bob = find_profile(&db_reload, "Bob");
        assert(r_alice != NULL && r_bob != NULL);
        assert(r_alice->elo == p_alice->elo);
        assert(r_bob->elo == p_bob->elo);
        assert(r_alice->matches_played == 2);

        // Sorting test
        sort_profiles_by_elo(&db_reload);
        assert(db_reload.profiles[0].elo >= db_reload.profiles[1].elo);
        assert(strcmp(db_reload.profiles[0].name, "Alice") == 0);

        // Test compute_elo_delta directly
        int16_t d_win = compute_elo_delta(1200, 1200, 1.0, 32);
        assert(d_win == 16);
        int16_t d_loss = compute_elo_delta(1200, 1200, 0.0, 32);
        assert(d_loss == -16);
        int16_t d_draw = compute_elo_delta(1200, 1200, 0.5, 32);
        assert(d_draw == 0);

        unlink(test_db_path);
    }

    // ---- T29 : Tournament <-> Profile Integration Cycle ----
    {
        const char *t29_db_path = "/tmp/cte_test_t29_profiles.dat";
        unlink(t29_db_path);

        s_cte_profile_db db;
        err = init_profile_db(&db, t29_db_path);
        assert(err == e_ok);

        s_cte_profile *p_human = find_or_create_profile(&db, "TestPlayer");
        assert(p_human != NULL);
        assert(p_human->elo == CTE_DEFAULT_ELO);
        assert(save_profiles(&db) == e_ok);

        // Run tournament with profile_db attached (persist_ai = false)
        s_cte_tournament t_prof;
        s_cte_tournament_config cfg_prof = {
            .type = TOURNAMENT_ROUND_ROBIN,
            .nb_participants = 2,
            .winning_score = 15,
            .max_rounds = 1,
            .silent = true,
            .profile_db = &db,
            .persist_ai = false,
            .participants = {
                { .name = "TestPlayer", .evaluator = eval_greedy, .is_human = true,  .ai_type = AI_TYPE_RANDOM },
                { .name = "Bot_Dumb",   .evaluator = eval_dumb,   .is_human = false, .ai_type = AI_TYPE_DUMB },
            },
        };

        err = init_tournament(&t_prof, &cfg_prof);
        assert(err == e_ok);
        assert(t_prof.config.participants[0].elo_start == CTE_DEFAULT_ELO);
        assert(t_prof.config.participants[1].elo_start == 0);

        err = run_tournament(&t_prof);
        assert(err == e_ok);
        assert(t_prof.nb_matches == 1);

        err = sync_tournament_profiles(&t_prof);
        assert(err == e_ok);

        // Reload DB and verify human updated, AI not persisted
        s_cte_profile_db db_check;
        assert(init_profile_db(&db_check, t29_db_path) == e_ok);
        assert(db_check.count == 1);
        s_cte_profile *r_human = find_profile(&db_check, "TestPlayer");
        assert(r_human != NULL);
        assert(r_human->matches_played == 1);
        assert(r_human->elo == t_prof.config.participants[0].elo_current);
        assert(find_profile(&db_check, "Bot_Dumb") == NULL);

        free_tournament(&t_prof);

        // Now run tournament with persist_ai = true
        s_cte_tournament t_ai;
        s_cte_tournament_config cfg_ai = {
            .type = TOURNAMENT_ROUND_ROBIN,
            .nb_participants = 2,
            .winning_score = 15,
            .max_rounds = 1,
            .silent = true,
            .profile_db = &db_check,
            .persist_ai = true,
            .participants = {
                { .name = "AIPersist_Alpha", .evaluator = eval_greedy, .is_human = false, .ai_type = AI_TYPE_GREEDY },
                { .name = "AIPersist_Beta",  .evaluator = eval_random, .is_human = false, .ai_type = AI_TYPE_RANDOM },
            },
        };
        assert(init_tournament(&t_ai, &cfg_ai) == e_ok);
        assert(run_tournament(&t_ai) == e_ok);
        assert(sync_tournament_profiles(&t_ai) == e_ok);

        s_cte_profile_db db_ai_check;
        assert(init_profile_db(&db_ai_check, t29_db_path) == e_ok);
        assert(db_ai_check.count == 3); // TestPlayer + Alpha + Beta
        assert(find_profile(&db_ai_check, "AIPersist_Alpha") != NULL);
        assert(find_profile(&db_ai_check, "AIPersist_Beta") != NULL);

        free_tournament(&t_ai);
        unlink(t29_db_path);
    }

    // ---- T30 : Tablic Accumulator Across Rounds & Elo Symmetry ----
    {
        s_cte_game game_tablic;
        char *tablic_names[2] = { "P1_Tablic", "P2_Tablic" };
        err = init_game(&game_tablic, 2, tablic_names, false);
        assert(err == e_ok);
        game_tablic.players.players[0].evaluator = eval_greedy;
        game_tablic.players.players[1].evaluator = eval_greedy;

        struct s_cte_match match_tablic;
        err = init_match(&match_tablic, &game_tablic, 200);
        assert(err == e_ok);
        match_tablic.max_rounds = 3;

        s_cte_round_config r_cfg_tablic = {
            .first_player  = 0,
            .is_team_mode  = false,
            .evaluators    = { eval_greedy, eval_greedy, NULL, NULL },
            .eval_contexts = { NULL, NULL, NULL, NULL },
            .callbacks     = NULL,
            .ui_context    = NULL,
        };

        err = run_match(&match_tablic, &r_cfg_tablic);
        assert(err == e_ok);
        assert(match_tablic.round_nb == 3);

        // Verify Elo calculation symmetry
        int16_t elo_a = 1500;
        int16_t elo_b = 1300;
        int16_t delta_a_win = compute_elo_delta(elo_a, elo_b, 1.0, 32);
        int16_t delta_b_loss = compute_elo_delta(elo_b, elo_a, 0.0, 32);
        assert(abs(delta_a_win + delta_b_loss) <= 1);

        // Run a tournament match and check delta symmetry between participants
        s_cte_tournament t_sym;
        s_cte_tournament_config cfg_sym = {
            .type = TOURNAMENT_ROUND_ROBIN,
            .nb_participants = 2,
            .winning_score = 15,
            .max_rounds = 1,
            .silent = true,
            .participants = {
                { .name = "Sym_A", .evaluator = eval_greedy, .is_human = false, .ai_type = AI_TYPE_GREEDY },
                { .name = "Sym_B", .evaluator = eval_dumb,   .is_human = false, .ai_type = AI_TYPE_DUMB },
            },
        };
        assert(init_tournament(&t_sym, &cfg_sym) == e_ok);
        t_sym.config.participants[0].elo_start = 1400;
        t_sym.config.participants[0].elo_current = 1400;
        t_sym.config.participants[1].elo_start = 1000;
        t_sym.config.participants[1].elo_current = 1000;

        assert(run_tournament(&t_sym) == e_ok);
        int16_t d_a = t_sym.config.participants[0].elo_current - 1400;
        int16_t d_b = t_sym.config.participants[1].elo_current - 1000;
        assert(abs(d_a + d_b) <= 1);

        free_tournament(&t_sym);
        free_game(&game_tablic);
    }

    // ---- T32 : Validation de l'unicite et non-vacuite des participants de tournoi ----

    {
        s_cte_tournament t_val;
        s_cte_tournament_config cfg_val = {
            .type = TOURNAMENT_ROUND_ROBIN,
            .nb_participants = 2,
            .winning_score = 51,
            .participants = {
                { .name = "", .evaluator = eval_greedy, .is_human = false },
                { .name = "Player2", .evaluator = eval_dumb, .is_human = false },
            },
        };
        // Empty name should fail
        assert(init_tournament(&t_val, &cfg_val) == e_inval_val);

        // Duplicate name should fail (case-insensitive)
        snprintf(cfg_val.participants[0].name, sizeof(cfg_val.participants[0].name), "Alice");
        snprintf(cfg_val.participants[1].name, sizeof(cfg_val.participants[1].name), "alice");
        assert(init_tournament(&t_val, &cfg_val) == e_inval_val);

        // Distinct names succeed
        snprintf(cfg_val.participants[1].name, sizeof(cfg_val.participants[1].name), "Bob");
        assert(init_tournament(&t_val, &cfg_val) == e_ok);
        free_tournament(&t_val);
    }

    // ---- T33 : Cloisonnement strict de la persistance des IA (persist_ai == false) ----
    {
        const char *t33_db_path = "./t33_test_profiles.dat";
        unlink(t33_db_path);

        s_cte_profile_db db_init;
        assert(init_profile_db(&db_init, t33_db_path) == e_ok);
        s_cte_profile *p_ai = find_or_create_profile(&db_init, "PreExisting_Bot");
        assert(p_ai != NULL);
        p_ai->elo = 1500;
        p_ai->matches_played = 10;
        p_ai->total_points = 500;
        assert(save_profiles(&db_init) == e_ok);

        s_cte_profile_db db_tourn;
        assert(init_profile_db(&db_tourn, t33_db_path) == e_ok);

        s_cte_tournament t_iso;
        s_cte_tournament_config cfg_iso = {
            .type = TOURNAMENT_ROUND_ROBIN,
            .nb_participants = 2,
            .winning_score = 15,
            .max_rounds = 1,
            .silent = true,
            .profile_db = &db_tourn,
            .persist_ai = false, // AI persistence explicitly disabled
            .participants = {
                { .name = "PreExisting_Bot", .evaluator = eval_greedy, .is_human = false, .ai_type = AI_TYPE_GREEDY },
                { .name = "Other_Bot",       .evaluator = eval_dumb,   .is_human = false, .ai_type = AI_TYPE_DUMB },
            },
        };

        assert(init_tournament(&t_iso, &cfg_iso) == e_ok);
        assert(run_tournament(&t_iso) == e_ok);
        assert(sync_tournament_profiles(&t_iso) == e_ok);

        // Reload DB: PreExisting_Bot MUST NOT have been mutated
        s_cte_profile_db db_check;
        assert(init_profile_db(&db_check, t33_db_path) == e_ok);
        s_cte_profile *p_check = find_profile(&db_check, "PreExisting_Bot");
        assert(p_check != NULL);
        assert(p_check->elo == 1500); // Intact
        assert(p_check->matches_played == 10); // Intact
        assert(p_check->total_points == 500); // Intact
        assert(find_profile(&db_check, "Other_Bot") == NULL); // Not created

        free_tournament(&t_iso);
        unlink(t33_db_path);
    }

    // ---- T34 (part 2) : Validation de record_match_result_in_profile_path ----
    {
        const char *t34_db_path = "./t34_test_profiles.dat";
        unlink(t34_db_path);

        // Bad argument checks
        struct s_cte_match mock_match;
        memset(&mock_match, 0, sizeof(mock_match));
        mock_match.match_scores[0] = 105;
        mock_match.match_scores[1] = 80;
        mock_match.match_tablics[0] = 2;
        mock_match.match_tablics[1] = 0;

        struct s_cte_player_data pdata[2];
        memset(pdata, 0, sizeof(pdata));
        pdata[0].player_name = "Alice";
        pdata[0].is_human = true;
        pdata[1].player_name = "Bot Greedy";
        pdata[1].is_human = false;

        struct s_cte_players mock_players;
        memset(&mock_players, 0, sizeof(mock_players));
        mock_players.size = 2;
        mock_players.players = pdata;

        e_cte_ai_type ai_types[1] = { AI_TYPE_GREEDY };

        assert(record_match_result_in_profile_path(NULL, &mock_match, &mock_players, ai_types, 1, NULL, NULL, t34_db_path) == e_null);
        assert(record_match_result_in_profile_path("", &mock_match, &mock_players, ai_types, 1, NULL, NULL, t34_db_path) == e_null);
        assert(record_match_result_in_profile_path("Alice", NULL, &mock_players, ai_types, 1, NULL, NULL, t34_db_path) == e_null);
        assert(record_match_result_in_profile_path("Alice", &mock_match, NULL, ai_types, 1, NULL, NULL, t34_db_path) == e_null);

        // Match win against AI Greedy (opp_elo = 1000, Alice starting elo = 1200)
        s_cte_profile out_p;
        int16_t delta = 0;
        assert(record_match_result_in_profile_path("Alice", &mock_match, &mock_players, ai_types, 1, &out_p, &delta, t34_db_path) == e_ok);
        assert(strcmp(out_p.name, "Alice") == 0);
        assert(out_p.matches_played == 1);
        assert(out_p.matches_won == 1);
        assert(out_p.matches_lost == 0);
        assert(out_p.matches_tied == 0);
        assert(out_p.total_points == 105);
        assert(out_p.total_tablics == 2);
        assert(delta > 0);
        assert(out_p.elo == (int16_t)(CTE_DEFAULT_ELO + delta));

        // Persistence check
        s_cte_profile_db db_check;
        assert(init_profile_db(&db_check, t34_db_path) == e_ok);
        s_cte_profile *p_read = find_profile(&db_check, "Alice");
        assert(p_read != NULL);
        assert(p_read->matches_played == 1);
        assert(p_read->elo == out_p.elo);

        unlink(t34_db_path);
    }

    return 0;
}

#ifdef TEST_STANDALONE
int main(void) {
    run_test_tournament();
    printf("[PASS] test_tournament standalone\n");
    return 0;
}
#endif
