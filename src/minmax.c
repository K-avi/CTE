#include "minmax.h"
#include "backend_bitboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#define INF_SCORE 10000000

static bool is_friendly(const s_cte_pos *pos, uint8_t player_id, uint8_t root_player);

s_cte_pos pos_from_state(const s_cte_game_state *state){
    s_cte_pos res;
    memset(&res, 0, sizeof(res));
    if(!state) return res;

    res.table_bb = state->table_bb;

    res.nb_players = (state->players && state->players->size <= 4) ? state->players->size : 2;
    res.current_player = state->current_player_id;
    res.last_captor = -1;
    res.is_team_mode = state->is_team_mode && (res.nb_players == 4);

    if(state->players){
        for(uint8_t p = 0; p < res.nb_players; p++){
            const struct s_cte_player_data *pl = &state->players->players[p];
            res.hand_counts[p] = pl->hand.size;
            for(uint8_t i = 0; i < pl->hand.size; i++){
                t_card c = pl->hand.array[i];
                if(c < 52) res.hand_bb[p] |= (1ULL << c);
            }
            res.won_card_counts[p] = pl->won_cards.size;
            res.tablic_counts[p] = pl->nb_tablic;

            uint8_t pts = 0;
            for(uint8_t j = 0; j < pl->won_cards.size; j++){
                pts += get_points(pl->won_cards.array[j]);
            }
            res.card_points[p] = pts;
        }
    }

    return res;
}

s_cte_pos pos_apply_bitboard_move(const s_cte_pos *pos, t_card card_played, uint64_t capture_mask){
    s_cte_pos next;
    if(!pos){
        memset(&next, 0, sizeof(next));
        return next;
    }
    next = *pos;

    uint8_t p = pos->current_player;

    // 1. Remove card_played from player's hand bitboard
    if(card_played < 52){
        next.hand_bb[p] &= ~(1ULL << card_played);
        if(next.hand_counts[p] > 0) next.hand_counts[p]--;
    }

    // 2. Drop move
    if(capture_mask == 0){
        if(card_played < 52){
            next.table_bb |= (1ULL << card_played);
        }
        next.current_player = (uint8_t)((p + 1) % pos->nb_players);
        return next;
    }

    // 3. Capture move: calculate card points directly from bits
    uint8_t pts = get_points(card_played);
    uint64_t temp = capture_mask;
    uint8_t num_picked = 0;
    while(temp > 0){
        int bit = __builtin_ctzll(temp);
        pts += get_points((t_card)bit);
        num_picked++;
        temp &= (temp - 1);
    }

    next.card_points[p] += pts;
    next.won_card_counts[p] += (uint8_t)(1 + num_picked);
    next.table_bb &= ~capture_mask;
    if(next.table_bb == 0){
        next.tablic_counts[p]++;
    }
    next.last_captor = (int8_t)p;
    next.current_player = (uint8_t)((p + 1) % pos->nb_players);
    return next;
}

s_cte_pos pos_apply_move(const s_cte_pos *pos, const struct s_cte_move *move){
    if(!pos || !move){
        s_cte_pos empty = {0};
        return empty;
    }
    uint64_t mask = 0;
    for(uint8_t i = 0; i < move->cards_picked.size; i++){
        if(move->cards_picked.array[i] < 52){
            mask |= (1ULL << move->cards_picked.array[i]);
        }
    }
    return pos_apply_bitboard_move(pos, move->card_played, mask);
}

t_cteerr pos_gen_moves(struct s_cte_move_list *moves, const s_cte_pos *pos){
    if(!moves || !pos) return e_null;

    // Extract hand directly from bitboard (no struct table needed)
    struct s_cte_hand cur_hand;
    cur_hand.size = 0;
    uint64_t h_temp = pos->hand_bb[pos->current_player];
    while(h_temp > 0){
        int bit = __builtin_ctzll(h_temp);
        cur_hand.array[cur_hand.size++] = (t_card)bit;
        h_temp &= (h_temp - 1);
    }

    // Direct bitboard path: no round-trip through struct table
    s_cte_bitboard_move_list cpt;
    bitboard_gen_all_compact_moves_rank(&cpt, pos->table_bb, &cur_hand);

    // Convert compact moves to s_cte_move format
    if(!moves->moves && moves->max == 0){
        t_cteerr err = init_move_list(moves, cpt.size > 0 ? cpt.size : 16);
        if(err != e_ok) return err;
    }

    for(uint16_t i = 0; i < cpt.size; i++){
        struct s_cte_move m;
        m.card_played = cpt.moves[i].card_played;
        uint64_t mask = cpt.moves[i].capture_mask;
        if(mask == 0){
            m.cards_picked.size = 0;
        } else {
            m.cards_picked.size = (uint8_t)__builtin_popcountll(mask);
            uint8_t idx = 0;
            uint64_t temp = mask;
            while(temp > 0){
                m.cards_picked.array[idx++] = (uint8_t)__builtin_ctzll(temp);
                temp &= (temp - 1);
            }
        }
        if(moves->size >= moves->max){
            uint16_t new_cap = moves->max == 0 ? 16 : moves->max * 2;
            struct s_cte_move *new_arr = realloc(moves->moves, sizeof(struct s_cte_move) * new_cap);
            if(!new_arr) return e_realloc;
            moves->moves = new_arr;
            moves->max = new_cap;
        }
        moves->moves[moves->size++] = m;
    }

    return e_ok;
}

