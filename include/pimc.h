#ifndef __CTE_PIMC_H
#define __CTE_PIMC_H

#include "card_tracker.h"
#include "minmax.h"
#include "eval.h"

// Default PIMC parameters
#define CTE_PIMC_DEFAULT_WORLDS  30
#define CTE_PIMC_DEFAULT_DEPTH   2

// Optimization flags for PIMC search (can be combined via bitwise OR)
typedef enum {
    PIMC_OPT_NONE          = 0,
    PIMC_OPT_SOLVE_DEAL4   = 1 << 0,  // Exact terminal minimax in Deal 4 (deck empty)
    PIMC_OPT_DEPTH4        = 1 << 1,  // 4-ply nominal lookahead (vs 2-ply)
    PIMC_OPT_ADAPTIVE      = 1 << 2,  // Dynamic depth expansion on small hands
    PIMC_OPT_BORDA         = 1 << 3,  // Borda rank voting aggregation vs score average
    PIMC_OPT_NEG_INFERENCE = 1 << 4,  // Filter sampled hands containing refused cards
    PIMC_OPT_ISO_DEDUP     = 1 << 5,  // Deduplicate rank-isomorphic determinizations
    PIMC_OPT_ALL           = 0x3F
} e_cte_pimc_opt_flags;

// PIMC configuration: passed as eval_context for eval_fair
typedef struct {
    s_cte_card_tracker  tracker;       // Card tracker for the observer player
    uint16_t            num_worlds;    // Number of determinized worlds to sample (default 30)
    uint8_t             search_depth;  // Alpha-beta depth per world (default 2)
    e_cte_ubp_model     ubp_model;     // Upper bound pruning model
    uint32_t            rng_seed;      // RNG seed for deterministic replay
    uint32_t            opt_flags;     // Optimization feature flags (e_cte_pimc_opt_flags)
    bool                initialized;   // Whether the tracker has been initialized
    // Statistics (updated after each eval_fair call)
    uint64_t            total_nodes;   // Total nodes visited across all worlds
    uint32_t            total_worlds;  // Total worlds actually sampled
} s_cte_pimc_config;

// Initialize a PIMC config with defaults
void pimc_config_init(s_cte_pimc_config *cfg, uint16_t num_worlds,
                      uint8_t search_depth, uint32_t seed);

// Set optimization flags on an initialized PIMC config
void pimc_config_set_opts(s_cte_pimc_config *cfg, uint32_t flags);

// Run PIMC search: determinize num_worlds times, run alpha-beta on each,
// return the index of the move with the best average score.
uint16_t pimc_search(const s_cte_game_state *state,
                     const struct s_cte_move_list *moves,
                     s_cte_pimc_config *cfg);

#endif
