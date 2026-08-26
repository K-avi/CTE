#include "cte.h"
#include "backend_bitboard.h"
#include "bitboard_tables.h"
#include <stdio.h>
#include <stdlib.h>

int main(){
    const uint32_t N = 2000000;
    srand(1337);

    uint64_t *tables = malloc(sizeof(uint64_t) * N);
    struct s_cte_hand *hands = malloc(sizeof(struct s_cte_hand) * N);

    for(uint32_t i = 0; i < N; i++){
        uint8_t deck[52];
        for(uint8_t j = 0; j < 52; j++) deck[j] = j;
        for(int j = 51; j > 0; j--){
            int k = rand() % (j + 1);
            uint8_t tmp = deck[j];
            deck[j] = deck[k];
            deck[k] = tmp;
        }
        uint8_t t_sz = rand() % 6;
        tables[i] = 0;
        for(uint8_t j = 0; j < t_sz; j++) tables[i] |= (1ULL << deck[j]);

        uint8_t h_sz = 1 + (rand() % 6);
        hands[i].size = h_sz;
        for(uint8_t j = 0; j < h_sz; j++) hands[i].array[j] = deck[t_sz + j];
    }

    s_cte_bitboard_move_list list;
    uint64_t total_moves = 0;

    for(uint32_t i = 0; i < N; i++){
        bitboard_gen_all_compact_moves_table(&list, tables[i], &hands[i]);
        total_moves += list.size;
    }

    printf("Generated %lu moves over %u positions\n", (unsigned long)total_moves, N);
    free(tables);
    free(hands);
    return 0;
}