static inline uint16_t get_rank_mask(uint64_t bb){
    return (uint16_t)((bb & 0x1FFF) | ((bb >> 13) & 0x1FFF) | ((bb >> 26) & 0x1FFF) | ((bb >> 39) & 0x1FFF));
}

static inline int32_t evaluate_tablic_potential(const s_cte_pos *pos, uint8_t root_player){
    if(pos->table_bb == 0) return 0;

    uint8_t table_count = (uint8_t)__builtin_popcountll(pos->table_bb);
    if(table_count > 2) return 0;

    uint8_t cur = pos->current_player;
    uint16_t cur_ranks = get_rank_mask(pos->hand_bb[cur]);
    if(cur_ranks == 0) return 0;

    bool tablic_possible = false;

    if(table_count == 1){
        t_card tc = (t_card)__builtin_ctzll(pos->table_bb);
        uint8_t trank = (uint8_t)(tc % 13);
        if(cur_ranks & (1U << trank)){
            tablic_possible = true;
        }
    } else if(table_count == 2){
        uint64_t tbl = pos->table_bb;
        t_card tc1 = (t_card)__builtin_ctzll(tbl);
        t_card tc2 = (t_card)__builtin_ctzll(tbl & (tbl - 1));
        uint8_t r1 = (uint8_t)(tc1 % 13);
        uint8_t r2 = (uint8_t)(tc2 % 13);

        // Sum with standard nominal values: req_rank = r1 + r2 + 2
        uint8_t req_rank = (uint8_t)(r1 + r2 + 2);
        if(req_rank <= 12 && (cur_ranks & (1U << req_rank))){
            tablic_possible = true;
        }
        // If tc1 is Ace (rank 9), can count as 1: req_rank = r2 + 1
        if(!tablic_possible && r1 == 9 && (r2 + 1) <= 12 && (cur_ranks & (1U << (r2 + 1)))){
            tablic_possible = true;
        }
        // If tc2 is Ace (rank 9), can count as 1: req_rank = r1 + 1
        if(!tablic_possible && r2 == 9 && (r1 + 1) <= 12 && (cur_ranks & (1U << (r1 + 1)))){
            tablic_possible = true;
        }
    }

    if(!tablic_possible) return 0;

    int32_t bonus = (table_count == 1) ? 150 : 100;
    return is_friendly(pos, cur, root_player) ? bonus : -bonus;
}

