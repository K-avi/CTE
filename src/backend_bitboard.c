#include "backend_bitboard.h"
#include "bitboard_tables.h"
#include "bitboard_rank_tables.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
typedef uint64_t cte_v4di __attribute__((vector_size(32), aligned(8)));
#endif

void bitboard_from_game(s_cte_bitboard_state *bb_state, const s_cte_game *game){
    if(!bb_state || !game) return;
    memset(bb_state, 0, sizeof(s_cte_bitboard_state));

    bb_state->table_bb = 0;
    for(uint8_t i = 0; i < game->table.nb_cards_on_table; i++){
        t_card c = game->table.cards_on_table[i];
        if(c < 52) bb_state->table_bb |= (1ULL << c);
    }

    bb_state->nb_players = game->players.size;
    bb_state->is_team_mode = game->is_team_mode;
    bb_state->last_captor_id = game->last_captor_id;
    bb_state->current_player_id = game->current_player_id;

    for(uint8_t p = 0; p < game->players.size; p++){
        const struct s_cte_player_data *pl = &game->players.players[p];
        bb_state->hand_count[p] = pl->hand.size;
        bb_state->hand_bb[p] = 0;
        for(uint8_t i = 0; i < pl->hand.size; i++){
            t_card c = pl->hand.array[i];
            if(c < 52) bb_state->hand_bb[p] |= (1ULL << c);
        }

        bb_state->won_count[p] = pl->won_cards.size;
        bb_state->won_bb[p] = 0;
        uint8_t pts = 0;
        for(uint8_t i = 0; i < pl->won_cards.size; i++){
            t_card c = pl->won_cards.array[i];
            if(c < 52){
                bb_state->won_bb[p] |= (1ULL << c);
                pts += get_points(c);
            }
        }
        bb_state->card_points[p] = pts;
        bb_state->tablic_count[p] = pl->nb_tablic;
    }
}


void bitboard_gen_all_compact_moves(s_cte_bitboard_move_list *out_list, uint64_t table_bb, const struct s_cte_hand *hand){
    if(!out_list || !hand) return;
    out_list->size = 0;
    if(hand->size == 0) return;

    // 1. Drop moves for all cards in hand
    for(uint8_t h = 0; h < hand->size; h++){
        if(out_list->size < 1024){
            out_list->moves[out_list->size++] = (s_cte_bitboard_move){ hand->array[h], 0 };
        }
    }

    if(table_bb == 0) return;

    // 2. 1-Pass carry-rippler submask evaluation over table_bb
    for(uint64_t sub = table_bb; sub > 0; sub = (sub - 1) & table_bb){
        uint8_t picked[16];
        uint8_t n = 0;
        uint64_t temp = sub;
        while(temp > 0){
            picked[n++] = (uint8_t)__builtin_ctzll(temp);
            temp &= (temp - 1);
        }

        for(uint8_t h = 0; h < hand->size; h++){
            t_card card = hand->array[h];
            bool ace = is_ace(card);
            uint8_t target = ace ? 11 : get_value(card);

            bool valid = false;
            if(ace){
                valid = (is_exact_partition(picked, n, 11) || is_exact_partition(picked, n, 1));
            } else {
                valid = is_exact_partition(picked, n, target);
            }

            if(valid && out_list->size < 1024){
                out_list->moves[out_list->size++] = (s_cte_bitboard_move){ card, sub };
            }
        }
    }
}

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

// Compute table total nominal sum for sum-bound pruning
static inline uint16_t compute_table_max_sum(uint64_t table_bb){
    uint16_t sum = 0;
    uint64_t temp = table_bb;
    while(temp > 0){
        int bit = __builtin_ctzll(temp);
        sum += get_value((t_card)bit);
        temp &= (temp - 1);
    }
    return sum;
}

