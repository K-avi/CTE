#include "minmax.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define INF_SCORE 10000000

s_cte_pos pos_from_state(const s_cte_game_state *state){
    s_cte_pos res;
    memset(&res, 0, sizeof(res));
    if(!state) return res;

    res.table_count = (state->table->nb_cards_on_table < 20) ? state->table->nb_cards_on_table : 20;
    for(uint8_t i = 0; i < res.table_count; i++){
        res.table[i] = state->table->cards_on_table[i];
    }

    res.nb_players = (state->players && state->players->size <= 4) ? state->players->size : 2;
    res.current_player = state->current_player_id;
    res.last_captor = -1;
    res.is_team_mode = (res.nb_players == 4);

    for(uint8_t p = 0; p < res.nb_players; p++){
        const struct s_cte_player_data *pl = &state->players->players[p];
        res.hand_counts[p] = (pl->hand.size < 6) ? pl->hand.size : 6;
        for(uint8_t i = 0; i < res.hand_counts[p]; i++){
            res.hands[p][i] = pl->hand.array[i];
        }
        res.won_card_counts[p] = pl->won_cards.size;
        res.tablic_counts[p] = pl->nb_tablic;
        
        uint8_t pts = 0;
        for(uint8_t j = 0; j < pl->won_cards.size; j++){
            pts += get_points(pl->won_cards.array[j]);
        }
        res.card_points[p] = pts;
    }

    return res;
}

s_cte_pos pos_apply_move(const s_cte_pos *pos, const struct s_cte_move *move){
    s_cte_pos next;
    memset(&next, 0, sizeof(next));
    if(!pos || !move) return next;
    next = *pos;

    uint8_t p = pos->current_player;

    // 1. Remove card_played from player hand in snapshot
    int hand_idx = -1;
    for(uint8_t i = 0; i < next.hand_counts[p]; i++){
        if(next.hands[p][i] == move->card_played){
            hand_idx = i;
            break;
        }
    }
    if(hand_idx != -1){
        for(uint8_t i = (uint8_t)hand_idx; i + 1 < next.hand_counts[p]; i++){
            next.hands[p][i] = next.hands[p][i + 1];
        }
        next.hand_counts[p]--;
    }

    // 2. Drop move
    if(move->cards_picked.size == 0){
        if(next.table_count < 20){
            next.table[next.table_count++] = move->card_played;
        }
        next.current_player = (uint8_t)((p + 1) % pos->nb_players);
        return next;
    }

    // 3. Capture move
    s_cte_move_score score = score_move(move, pos->table_count);
    next.card_points[p] += score.card_points;
    next.won_card_counts[p] += score.nb_cards;
    if(score.is_tablic){
        next.tablic_counts[p]++;
    }
    next.last_captor = (int8_t)p;

    // Remove picked cards from table
    for(uint8_t k = 0; k < move->cards_picked.size; k++){
        t_card target = move->cards_picked.array[k];
        int tbl_idx = -1;
        for(uint8_t j = 0; j < next.table_count; j++){
            if(next.table[j] == target){
                tbl_idx = j;
                break;
            }
        }
        if(tbl_idx != -1){
            for(uint8_t j = (uint8_t)tbl_idx; j + 1 < next.table_count; j++){
                next.table[j] = next.table[j + 1];
            }
            next.table_count--;
        }
    }

    next.current_player = (uint8_t)((p + 1) % pos->nb_players);
    return next;
}

t_cteerr pos_gen_moves(struct s_cte_move_list *moves, const s_cte_pos *pos){
    if(!moves || !pos) return e_null;

    struct table tbl;
    tbl.nb_cards_on_table = pos->table_count;
    for(uint8_t i = 0; i < pos->table_count; i++){
        tbl.cards_on_table[i] = pos->table[i];
    }

    struct s_cte_hand cur_hand;
    cur_hand.size = pos->hand_counts[pos->current_player];
    for(uint8_t i = 0; i < cur_hand.size; i++){
        cur_hand.array[i] = pos->hands[pos->current_player][i];
    }

    return gen_all_moves(moves, &tbl, &cur_hand);
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

    struct s_cte_move_list moves;
    t_cteerr err = init_move_list(&moves, 16);
    if(err != e_ok) return pos_evaluate(pos, root_player);

    err = pos_gen_moves(&moves, pos);
    if(err != e_ok || moves.size == 0){
        free_move_list(&moves);
        return pos_evaluate(pos, root_player);
    }

    bool maximizing = is_friendly(pos, pos->current_player, root_player);

    if(maximizing){
        int32_t max_eval = -INF_SCORE;
        for(uint16_t i = 0; i < moves.size; i++){
            s_cte_pos next_pos = pos_apply_move(pos, &moves.moves[i]);
            int32_t eval = alphabeta_search(&next_pos, depth - 1, alpha, beta, root_player);
            if(eval > max_eval) max_eval = eval;
            if(eval > alpha) alpha = eval;
            if(beta <= alpha) break; // Beta cutoff
        }
        free_move_list(&moves);
        return max_eval;
    } else {
        int32_t min_eval = +INF_SCORE;
        for(uint16_t i = 0; i < moves.size; i++){
            s_cte_pos next_pos = pos_apply_move(pos, &moves.moves[i]);
            int32_t eval = alphabeta_search(&next_pos, depth - 1, alpha, beta, root_player);
            if(eval < min_eval) min_eval = eval;
            if(eval < beta) beta = eval;
            if(beta <= alpha) break; // Alpha cutoff
        }
        free_move_list(&moves);
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