int32_t pos_evaluate(const s_cte_pos *pos, uint8_t root_player){
    if(!pos) return 0;

    uint8_t p = root_player % pos->nb_players;

    if(pos->is_team_mode && pos->nb_players == 4){
        uint8_t my_team = (uint8_t)(p % 2);
        uint8_t opp_team = (uint8_t)(1 - my_team);

        int32_t my_pts = (pos->card_points[my_team] + pos->card_points[my_team + 2])
                       + 2 * (pos->tablic_counts[my_team] + pos->tablic_counts[my_team + 2]);
        int32_t opp_pts = (pos->card_points[opp_team] + pos->card_points[opp_team + 2])
                        + 2 * (pos->tablic_counts[opp_team] + pos->tablic_counts[opp_team + 2]);

        int32_t my_cards = pos->won_card_counts[my_team] + pos->won_card_counts[my_team + 2];
        int32_t opp_cards = pos->won_card_counts[opp_team] + pos->won_card_counts[opp_team + 2];

        // Continuous majority interpolation: smooth scaling towards the +3 pts (300 units) bonus
        int32_t majority_units = 0;
        if(my_cards >= 27){
            majority_units = +300;
        } else if(opp_cards >= 27){
            majority_units = -300;
        } else {
            int32_t card_adv = my_cards - opp_cards;
            majority_units = (card_adv * 300) / 27;
        }

        int32_t score = (my_pts - opp_pts) * 100 + majority_units + (my_cards - opp_cards) * 5;

        // Last captor bonus: fractional tie-breaker credit for table points and cards
        if(pos->last_captor >= 0 && pos->table_bb > 0){
            uint8_t table_pts = 0;
            uint8_t table_cards = 0;
            uint64_t tbl = pos->table_bb;
            while(tbl > 0){
                table_pts += get_points((t_card)__builtin_ctzll(tbl));
                table_cards++;
                tbl &= (tbl - 1);
            }
            int32_t lc_bonus = (int32_t)table_pts * 20 + (int32_t)table_cards * 1;
            if(is_friendly(pos, (uint8_t)pos->last_captor, p)){
                score += lc_bonus;
            } else {
                score -= lc_bonus;
            }
        }

        // Hand capture quality: bonus for having cards matching table ranks
        if(pos->table_bb > 0){
            uint16_t tbl_ranks = get_rank_mask(pos->table_bb);
            uint16_t my_ranks = get_rank_mask(pos->hand_bb[my_team] | pos->hand_bb[my_team + 2]);
            uint16_t opp_ranks = get_rank_mask(pos->hand_bb[opp_team] | pos->hand_bb[opp_team + 2]);
            int32_t my_match = __builtin_popcount(my_ranks & tbl_ranks);
            int32_t opp_match = __builtin_popcount(opp_ranks & tbl_ranks);
            score += (my_match - opp_match) * 15;
        }

        // Tablic potential: bonus/penalty if immediate table clearance is reachable
        score += evaluate_tablic_potential(pos, root_player);

        return score;
    } else {
        int32_t my_pts = pos->card_points[p] + 2 * pos->tablic_counts[p];
        int32_t my_cards = pos->won_card_counts[p];

        int32_t max_opp_pts = 0;
        int32_t max_opp_cards = 0;
        for(uint8_t i = 0; i < pos->nb_players; i++){
            if(i == p) continue;
            int32_t o_pts = pos->card_points[i] + 2 * pos->tablic_counts[i];
            if(o_pts > max_opp_pts) max_opp_pts = o_pts;
            if(pos->won_card_counts[i] > max_opp_cards) max_opp_cards = pos->won_card_counts[i];
        }

        // Continuous majority interpolation: smooth scaling towards the +3 pts (300 units) bonus
        int32_t majority_units = 0;
        if(my_cards >= 27){
            majority_units = +300;
        } else if(max_opp_cards >= 27){
            majority_units = -300;
        } else {
            int32_t card_adv = my_cards - max_opp_cards;
            majority_units = (card_adv * 300) / 27;
        }

        int32_t score = (my_pts - max_opp_pts) * 100 + majority_units + (my_cards - max_opp_cards) * 5;

        // Last captor bonus: fractional tie-breaker credit for table points and cards
        if(pos->last_captor >= 0 && pos->table_bb > 0){
            uint8_t table_pts = 0;
            uint8_t table_cards = 0;
            uint64_t tbl = pos->table_bb;
            while(tbl > 0){
                table_pts += get_points((t_card)__builtin_ctzll(tbl));
                table_cards++;
                tbl &= (tbl - 1);
            }
            int32_t lc_bonus = (int32_t)table_pts * 20 + (int32_t)table_cards * 1;
            if((uint8_t)pos->last_captor == p){
                score += lc_bonus;
            } else {
                score -= lc_bonus;
            }
        }

        // Hand capture quality: bonus for having cards matching table ranks
        if(pos->table_bb > 0){
            uint16_t tbl_ranks = get_rank_mask(pos->table_bb);
            uint16_t my_ranks = get_rank_mask(pos->hand_bb[p]);
            int32_t my_match = __builtin_popcount(my_ranks & tbl_ranks);
            int32_t max_opp_match = 0;
            for(uint8_t i = 0; i < pos->nb_players; i++){
                if(i == p) continue;
                uint16_t o_ranks = get_rank_mask(pos->hand_bb[i]);
                int32_t o_match = __builtin_popcount(o_ranks & tbl_ranks);
                if(o_match > max_opp_match) max_opp_match = o_match;
            }
            score += (my_match - max_opp_match) * 15;
        }

        // Tablic potential: bonus/penalty if immediate table clearance is reachable
        score += evaluate_tablic_potential(pos, root_player);

        return score;
    }
}

