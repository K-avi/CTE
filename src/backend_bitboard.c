#include "backend_bitboard.h"
#include "bitboard_rank_tables.h"
#include <stdlib.h>
#include <string.h>

// Push move directly with zero heap allocations (100% stack)
static t_cteerr bitboard_push_move(struct s_cte_move_list *moves, t_card card, uint64_t capture_mask){
    struct s_cte_move m;
    m.card_played = card;

    if(capture_mask == 0){
        m.cards_picked.size = 0;
    } else {
        uint8_t pop = (uint8_t)__builtin_popcountll(capture_mask);
        m.cards_picked.size = pop;
        uint64_t temp = capture_mask;
        uint8_t idx = 0;
        while(temp > 0){
            int bit = __builtin_ctzll(temp);
            m.cards_picked.array[idx++] = (uint8_t)bit;
            temp &= (temp - 1); // BMI2 BLSR
        }
    }

    if(moves->size >= moves->max){
        uint16_t new_cap = (moves->max == 0) ? 16 : (moves->max * 2);
        struct s_cte_move *new_arr = realloc(moves->moves, sizeof(struct s_cte_move) * new_cap);
        if(!new_arr) return e_realloc;
        moves->moves = new_arr;
        moves->max = new_cap;
    }

    moves->moves[moves->size++] = m;
    return e_ok;
}

// Combine disjoint base masks into multi-capture unions (with fast bloom filter deduplication)
static void combine_disjoint_masks(uint64_t current_union,
                                  uint8_t start_idx,
                                  uint8_t num_active,
                                  const uint64_t *active_masks,
                                  uint64_t *out_captures,
                                  uint16_t *out_count,
                                  uint16_t max_captures,
                                  uint64_t *bloom)
{
    for(uint8_t i = start_idx; i < num_active; i++){
        uint64_t next_mask = active_masks[i];
        if((current_union & next_mask) == 0){
            uint64_t new_union = current_union | next_mask;

            uint8_t h = (uint8_t)(((new_union) ^ (new_union >> 11) ^ (new_union >> 23)) & 255);
            uint8_t word = h >> 6;
            uint64_t bit = 1ULL << (h & 63);

            if((bloom[word] & bit) == 0){
                bloom[word] |= bit;
                if(*out_count < max_captures){
                    out_captures[(*out_count)++] = new_union;
                }
            } else {
                bool exists = false;
                for(uint16_t k = 0; k < *out_count; k++){
                    if(out_captures[k] == new_union){
                        exists = true;
                        break;
                    }
                }
                if(!exists && *out_count < max_captures){
                    out_captures[(*out_count)++] = new_union;
                }
            }
            combine_disjoint_masks(new_union, (uint8_t)(i + 1), num_active, active_masks,
                                   out_captures, out_count, max_captures, bloom);
        }
    }
}

// ============================================================================
// CTE_BACKEND_BITBOARD : Compact SWAR Rank Patterns (7.1 KB RAM, 100% L1)
// ============================================================================