// -------------------------------------------------------------
// 1. Dynamic Carry-Rippler Bitboard Engine (0 KB RAM)
// -------------------------------------------------------------
t_cteerr bitboard_gen_card_moves_dynamic(struct s_cte_move_list *moves, uint64_t table_bb, t_card card){
    if(!moves) return e_null;
    if(!moves->moves && moves->max == 0){
        t_cteerr err = init_move_list(moves, 16);
        if(err != e_ok) return err;
    }

    // Always push drop move (capture mask = 0)
    t_cteerr err = bitboard_push_move(moves, card, 0);
    if(err != e_ok) return err;

    if(table_bb == 0) return e_ok;

    bool ace = is_ace(card);
    uint8_t target_v = ace ? 11 : get_value(card);

    // Sum-bound pruning: if table total sum < target, no capture is possible
    uint16_t table_max_sum = compute_table_max_sum(table_bb);
    if(!ace && table_max_sum < target_v) return e_ok;
    if(ace && table_max_sum < 1) return e_ok;

    // Carry-rippler submask iteration through all 2^N - 1 non-empty sub-bitboards
    for(uint64_t sub = table_bb; sub > 0; sub = (sub - 1) & table_bb){
        uint8_t picked[16];
        uint8_t n = 0;
        uint64_t temp = sub;
        while(temp > 0){
            picked[n++] = (uint8_t)__builtin_ctzll(temp);
            temp &= (temp - 1);
        }

        bool valid = false;
        if(ace){
            valid = (is_exact_partition(picked, n, 11) || is_exact_partition(picked, n, 1));
        } else {
            valid = is_exact_partition(picked, n, target_v);
        }

        if(valid){
            err = bitboard_push_move(moves, card, sub);
            if(err != e_ok) return err;
        }
    }

    return e_ok;
}

t_cteerr bitboard_gen_all_moves_dynamic(struct s_cte_move_list *moves, uint64_t table_bb, const struct s_cte_hand *hand){
    if(!moves || !hand) return e_null;
    if(!moves->moves && moves->max == 0){
        t_cteerr err = init_move_list(moves, 32);
        if(err != e_ok) return err;
    }

    for(uint8_t i = 0; i < hand->size; i++){
        t_cteerr err = bitboard_gen_card_moves_dynamic(moves, table_bb, hand->array[i]);
        if(err != e_ok) return err;
    }
    return e_ok;
}