// Frozen baseline evaluation (pre-optimization snapshot) for A/B benchmarking.
// This is the original pos_evaluate without any of the optimization patches.
int32_t pos_evaluate_v0(const s_cte_pos *pos, uint8_t root_player){
    if(!pos) return 0;
    uint8_t p = root_player % pos->nb_players;

    if(pos->is_team_mode && pos->nb_players == 4){
        uint8_t my_team = (uint8_t)(p % 2);
        uint8_t opp_team = (uint8_t)(1 - my_team);
        int32_t my_pts = (pos->card_points[my_team] + pos->card_points[my_team + 2])
                       + 2 * (pos->tablic_counts[my_team] + pos->tablic_counts[my_team + 2]);
        int32_t opp_pts = (pos->card_points[opp_team] + pos->card_points[opp_team + 2])
                        + 2 * (pos->tablic_counts[opp_team] + pos->tablic_counts[opp_team + 2]);
        int32_t my_cards = pos->won_card_counts[my_team] + pos->won_card_counts[my_team + 2];
        int32_t opp_cards = pos->won_card_counts[opp_team] + pos->won_card_counts[opp_team + 2];
        int32_t majority_bonus = 0;
        if(my_cards >= 27) majority_bonus = +3;
        else if(opp_cards >= 27) majority_bonus = -3;
        return (my_pts - opp_pts + majority_bonus) * 100 + (my_cards - opp_cards) * 5;
    } else {
        int32_t my_pts = pos->card_points[p] + 2 * pos->tablic_counts[p];
        int32_t my_cards = pos->won_card_counts[p];
        int32_t max_opp_pts = 0;
        int32_t max_opp_cards = 0;
        for(uint8_t i = 0; i < pos->nb_players; i++){
            if(i == p) continue;
            int32_t o_pts = pos->card_points[i] + 2 * pos->tablic_counts[i];
            if(o_pts > max_opp_pts) max_opp_pts = o_pts;
            if(pos->won_card_counts[i] > max_opp_cards) max_opp_cards = pos->won_card_counts[i];
        }
        int32_t majority_bonus = 0;
        if(my_cards >= 27) majority_bonus = +3;
        else if(max_opp_cards >= 27) majority_bonus = -3;
        return (my_pts - max_opp_pts + majority_bonus) * 100 + (my_cards - max_opp_cards) * 5;
    }
}

static bool is_friendly(const s_cte_pos *pos, uint8_t player_id, uint8_t root_player){
    if(player_id == root_player) return true;
    if(pos->is_team_mode && pos->nb_players == 4){
        return (player_id % 2) == (root_player % 2);
    }
    return false;
}

static inline int16_t score_compact_move(const s_cte_bitboard_move *m, uint64_t table_bb){
    if(m->capture_mask == 0){
        // Drop move: penalize dropping valuable cards
        return (int16_t)(-(get_points(m->card_played) * 100));
    }
    int16_t score = 0;
    if(table_bb > 0 && m->capture_mask == table_bb){
        score += 10000; // Immediate Tablic bonus
    }
    uint8_t pts = get_points(m->card_played);
    uint64_t temp = m->capture_mask;
    uint8_t count = 0;
    while(temp > 0){
        int bit = __builtin_ctzll(temp);
        pts += get_points((t_card)bit);
        count++;
        temp &= (temp - 1);
    }
    score += (int16_t)(pts * 100 + count * 10);
    return score;
}

static void order_compact_moves(s_cte_bitboard_move_list *cpt, uint64_t table_bb){
    if(cpt->size <= 1) return;
    int16_t scores[1024];
    for(uint16_t i = 0; i < cpt->size; i++){
        scores[i] = score_compact_move(&cpt->moves[i], table_bb);
    }
    for(uint16_t i = 1; i < cpt->size; i++){
        s_cte_bitboard_move key_m = cpt->moves[i];
        int16_t key_s = scores[i];
        int j = (int)i - 1;
        while(j >= 0 && scores[j] < key_s){
            cpt->moves[j + 1] = cpt->moves[j];
            scores[j + 1] = scores[j];
            j--;
        }
        cpt->moves[j + 1] = key_m;
        scores[j + 1] = key_s;
    }
}

