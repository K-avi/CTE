#include "cte.h"
#include "backend_bitboard.h"
#include "bitboard_tables.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>

static uint64_t move_to_bitmask(const struct s_cte_move *m){
    uint64_t mask = 0;
    for(uint8_t i = 0; i < m->cards_picked.size; i++){
        mask |= (1ULL << m->cards_picked.array[i]);
    }
    return mask;
}

static bool are_move_lists_identical(const struct s_cte_move_list *l1, const struct s_cte_move_list *l2){
    if(!l1 || !l2) return false;
    if(l1->size != l2->size) return false;

    for(uint16_t i = 0; i < l1->size; i++){
        const struct s_cte_move *m1 = &l1->moves[i];
        uint64_t mask1 = move_to_bitmask(m1);
        bool found = false;

        for(uint16_t j = 0; j < l2->size; j++){
            const struct s_cte_move *m2 = &l2->moves[j];
            if(m1->card_played == m2->card_played && m1->cards_picked.size == m2->cards_picked.size){
                uint64_t mask2 = move_to_bitmask(m2);
                if(mask1 == mask2){
                    found = true;
                    break;
                }
            }
        }
        if(!found) return false;
    }
    return true;
}

int main(){
    printf("=======================================================\n");
    printf("     CTE - 3-WAY BITBOARD DIFFERENTIAL TEST SUITE      \n");
    printf("=======================================================\n\n");

    const s_cte_engine_backend *be_arr   = cte_get_backend(CTE_BACKEND_ARRAY);
    const s_cte_engine_backend *be_bb    = cte_get_backend(CTE_BACKEND_BITBOARD);
    const s_cte_engine_backend *be_tbl   = cte_get_backend(CTE_BACKEND_BITBOARD_TABLE);
    const s_cte_engine_backend *be_rnk   = cte_get_backend(CTE_BACKEND_BITBOARD_RANK);

    assert(be_arr != NULL);
    assert(be_bb != NULL);
    assert(be_tbl != NULL);
    assert(be_rnk != NULL);

    // -------------------------------------------------------------
    // BT1: Validation Mathématique Interne de la Table Pivot 1D
    // -------------------------------------------------------------
    printf("[BT1] Validating mathematical consistency of %u 1D pivot precomputed masks...\n", CTE_TOTAL_BASE_MASKS);
    uint32_t verified_masks = 0;

    for(uint8_t target_val = 1; target_val <= 14; target_val++){
        uint64_t expected_reach = 0;

        for(uint8_t c = 0; c < 52; c++){
            uint16_t idx = CTE_PIVOT_INDEX(c, target_val);
            uint16_t offset = g_pivot_offsets[idx];
            uint16_t count  = g_pivot_counts[idx];

            for(uint16_t i = 0; i < count; i++){
                uint64_t mask = g_pivot_subset_masks[offset + i];
                assert(mask > 0);

                // Lowest set bit MUST be exactly card c (Pivot property)
                assert(__builtin_ctzll(mask) == c);

                expected_reach |= mask;

                uint16_t sum_11 = 0;
                uint8_t aces = 0;
                for(uint8_t k = 0; k < 52; k++){
                    if(mask & (1ULL << k)){
                        sum_11 += get_value(k);
                        if(is_ace(k)) aces++;
                    }
                }

                assert(sum_11 >= target_val);
                uint16_t diff = sum_11 - target_val;
                assert(diff % 10 == 0);
                assert((diff / 10) <= aces);

                verified_masks++;
            }
        }
        // Reachability mask must match exact bitwise OR
        assert(g_reachability_mask[target_val] == expected_reach);
    }
    assert(verified_masks == CTE_TOTAL_BASE_MASKS);
    printf("      -> PASS: 100%% of 1D pivot masks and reachability bitmasks verified.\n\n");

    // -------------------------------------------------------------
    // BT2: Prises Triples & As à double valeur (3 Backends)
    // -------------------------------------------------------------
    printf("[BT2] Validating critical tactical scenarios across all 3 backends...\n");
    uint64_t table_bb = (1ULL << 11) | (1ULL << 9) | (1ULL << 6) | (1ULL << 4) | (1ULL << 7) | (1ULL << 3);

    struct s_cte_move_list moves_arr, moves_bb, moves_tbl;
    init_move_list(&moves_arr, 16);
    init_move_list(&moves_bb, 16);
    init_move_list(&moves_tbl, 16);

    be_arr->gen_card_moves(&moves_arr, table_bb, 12);
    be_bb->gen_card_moves(&moves_bb, table_bb, 12);
    be_tbl->gen_card_moves(&moves_tbl, table_bb, 12);

    assert(are_move_lists_identical(&moves_arr, &moves_bb));
    assert(are_move_lists_identical(&moves_arr, &moves_tbl));

    free_move_list(&moves_arr);
    free_move_list(&moves_bb);
    free_move_list(&moves_tbl);
    printf("      -> PASS: Triple capture moves are 100%% identical across all 3 engines.\n\n");

    // -------------------------------------------------------------
    // BT3 & BT4: Differential Fuzzing 3-Voies (10 000 positions)
    // -------------------------------------------------------------
    printf("[BT3 & BT4] Running 3-Way Differential Fuzzing (10,000 positions)...\n");
    srand(12345);

    uint32_t total_tested_moves = 0;
    for(uint32_t iter = 0; iter < 10000; iter++){
        uint8_t deck_shuff[52];
        for(uint8_t i = 0; i < 52; i++) deck_shuff[i] = i;
        for(int i = 51; i > 0; i--){
            int j = rand() % (i + 1);
            uint8_t tmp = deck_shuff[i];
            deck_shuff[i] = deck_shuff[j];
            deck_shuff[j] = tmp;
        }

        uint8_t table_sz = (uint8_t)(rand() % 11);
        uint8_t hand_sz  = (uint8_t)(1 + (rand() % 6));

        uint64_t table_bb_rand = 0;
        for(uint8_t i = 0; i < table_sz; i++){
            table_bb_rand |= (1ULL << deck_shuff[i]);
        }

        struct s_cte_hand rand_hand;
        rand_hand.size = hand_sz;
        for(uint8_t i = 0; i < hand_sz; i++){
            rand_hand.array[i] = deck_shuff[table_sz + i];
        }

        init_move_list(&moves_arr, 16);
        init_move_list(&moves_bb, 16);
        init_move_list(&moves_tbl, 16);

        be_arr->gen_all_moves(&moves_arr, table_bb_rand, &rand_hand);
        be_bb->gen_all_moves(&moves_bb, table_bb_rand, &rand_hand);
        be_tbl->gen_all_moves(&moves_tbl, table_bb_rand, &rand_hand);

        // 1. Zéro faux positif
        for(uint16_t m = 0; m < moves_bb.size; m++){
            bool is_leg = false;
            t_cteerr err_leg = is_legal(&is_leg, table_bb_rand, &moves_bb.moves[m]);
            assert(err_leg == e_ok && is_leg == true);
        }
        for(uint16_t m = 0; m < moves_tbl.size; m++){
            bool is_leg = false;
            t_cteerr err_leg = is_legal(&is_leg, table_bb_rand, &moves_tbl.moves[m]);
            assert(err_leg == e_ok && is_leg == true);
        }

        // 2. Isomorphisme parfait
        assert(moves_arr.size == moves_bb.size);
        assert(moves_arr.size == moves_tbl.size);
        assert(are_move_lists_identical(&moves_arr, &moves_bb));
        assert(are_move_lists_identical(&moves_arr, &moves_tbl));

        total_tested_moves += moves_bb.size;

        free_move_list(&moves_arr);
        free_move_list(&moves_bb);
        free_move_list(&moves_tbl);

        if((iter + 1) % 2500 == 0){
            printf("      -> Progress: %5u / 10,000 configurations verified (%u moves validated)\n",
                   iter + 1, total_tested_moves);
        }
    }
    printf("      -> PASS: 10,000 / 10,000 configurations 100%% identical (0 illegal, 0 missed).\n\n");

    // -------------------------------------------------------------
    // BT5: Simulation 500 Manches sous Bitboard Tables
    // -------------------------------------------------------------
    printf("[BT5] Running 500 complete multi-player rounds via 1D Pivot Bitboard Tables...\n");
    s_cte_game game_fuzz;
    char *names_fuzz[4] = { "Bot_A", "Bot_B", "Bot_C", "Bot_D" };
    be_tbl->init_game(&game_fuzz, 4, names_fuzz, true);

    s_cte_round_config r_cfg = {
        .first_player  = 0,
        .is_team_mode  = true,
        .evaluators    = { eval_greedy, eval_cheater, eval_greedy, eval_dumb },
        .eval_contexts = { NULL, NULL, NULL, NULL },
    };

    for(unsigned int s = 0; s < 500; s++){
        reset_all_players(&game_fuzz.players);
        srand(s * 7 + 11);

        t_cteerr err = be_tbl->run_round(&game_fuzz, &r_cfg);
        assert(err == e_ok);
        assert(game_fuzz.deck.cur_card == 52);
        assert(game_fuzz.table_bb == 0);

        uint8_t total_c = 0;
        uint8_t total_pts = 0;
        for(uint8_t p = 0; p < 4; p++){
            total_c += game_fuzz.players.players[p].won_cards.size;
            for(uint8_t c = 0; c < game_fuzz.players.players[p].won_cards.size; c++){
                total_pts += get_points(game_fuzz.players.players[p].won_cards.array[c]);
            }
        }
        assert(total_c == 52);
        assert(total_pts == 22);
    }
    be_tbl->free_game(&game_fuzz);
    // -------------------------------------------------------------
    // BT6: Validation Différentielle du Générateur Compact 1-Passe (10 000 positions)
    // -------------------------------------------------------------
    printf("[BT6] Validating 1-Pass Compact Bitboard Generator vs Array Oracle (10,000 positions)...\n");
    srand(98765);

    for(uint32_t iter = 0; iter < 10000; iter++){
        uint8_t deck_shuff[52];
        for(uint8_t i = 0; i < 52; i++) deck_shuff[i] = i;
        for(int i = 51; i > 0; i--){
            int j = rand() % (i + 1);
            uint8_t tmp = deck_shuff[i];
            deck_shuff[i] = deck_shuff[j];
            deck_shuff[j] = tmp;
        }

        uint8_t table_sz = (uint8_t)(rand() % 11);
        uint8_t hand_sz  = (uint8_t)(1 + (rand() % 6));

        uint64_t table_bb = 0;
        for(uint8_t i = 0; i < table_sz; i++){
            table_bb |= (1ULL << deck_shuff[i]);
        }

        struct s_cte_hand rand_hand;
        rand_hand.size = hand_sz;
        for(uint8_t i = 0; i < hand_sz; i++){
            rand_hand.array[i] = deck_shuff[table_sz + i];
        }

        init_move_list(&moves_arr, 16);
        be_arr->gen_all_moves(&moves_arr, table_bb, &rand_hand);

        s_cte_bitboard_move_list compact_list;
        bitboard_gen_all_compact_moves(&compact_list, table_bb, &rand_hand);

        assert(moves_arr.size == compact_list.size);

        for(uint16_t i = 0; i < moves_arr.size; i++){
            const struct s_cte_move *m_arr = &moves_arr.moves[i];
            uint64_t mask_arr = move_to_bitmask(m_arr);
            bool found = false;

            for(uint16_t j = 0; j < compact_list.size; j++){
                const s_cte_bitboard_move *m_c = &compact_list.moves[j];
                if(m_arr->card_played == m_c->card_played && mask_arr == m_c->capture_mask){
                    found = true;
                    break;
                }
            }
            assert(found);
        }

        free_move_list(&moves_arr);
    }
    // -------------------------------------------------------------
    // BT7: Validation Différentielle du Générateur 1-Passe Tables Pivot (10 000 positions)
    // -------------------------------------------------------------
    printf("[BT7] Validating 1-Pass 1D Pivot Tables Compact Generator vs Array Oracle (10,000 positions)...\n");
    srand(54321);

    for(uint32_t iter = 0; iter < 10000; iter++){
        uint8_t deck_shuff[52];
        for(uint8_t i = 0; i < 52; i++) deck_shuff[i] = i;
        for(int i = 51; i > 0; i--){
            int j = rand() % (i + 1);
            uint8_t tmp = deck_shuff[i];
            deck_shuff[i] = deck_shuff[j];
            deck_shuff[j] = tmp;
        }

        uint8_t table_sz = (uint8_t)(rand() % 11);
        uint8_t hand_sz  = (uint8_t)(1 + (rand() % 6));

        uint64_t table_bb = 0;
        for(uint8_t i = 0; i < table_sz; i++){
            table_bb |= (1ULL << deck_shuff[i]);
        }

        struct s_cte_hand rand_hand;
        rand_hand.size = hand_sz;
        for(uint8_t i = 0; i < hand_sz; i++){
            rand_hand.array[i] = deck_shuff[table_sz + i];
        }

        init_move_list(&moves_arr, 16);
        be_arr->gen_all_moves(&moves_arr, table_bb, &rand_hand);

        s_cte_bitboard_move_list compact_tbl;
        bitboard_gen_all_compact_moves_table(&compact_tbl, table_bb, &rand_hand);

        assert(moves_arr.size == compact_tbl.size);

        for(uint16_t i = 0; i < moves_arr.size; i++){
            const struct s_cte_move *m_arr = &moves_arr.moves[i];
            uint64_t mask_arr = move_to_bitmask(m_arr);
            bool found = false;

            for(uint16_t j = 0; j < compact_tbl.size; j++){
                const s_cte_bitboard_move *m_c = &compact_tbl.moves[j];
                if(m_arr->card_played == m_c->card_played && mask_arr == m_c->capture_mask){
                    found = true;
                    break;
                }
            }
            assert(found);
        }

        free_move_list(&moves_arr);
    }
    printf("      -> PASS: 10,000 / 10,000 configurations 100%% identical with 1-Pass Pivot Tables.\n\n");

    // -------------------------------------------------------------
    // BT8: Validation Différentielle du Générateur Motifs de Rangs SWAR (10 000 positions)
    // -------------------------------------------------------------
    printf("[BT8] Validating 1-Pass SWAR Rank Patterns Compact Generator (7.1 KB) vs Array Oracle (10,000 positions)...\n");
    srand(98765);

    for(uint32_t iter = 0; iter < 10000; iter++){
        uint8_t deck_shuff[52];
        for(uint8_t i = 0; i < 52; i++) deck_shuff[i] = i;
        for(int i = 51; i > 0; i--){
            int j = rand() % (i + 1);
            uint8_t tmp = deck_shuff[i];
            deck_shuff[i] = deck_shuff[j];
            deck_shuff[j] = tmp;
        }

        uint8_t table_sz = (uint8_t)(rand() % 11);
        uint8_t hand_sz  = (uint8_t)(1 + (rand() % 6));

        uint64_t table_bb = 0;
        for(uint8_t i = 0; i < table_sz; i++){
            table_bb |= (1ULL << deck_shuff[i]);
        }

        struct s_cte_hand rand_hand;
        rand_hand.size = hand_sz;
        for(uint8_t i = 0; i < hand_sz; i++){
            rand_hand.array[i] = deck_shuff[table_sz + i];
        }

        init_move_list(&moves_arr, 16);
        be_arr->gen_all_moves(&moves_arr, table_bb, &rand_hand);

        s_cte_bitboard_move_list compact_rnk;
        bitboard_gen_all_compact_moves_rank(&compact_rnk, table_bb, &rand_hand);

        assert(moves_arr.size == compact_rnk.size);

        for(uint16_t i = 0; i < moves_arr.size; i++){
            const struct s_cte_move *m_arr = &moves_arr.moves[i];
            uint64_t mask_arr = move_to_bitmask(m_arr);
            bool found = false;

            for(uint16_t j = 0; j < compact_rnk.size; j++){
                const s_cte_bitboard_move *m_c = &compact_rnk.moves[j];
                if(m_arr->card_played == m_c->card_played && mask_arr == m_c->capture_mask){
                    found = true;
                    break;
                }
            }
            assert(found);
        }

        free_move_list(&moves_arr);
    }
    printf("      -> PASS: 10,000 / 10,000 configurations 100%% identical with 1-Pass SWAR Rank Patterns.\n\n");

    // -------------------------------------------------------------
    // BT9: Simulation 500 Manches sous SWAR Rank Patterns Backend
    // -------------------------------------------------------------
    printf("[BT9] Running 500 complete multi-player rounds via SWAR Rank Patterns Backend...\n");
    s_cte_game game_rnk;
    char *names_rnk[4] = { "Rank_A", "Rank_B", "Rank_C", "Rank_D" };
    be_rnk->init_game(&game_rnk, 4, names_rnk, true);

    for(unsigned int s = 0; s < 500; s++){
        reset_all_players(&game_rnk.players);
        srand(s * 13 + 37);

        t_cteerr err = be_rnk->run_round(&game_rnk, &r_cfg);
        assert(err == e_ok);
        assert(game_rnk.deck.cur_card == 52);
        assert(game_rnk.table_bb == 0);

        uint8_t total_c = 0;
        uint8_t total_pts = 0;
        for(uint8_t p = 0; p < 4; p++){
            total_c += game_rnk.players.players[p].won_cards.size;
            for(uint8_t c = 0; c < game_rnk.players.players[p].won_cards.size; c++){
                total_pts += get_points(game_rnk.players.players[p].won_cards.array[c]);
            }
        }
        assert(total_c == 52);
        assert(total_pts == 22);
    }
    be_rnk->free_game(&game_rnk);
    printf("      -> PASS: 500 / 500 rounds completed with strict card and points conservation.\n\n");

    printf("=======================================================\n");
    printf("     ALL 4-WAY BITBOARD TESTS PASSED WITH 0 ERRORS !   \n");
    printf("=======================================================\n");
    return 0;
}

