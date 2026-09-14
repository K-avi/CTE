#include "pimc.h"
#include <string.h>
#include <limits.h>

void pimc_config_init(s_cte_pimc_config *cfg, uint16_t num_worlds,
                      uint8_t search_depth, uint32_t seed)
{
    if(!cfg) return;
    memset(cfg, 0, sizeof(s_cte_pimc_config));
    cfg->num_worlds   = num_worlds > 0 ? num_worlds : CTE_PIMC_DEFAULT_WORLDS;
    cfg->search_depth = search_depth > 0 ? search_depth : CTE_PIMC_DEFAULT_DEPTH;
    cfg->ubp_model    = UBP_STRICT_ADMISSIBLE;
    cfg->rng_seed     = seed > 0 ? seed : 42;
    cfg->initialized  = false;
}

uint16_t pimc_search(const s_cte_game_state *state,
                     const struct s_cte_move_list *moves,
                     s_cte_pimc_config *cfg)
{
    if(!state || !moves || moves->size == 0 || !cfg) return 0;
    if(moves->size == 1) return 0;

    uint8_t observer = state->current_player_id;
    uint8_t nb_players = (state->players && state->players->size <= 4) ? state->players->size : 2;

    // Refresh the tracker from current board state on every search call
    tracker_init_from_state(&cfg->tracker, observer, state);
    cfg->initialized = true;

    // Collect hand sizes for all players
    uint8_t hand_sizes[4] = {0};
    uint64_t own_hand_bb = 0;
    if(state->players){
        for(uint8_t p = 0; p < nb_players; p++){
            hand_sizes[p] = state->players->players[p].hand.size;
            if(p == observer){
                for(uint8_t i = 0; i < state->players->players[p].hand.size; i++){
                    if(state->players->players[p].hand.array[i] < 52)
                        own_hand_bb |= (1ULL << state->players->players[p].hand.array[i]);
                }
            }
        }
    }

    // Accumulate scores per root move across all worlds
    int64_t score_sums[512] = {0};  // More than enough for any move list
    uint16_t num_moves = moves->size < 512 ? moves->size : 512;

    cfg->total_nodes = 0;
    cfg->total_worlds = 0;

    for(uint16_t w = 0; w < cfg->num_worlds; w++){
        // 1. Determinize: sample opponent hands from unseen cards
        uint64_t det_hands[4] = {0};
        tracker_determinize(&cfg->tracker, own_hand_bb, hand_sizes,
                            nb_players, det_hands, &cfg->rng_seed);

        // 2. Build a full s_cte_pos with the determinized hands
        s_cte_pos pos = pos_from_state(state);
        for(uint8_t p = 0; p < nb_players; p++){
            pos.hand_bb[p] = det_hands[p];
        }

        // 3. Evaluate each root move via alpha-beta in this determinized world
        s_cte_search_config search_cfg = {
            .max_depth      = cfg->search_depth,
            .timeout_ms     = 0,
            .ubp_model      = cfg->ubp_model,
            .multi_deal     = false,
            .solve_deal4    = false,
            .nodes_visited  = 0,
            .ubp_cutoffs    = 0
        };

        // Use search_best_move which does iterative deepening and returns candidates
        search_best_move(&pos, moves, &search_cfg);
        cfg->total_nodes += search_cfg.nodes_visited;

        // Accumulate each candidate's score
        for(uint16_t c = 0; c < search_cfg.num_candidates && c < num_moves; c++){
            uint16_t move_idx = search_cfg.candidates[c].move_idx;
            if(move_idx < num_moves){
                score_sums[move_idx] += search_cfg.candidates[c].score;
            }
        }

        cfg->total_worlds++;
    }

    // 4. Pick the move with the highest average score
    uint16_t best_idx = 0;
    int64_t best_avg = INT64_MIN;
    for(uint16_t i = 0; i < num_moves; i++){
        if(score_sums[i] > best_avg){
            best_avg = score_sums[i];
            best_idx = i;
        }
    }

    return best_idx;
}