int32_t compute_upper_bound(const s_cte_pos *pos, uint8_t root_player, uint8_t depth, e_cte_ubp_model model){
    if(!pos || model == UBP_NONE) return +INF_SCORE;

    uint8_t p = root_player % pos->nb_players;
    int32_t cur_my_pts, cur_my_cards, cur_my_tablics;
    int32_t max_opp_pts = 0, max_opp_cards = 0;
    int32_t total_won_pts = 0, total_won_cards = 0;

    if(pos->is_team_mode && pos->nb_players == 4){
        uint8_t my_team = (uint8_t)(p % 2);
        uint8_t opp_team = (uint8_t)(1 - my_team);

        cur_my_pts = pos->card_points[my_team] + pos->card_points[my_team + 2];
        cur_my_tablics = pos->tablic_counts[my_team] + pos->tablic_counts[my_team + 2];
        cur_my_cards = pos->won_card_counts[my_team] + pos->won_card_counts[my_team + 2];

        int32_t opp_pts = pos->card_points[opp_team] + pos->card_points[opp_team + 2];
        int32_t opp_tablics = pos->tablic_counts[opp_team] + pos->tablic_counts[opp_team + 2];
        max_opp_pts = opp_pts + 2 * opp_tablics;
        max_opp_cards = pos->won_card_counts[opp_team] + pos->won_card_counts[opp_team + 2];

        total_won_pts = cur_my_pts + opp_pts;
        total_won_cards = cur_my_cards + max_opp_cards;
    } else {
        cur_my_pts = pos->card_points[p];
        cur_my_tablics = pos->tablic_counts[p];
        cur_my_cards = pos->won_card_counts[p];
        total_won_pts = cur_my_pts;
        total_won_cards = cur_my_cards;

        for(uint8_t i = 0; i < pos->nb_players; i++){
            if(i == p) continue;
            int32_t o_pts = pos->card_points[i] + 2 * pos->tablic_counts[i];
            if(o_pts > max_opp_pts) max_opp_pts = o_pts;
            if(pos->won_card_counts[i] > max_opp_cards) max_opp_cards = pos->won_card_counts[i];
            total_won_pts += pos->card_points[i];
            total_won_cards += pos->won_card_counts[i];
        }
    }

    int32_t rem_pts = 22 - total_won_pts;
    if(rem_pts < 0) rem_pts = 0;
    int32_t rem_cards = 52 - total_won_cards;
    if(rem_cards < 0) rem_cards = 0;

    int32_t my_base_pts = cur_my_pts + 2 * cur_my_tablics;
    int32_t my_max_pts;
    int32_t max_majority = 0;
    int32_t est_cards_diff;

    if(model == UBP_TIGHT_HEURISTIC){
        // Empirical capture estimation: points in hand + table + 50% of opp hands
        uint8_t hand_pts = 0;
        uint64_t my_h = pos->hand_bb[p];
        if(pos->is_team_mode && pos->nb_players == 4) my_h |= pos->hand_bb[p + 2];
        while(my_h > 0){
            int b = __builtin_ctzll(my_h);
            hand_pts += get_points((t_card)b);
            my_h &= (my_h - 1);
        }
        uint8_t tbl_pts = 0;
        uint64_t tbl = pos->table_bb;
        while(tbl > 0){
            int b = __builtin_ctzll(tbl);
            tbl_pts += get_points((t_card)b);
            tbl &= (tbl - 1);
        }
        int32_t opp_hand_pts = rem_pts - (hand_pts + tbl_pts);
        if(opp_hand_pts < 0) opp_hand_pts = 0;
        int32_t capturable_pts = hand_pts + tbl_pts + (opp_hand_pts / 2);

        if(cur_my_cards + (rem_cards / 2) >= 27) max_majority = +3;
        else if(max_opp_cards >= 27) max_majority = -3;

        my_max_pts = my_base_pts + capturable_pts + max_majority;
        est_cards_diff = (cur_my_cards + rem_cards / 2) - max_opp_cards;
    } else {
        // Strict admissible or no-tablic bound
        int32_t max_tablic_pts = 0;
        if(model == UBP_STRICT_ADMISSIBLE){
            uint8_t my_turns = (depth + 1) / pos->nb_players;
            uint8_t my_hand = pos->hand_counts[p];
            if(pos->is_team_mode && pos->nb_players == 4) my_hand += pos->hand_counts[p + 2];
            uint8_t max_tabs = my_turns < my_hand ? my_turns : my_hand;
            max_tablic_pts = 2 * max_tabs;
        }

        if(cur_my_cards + rem_cards >= 27) max_majority = +3;
        else if(max_opp_cards >= 27) max_majority = -3;

        my_max_pts = my_base_pts + rem_pts + max_tablic_pts + max_majority;
        est_cards_diff = (cur_my_cards + rem_cards) - max_opp_cards;
    }

    return (my_max_pts - max_opp_pts) * 100 + est_cards_diff * 5;
}

