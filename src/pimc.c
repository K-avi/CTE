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
    cfg->opt_flags    = CTE_PIMC_DEFAULT_OPTS;
}

void pimc_config_set_opts(s_cte_pimc_config *cfg, uint32_t flags)
{
    if(!cfg) return;
    cfg->opt_flags = flags;
}

static inline uint64_t hand_canonical_signature(uint64_t hand_bb){
    uint8_t tokens[8];
    uint8_t count = 0;
    uint64_t temp = hand_bb;
    while(temp > 0 && count < 8){
        uint8_t card = (uint8_t)__builtin_ctzll(temp);
        temp &= (temp - 1);
        uint8_t token;
        if(card == 0){
            token = 100; // 2 of clubs (special 1 pt)
        } else if(card == 21){
            token = 200; // 10 of diamonds (special 2 pts)
        } else {
            token = (card % 13) + 1;
        }
        tokens[count++] = token;
    }
    for(uint8_t i = 1; i < count; i++){
        uint8_t key = tokens[i];
        int j = (int)i - 1;
        while(j >= 0 && tokens[j] > key){
            tokens[j + 1] = tokens[j];
            j--;
        }
        tokens[j + 1] = key;
    }
    uint64_t sig = 0;
    for(uint8_t i = 0; i < count && i < 8; i++){
        sig |= ((uint64_t)tokens[i]) << (i * 8);
    }
    return sig;
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

    struct {
        uint64_t sig;
        int32_t scores[128];
    } iso_cache[64];
    uint16_t iso_cache_size = 0;

    for(uint16_t w = 0; w < cfg->num_worlds; w++){
        // 1. Determinize: sample opponent hands from unseen cards
        uint64_t det_hands[4] = {0};
        uint64_t opp_sig = 0;
        int hit_idx = -1;

        if(cfg->opt_flags & PIMC_OPT_ISO_DEDUP){
            for(uint8_t retry = 0; retry < 3; retry++){
                tracker_determinize(&cfg->tracker, own_hand_bb, hand_sizes,
                                    nb_players, det_hands, &cfg->rng_seed);
                opp_sig = 0;
                for(uint8_t p = 0; p < nb_players; p++){
                    if(p == observer) continue;
                    opp_sig = (opp_sig * 31337ULL) ^ hand_canonical_signature(det_hands[p]);
                }
                hit_idx = -1;
                for(uint16_t i = 0; i < iso_cache_size; i++){
                    if(iso_cache[i].sig == opp_sig){
                        hit_idx = (int)i;
                        break;
                    }
                }
                if(hit_idx < 0) break; // Found an unseen signature
            }

            if(hit_idx >= 0){
                // Cache hit: reuse evaluated move scores without running minimax
                for(uint16_t i = 0; i < num_moves && i < 128; i++){
                    score_sums[i] += iso_cache[hit_idx].scores[i];
                }
                cfg->total_worlds++;
                continue;
            }
        } else {
            tracker_determinize(&cfg->tracker, own_hand_bb, hand_sizes,
                                nb_players, det_hands, &cfg->rng_seed);
        }

        // 2. Build a full s_cte_pos with the determinized hands
        s_cte_pos pos = pos_from_state(state);
        for(uint8_t p = 0; p < nb_players; p++){
            pos.hand_bb[p] = det_hands[p];
        }

        uint8_t eff_depth = (cfg->opt_flags & PIMC_OPT_DEPTH4) ? 4 : cfg->search_depth;
        if(cfg->opt_flags & PIMC_OPT_ADAPTIVE){
            uint8_t rem_deal_plies = 0;
            for(uint8_t p = 0; p < nb_players; p++){
                rem_deal_plies += hand_sizes[p];
            }
            if(rem_deal_plies <= 6 && rem_deal_plies > eff_depth){
                eff_depth = rem_deal_plies;
            }
        }
        s_cte_search_config search_cfg = {
            .max_depth      = eff_depth,
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

        // Accumulate each candidate's score (raw average or Borda rank voting)
        if(cfg->opt_flags & PIMC_OPT_BORDA){
            uint16_t num_c = search_cfg.num_candidates;
            int64_t current_points = num_c;
            for(uint16_t c = 0; c < num_c && c < num_moves; c++){
                if(search_cfg.candidates[c].refuted) continue;
                if(c > 0 && search_cfg.candidates[c].score < search_cfg.candidates[c - 1].score){
                    current_points = num_c - c;
                }
                uint16_t move_idx = search_cfg.candidates[c].move_idx;
                if(move_idx < num_moves){
                    score_sums[move_idx] += current_points;
                }
            }
        } else {
            for(uint16_t c = 0; c < search_cfg.num_candidates && c < num_moves; c++){
                uint16_t move_idx = search_cfg.candidates[c].move_idx;
                if(move_idx < num_moves){
                    score_sums[move_idx] += search_cfg.candidates[c].score;
                }
            }
        }

        // Cache candidate scores for this rank isomorphism
        if((cfg->opt_flags & PIMC_OPT_ISO_DEDUP) && iso_cache_size < 64){
            iso_cache[iso_cache_size].sig = opp_sig;
            memset(iso_cache[iso_cache_size].scores, 0, sizeof(iso_cache[iso_cache_size].scores));
            for(uint16_t c = 0; c < search_cfg.num_candidates && c < num_moves; c++){
                uint16_t move_idx = search_cfg.candidates[c].move_idx;
                if(move_idx < num_moves && move_idx < 128){
                    iso_cache[iso_cache_size].scores[move_idx] = search_cfg.candidates[c].score;
                }
            }
            iso_cache_size++;
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
