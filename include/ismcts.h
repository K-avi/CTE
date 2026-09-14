#ifndef __CTE_ISMCTS_H
#define __CTE_ISMCTS_H

#include "card_tracker.h"
#include "minmax.h"
#include "eval.h"

// Default ISMCTS parameters
#define CTE_ISMCTS_DEFAULT_ITERS  1000
#define CTE_ISMCTS_DEFAULT_MS     0       // 0 = use iteration budget, not time
#define CTE_ISMCTS_DEFAULT_C      1.414   // sqrt(2), standard UCB1 exploration constant
#define CTE_ISMCTS_NODE_POOL_SIZE 65536   // ~64K nodes (~4MB with 64 bytes/node)

// ISMCTS tree node: stored in a flat arena pool
typedef struct s_cte_ismcts_node {
    int32_t   parent_idx;       // Index of parent in arena (-1 for root)
    int32_t   first_child_idx;  // Index of first child (-1 if leaf)
    int32_t   next_sibling_idx; // Index of next sibling (-1 if last)
    uint16_t  move_idx;         // Move index that led to this node
    uint32_t  visit_count;      // N(v)
    double    total_reward;     // Q(v) — sum of rewards
    uint8_t   player_to_move;   // Player who acts at this node
    uint8_t   num_children;     // Number of expanded children
    uint8_t   num_legal_moves;  // Total legal moves at this information set
    bool      fully_expanded;   // All children expanded
} s_cte_ismcts_node;

// Arena allocator for tree nodes (pre-allocated, zero malloc in hot loop)
typedef struct {
    s_cte_ismcts_node *nodes;       // Flat array of nodes
    uint32_t           capacity;    // Max nodes in pool
    uint32_t           size;        // Current number of allocated nodes
} s_cte_ismcts_arena;

// ISMCTS configuration: passed as eval_context for eval_ismcts
typedef struct {
    s_cte_card_tracker  tracker;        // Card tracker for the observer
    uint32_t            max_iterations; // Iteration budget (default 1000)
    uint32_t            max_ms;         // Time budget in ms (0 = use iterations)
    double              exploration_c;  // UCB1 exploration constant (default sqrt(2))
    uint32_t            rng_seed;       // RNG seed
    bool                initialized;    // Tracker initialization flag
    // Arena (allocated once, reused across calls within a round)
    s_cte_ismcts_arena  arena;
    // Statistics
    uint32_t            total_iterations; // Iterations actually performed
    uint64_t            total_rollouts;   // Rollout nodes evaluated
} s_cte_ismcts_config;

// Initialize ISMCTS config with defaults. Allocates the node arena.
// Call ismcts_config_free() when done (e.g., at round end).
void ismcts_config_init(s_cte_ismcts_config *cfg, uint32_t max_iterations,
                        uint32_t max_ms, uint32_t seed);

// Free the ISMCTS arena memory.
void ismcts_config_free(s_cte_ismcts_config *cfg);

// Run SO-ISMCTS search. Returns the index of the best move.
uint16_t ismcts_search(const s_cte_game_state *state,
                       const struct s_cte_move_list *moves,
                       s_cte_ismcts_config *cfg);

#endif
