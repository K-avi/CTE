#include "eval.h"
#include "minmax.h"
#include <stdlib.h>

// Pure uniform-random move evaluator (zero I/O)
uint16_t eval_random(const s_cte_game_state *state,
                     const struct s_cte_move_list *moves,
                     void *ctx)
{
    (void)state;
    (void)ctx;
    if(!moves || moves->size == 0) return 0;
    return (uint16_t)(rand() % moves->size);
}

// Dumb move evaluator : always tries to drop a card if possible, else random move
uint16_t eval_dumb(const s_cte_game_state *state,
                   const struct s_cte_move_list *moves,
                   void *ctx)
{
    (void)state;
    (void)ctx;
    if(!moves || moves->size == 0) return 0;

    // Look for a drop move (cards_picked.size == 0)
    for(uint16_t i = 0; i < moves->size; i++){
        if(moves->moves[i].cards_picked.size == 0){
            return i;
        }
    }
    return 0;
}

// Tries to get max points & pickup max amount of cards while doing so
uint16_t eval_greedy(const s_cte_game_state *state,
                     const struct s_cte_move_list *moves,
                     void *ctx)
{
    (void)ctx;
    if(!moves || moves->size == 0) return 0;

    uint16_t cur_best = 0; 
    s_cte_move_score best_score = score_move(&moves->moves[0], state->table_bb);

    for(uint16_t i = 1; i < moves->size; i++){
        s_cte_move_score current_score = score_move(&moves->moves[i], state->table_bb);
        if(current_score.total_points > best_score.total_points){
            cur_best = i;
            best_score = current_score;
        } else if(current_score.total_points == best_score.total_points){
            if(current_score.nb_cards > best_score.nb_cards){
                cur_best = i;
                best_score = current_score;
            }
        }
    }
    return cur_best;
}

// Minimax / Alpha-Beta lookahead (2-ply default or deeper with ctx)
uint16_t eval_cheater(const s_cte_game_state *state,
                      const struct s_cte_move_list *moves,
                      void *ctx)
{
    if(!moves || moves->size == 0 || !state) return 0;

    s_cte_pos pos = pos_from_state(state);
    s_cte_search_config cfg = {
        .max_depth = 2,
        .timeout_ms = 0
    };
    if(ctx != NULL){
        cfg = *(const s_cte_search_config *)ctx;
    }

    return search_best_move(&pos, moves, &cfg);
}

t_evaluator cte_get_evaluator(e_cte_ai_type type, const char **name_out){
    switch(type){
        case AI_TYPE_DUMB:
            if(name_out) *name_out = "Dumb";
            return eval_dumb;
        case AI_TYPE_GREEDY:
            if(name_out) *name_out = "Greedy";
            return eval_greedy;
        case AI_TYPE_CHEATER:
            if(name_out) *name_out = "Cheater";
            return eval_cheater;
        case AI_TYPE_RANDOM:
        default:
            if(name_out) *name_out = "Random";
            return eval_random;
    }
}

