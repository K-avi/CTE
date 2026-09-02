#include "minmax.h"
#include "backend_bitboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define INF_SCORE 10000000

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

        // Estimated majority bonus
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

static int32_t alphabeta_search(const s_cte_pos *pos,
                               uint8_t depth,
                               int32_t alpha,
                               int32_t beta,
                               uint8_t root_player)
{
    if(depth == 0){
        return pos_evaluate(pos, root_player);
    }

    bool all_empty = true;
    for(uint8_t i = 0; i < pos->nb_players; i++){
        if(pos->hand_counts[i] > 0){
            all_empty = false;
            break;
        }
    }
    if(all_empty){
        return pos_evaluate(pos, root_player);
    }

    if(pos->hand_counts[pos->current_player] == 0){
        s_cte_pos next_pos = *pos;
        next_pos.current_player = (uint8_t)((pos->current_player + 1) % pos->nb_players);
        return alphabeta_search(&next_pos, depth, alpha, beta, root_player);
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

    bool maximizing = is_friendly(pos, pos->current_player, root_player);

    if(maximizing){
        int32_t max_eval = -INF_SCORE;
        for(uint16_t i = 0; i < cpt.size; i++){
            s_cte_pos next_pos = pos_apply_bitboard_move(pos, cpt.moves[i].card_played, cpt.moves[i].capture_mask);
            int32_t eval = alphabeta_search(&next_pos, depth - 1, alpha, beta, root_player);
            if(eval > max_eval) max_eval = eval;
            if(eval > alpha) alpha = eval;
            if(beta <= alpha) break; // Beta cutoff
        }
        return max_eval;
    } else {
        int32_t min_eval = +INF_SCORE;
        for(uint16_t i = 0; i < cpt.size; i++){
            s_cte_pos next_pos = pos_apply_bitboard_move(pos, cpt.moves[i].card_played, cpt.moves[i].capture_mask);
            int32_t eval = alphabeta_search(&next_pos, depth - 1, alpha, beta, root_player);
            if(eval < min_eval) min_eval = eval;
            if(eval < beta) beta = eval;
            if(beta <= alpha) break; // Alpha cutoff
        }
        return min_eval;
    }
}

uint16_t search_best_move(const s_cte_pos *pos,
                          const struct s_cte_move_list *moves,
                          const s_cte_search_config *config)
{
    if(!moves || moves->size <= 1 || !pos) return 0;

    uint8_t depth = (config && config->max_depth > 0) ? config->max_depth : 2;
    uint8_t root_player = pos->current_player;

    uint16_t best_move_idx = 0;
    int32_t best_score = -INF_SCORE;
    int32_t alpha = -INF_SCORE;
    int32_t beta = +INF_SCORE;

    for(uint16_t i = 0; i < moves->size; i++){
        s_cte_pos next_pos = pos_apply_move(pos, &moves->moves[i]);
        int32_t score = alphabeta_search(&next_pos, depth - 1, alpha, beta, root_player);

        if(score > best_score){
            best_score = score;
            best_move_idx = i;
        }
        if(score > alpha){
            alpha = score;
        }
    }

    return best_move_idx;
}
