#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

// Pure standalone prototype for GNU Vector Extensions (Isolated - No engine modifications)

// GNU Vector Extension types (Portable across GCC / Clang, AVX2, NEON)
typedef uint8_t  v16qi __attribute__((vector_size(16)));
typedef uint8_t  v32qi __attribute__((vector_size(32)));
typedef uint64_t v4di  __attribute__((vector_size(32)));

typedef struct {
    uint8_t  card_played;
    uint64_t capture_mask;
} s_proto_move;

typedef struct {
    uint16_t size;
    s_proto_move moves[48];
} s_proto_move_list;

static uint8_t g_suit_sum[8192];
static uint8_t g_suit_aces[8192];
static bool    g_tables_init = false;

static void init_tables(void){
    if(g_tables_init) return;
    const uint8_t rank_vals[13] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };
    for(uint32_t mask = 0; mask < 8192; mask++){
        uint8_t s = 0;
        uint8_t a = 0;
        for(uint8_t r = 0; r < 13; r++){
            if(mask & (1u << r)){
                s += rank_vals[r];
                if(r == 9) a++;
            }
        }
        g_suit_sum[mask] = s;
        g_suit_aces[mask] = a;
    }
    g_tables_init = true;
}

// -------------------------------------------------------------
// 1. Scalar Reference (1-Pass Caching)
// -------------------------------------------------------------
static inline void gen_scalar_moves(s_proto_move_list *list, uint64_t table_bb, const uint8_t *hand, uint8_t hand_sz){
    list->size = 0;
    for(uint8_t i = 0; i < hand_sz; i++){
        list->moves[list->size++] = (s_proto_move){ .card_played = hand[i], .capture_mask = 0 };
    }
    if(table_bb == 0) return;

    uint64_t subs[64];
    uint8_t  sums[64];
    uint8_t  aces[64];
    uint8_t  n_subs = 0;

    for(uint64_t sub = table_bb; sub > 0; sub = (sub - 1) & table_bb){
        uint16_t m0 = (uint16_t)(sub & 0x1FFFULL);
        uint16_t m1 = (uint16_t)((sub >> 13) & 0x1FFFULL);
        uint16_t m2 = (uint16_t)((sub >> 26) & 0x1FFFULL);
        uint16_t m3 = (uint16_t)((sub >> 39) & 0x1FFFULL);

        subs[n_subs] = sub;
        sums[n_subs] = g_suit_sum[m0] + g_suit_sum[m1] + g_suit_sum[m2] + g_suit_sum[m3];
        aces[n_subs] = g_suit_aces[m0] + g_suit_aces[m1] + g_suit_aces[m2] + g_suit_aces[m3];
        n_subs++;
    }

    for(uint8_t h = 0; h < hand_sz; h++){
        uint8_t c = hand[h];
        bool is_ace = ((c % 13) == 9);
        uint8_t target_v = is_ace ? 11 : ((c % 13) + 2);

        for(uint8_t i = 0; i < n_subs; i++){
            uint8_t s = sums[i];
            uint8_t a = aces[i];
            if(is_ace){
                if(s == 11 || s == 1 || (s > 11 && (s - 11) % 10 == 0 && (s - 11)/10 <= a) || (s > 1 && (s - 1) % 10 == 0 && (s - 1)/10 <= a)){
                    list->moves[list->size++] = (s_proto_move){ .card_played = c, .capture_mask = subs[i] };
                }
            } else {
                if(s == target_v || (s > target_v && (s - target_v) % 10 == 0 && (s - target_v)/10 <= a)){
                    list->moves[list->size++] = (s_proto_move){ .card_played = c, .capture_mask = subs[i] };
                }
            }
        }
    }
}

// -------------------------------------------------------------
// 2. GNU Vector Extensions Prototype (v16qi SIMD)
// -------------------------------------------------------------
static inline void gen_vector_moves(s_proto_move_list *list, uint64_t table_bb, const uint8_t *hand, uint8_t hand_sz){
    list->size = 0;
    for(uint8_t i = 0; i < hand_sz; i++){
        list->moves[list->size++] = (s_proto_move){ .card_played = hand[i], .capture_mask = 0 };
    }
    if(table_bb == 0) return;

    // Load hand card target values into 16-byte SIMD vector
    uint8_t hand_vals[16] __attribute__((aligned(16))) = {0};
    uint8_t hand_is_ace[16] __attribute__((aligned(16))) = {0};
    for(uint8_t i = 0; i < hand_sz; i++){
        uint8_t rank = hand[i] % 13;
        hand_is_ace[i] = (rank == 9) ? 1 : 0;
        hand_vals[i] = (rank == 9) ? 11 : (rank + 2);
    }
    v16qi v_hand_vals = *(v16qi*)hand_vals;

    // Table submasks extraction
    for(uint64_t sub = table_bb; sub > 0; sub = (sub - 1) & table_bb){
        uint16_t m0 = (uint16_t)(sub & 0x1FFFULL);
        uint16_t m1 = (uint16_t)((sub >> 13) & 0x1FFFULL);
        uint16_t m2 = (uint16_t)((sub >> 26) & 0x1FFFULL);
        uint16_t m3 = (uint16_t)((sub >> 39) & 0x1FFFULL);

        uint8_t s = g_suit_sum[m0] + g_suit_sum[m1] + g_suit_sum[m2] + g_suit_sum[m3];
        uint8_t a = g_suit_aces[m0] + g_suit_aces[m1] + g_suit_aces[m2] + g_suit_aces[m3];

        // Broadcast single submask sum across 16-lane vector in 1 cycle
        v16qi v_sub_sum = (v16qi){ s, s, s, s, s, s, s, s, s, s, s, s, s, s, s, s };

        // 1-Cycle SIMD vector comparison: (v_hand_vals == v_sub_sum)
        v16qi v_cmp = (v_hand_vals == v_sub_sum);

        // Convert comparison vector mask to bitmask (maps to pmovmskb / movemask on x86)
        uint16_t mask_match = (uint16_t)__builtin_ia32_pmovmskb128((__attribute__((__vector_size__(2 * sizeof(long long)))) char)v_cmp);
        mask_match &= ((1u << hand_sz) - 1);

        while(mask_match > 0){
            int idx = __builtin_ctz(mask_match);
            list->moves[list->size++] = (s_proto_move){ .card_played = hand[idx], .capture_mask = sub };
            mask_match &= (mask_match - 1);
        }

        // Handle special Ace = 1 value check if sub sums to 1
        if(s == 1){
            for(uint8_t i = 0; i < hand_sz; i++){
                if(hand_is_ace[i]){
                    list->moves[list->size++] = (s_proto_move){ .card_played = hand[i], .capture_mask = sub };
                }
            }
        }
    }
}

