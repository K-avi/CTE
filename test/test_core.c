#include "test_common.h"

struct s_rot_test_ctx {
    uint8_t turns;
    uint8_t first_players[2];
    bool round_started;
};

static void on_round_rot_test(uint8_t round_nb, void *ui_ctx) {
    (void)round_nb;
    struct s_rot_test_ctx *ctx = (struct s_rot_test_ctx*)ui_ctx;
    ctx->round_started = true;
}

static void on_turn_rot_test(const s_cte_game_state *st, const struct s_cte_move_list *mv, void *ui_ctx) {
    (void)mv;
    struct s_rot_test_ctx *ctx = (struct s_rot_test_ctx*)ui_ctx;
    if (ctx->round_started) {
        ctx->round_started = false;
        if (ctx->turns < 2) {
            ctx->first_players[ctx->turns++] = st->current_player_id;
        }
    }
}

int run_test_core(void) {
    unsigned int mseed = test_get_seed();
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

    e7 = init_players(&p_err, 0, (char*[]){NULL});
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
    srand(mseed ^ 0x42);
    struct deck local_deck;
    init_deck(&local_deck);
    shuffle_deck(&local_deck);
    bool shuffled = false;
    for(int i = 0; i < 52; i++){
        if(local_deck.cards[i] != i){
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

    // ---- Tests match ----
    reset_all_players(players);
    struct s_cte_match match;
    err = init_match(&match, &game, 101);
    assert(err == e_ok);
    assert(match.winning_score == 101);

    s_cte_round_config config = {
        .first_player    = 0,
        .evaluators      = { eval_random, eval_random, NULL, NULL },
        .eval_contexts   = { NULL, NULL, NULL, NULL },
    };

    srand(mseed ^ 0x99);
    err = run_match(&match, &config);
    assert(err == e_ok);
    assert(match_is_over(&match));
    assert(match_winner(&match) >= 0);

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

    srand(mseed ^ 0x777);
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

    srand(mseed ^ 0x888);
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
    srand(mseed ^ 0x999);
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

    // ---- T23 : award_remaining_table_cards ----
    reset_all_players(&game.players);
    game.table_bb = (1ULL << 0) | (1ULL << 1) | (1ULL << 2) | (1ULL << 3);
    game.last_captor_id = 0;
    err = award_remaining_table_cards(&game);
    assert(err == e_ok);
    assert(game.table_bb == 0);
    assert(game.players.players[0].won_cards.size == 4);

    game.last_captor_id = 99; // Invalide
    err = award_remaining_table_cards(&game);
    assert(err == e_inval_val);

    // ---- T31 : Alternance du donneur / premier joueur entre les manches ----
    {
        s_cte_game game_rot;
        char *rot_names[2] = { "Rot_P1", "Rot_P2" };
        err = init_game(&game_rot, 2, rot_names, false);
        assert(err == e_ok);
        game_rot.players.players[0].evaluator = eval_random;
        game_rot.players.players[1].evaluator = eval_random;

        struct s_cte_match match_rot;
        err = init_match(&match_rot, &game_rot, 500);
        assert(err == e_ok);
        match_rot.max_rounds = 2;

        struct s_rot_test_ctx rot_ctx = {0};

        s_cte_ui_callbacks rot_cbs = {
            .on_round_start = on_round_rot_test,
            .on_turn_start  = on_turn_rot_test,
        };

        s_cte_round_config r_cfg_rot = {
            .first_player  = 0,
            .is_team_mode  = false,
            .evaluators    = { eval_random, eval_random, NULL, NULL },
            .eval_contexts = { NULL, NULL, NULL, NULL },
            .callbacks     = &rot_cbs,
            .ui_context    = &rot_ctx,
        };

        srand(mseed ^ 0x4242);
        assert(run_match(&match_rot, &r_cfg_rot) == e_ok);
        assert(match_rot.round_nb == 2);
        assert(rot_ctx.turns == 2);
        assert(rot_ctx.first_players[0] == 0); // Round 1 starts with P0
        assert(rot_ctx.first_players[1] == 1); // Round 2 starts with P1 (alternated!)

        free_game(&game_rot);
    }

    free_game(&game);
    return 0;
}

#ifdef TEST_STANDALONE
int main(void) {
    test_get_seed();
    run_test_core();
    printf("[PASS] test_core standalone\n");
    return 0;
}
#endif