int32_t compute_lower_bound(const s_cte_pos *pos, uint8_t root_player, uint8_t depth, e_cte_ubp_model model){
    if(!pos || model == UBP_NONE) return -INF_SCORE;

    uint8_t p = root_player % pos->nb_players;
    int32_t cur_my_pts, cur_my_cards, cur_my_tablics;
    int32_t max_opp_pts = 0, max_opp_cards = 0;
    int32_t total_won_pts = 0, total_won_cards = 0;

    if(pos->is_team_mode && pos->nb_players == 4){
        uint8_t my_team = (uint8_t)(p % 2);
        uint8_t opp_team = (uint8_t)(1 - my_team);

        cur_my_pts = pos->card_points[my_team] + pos->card_points[my_team + 2];
        cur_my_tablics = pos->tablic_counts[my_team] + pos->tablic_counts[my_team + 2];
        cur_my_cards = pos->won_card_counts[my_team] + pos->won_card_counts[my_team + 2];

        int32_t opp_pts = pos->card_points[opp_team] + pos->card_points[opp_team + 2];
        int32_t opp_tablics = pos->tablic_counts[opp_team] + pos->tablic_counts[opp_team + 2];
        max_opp_pts = opp_pts + 2 * opp_tablics;
        max_opp_cards = pos->won_card_counts[opp_team] + pos->won_card_counts[opp_team + 2];

        total_won_pts = cur_my_pts + opp_pts;
        total_won_cards = cur_my_cards + max_opp_cards;
    } else {
        cur_my_pts = pos->card_points[p];
        cur_my_tablics = pos->tablic_counts[p];
        cur_my_cards = pos->won_card_counts[p];
        total_won_pts = cur_my_pts;
        total_won_cards = cur_my_cards;

        for(uint8_t i = 0; i < pos->nb_players; i++){
            if(i == p) continue;
            int32_t o_pts = pos->card_points[i] + 2 * pos->tablic_counts[i];
            if(o_pts > max_opp_pts) max_opp_pts = o_pts;
            if(pos->won_card_counts[i] > max_opp_cards) max_opp_cards = pos->won_card_counts[i];
            total_won_pts += pos->card_points[i];
            total_won_cards += pos->won_card_counts[i];
        }
    }

    int32_t rem_pts = 22 - total_won_pts;
    if(rem_pts < 0) rem_pts = 0;
    int32_t rem_cards = 52 - total_won_cards;
    if(rem_cards < 0) rem_cards = 0;

    int32_t my_base_pts = cur_my_pts + 2 * cur_my_tablics;
    int32_t min_majority = 0;
    if(cur_my_cards >= 27) min_majority = +3;
    else if(max_opp_cards + rem_cards >= 27) min_majority = -3;

    int32_t opp_max_tablic_pts = 0;
    if(model == UBP_STRICT_ADMISSIBLE){
        uint8_t opp_turns = depth;
        opp_max_tablic_pts = 2 * opp_turns;
    }

    int32_t opp_max_pts = max_opp_pts + rem_pts + opp_max_tablic_pts;
    int32_t min_cards_diff = cur_my_cards - (max_opp_cards + rem_cards);

    return (my_base_pts - opp_max_pts + min_majority) * 100 + min_cards_diff * 5;
}

typedef int32_t (*t_eval_fn)(const s_cte_pos*, uint8_t);

