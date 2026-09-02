#include "cte.h"
#include "minmax.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(){
    const uint32_t NUM_SAMPLES = 5000;
    srand(42);

    s_cte_pos *positions = malloc(sizeof(s_cte_pos) * NUM_SAMPLES);
    struct s_cte_move_list *move_lists = malloc(sizeof(struct s_cte_move_list) * NUM_SAMPLES);

    for(uint32_t i = 0; i < NUM_SAMPLES; i++){
        uint8_t deck[52];
        for(uint8_t j = 0; j < 52; j++) deck[j] = j;
        for(int j = 51; j > 0; j--){
            int k = rand() % (j + 1);
            uint8_t tmp = deck[j];
            deck[j] = deck[k];
            deck[k] = tmp;
        }

        positions[i].nb_players = 2;
        positions[i].current_player = 0;
        positions[i].table_bb = 0;
        uint8_t t_sz = 2 + (rand() % 4);
        for(uint8_t j = 0; j < t_sz; j++) positions[i].table_bb |= (1ULL << deck[j]);

        positions[i].hand_counts[0] = 4;
        positions[i].hand_bb[0] = 0;
        for(uint8_t j = 0; j < 4; j++) positions[i].hand_bb[0] |= (1ULL << deck[t_sz + j]);

        positions[i].hand_counts[1] = 4;
        positions[i].hand_bb[1] = 0;
        for(uint8_t j = 0; j < 4; j++) positions[i].hand_bb[1] |= (1ULL << deck[t_sz + 4 + j]);

        init_move_list(&move_lists[i], 16);
        pos_gen_moves(&move_lists[i], &positions[i]);
    }

    s_cte_search_config cfg = { .max_depth = 4, .timeout_ms = 0 };

    clock_t t0 = clock();
    uint64_t total_evals = 0;
    for(uint32_t i = 0; i < NUM_SAMPLES; i++){
        total_evals += search_best_move(&positions[i], &move_lists[i], &cfg);
    }
    clock_t t1 = clock();

    double elapsed = (double)(t1 - t0) / CLOCKS_PER_SEC;
    printf("Minimax Depth 4: %u positions in %.3f s -> %.1f us/pos (%.0f pos/s)\n",
           NUM_SAMPLES, elapsed, (elapsed * 1e6) / NUM_SAMPLES, (double)NUM_SAMPLES / elapsed);

    for(uint32_t i = 0; i < NUM_SAMPLES; i++){
        free_move_list(&move_lists[i]);
    }
    free(positions);
    free(move_lists);
    return 0;
}