static uint8_t collect_active_base_masks_rank(const uint8_t table_suits[13],
                                              uint64_t table_swar,
                                              uint8_t target_val,
                                              uint64_t *out_masks,
                                              uint8_t max_out)
{
    if(target_val == 0 || target_val > 14) return 0;

    uint8_t found = 0;
    uint16_t offset = g_rank_pattern_offsets[target_val];
    uint16_t count  = g_rank_pattern_counts[target_val];

    for(uint16_t i = 0; i < count; i++){
        const s_cte_rank_pattern *p = &g_rank_patterns[offset + i];

        uint64_t diff = table_swar - p->packed_swar;
        if(((diff & CTE_SWAR_GUARD_MASK) == CTE_SWAR_GUARD_MASK)){
            uint8_t n_ranks = p->num_ranks;
            if(n_ranks == 1){
                uint8_t r0 = p->ranks[0];
                uint8_t nd0 = p->counts[0];
                uint8_t s0 = table_suits[r0];
                uint8_t n_comb0 = g_suit_combination_counts[s0][nd0];
                for(uint8_t c0 = 0; c0 < n_comb0; c0++){
                    if(found < max_out){
                        out_masks[found++] = g_suit_to_rank0[g_suit_combinations[s0][nd0][c0]] << r0;
                    }
                }
            } else if(n_ranks == 2){
                uint8_t r0 = p->ranks[0], nd0 = p->counts[0], s0 = table_suits[r0];
                uint8_t r1 = p->ranks[1], nd1 = p->counts[1], s1 = table_suits[r1];
                uint8_t n_comb0 = g_suit_combination_counts[s0][nd0];
                uint8_t n_comb1 = g_suit_combination_counts[s1][nd1];
                for(uint8_t c0 = 0; c0 < n_comb0; c0++){
                    uint64_t m0 = g_suit_to_rank0[g_suit_combinations[s0][nd0][c0]] << r0;
                    for(uint8_t c1 = 0; c1 < n_comb1; c1++){
                        if(found < max_out){
                            out_masks[found++] = m0 | (g_suit_to_rank0[g_suit_combinations[s1][nd1][c1]] << r1);
                        }
                    }
                }
            } else if(n_ranks == 3){
                uint8_t r0 = p->ranks[0], nd0 = p->counts[0], s0 = table_suits[r0];
                uint8_t r1 = p->ranks[1], nd1 = p->counts[1], s1 = table_suits[r1];
                uint8_t r2 = p->ranks[2], nd2 = p->counts[2], s2 = table_suits[r2];
                uint8_t n_comb0 = g_suit_combination_counts[s0][nd0];
                uint8_t n_comb1 = g_suit_combination_counts[s1][nd1];
                uint8_t n_comb2 = g_suit_combination_counts[s2][nd2];
                for(uint8_t c0 = 0; c0 < n_comb0; c0++){
                    uint64_t m0 = g_suit_to_rank0[g_suit_combinations[s0][nd0][c0]] << r0;
                    for(uint8_t c1 = 0; c1 < n_comb1; c1++){
                        uint64_t m01 = m0 | (g_suit_to_rank0[g_suit_combinations[s1][nd1][c1]] << r1);
                        for(uint8_t c2 = 0; c2 < n_comb2; c2++){
                            if(found < max_out){
                                out_masks[found++] = m01 | (g_suit_to_rank0[g_suit_combinations[s2][nd2][c2]] << r2);
                            }
                        }
                    }
                }
            } else { // n_ranks == 4
                uint8_t r0 = p->ranks[0], nd0 = p->counts[0], s0 = table_suits[r0];
                uint8_t r1 = p->ranks[1], nd1 = p->counts[1], s1 = table_suits[r1];
                uint8_t r2 = p->ranks[2], nd2 = p->counts[2], s2 = table_suits[r2];
                uint8_t r3 = p->ranks[3], nd3 = p->counts[3], s3 = table_suits[r3];
                uint8_t n_comb0 = g_suit_combination_counts[s0][nd0];
                uint8_t n_comb1 = g_suit_combination_counts[s1][nd1];
                uint8_t n_comb2 = g_suit_combination_counts[s2][nd2];
                uint8_t n_comb3 = g_suit_combination_counts[s3][nd3];
                for(uint8_t c0 = 0; c0 < n_comb0; c0++){
                    uint64_t m0 = g_suit_to_rank0[g_suit_combinations[s0][nd0][c0]] << r0;
                    for(uint8_t c1 = 0; c1 < n_comb1; c1++){
                        uint64_t m01 = m0 | (g_suit_to_rank0[g_suit_combinations[s1][nd1][c1]] << r1);
                        for(uint8_t c2 = 0; c2 < n_comb2; c2++){
                            uint64_t m012 = m01 | (g_suit_to_rank0[g_suit_combinations[s2][nd2][c2]] << r2);
                            for(uint8_t c3 = 0; c3 < n_comb3; c3++){
                                if(found < max_out){
                                    out_masks[found++] = m012 | (g_suit_to_rank0[g_suit_combinations[s3][nd3][c3]] << r3);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return found;
}

void bitboard_gen_all_compact_moves_rank(s_cte_bitboard_move_list *out_list, uint64_t table_bb, const struct s_cte_hand *hand){
    if(!out_list) return;
    out_list->size = 0;
    if(!hand || hand->size == 0) return;

    if(table_bb == 0){
        for(uint8_t h = 0; h < hand->size; h++){
            out_list->moves[out_list->size++] = (s_cte_bitboard_move){ hand->array[h], 0 };
        }
        return;
    }

    uint8_t table_suits[13] = {0};
    uint64_t table_swar = CTE_SWAR_GUARD_MASK;
    uint64_t temp = table_bb;
    while(temp > 0){
        int c = __builtin_ctzll(temp);
        uint8_t r = (uint8_t)(c % 13);
        table_suits[r] |= (1 << (c / 13));
        table_swar += (1ULL << (r * 4));
        temp &= (temp - 1);
    }

    for(uint8_t h = 0; h < hand->size; h++){
        out_list->moves[out_list->size++] = (s_cte_bitboard_move){ hand->array[h], 0 };
    }

    uint64_t active_masks[64];
    uint64_t cap_by_slot[8][256];
    uint16_t cap_count[8] = {0};
    uint64_t bloom_by_slot[8][4] = {{0}};
    int8_t   val_to_slot[15];
    memset(val_to_slot, -1, sizeof(val_to_slot));
    uint8_t  num_slots = 0;

    for(uint8_t h = 0; h < hand->size; h++){
        t_card card = hand->array[h];
        bool ace = is_ace(card);

        if(ace){
            int8_t s11 = val_to_slot[11];
            if(s11 == -1 && num_slots < 8){
                s11 = (int8_t)(num_slots++);
                val_to_slot[11] = s11;
                uint8_t n11 = collect_active_base_masks_rank(table_suits, table_swar, 11, active_masks, 64);
                if(n11 > 0){
                    combine_disjoint_masks(0, 0, n11, active_masks,
                                           cap_by_slot[s11], &cap_count[s11], 256, bloom_by_slot[s11]);
                }
            }
            if(s11 >= 0){
                for(uint16_t k = 0; k < cap_count[s11]; k++){
                    if(out_list->size < 1024){
                        out_list->moves[out_list->size++] = (s_cte_bitboard_move){ card, cap_by_slot[s11][k] };
                    }
                }
            }

            int8_t s1 = val_to_slot[1];
            if(s1 == -1 && num_slots < 8){
                s1 = (int8_t)(num_slots++);
                val_to_slot[1] = s1;
                uint8_t n1 = collect_active_base_masks_rank(table_suits, table_swar, 1, active_masks, 64);
                if(n1 > 0){
                    combine_disjoint_masks(0, 0, n1, active_masks,
                                           cap_by_slot[s1], &cap_count[s1], 256, bloom_by_slot[s1]);
                }
            }
            if(s1 >= 0){
                for(uint16_t k = 0; k < cap_count[s1]; k++){
                    uint64_t m1 = cap_by_slot[s1][k];
                    uint8_t h1 = (uint8_t)(((m1) ^ (m1 >> 11) ^ (m1 >> 23)) & 255);
                    bool already = false;
                    if(s11 >= 0 && (bloom_by_slot[s11][h1 >> 6] & (1ULL << (h1 & 63))) != 0){
                        for(uint16_t j = 0; j < cap_count[s11]; j++){
                            if(cap_by_slot[s11][j] == m1){
                                already = true;
                                break;
                            }
                        }
                    }
                    if(!already && out_list->size < 1024){
                        out_list->moves[out_list->size++] = (s_cte_bitboard_move){ card, m1 };
                    }
                }
            }
        } else {
            uint8_t v = get_value(card);
            int8_t sv = val_to_slot[v];
            if(sv == -1 && num_slots < 8){
                sv = (int8_t)(num_slots++);
                val_to_slot[v] = sv;
                uint8_t nv = collect_active_base_masks_rank(table_suits, table_swar, v, active_masks, 64);
                if(nv > 0){
                    combine_disjoint_masks(0, 0, nv, active_masks,
                                           cap_by_slot[sv], &cap_count[sv], 256, bloom_by_slot[sv]);
                }
            }
            if(sv >= 0){
                for(uint16_t k = 0; k < cap_count[sv]; k++){
                    if(out_list->size < 1024){
                        out_list->moves[out_list->size++] = (s_cte_bitboard_move){ card, cap_by_slot[sv][k] };
                    }
                }
            }
        }
    }
}

t_cteerr bitboard_gen_card_moves_rank(struct s_cte_move_list *moves, uint64_t table_bb, t_card card){
    if(!moves) return e_null;
    if(!moves->moves && moves->max == 0){
        t_cteerr err = init_move_list(moves, 16);
        if(err != e_ok) return err;
    }

    t_cteerr err = bitboard_push_move(moves, card, 0);
    if(err != e_ok) return err;

    if(table_bb == 0) return e_ok;

    uint8_t table_suits[13] = {0};
    uint64_t table_swar = CTE_SWAR_GUARD_MASK;
    uint64_t temp = table_bb;
    while(temp > 0){
        int c = __builtin_ctzll(temp);
        uint8_t r = (uint8_t)(c % 13);
        table_suits[r] |= (1 << (c / 13));
        table_swar += (1ULL << (r * 4));
        temp &= (temp - 1);
    }

    bool ace = is_ace(card);
    uint8_t val = get_value(card);

    uint64_t active_masks[64];
    uint64_t capture_masks[256];
    uint16_t capture_count = 0;
    uint64_t bloom[4] = {0};

    if(ace){
        uint8_t n11 = collect_active_base_masks_rank(table_suits, table_swar, 11, active_masks, 64);
        if(n11 > 0){
            combine_disjoint_masks(0, 0, n11, active_masks, capture_masks, &capture_count, 256, bloom);
        }

        uint64_t active_masks_1[64];
        uint8_t n1 = collect_active_base_masks_rank(table_suits, table_swar, 1, active_masks_1, 64);
        if(n1 > 0){
            combine_disjoint_masks(0, 0, n1, active_masks_1, capture_masks, &capture_count, 256, bloom);
        }
    } else {
        uint8_t n = collect_active_base_masks_rank(table_suits, table_swar, val, active_masks, 64);
        if(n > 0){
            combine_disjoint_masks(0, 0, n, active_masks, capture_masks, &capture_count, 256, bloom);
        }
    }

    for(uint16_t i = 0; i < capture_count; i++){
        err = bitboard_push_move(moves, card, capture_masks[i]);
        if(err != e_ok) return err;
    }

    return e_ok;
}

t_cteerr bitboard_gen_all_moves_rank(struct s_cte_move_list *moves, uint64_t table_bb, const struct s_cte_hand *hand){
    if(!moves || !hand) return e_null;
    s_cte_bitboard_move_list cpt;
    bitboard_gen_all_compact_moves_rank(&cpt, table_bb, hand);
    if(!moves->moves && moves->max == 0){
        t_cteerr err = init_move_list(moves, cpt.size > 0 ? cpt.size : 16);
        if(err != e_ok) return err;
    }
    for(uint16_t i = 0; i < cpt.size; i++){
        t_cteerr err = bitboard_push_move(moves, cpt.moves[i].card_played, cpt.moves[i].capture_mask);
        if(err != e_ok) return err;
    }
    return e_ok;
}

const s_cte_engine_backend g_backend_bitboard = {
    .type           = CTE_BACKEND_BITBOARD,
    .name           = "Bitboard SWAR (default)",
    .is_legal       = is_legal,
    .gen_card_moves = bitboard_gen_card_moves_rank,
    .gen_all_moves  = bitboard_gen_all_moves_rank,
};