static int32_t alphabeta_search(const s_cte_pos *pos,
                               uint8_t depth,
                               int32_t alpha,
                               int32_t beta,
                               uint8_t root_player,
                               e_cte_ubp_model ubp_model,
                               uint64_t *node_counter,
                               uint64_t *ubp_cutoff_counter,
                               t_eval_fn eval_fn)
{
    if(node_counter) (*node_counter)++;

    if(depth == 0){
        return eval_fn(pos, root_player);
    }

    bool all_empty = true;
    for(uint8_t i = 0; i < pos->nb_players; i++){
        if(pos->hand_counts[i] > 0){
            all_empty = false;
            break;
        }
    }
    if(all_empty){
        // Endgame terminal: award remaining table cards to last captor
        if(pos->table_bb > 0 && pos->last_captor >= 0 && pos->last_captor < pos->nb_players){
            s_cte_pos terminal = *pos;
            uint8_t captor = (uint8_t)terminal.last_captor;
            uint64_t temp = terminal.table_bb;
            uint8_t extra_pts = 0;
            uint8_t extra_cards = 0;
            while(temp > 0){
                int bit = __builtin_ctzll(temp);
                extra_pts += get_points((t_card)bit);
                extra_cards++;
                temp &= (temp - 1);
            }
            terminal.card_points[captor] += extra_pts;
            terminal.won_card_counts[captor] += extra_cards;
            terminal.table_bb = 0;
            return eval_fn(&terminal, root_player);
        }
        return eval_fn(pos, root_player);
    }

    if(pos->hand_counts[pos->current_player] == 0){
        s_cte_pos next_pos = *pos;
        next_pos.current_player = (uint8_t)((pos->current_player + 1) % pos->nb_players);
        return alphabeta_search(&next_pos, depth, alpha, beta, root_player, ubp_model, node_counter, ubp_cutoff_counter, eval_fn);
    }

    bool maximizing = is_friendly(pos, pos->current_player, root_player);

    if(ubp_model != UBP_NONE && depth > 0){
        if(maximizing){
            int32_t ub = compute_upper_bound(pos, root_player, depth, ubp_model);
            if(ub <= alpha){
                if(ubp_cutoff_counter) (*ubp_cutoff_counter)++;
                return ub;
            }
        } else {
            int32_t lb = compute_lower_bound(pos, root_player, depth, ubp_model);
            if(lb >= beta){
                if(ubp_cutoff_counter) (*ubp_cutoff_counter)++;
                return lb;
            }
        }
    }

    struct s_cte_hand cur_hand;
    cur_hand.size = 0;
    uint64_t h_temp = pos->hand_bb[pos->current_player];
    while(h_temp > 0){
        cur_hand.array[cur_hand.size++] = (t_card)__builtin_ctzll(h_temp);
        h_temp &= (h_temp - 1);
    }

    s_cte_bitboard_move_list cpt;
    bitboard_gen_all_compact_moves_rank(&cpt, pos->table_bb, &cur_hand);
    if(cpt.size == 0){
        return pos_evaluate(pos, root_player);
    }

    // Move ordering: evaluate tactical captures before drops
    order_compact_moves(&cpt, pos->table_bb);

    if(maximizing){
        int32_t max_eval = -INF_SCORE;
        for(uint16_t i = 0; i < cpt.size; i++){
            s_cte_pos next_pos = pos_apply_bitboard_move(pos, cpt.moves[i].card_played, cpt.moves[i].capture_mask);
            int32_t eval = alphabeta_search(&next_pos, depth - 1, alpha, beta, root_player, ubp_model, node_counter, ubp_cutoff_counter, eval_fn);
            if(eval > max_eval) max_eval = eval;
            if(eval > alpha) alpha = eval;
            if(beta <= alpha) break; // Beta cutoff
        }
        return max_eval;
    } else {
        int32_t min_eval = +INF_SCORE;
        for(uint16_t i = 0; i < cpt.size; i++){
            s_cte_pos next_pos = pos_apply_bitboard_move(pos, cpt.moves[i].card_played, cpt.moves[i].capture_mask);
            int32_t eval = alphabeta_search(&next_pos, depth - 1, alpha, beta, root_player, ubp_model, node_counter, ubp_cutoff_counter, eval_fn);
            if(eval < min_eval) min_eval = eval;
            if(eval < beta) beta = eval;
            if(beta <= alpha) break; // Alpha cutoff
        }
        return min_eval;
    }
}

static inline int16_t score_root_move(const struct s_cte_move *m, uint64_t table_bb){
    if(m->cards_picked.size == 0){
        return (int16_t)(-(get_points(m->card_played) * 100));
    }
    int16_t score = 0;
    uint64_t mask = 0;
    uint8_t pts = get_points(m->card_played);
    for(uint8_t i = 0; i < m->cards_picked.size; i++){
        t_card c = m->cards_picked.array[i];
        pts += get_points(c);
        if(c < 52) mask |= (1ULL << c);
    }
    if(table_bb > 0 && mask == table_bb){
        score += 10000;
    }
    score += (int16_t)(pts * 100 + m->cards_picked.size * 10);
    return score;
}

