#include "cte.h"
#include "backend_bitboard.h"
#include "bitboard_tables.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(){
    const s_cte_engine_backend *be_arr = cte_get_backend(CTE_BACKEND_ARRAY);
    const s_cte_engine_backend *be_dyn = cte_get_backend(CTE_BACKEND_BITBOARD);
    const s_cte_engine_backend *be_tbl = cte_get_backend(CTE_BACKEND_BITBOARD_TABLE);

    printf("==================================================================================\n");
    printf("      CTE BENCHMARK AVEC TABLES 1D PIVOT OPTIMISÉES (-O3 -march=native)           \n");
    printf("==================================================================================\n\n");

    const uint32_t NUM_SAMPLES = 200000;
    srand(1337);

    struct table *tables = malloc(sizeof(struct table) * NUM_SAMPLES);
    struct s_cte_hand *hands = malloc(sizeof(struct s_cte_hand) * NUM_SAMPLES);

    for(uint32_t i = 0; i < NUM_SAMPLES; i++){
        uint8_t deck[52];
        for(uint8_t j = 0; j < 52; j++) deck[j] = j;
        for(int j = 51; j > 0; j--){
            int k = rand() % (j + 1);
            uint8_t tmp = deck[j];
            deck[j] = deck[k];
            deck[k] = tmp;
        }

        uint8_t t_sz = rand() % 6; 
        tables[i].nb_cards_on_table = t_sz;
        for(uint8_t j = 0; j < t_sz; j++) tables[i].cards_on_table[j] = deck[j];

        uint8_t h_sz = 1 + (rand() % 6);
        hands[i].size = h_sz;
        for(uint8_t j = 0; j < h_sz; j++) hands[i].array[j] = deck[t_sz + j];
    }

    struct s_cte_move_list moves;
    init_move_list(&moves, 32);

    // 1. Array Backend (Heap Malloc)
    clock_t t0 = clock();
    uint64_t moves_arr_cnt = 0;
    for(uint32_t i = 0; i < NUM_SAMPLES; i++){
        moves.size = 0;
        be_arr->gen_all_moves(&moves, &tables[i], &hands[i]);
        moves_arr_cnt += moves.size;
        for(uint16_t m = 0; m < moves.size; m++) free_move(&moves.moves[m]);
    }
    clock_t t1 = clock();
    double time_arr = (double)(t1 - t0) / CLOCKS_PER_SEC;

    // 2. Bitboard Dynamique Carry-Rippler
    clock_t t2 = clock();
    uint64_t moves_dyn_cnt = 0;
    for(uint32_t i = 0; i < NUM_SAMPLES; i++){
        moves.size = 0;
        be_dyn->gen_all_moves(&moves, &tables[i], &hands[i]);
        moves_dyn_cnt += moves.size;
        for(uint16_t m = 0; m < moves.size; m++) free_move(&moves.moves[m]);
    }
    clock_t t3 = clock();
    double time_dyn = (double)(t3 - t2) / CLOCKS_PER_SEC;

    // 3. Bitboard 1D Pivot Tables (Optimisé Reachability + Pivot Lookup)
    clock_t t4 = clock();
    uint64_t moves_tbl_cnt = 0;
    for(uint32_t i = 0; i < NUM_SAMPLES; i++){
        moves.size = 0;
        be_tbl->gen_all_moves(&moves, &tables[i], &hands[i]);
        moves_tbl_cnt += moves.size;
        for(uint16_t m = 0; m < moves.size; m++) free_move(&moves.moves[m]);
    }
    clock_t t5 = clock();
    double time_tbl = (double)(t5 - t4) / CLOCKS_PER_SEC;

    // 4. Bitboard 1-Pass Compact Dynamic (Carry-Rippler, 100% stack)
    uint64_t *table_bbs = malloc(sizeof(uint64_t) * NUM_SAMPLES);
    for(uint32_t i = 0; i < NUM_SAMPLES; i++){
        table_bbs[i] = 0;
        for(uint8_t j = 0; j < tables[i].nb_cards_on_table; j++){
            table_bbs[i] |= (1ULL << tables[i].cards_on_table[j]);
        }
    }

    clock_t t6 = clock();
    uint64_t moves_cpt_dyn = 0;
    s_cte_bitboard_move_list compact_list_dyn;
    for(uint32_t i = 0; i < NUM_SAMPLES; i++){
        bitboard_gen_all_compact_moves(&compact_list_dyn, table_bbs[i], &hands[i]);
        moves_cpt_dyn += compact_list_dyn.size;
    }
    clock_t t7 = clock();
    double time_cpt_dyn = (double)(t7 - t6) / CLOCKS_PER_SEC;

    // 5. Bitboard 1-Pass Compact Pivot Tables (Ultra-Fast, 0 recursion, 100% stack)
    clock_t t8 = clock();
    uint64_t moves_cpt_tbl = 0;
    s_cte_bitboard_move_list compact_list_tbl;
    for(uint32_t i = 0; i < NUM_SAMPLES; i++){
        bitboard_gen_all_compact_moves_table(&compact_list_tbl, table_bbs[i], &hands[i]);
        moves_cpt_tbl += compact_list_tbl.size;
    }
    clock_t t9 = clock();
    double time_cpt_tbl = (double)(t9 - t8) / CLOCKS_PER_SEC;

    // 6. Bitboard 1-Pass Compact SWAR Rank Patterns (7.1 KB RAM, 100% L1 cache)
    clock_t t10 = clock();
    uint64_t moves_cpt_rnk = 0;
    s_cte_bitboard_move_list compact_list_rnk;
    for(uint32_t i = 0; i < NUM_SAMPLES; i++){
        bitboard_gen_all_compact_moves_rank(&compact_list_rnk, table_bbs[i], &hands[i]);
        moves_cpt_rnk += compact_list_rnk.size;
    }
    clock_t t11 = clock();
    double time_cpt_rnk = (double)(t11 - t10) / CLOCKS_PER_SEC;

    free_move_list(&moves);
    free(tables);
    free(hands);
    free(table_bbs);

    double mops_arr = (double)moves_arr_cnt / (time_arr * 1000000.0);
    double mops_dyn = (double)moves_dyn_cnt / (time_dyn * 1000000.0);
    double mops_tbl = (double)moves_tbl_cnt / (time_tbl * 1000000.0);
    double mops_cdy = (double)moves_cpt_dyn / (time_cpt_dyn * 1000000.0);
    double mops_ctb = (double)moves_cpt_tbl / (time_cpt_tbl * 1000000.0);
    double mops_crk = (double)moves_cpt_rnk / (time_cpt_rnk * 1000000.0);

    double ns_pos_arr = (time_arr * 1e9) / NUM_SAMPLES;
    double ns_pos_dyn = (time_dyn * 1e9) / NUM_SAMPLES;
    double ns_pos_tbl = (time_tbl * 1e9) / NUM_SAMPLES;
    double ns_pos_cdy = (time_cpt_dyn * 1e9) / NUM_SAMPLES;
    double ns_pos_ctb = (time_cpt_tbl * 1e9) / NUM_SAMPLES;
    double ns_pos_crk = (time_cpt_rnk * 1e9) / NUM_SAMPLES;

    printf(" Evaluated: 200,000 realistic board states (%lu moves generated per backend)\n\n", (unsigned long)moves_arr_cnt);
    printf(" | Implementation                        | Temps (s) | Débit (Mcoups/s) | Temps / Pos | Cycles @ 2.2 GHz |\n");
    printf(" |---------------------------------------|-----------|------------------|-------------|------------------|\n");
    printf(" | Array (Référence Tableau)             | %7.3f s | %8.2f Mcoups/s | %7.1f ns  | %12.0f cycles |\n", time_arr, mops_arr, ns_pos_arr, ns_pos_arr * 2.2);
    printf(" | Bitboard Dynamique (Carry-Rippler)    | %7.3f s | %8.2f Mcoups/s | %7.1f ns  | %12.0f cycles |\n", time_dyn, mops_dyn, ns_pos_dyn, ns_pos_dyn * 2.2);
    printf(" | Bitboard 1D Pivot Tables (Optimisé)   | %7.3f s | %8.2f Mcoups/s | %7.1f ns  | %12.0f cycles |\n", time_tbl, mops_tbl, ns_pos_tbl, ns_pos_tbl * 2.2);
    printf(" | Bitboard Compact Dynamique (Option A) | %7.3f s | %8.2f Mcoups/s | %7.1f ns  | %12.0f cycles |\n", time_cpt_dyn, mops_cdy, ns_pos_cdy, ns_pos_cdy * 2.2);
    printf(" | Bitboard Compact 1D Pivot Tables 1-P  | %7.3f s | %8.2f Mcoups/s | %7.1f ns  | %12.0f cycles |\n", time_cpt_tbl, mops_ctb, ns_pos_ctb, ns_pos_ctb * 2.2);
    printf(" | Bitboard Compact SWAR Rank Pat. (L1)  | %7.3f s | %8.2f Mcoups/s | %7.1f ns  | %12.0f cycles |\n", time_cpt_rnk, mops_crk, ns_pos_crk, ns_pos_crk * 2.2);
    printf("==================================================================================\n");
    printf(" -> GAIN TABLES PIVOT COMPACTES : %.2fx plus rapide que le backend Array de référence !\n", time_arr / time_cpt_tbl);
    printf(" -> GAIN SWAR RANK PATTERNS L1  : %.2fx plus rapide que le backend Array de référence !\n\n", time_arr / time_cpt_rnk);

    return 0;
}
