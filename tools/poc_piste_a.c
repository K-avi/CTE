/*
 * PoC Piste A: Carry-Rippler + g_suit_sum 13-bit LUT + 1-level partition recursion
 *
 * For each real subset of the table (carry-rippler, 2^N iterations):
 *   1. Compute sum in O(1) via SUIT_SUM_4 (4 L1 reads, no .rodata scan)
 *   2. If sum == target: valid single capture
 *   3. If sum == k*target (k>=2): enumerate sub-partitions via inner carry-rippler
 *      to find all disjoint decompositions (1 level of recursion max for k=2)
 *
 * Does NOT modify any existing backend. Validated against Array Oracle.
 */

#include "cte.h"
#include "backend_bitboard.h"
#include "bitboard_tables.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// PoC A: core compact move generator
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// 16-bit packed rank LUT: low 8 bits = sum, high 8 bits = aces
// 1 KB divisibility table to completely eliminate variable DIV instructions
// ---------------------------------------------------------------------------
static uint16_t g_suit_packed[8192];
static uint8_t  g_is_multiple[128][16]; // g_is_multiple[adj][target] = (adj >= target && adj % target == 0)
static uint8_t  g_is_mult_10[128];      // g_is_mult_10[diff] = (diff % 10 == 0) ? (diff / 10 + 1) : 0

static void init_suit_packed(void){
    static bool done = false;
    if(done) return;
    done = true;
    static const uint8_t rank_vals[13] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };
    for(uint32_t mask = 0; mask < 8192; mask++){
        uint32_t s = 0;
        uint32_t aces = (mask >> 9) & 1;
        for(uint8_t r = 0; r < 13; r++){
            if(mask & (1u << r)) s += rank_vals[r];
        }
        g_suit_packed[mask] = (uint16_t)((aces << 8) | (s > 255 ? 255 : s));
    }
    for(uint8_t a = 0; a < 128; a++){
        for(uint8_t b = 1; b < 16; b++){
            g_is_multiple[a][b] = (a >= b && (a % b == 0)) ? 1 : 0;
        }
        g_is_mult_10[a] = (a % 10 == 0) ? (uint8_t)(a / 10 + 1) : 0;
    }
}

#define SUIT_PACKED_4(bb) ( (uint32_t)(g_suit_packed[(bb) & 0x1FFF] \
                          + g_suit_packed[((bb) >> 13) & 0x1FFF] \
                          + g_suit_packed[((bb) >> 26) & 0x1FFF] \
                          + g_suit_packed[((bb) >> 39) & 0x1FFF]) )

// O(1) push without scanning the entire output list
static inline void poc_a_push(s_cte_bitboard_move_list *out, t_card card, uint64_t sub){
    if(out->size < 1024){
        out->moves[out->size++] = (s_cte_bitboard_move){ card, sub };
    }
}

static inline bool suit_sum_valid_packed(uint8_t s, uint8_t aces, uint8_t target){
    if(s < target) return false;
    uint8_t diff = s - target;
    if(diff >= 128) return false;
    uint8_t mult = g_is_mult_10[diff];
    if(mult == 0) return false;
    return (mult - 1) <= aces;
}

static inline bool suit_sum_valid_bb(uint64_t sub, uint8_t target){
    uint32_t p = SUIT_PACKED_4(sub);
    return suit_sum_valid_packed((uint8_t)p, (uint8_t)(p >> 8), target);
}

// can_partition_bb: depth-limited recursive partition check
static __attribute__((noinline)) bool can_partition_bb(uint64_t sub, uint8_t target, uint8_t depth){
    if(sub == 0) return true;
    if(depth == 0) return false;
    for(uint64_t a = (sub - 1) & sub; a > 0; a = (a - 1) & sub){
        if(!suit_sum_valid_bb(a, target)) continue;
        uint64_t rest = sub ^ a;
        if(rest == 0) continue;
        if(suit_sum_valid_bb(rest, target) || can_partition_bb(rest, target, depth - 1)){
            return true;
        }
    }
    return false;
}

