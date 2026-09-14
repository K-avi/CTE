#include "eval.h"
#include "minmax.h"
#include "pimc.h"
#include "ismcts.h"
#include <stdlib.h>
#include <string.h>

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
        .timeout_ms = 0,
        .ubp_model = UBP_STRICT_ADMISSIBLE,
        .nodes_visited = 0,
        .ubp_cutoffs = 0
    };
    if(ctx != NULL){
        cfg = *(const s_cte_search_config *)ctx;
        cfg.nodes_visited = 0;
        cfg.ubp_cutoffs = 0;
    }

    uint16_t res = search_best_move(&pos, moves, &cfg);
    if(ctx != NULL){
        s_cte_search_config *out_cfg = (s_cte_search_config *)ctx;
        out_cfg->nodes_visited += cfg.nodes_visited;
        out_cfg->ubp_cutoffs += cfg.ubp_cutoffs;
        out_cfg->depth_reached = cfg.depth_reached;
        out_cfg->num_candidates = cfg.num_candidates;
        memcpy(out_cfg->candidates, cfg.candidates, sizeof(cfg.candidates));
    }
    return res;
}

// Deep Omniscient Oracle evaluator (multi-deal lookahead + Deal 4 exact resolution)
uint16_t eval_oracle(const s_cte_game_state *state,
                     const struct s_cte_move_list *moves,
                     void *ctx)
{
    s_cte_search_config cfg = {
        .max_depth     = 6,
        .timeout_ms    = 0,
        .ubp_model     = UBP_NO_TABLIC,
        .multi_deal    = true,
        .solve_deal4   = true,
        .nodes_visited = 0,
        .ubp_cutoffs   = 0
    };
    if(ctx != NULL){
        cfg = *(const s_cte_search_config *)ctx;
        cfg.multi_deal = true;
        cfg.solve_deal4 = true;
    }
    return eval_cheater(state, moves, &cfg);
}

// Fair greedy: same heuristic as eval_greedy but branded as a fair AI
// (no opponent hand access; greedy never uses it anyway)
uint16_t eval_fair_greedy(const s_cte_game_state *state,
                          const struct s_cte_move_list *moves,
                          void *ctx)
{
    return eval_greedy(state, moves, ctx);
}

// Fair AI: PIMC determinization + alpha-beta search
uint16_t eval_fair(const s_cte_game_state *state,
                   const struct s_cte_move_list *moves,
                   void *ctx)
{
    if(!state || !moves || moves->size == 0) return 0;
    if(moves->size == 1) return 0;

    s_cte_pimc_config default_cfg;
    if(!ctx){
        pimc_config_init(&default_cfg, CTE_PIMC_DEFAULT_WORLDS,
                         CTE_PIMC_DEFAULT_DEPTH, 42);
        ctx = &default_cfg;
    }

    return pimc_search(state, moves, (s_cte_pimc_config *)ctx);
}

// ISMCTS AI: Single-Observer Information Set Monte Carlo Tree Search
uint16_t eval_ismcts(const s_cte_game_state *state,
                     const struct s_cte_move_list *moves,
                     void *ctx)
{
    if(!state || !moves || moves->size == 0) return 0;
    if(moves->size == 1) return 0;

    s_cte_ismcts_config default_cfg;
    bool free_needed = false;
    if(!ctx){
        ismcts_config_init(&default_cfg, CTE_ISMCTS_DEFAULT_ITERS,
                           0, 42);
        ctx = &default_cfg;
        free_needed = true;
    }

    uint16_t choice = ismcts_search(state, moves, (s_cte_ismcts_config *)ctx);
    if(free_needed){
        ismcts_config_free(&default_cfg);
    }
    return choice;
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
        case AI_TYPE_ORACLE:
            if(name_out) *name_out = "Oracle";
            return eval_oracle;
        case AI_TYPE_FAIR_GREEDY:
            if(name_out) *name_out = "Fair-Greedy";
            return eval_fair_greedy;
        case AI_TYPE_FAIR:
            if(name_out) *name_out = "Fair";
            return eval_fair;
        case AI_TYPE_ISMCTS:
            if(name_out) *name_out = "ISMCTS";
            return eval_ismcts;
        case AI_TYPE_RANDOM:
        default:
            if(name_out) *name_out = "Random";
            return eval_random;
    }
}