static inline uint64_t get_time_us(void){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static void sort_candidates_by_score(s_cte_root_candidate *cands, uint16_t count){
    if(count <= 1) return;
    for(uint16_t i = 1; i < count; i++){
        s_cte_root_candidate key = cands[i];
        int j = (int)i - 1;
        while(j >= 0 && cands[j].score < key.score){
            cands[j + 1] = cands[j];
            j--;
        }
        cands[j + 1] = key;
    }
}

uint16_t search_best_move(const s_cte_pos *pos,
                          const struct s_cte_move_list *moves,
                          s_cte_search_config *config)
{
    if(!moves || moves->size == 0 || !pos) return 0;
    if(moves->size == 1) return 0;

    uint8_t target_depth = (config && config->max_depth > 0) ? config->max_depth : 2;
    uint32_t timeout_ms = config ? config->timeout_ms : 0;
    uint8_t root_player = pos->current_player;
    e_cte_ubp_model ubp_model = config ? config->ubp_model : UBP_STRICT_ADMISSIBLE;
    uint64_t *node_counter = (config != NULL) ? &config->nodes_visited : NULL;
    uint64_t *ubp_cutoff_counter = (config != NULL) ? &config->ubp_cutoffs : NULL;

    uint16_t num_candidates = moves->size < CTE_MAX_ROOT_CANDIDATES ? moves->size : CTE_MAX_ROOT_CANDIDATES;
    s_cte_root_candidate cands[CTE_MAX_ROOT_CANDIDATES];

    for(uint16_t i = 0; i < num_candidates; i++){
        cands[i].move_idx = i;
        cands[i].score = score_root_move(&moves->moves[i], pos->table_bb);
        cands[i].depth_completed = 0;
        cands[i].nodes_spent = 0;
        cands[i].refuted = false;
    }
    // Initial heuristic ordering so best tactical captures lead depth 1
    sort_candidates_by_score(cands, num_candidates);

    uint16_t best_move_idx = cands[0].move_idx;
    uint8_t depth_reached = 0;

    uint64_t t_start_us = (timeout_ms > 0) ? get_time_us() : 0;
    uint64_t time_limit_us = (uint64_t)timeout_ms * 1000ULL;

    for(uint8_t d = 1; d <= target_depth; d++){
        if(timeout_ms > 0 && d > 1){
            uint64_t elapsed_us = get_time_us() - t_start_us;
            if(elapsed_us >= time_limit_us){
                break;
            }
        }

        int32_t alpha = -INF_SCORE;
        int32_t beta = +INF_SCORE;
        int32_t d_best_score = -INF_SCORE;
        uint16_t d_best_move_idx = cands[0].move_idx;
        bool interrupted = false;

        for(uint16_t i = 0; i < num_candidates; i++){
            cands[i].refuted = false;
        }

        for(uint16_t i = 0; i < num_candidates; i++){
            if(cands[i].refuted) continue;

            if(timeout_ms > 0 && (d > 1 || i > 0)){
                uint64_t elapsed_us = get_time_us() - t_start_us;
                if(elapsed_us >= time_limit_us){
                    interrupted = true;
                    break;
                }
            }

            s_cte_pos next_pos = pos_apply_move(pos, &moves->moves[cands[i].move_idx]);

            // Root UBP check: if branch upper bound cannot beat current alpha, mark refuted
            if(ubp_model != UBP_NONE && d > 1 && alpha > -INF_SCORE){
                int32_t ub = compute_upper_bound(&next_pos, root_player, d - 1, ubp_model);
                if(ub <= alpha){
                    cands[i].refuted = true;
                    if(ubp_cutoff_counter) (*ubp_cutoff_counter)++;
                    continue;
                }
            }

            uint64_t nodes_before = node_counter ? *node_counter : 0;
            t_eval_fn eval_fn = (config && config->eval_fn) ? config->eval_fn : pos_evaluate;
            int32_t score = alphabeta_search(&next_pos, d - 1, alpha, beta, root_player, ubp_model, node_counter, ubp_cutoff_counter, eval_fn);
            uint64_t nodes_after = node_counter ? *node_counter : 0;
            if(node_counter) cands[i].nodes_spent += (nodes_after - nodes_before);

            cands[i].score = score;
            cands[i].depth_completed = d;

            if(score > d_best_score){
                d_best_score = score;
                d_best_move_idx = cands[i].move_idx;
            }
            if(score > alpha){
                alpha = score;
            }
        }

        if(!interrupted){
            best_move_idx = d_best_move_idx;
            depth_reached = d;
            // Order candidates by depth d score: principal variation is tried first at depth d+1!
            sort_candidates_by_score(cands, num_candidates);
        } else {
            // Partial depth interrupted by timeout: discard and retain best_move_idx from completed depth
            break;
        }
    }

    if(config != NULL){
        config->depth_reached = depth_reached;
        config->num_candidates = num_candidates;
        for(uint16_t i = 0; i < num_candidates; i++){
            config->candidates[i] = cands[i];
        }
    }

    return best_move_idx;
}