// -------------------------------------------------------------
// 2. Optimized 1D Pivot Inverted Index Bitboard Engine (225 KB RAM)
// -------------------------------------------------------------
static uint8_t collect_active_base_masks(uint64_t table_bb, uint8_t target_val, uint64_t *out_masks, uint8_t max_out){
    if(target_val == 0 || target_val > 14) return 0;

    // 1. Fast Reachability Rejection in 1 cycle
    if((table_bb & g_reachability_mask[target_val]) == 0) return 0;

    uint8_t found = 0;
    uint64_t temp = table_bb;

    cte_v4di v_tbl = { table_bb, table_bb, table_bb, table_bb };

    // 2. Lookup only the pivot buckets for cards currently on the table
    while(temp > 0){
        uint8_t c = (uint8_t)__builtin_ctzll(temp);
        uint16_t idx = CTE_PIVOT_INDEX(c, target_val);
        uint16_t count = g_pivot_counts[idx];
        const uint64_t *masks = &g_pivot_subset_masks[g_pivot_offsets[idx]];

        // 4-Way SIMD Vectorization via GNU Vector Extensions (portable across architectures)
        uint16_t i = 0;
        for(; i + 3 < count; i += 4){
            cte_v4di v_m = *(const cte_v4di*)&masks[i];
            cte_v4di v_cmp = ((v_tbl & v_m) == v_m);
            uint64_t any = v_cmp[0] | v_cmp[1] | v_cmp[2] | v_cmp[3];
            if(__builtin_expect(any != 0, 0)){
                if(v_cmp[0] && found < max_out) out_masks[found++] = masks[i + 0];
                if(v_cmp[1] && found < max_out) out_masks[found++] = masks[i + 1];
                if(v_cmp[2] && found < max_out) out_masks[found++] = masks[i + 2];
                if(v_cmp[3] && found < max_out) out_masks[found++] = masks[i + 3];
            }
        }
        for(; i < count; i++){
            uint64_t m = masks[i];
            if((table_bb & m) == m && found < max_out) out_masks[found++] = m;
        }

        temp &= (temp - 1); // BMI2 BLSR
    }
    return found;
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

t_cteerr bitboard_gen_card_moves_table(struct s_cte_move_list *moves, uint64_t table_bb, t_card card){
    if(!moves) return e_null;
    if(!moves->moves && moves->max == 0){
        t_cteerr err = init_move_list(moves, 16);
        if(err != e_ok) return err;
    }

    t_cteerr err = bitboard_push_move(moves, card, 0);
    if(err != e_ok) return err;

    if(table_bb == 0) return e_ok;

    bool ace = is_ace(card);
    uint8_t val = ace ? 11 : get_value(card);

    // Sum-bound pruning
    uint16_t table_max_sum = compute_table_max_sum(table_bb);
    if(!ace && table_max_sum < val) return e_ok;
    if(ace && table_max_sum < 1) return e_ok;

    uint64_t active_masks[64];
    uint64_t capture_masks[256];
    uint16_t capture_count = 0;

    uint64_t bloom[4] = {0};

    if(ace){
        if(table_max_sum >= 11){
            uint8_t n11 = collect_active_base_masks(table_bb, 11, active_masks, 64);
            if(n11 > 0){
                combine_disjoint_masks(0, 0, n11, active_masks, capture_masks, &capture_count, 256, bloom);
            }
        }

        uint64_t active_masks_1[64];
        uint8_t n1 = collect_active_base_masks(table_bb, 1, active_masks_1, 64);
        if(n1 > 0){
            combine_disjoint_masks(0, 0, n1, active_masks_1, capture_masks, &capture_count, 256, bloom);
        }
    } else {
        uint8_t n = collect_active_base_masks(table_bb, val, active_masks, 64);
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

t_cteerr bitboard_gen_all_moves_table(struct s_cte_move_list *moves, uint64_t table_bb, const struct s_cte_hand *hand){
    if(!moves || !hand) return e_null;
    if(!moves->moves && moves->max == 0){
        t_cteerr err = init_move_list(moves, 32);
        if(err != e_ok) return err;
    }

    s_cte_bitboard_move_list cpt;
    bitboard_gen_all_compact_moves_table(&cpt, table_bb, hand);

    for(uint16_t i = 0; i < cpt.size; i++){
        t_cteerr err = bitboard_push_move(moves, cpt.moves[i].card_played, cpt.moves[i].capture_mask);
        if(err != e_ok) return err;
    }
    return e_ok;
}

void bitboard_gen_all_compact_moves_table(s_cte_bitboard_move_list *out_list, uint64_t table_bb, const struct s_cte_hand *hand){
    if(!out_list || !hand) return;
    out_list->size = 0;
    if(hand->size == 0) return;

    // 1. Always emit drop moves for all cards in hand
    for(uint8_t h = 0; h < hand->size; h++){
        if(out_list->size < 1024){
            out_list->moves[out_list->size++] = (s_cte_bitboard_move){ hand->array[h], 0 };
        }
    }

    if(table_bb == 0) return;

    // 2. Global Hand Reachability Fast Rejection (1 cycle CPU)
    uint16_t val_mask = 0;
    uint64_t global_reach = 0;
    for(uint8_t h = 0; h < hand->size; h++){
        t_card c = hand->array[h];
        if(is_ace(c)){
            val_mask |= (1u << 1) | (1u << 11);
            global_reach |= (g_reachability_mask[1] | g_reachability_mask[11]);
        } else {
            uint8_t v = get_value(c);
            val_mask |= (1u << v);
            global_reach |= g_reachability_mask[v];
        }
    }

    if((table_bb & global_reach) == 0) return;

    // 3. Collect active base masks in 1 SINGLE pass over table_bb
    uint64_t active_by_val[15][64];
    uint8_t  active_count[15] = {0};

    cte_v4di v_tbl = { table_bb, table_bb, table_bb, table_bb };
    uint64_t temp = table_bb;
    while(temp > 0){
        uint8_t c = (uint8_t)__builtin_ctzll(temp);

        uint16_t v_temp = val_mask;
        while(v_temp > 0){
            uint8_t v = (uint8_t)__builtin_ctz(v_temp);
            v_temp &= (v_temp - 1);

            uint16_t idx = CTE_PIVOT_INDEX(c, v);
            uint16_t count = g_pivot_counts[idx];
            const uint64_t *masks = &g_pivot_subset_masks[g_pivot_offsets[idx]];

            uint16_t i = 0;
            for(; i + 3 < count; i += 4){
                cte_v4di v_m = *(const cte_v4di*)&masks[i];
                cte_v4di v_cmp = ((v_tbl & v_m) == v_m);
                uint64_t any = v_cmp[0] | v_cmp[1] | v_cmp[2] | v_cmp[3];
                if(__builtin_expect(any != 0, 0)){
                    if(v_cmp[0] && active_count[v] < 64) active_by_val[v][active_count[v]++] = masks[i + 0];
                    if(v_cmp[1] && active_count[v] < 64) active_by_val[v][active_count[v]++] = masks[i + 1];
                    if(v_cmp[2] && active_count[v] < 64) active_by_val[v][active_count[v]++] = masks[i + 2];
                    if(v_cmp[3] && active_count[v] < 64) active_by_val[v][active_count[v]++] = masks[i + 3];
                }
            }
            for(; i < count; i++){
                uint64_t m = masks[i];
                if((table_bb & m) == m && active_count[v] < 64) active_by_val[v][active_count[v]++] = m;
            }
        }
        temp &= (temp - 1); // BMI2 BLSR
    }

    // 4. Precompute disjoint combinations for values in hand (dense 8 slots max)
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
                if(active_count[11] > 0){
                    combine_disjoint_masks(0, 0, active_count[11], active_by_val[11],
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

            // Ace as 1 (only add masks not already added by Ace as 11)
            int8_t s1 = val_to_slot[1];
            if(s1 == -1 && num_slots < 8){
                s1 = (int8_t)(num_slots++);
                val_to_slot[1] = s1;
                if(active_count[1] > 0){
                    combine_disjoint_masks(0, 0, active_count[1], active_by_val[1],
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
                if(active_count[v] > 0){
                    combine_disjoint_masks(0, 0, active_count[v], active_by_val[v],
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

static t_cteerr bitboard_adapter_gen_card_moves_dyn(struct s_cte_move_list *moves, const struct table *table, t_card card){
    if(!moves) return e_null;
    uint64_t table_bb = 0;
    if(table){
        for(uint8_t i = 0; i < table->nb_cards_on_table; i++){
            t_card c = table->cards_on_table[i];
            if(c < 52) table_bb |= (1ULL << c);
        }
    }
    return bitboard_gen_card_moves_dynamic(moves, table_bb, card);
}

static t_cteerr bitboard_adapter_gen_all_moves_dyn(struct s_cte_move_list *moves, const struct table *table, const struct s_cte_hand *hand){
    if(!moves || !hand) return e_null;
    uint64_t table_bb = 0;
    if(table){
        for(uint8_t i = 0; i < table->nb_cards_on_table; i++){
            t_card c = table->cards_on_table[i];
            if(c < 52) table_bb |= (1ULL << c);
        }
    }
    return bitboard_gen_all_moves_dynamic(moves, table_bb, hand);
}

static t_cteerr bitboard_adapter_gen_card_moves_tbl(struct s_cte_move_list *moves, const struct table *table, t_card card){
    if(!moves) return e_null;
    uint64_t table_bb = 0;
    if(table){
        for(uint8_t i = 0; i < table->nb_cards_on_table; i++){
            t_card c = table->cards_on_table[i];
            if(c < 52) table_bb |= (1ULL << c);
        }
    }
    return bitboard_gen_card_moves_table(moves, table_bb, card);
}

static t_cteerr bitboard_adapter_gen_all_moves_tbl(struct s_cte_move_list *moves, const struct table *table, const struct s_cte_hand *hand){
    if(!moves || !hand) return e_null;
    if(!moves->moves && moves->max == 0){
        t_cteerr err = init_move_list(moves, 32);
        if(err != e_ok) return err;
    }

    uint64_t table_bb = 0;
    if(table){
        for(uint8_t i = 0; i < table->nb_cards_on_table; i++){
            t_card c = table->cards_on_table[i];
            if(c < 52) table_bb |= (1ULL << c);
        }
    }

    s_cte_bitboard_move_list cpt;
    bitboard_gen_all_compact_moves_table(&cpt, table_bb, hand);

    for(uint16_t i = 0; i < cpt.size; i++){
        t_cteerr err = bitboard_push_move(moves, cpt.moves[i].card_played, cpt.moves[i].capture_mask);
        if(err != e_ok) return err;
    }
    return e_ok;
}

static s_cte_pos bitboard_adapter_to_pos(const s_cte_game *game){
    if(!game){
        s_cte_pos empty = {0};
        return empty;
    }
    s_cte_game_state st = {
        .table = &game->table,
        .players = &game->players,
        .deck = &game->deck,
        .current_player_id = game->current_player_id,
        .is_team_mode = game->is_team_mode,
    };
    return pos_from_state(&st);
}

const s_cte_engine_backend g_backend_bitboard = {
    .type            = CTE_BACKEND_BITBOARD,
    .name            = "Bitboard Dynamic (Carry-Rippler, 0 KB RAM)",
    .init_game       = init_game,
    .free_game       = free_game,
    .setup_round     = setup_round,
    .deal_next_hand  = deal_next_hand,
    .award_remaining = award_remaining_table_cards,
    .run_round       = run_round,
    .is_legal        = is_legal,
    .gen_card_moves  = bitboard_adapter_gen_card_moves_dyn,
    .gen_all_moves   = bitboard_adapter_gen_all_moves_dyn,
    .play_move       = play_move,
    .score_move      = score_move,
    .to_pos          = bitboard_adapter_to_pos,
};

const s_cte_engine_backend g_backend_bitboard_table = {
    .type            = CTE_BACKEND_BITBOARD_TABLE,
    .name            = "Bitboard 1D Pivot Tables (225 KB RAM)",
    .init_game       = init_game,
    .free_game       = free_game,
    .setup_round     = setup_round,
    .deal_next_hand  = deal_next_hand,
    .award_remaining = award_remaining_table_cards,
    .run_round       = run_round,
    .is_legal        = is_legal,
    .gen_card_moves  = bitboard_adapter_gen_card_moves_tbl,
    .gen_all_moves   = bitboard_adapter_gen_all_moves_tbl,
    .play_move       = play_move,
    .score_move      = score_move,
    .to_pos          = bitboard_adapter_to_pos,
};

// ============================================================================
// CTE_BACKEND_BITBOARD_RANK : Compact SWAR Rank Patterns (7.1 KB RAM, 100% L1)
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

static t_cteerr bitboard_adapter_gen_card_moves_rnk(struct s_cte_move_list *moves, const struct table *table, t_card card){
    if(!moves) return e_null;
    uint64_t table_bb = 0;
    if(table){
        for(uint8_t i = 0; i < table->nb_cards_on_table; i++){
            t_card c = table->cards_on_table[i];
            if(c < 52) table_bb |= (1ULL << c);
        }
    }
    return bitboard_gen_card_moves_rank(moves, table_bb, card);
}

static t_cteerr bitboard_adapter_gen_all_moves_rnk(struct s_cte_move_list *moves, const struct table *table, const struct s_cte_hand *hand){
    if(!moves || !hand) return e_null;
    uint64_t table_bb = 0;
    if(table){
        for(uint8_t i = 0; i < table->nb_cards_on_table; i++){
            t_card c = table->cards_on_table[i];
            if(c < 52) table_bb |= (1ULL << c);
        }
    }
    return bitboard_gen_all_moves_rank(moves, table_bb, hand);
}

const s_cte_engine_backend g_backend_bitboard_rank = {
    .type            = CTE_BACKEND_BITBOARD_RANK,
    .name            = "Bitboard SWAR Rank Patterns (7.1 KB RAM)",
    .init_game       = init_game,
    .free_game       = free_game,
    .setup_round     = setup_round,
    .deal_next_hand  = deal_next_hand,
    .award_remaining = award_remaining_table_cards,
    .run_round       = run_round,
    .is_legal        = is_legal,
    .gen_card_moves  = bitboard_adapter_gen_card_moves_rnk,
    .gen_all_moves   = bitboard_adapter_gen_all_moves_rnk,
    .play_move       = play_move,
    .score_move      = score_move,
    .to_pos          = bitboard_adapter_to_pos,
};