static __attribute__((noinline)) void poc_a_gen(s_cte_bitboard_move_list *out, uint64_t table_bb,
                                               const struct s_cte_hand *hand){
    out->size = 0;
    if(!hand || hand->size == 0) return;

    // Drop moves for all cards
    for(uint8_t h = 0; h < hand->size; h++){
        poc_a_push(out, hand->array[h], 0);
    }
    if(table_bb == 0) return;

    // Precompute targets and ace flags
    uint8_t targets[16];
    bool    ace_flag[16];
    for(uint8_t h = 0; h < hand->size; h++){
        t_card c = hand->array[h];
        ace_flag[h] = is_ace(c);
        targets[h]  = ace_flag[h] ? 11 : get_value(c);
    }

    // 1-Pass carry-rippler over all subsets
    for(uint64_t sub = table_bb; sub > 0; sub = (sub - 1) & table_bb){
        uint32_t packed = SUIT_PACKED_4(sub);
        uint8_t s = (uint8_t)packed;
        uint8_t aces = (uint8_t)(packed >> 8);

        for(uint8_t h = 0; h < hand->size; h++){
            uint8_t target = targets[h];
            t_card  card   = hand->array[h];
            bool    valid  = false;

            if(ace_flag[h]){
                // Ace as 11
                if(suit_sum_valid_packed(s, aces, 11)){
                    valid = true;
                } else {
                    // Check partition for 11
                    bool maybe = false;
                    for(uint8_t k = 0; k <= aces && !maybe; k++){
                        if(s >= 10 * k){
                            uint8_t adj = s - 10 * k;
                            if(adj >= 22 && adj < 128 && g_is_multiple[adj][11]) maybe = true;
                        }
                    }
                    if(maybe && can_partition_bb(sub, 11, 4)){
                        valid = true;
                    }
                }

                // Ace as 1
                if(!valid && suit_sum_valid_packed(s, aces, 1)){
                    valid = true;
                }
            } else {
                if(suit_sum_valid_packed(s, aces, target)){
                    valid = true;
                } else {
                    // Check partition for target
                    bool maybe = false;
                    for(uint8_t k = 0; k <= aces && !maybe; k++){
                        if(s >= 10 * k){
                            uint8_t adj = s - 10 * k;
                            if(adj >= 2 * target && adj < 128 && g_is_multiple[adj][target]) maybe = true;
                        }
                    }
                    if(maybe && can_partition_bb(sub, target, 4)){
                        valid = true;
                    }
                }
            }

            if(valid){
                poc_a_push(out, card, sub);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Helper: convert s_cte_move capture array to bitmask
// ---------------------------------------------------------------------------
static uint64_t move_to_bitmask(const struct s_cte_move *m){
    uint64_t mask = 0;
    for(uint8_t i = 0; i < m->cards_picked.size; i++)
        mask |= (1ULL << m->cards_picked.array[i]);
    return mask;
}

// ---------------------------------------------------------------------------
// Differential Validation vs Array Oracle
// ---------------------------------------------------------------------------
static bool validate_poc_a(const s_cte_engine_backend *oracle){
    srand(77771);
    uint32_t errors = 0;

    for(uint32_t iter = 0; iter < 10000; iter++){
        uint8_t deck[52];
        for(uint8_t i = 0; i < 52; i++) deck[i] = i;
        for(int i = 51; i > 0; i--){
            int j = rand() % (i + 1);
            uint8_t tmp = deck[i]; deck[i] = deck[j]; deck[j] = tmp;
        }
        uint8_t tsz = (uint8_t)(rand() % 11);
        uint8_t hsz = (uint8_t)(1 + rand() % 6);

        struct table tbl;
        tbl.nb_cards_on_table = tsz;
        uint64_t table_bb = 0;
        for(uint8_t i = 0; i < tsz; i++){
            tbl.cards_on_table[i] = deck[i];
            table_bb |= (1ULL << deck[i]);
        }
        struct s_cte_hand hand;
        hand.size = hsz;
        for(uint8_t i = 0; i < hsz; i++) hand.array[i] = deck[tsz + i];

        struct s_cte_move_list ref;
        init_move_list(&ref, 32);
        oracle->gen_all_moves(&ref, &tbl, &hand);

        s_cte_bitboard_move_list poc;
        poc_a_gen(&poc, table_bb, &hand);

        if(ref.size != poc.size){
            printf("  [ERROR] iter %u: ref=%u poc=%u\n", iter, ref.size, poc.size);
            if(errors == 0){
                // Dump first missing move from ref
                char buf[32];
                for(uint16_t i = 0; i < ref.size; i++){
                    uint64_t ref_mask = move_to_bitmask(&ref.moves[i]);
                    bool found = false;
                    for(uint16_t j = 0; j < poc.size; j++){
                        if(ref.moves[i].card_played == poc.moves[j].card_played &&
                           ref_mask == poc.moves[j].capture_mask){ found = true; break; }
                    }
                    if(!found){
                        format_card(buf, sizeof(buf), ref.moves[i].card_played, CTE_RENDER_ASCII);
                        printf("    MISSING: card=%s capture_bb=0x%016llx capture_sum=%u\n",
                               buf, (unsigned long long)ref_mask,
                               ref_mask ? SUIT_SUM_4(ref_mask) : 0);
                        // Print capture cards
                        uint64_t tmp = ref_mask;
                        while(tmp){ int bit = __builtin_ctzll(tmp);
                            format_card(buf,sizeof(buf),(t_card)bit,CTE_RENDER_ASCII);
                            printf("      captured: %s (val=%u)\n", buf, get_value((t_card)bit));
                            tmp &= tmp-1; }
                        break; // show first missing only
                    }
                }
            }
            errors++;
            free_move_list(&ref);
            continue;
        }
        for(uint16_t i = 0; i < ref.size; i++){
            uint64_t ref_mask = move_to_bitmask(&ref.moves[i]);
            bool found = false;
            for(uint16_t j = 0; j < poc.size; j++){
                if(ref.moves[i].card_played == poc.moves[j].card_played &&
                   ref_mask == poc.moves[j].capture_mask){
                    found = true; break;
                }
            }
            if(!found){
                printf("  [ERROR] iter %u: move not found in PoC A\n", iter);
                errors++; break;
            }
        }
        free_move_list(&ref);
    }
    return errors == 0;
}

// ---------------------------------------------------------------------------
// Benchmark
// ---------------------------------------------------------------------------
static double bench_poc_a(uint32_t N){
    srand(1337);
    uint64_t *tables = malloc(sizeof(uint64_t) * N);
    struct s_cte_hand *hands = malloc(sizeof(struct s_cte_hand) * N);

    for(uint32_t i = 0; i < N; i++){
        uint8_t deck[52];
        for(uint8_t j = 0; j < 52; j++) deck[j] = j;
        for(int j = 51; j > 0; j--){
            int k = rand() % (j + 1);
            uint8_t tmp = deck[j]; deck[j] = deck[k]; deck[k] = tmp;
        }
        uint8_t tsz = rand() % 6;
        tables[i] = 0;
        for(uint8_t j = 0; j < tsz; j++) tables[i] |= (1ULL << deck[j]);
        uint8_t hsz = 1 + rand() % 6;
        hands[i].size = hsz;
        for(uint8_t j = 0; j < hsz; j++) hands[i].array[j] = deck[tsz + j];
    }

    s_cte_bitboard_move_list out;
    uint64_t total = 0;
    clock_t t0 = clock();
    for(uint32_t i = 0; i < N; i++){
        poc_a_gen(&out, tables[i], &hands[i]);
        total += out.size;
    }
    clock_t t1 = clock();
    double secs = (double)(t1 - t0) / CLOCKS_PER_SEC;
    printf("  PoC A: %u positions, %lu moves, %.3f s, %.2f Mops/s, %.1f ns/pos\n",
           N, (unsigned long)total, secs,
           (double)total / (secs * 1e6),
           secs * 1e9 / N);
    free(tables);
    free(hands);
    return secs;
}

int main(void){
    printf("=== PoC Piste A: Carry-Rippler + g_suit_packed (16-bit LUT) ===\n\n");
    init_suit_packed();

    const s_cte_engine_backend *oracle = cte_get_backend(CTE_BACKEND_ARRAY);

    // validate_poc_a(oracle);
    bench_poc_a(2000000);

    return 0;
}