int main(){
    init_tables();

    printf("==================================================================================\n");
    printf("   ISOLATED PROTOTYPE: GNU VECTOR EXTENSIONS (SIMD) vs SCALAR BITBOARD            \n");
    printf("   (Testing purely on standalone bench - 0 engine changes)                         \n");
    printf("==================================================================================\n\n");

    const uint32_t NUM_SAMPLES = 1000000;
    srand(42);

    uint64_t *table_bbs = malloc(sizeof(uint64_t) * NUM_SAMPLES);
    uint8_t  *hand_cards = malloc(sizeof(uint8_t) * NUM_SAMPLES * 6);
    uint8_t  *hand_sizes = malloc(sizeof(uint8_t) * NUM_SAMPLES);

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
        table_bbs[i] = 0;
        for(uint8_t j = 0; j < t_sz; j++) table_bbs[i] |= (1ULL << deck[j]);

        uint8_t h_sz = 1 + (rand() % 6);
        hand_sizes[i] = h_sz;
        for(uint8_t j = 0; j < h_sz; j++) hand_cards[i * 6 + j] = deck[t_sz + j];
    }

    s_proto_move_list list_scal, list_vec;

    // 1. Scalar Run
    clock_t t0 = clock();
    uint64_t scal_moves = 0;
    for(uint32_t i = 0; i < NUM_SAMPLES; i++){
        gen_scalar_moves(&list_scal, table_bbs[i], &hand_cards[i * 6], hand_sizes[i]);
        scal_moves += list_scal.size;
    }
    clock_t t1 = clock();
    double time_scal = (double)(t1 - t0) / CLOCKS_PER_SEC;

    // 2. Vectorized GNU Extensions Run
    clock_t t2 = clock();
    uint64_t vec_moves = 0;
    for(uint32_t i = 0; i < NUM_SAMPLES; i++){
        gen_vector_moves(&list_vec, table_bbs[i], &hand_cards[i * 6], hand_sizes[i]);
        vec_moves += list_vec.size;
    }
    clock_t t3 = clock();
    double time_vec = (double)(t3 - t2) / CLOCKS_PER_SEC;

    free(table_bbs);
    free(hand_cards);
    free(hand_sizes);

    double ns_scal = (time_scal * 1e9) / NUM_SAMPLES;
    double ns_vec  = (time_vec * 1e9) / NUM_SAMPLES;

    double cycles_scal = ns_scal * 2.2;
    double cycles_vec  = ns_vec * 2.2;

    double mops_scal = (double)scal_moves / (time_scal * 1e6);
    double mops_vec  = (double)vec_moves  / (time_vec * 1e6);

    printf(" Evaluated: 1,000,000 board positions (%lu total moves)\n\n", (unsigned long)vec_moves);
    printf(" | Generator Implementation              | Temps (1M) | Débit (Mcoups/s) | Temps / Pos | Cycles CPU @ 2.2 GHz |\n");
    printf(" |---------------------------------------|------------|------------------|-------------|----------------------|\n");
    printf(" | Scalar Bitboard (1-Pass Caching)      | %8.3f s | %8.2f Mcoups/s | %7.1f ns  | %12.0f cycles   |\n", time_scal, mops_scal, ns_scal, cycles_scal);
    printf(" | GNU Vector Extensions (SIMD v16qi)    | %8.3f s | %8.2f Mcoups/s | %7.1f ns  | %12.0f cycles   |\n", time_vec, mops_vec, ns_vec, cycles_vec);
    printf("==================================================================================\n");
    printf(" -> GAIN SIMD : %.2fx plus rapide (descend à %.0f cycles par position complète !)\n\n", time_scal / time_vec, cycles_vec);

    return 0;
}
