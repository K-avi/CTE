#include "ismcts.h"
#include "backend_bitboard.h"
#include "card_tracker.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <float.h>

// ---------------------------------------------------------------------------
// xorshift32 PRNG (same as card_tracker.c but local to avoid link conflicts)
// ---------------------------------------------------------------------------
static inline uint32_t xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static inline uint64_t get_time_us_ismcts(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

// ---------------------------------------------------------------------------
// Arena allocator
// ---------------------------------------------------------------------------
static void arena_init(s_cte_ismcts_arena *arena, uint32_t capacity)
{
    arena->capacity = capacity;
    arena->size = 0;
    arena->nodes = (s_cte_ismcts_node *)calloc(capacity, sizeof(s_cte_ismcts_node));
}

static void arena_reset(s_cte_ismcts_arena *arena)
{
    arena->size = 0;
}

static int32_t arena_alloc(s_cte_ismcts_arena *arena)
{
    if(arena->size >= arena->capacity) return -1;
    int32_t idx = (int32_t)arena->size;
    s_cte_ismcts_node *node = &arena->nodes[idx];
    memset(node, 0, sizeof(s_cte_ismcts_node));
    node->parent_idx = -1;
    node->first_child_idx = -1;
    node->next_sibling_idx = -1;
    arena->size++;
    return idx;
}

// ---------------------------------------------------------------------------
// Init / Free
// ---------------------------------------------------------------------------
void ismcts_config_init(s_cte_ismcts_config *cfg, uint32_t max_iterations,
                        uint32_t max_ms, uint32_t seed)
{
    if(!cfg) return;
    memset(cfg, 0, sizeof(s_cte_ismcts_config));
    cfg->max_iterations = max_iterations > 0 ? max_iterations : CTE_ISMCTS_DEFAULT_ITERS;
    cfg->max_ms = max_ms;
    cfg->exploration_c = CTE_ISMCTS_DEFAULT_C;
    cfg->rng_seed = seed > 0 ? seed : 42;
    cfg->initialized = false;
    arena_init(&cfg->arena, CTE_ISMCTS_NODE_POOL_SIZE);
}

void ismcts_config_free(s_cte_ismcts_config *cfg)
{
    if(!cfg) return;
    free(cfg->arena.nodes);
    cfg->arena.nodes = NULL;
    cfg->arena.capacity = 0;
    cfg->arena.size = 0;
}

// ---------------------------------------------------------------------------
// UCB1 selection score
// ---------------------------------------------------------------------------
static inline double ucb1_score(const s_cte_ismcts_node *node, uint32_t parent_visits, double c)
{
    if(node->visit_count == 0) return DBL_MAX; // Unvisited child — always selected first
    double exploitation = node->total_reward / (double)node->visit_count;
    double exploration = c * sqrt(log((double)parent_visits) / (double)node->visit_count);
    return exploitation + exploration;
}

// ---------------------------------------------------------------------------
// Fast zero-allocation bitboard rollout policy (greedy heuristic playout)
// ---------------------------------------------------------------------------
static int32_t rollout(s_cte_pos pos, uint8_t root_player, uint32_t *seed)
{
    for(int safety = 0; safety < 100; safety++){
        // Check if all hands empty (terminal for this deal)
        bool all_empty = true;
        for(uint8_t p = 0; p < pos.nb_players; p++){
            if(pos.hand_counts[p] > 0){ all_empty = false; break; }
        }
        if(all_empty) break;

        // Skip players with empty hands
        if(pos.hand_counts[pos.current_player] == 0){
            pos.current_player = (uint8_t)((pos.current_player + 1) % pos.nb_players);
            continue;
        }

        // Extract current player's hand into s_cte_hand
        struct s_cte_hand cur_hand;
        cur_hand.size = 0;
        uint64_t h_temp = pos.hand_bb[pos.current_player];
        while(h_temp > 0){
            cur_hand.array[cur_hand.size++] = (t_card)__builtin_ctzll(h_temp);
            h_temp &= (h_temp - 1);
        }

        // Fast bitboard move generation directly on stack
        s_cte_bitboard_move_list cpt;
        bitboard_gen_all_compact_moves_rank(&cpt, pos.table_bb, &cur_hand);
        if(cpt.size == 0) break;

        // Greedy selection: pick move capturing the most points (or random drop if all 0)
        uint16_t best_idx = 0;
        int16_t best_pts = -1;
        for(uint16_t i = 0; i < cpt.size; i++){
            int16_t pts = 0;
            if(cpt.moves[i].capture_mask > 0){
                uint64_t cap = cpt.moves[i].capture_mask;
                while(cap > 0){
                    int bit = __builtin_ctzll(cap);
                    pts = (int16_t)(pts + get_points((t_card)bit));
                    cap &= (cap - 1);
                }
                pts = (int16_t)(pts + get_points(cpt.moves[i].card_played));
                if((pos.table_bb & ~cpt.moves[i].capture_mask) == 0){
                    pts = (int16_t)(pts + 10); // Tablić bonus
                }
            }
            if(pts > best_pts){
                best_pts = pts;
                best_idx = i;
            }
        }

        if(best_pts <= 0){
            best_idx = (uint16_t)(xorshift32(seed) % cpt.size);
        }

        pos = pos_apply_bitboard_move(&pos, cpt.moves[best_idx].card_played, cpt.moves[best_idx].capture_mask);
    }

    // Award remaining table cards to last captor
    if(pos.table_bb > 0 && pos.last_captor >= 0 && pos.last_captor < pos.nb_players){
        uint8_t captor = (uint8_t)pos.last_captor;
        uint64_t temp = pos.table_bb;
        while(temp > 0){
            int bit = __builtin_ctzll(temp);
            pos.card_points[captor] += get_points((t_card)bit);
            pos.won_card_counts[captor]++;
            temp &= (temp - 1);
        }
        pos.table_bb = 0;
    }

    return pos_evaluate(&pos, root_player);
}

// ---------------------------------------------------------------------------
// SO-ISMCTS main search
// ---------------------------------------------------------------------------
uint16_t ismcts_search(const s_cte_game_state *state,
                       const struct s_cte_move_list *moves,
                       s_cte_ismcts_config *cfg)
{
    if(!state || !moves || moves->size == 0 || !cfg) return 0;
    if(moves->size == 1) return 0;

    uint8_t observer = state->current_player_id;
    uint8_t nb_players = (state->players && state->players->size <= 4) ? state->players->size : 2;

    // Refresh tracker from the current game state on every turn
    tracker_init_from_state(&cfg->tracker, observer, state);
    cfg->initialized = true;

    // Collect hand sizes and own hand bitboard
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

    // Reset arena for this search
    arena_reset(&cfg->arena);

    // Create root node
    int32_t root_idx = arena_alloc(&cfg->arena);
    if(root_idx < 0) return 0;
    cfg->arena.nodes[root_idx].player_to_move = observer;
    cfg->arena.nodes[root_idx].num_legal_moves = (uint8_t)(moves->size < 255 ? moves->size : 255);

    // Pre-allocate child nodes for each legal root move
    uint16_t num_moves = moves->size;
    int32_t prev_child = -1;
    for(uint16_t i = 0; i < num_moves; i++){
        int32_t c_idx = arena_alloc(&cfg->arena);
        if(c_idx < 0) break;
        cfg->arena.nodes[c_idx].parent_idx = root_idx;
        cfg->arena.nodes[c_idx].move_idx = i;
        cfg->arena.nodes[c_idx].player_to_move = observer;
        cfg->arena.nodes[c_idx].visit_count = 0;
        cfg->arena.nodes[c_idx].total_reward = 0.0;
        cfg->arena.nodes[c_idx].next_sibling_idx = -1;

        if(prev_child < 0){
            cfg->arena.nodes[root_idx].first_child_idx = c_idx;
        } else {
            cfg->arena.nodes[prev_child].next_sibling_idx = c_idx;
        }
        prev_child = c_idx;
        cfg->arena.nodes[root_idx].num_children++;
    }
    cfg->arena.nodes[root_idx].fully_expanded = true;

    cfg->total_iterations = 0;
    cfg->total_rollouts = 0;

    uint64_t start_us = (cfg->max_ms > 0) ? get_time_us_ismcts() : 0;
    uint64_t time_limit_us = (uint64_t)cfg->max_ms * 1000ULL;

    for(uint32_t iter = 0; iter < cfg->max_iterations; iter++){
        // Optional time-budget cutoff
        if(cfg->max_ms > 0 && iter > 0 && (iter & 63) == 0){
            if(get_time_us_ismcts() - start_us >= time_limit_us) break;
        }

        // 1. Determinize: sample plausible opponent hands from unseen cards
        uint64_t det_hands[4] = {0};
        uint32_t det_seed = cfg->rng_seed;
        cfg->rng_seed = xorshift32(&cfg->rng_seed);
        tracker_determinize(&cfg->tracker, own_hand_bb, hand_sizes,
                            nb_players, det_hands, &det_seed);

        // 2. Select: pick root move using UCB1 over root children
        int32_t best_child = -1;
        double best_score = -DBL_MAX;
        uint32_t root_visits = cfg->arena.nodes[root_idx].visit_count;

        int32_t child_idx = cfg->arena.nodes[root_idx].first_child_idx;
        while(child_idx >= 0){
            s_cte_ismcts_node *child = &cfg->arena.nodes[child_idx];
            if(child->visit_count == 0){
                best_child = child_idx;
                break; // Unvisited child takes priority
            }
            double score = ucb1_score(child, root_visits, cfg->exploration_c);
            if(score > best_score){
                best_score = score;
                best_child = child_idx;
            }
            child_idx = child->next_sibling_idx;
        }

        if(best_child < 0) best_child = cfg->arena.nodes[root_idx].first_child_idx;
        if(best_child < 0) break;

        // 3. Apply the selected root move in this determinized world
        uint16_t chosen_move_idx = cfg->arena.nodes[best_child].move_idx;
        s_cte_pos sim_pos = pos_from_state(state);
        for(uint8_t p = 0; p < nb_players; p++){
            sim_pos.hand_bb[p] = det_hands[p];
        }
        sim_pos = pos_apply_move(&sim_pos, &moves->moves[chosen_move_idx]);

        // 4. Rollout: play out from sim_pos using fast bitboard greedy rollout
        uint32_t rollout_seed = cfg->rng_seed;
        int32_t reward = rollout(sim_pos, observer, &rollout_seed);
        cfg->total_rollouts++;

        // 5. Backpropagate: update visit counts and total reward
        cfg->arena.nodes[best_child].visit_count++;
        cfg->arena.nodes[best_child].total_reward += (double)reward;
        cfg->arena.nodes[root_idx].visit_count++;
        cfg->arena.nodes[root_idx].total_reward += (double)reward;

        cfg->total_iterations++;
    }

    // Select the most-visited root move
    uint16_t best_move = 0;
    uint32_t max_visits = 0;

    int32_t child_idx = cfg->arena.nodes[root_idx].first_child_idx;
    while(child_idx >= 0){
        s_cte_ismcts_node *child = &cfg->arena.nodes[child_idx];
        if(child->visit_count > max_visits){
            max_visits = child->visit_count;
            best_move = child->move_idx;
        }
        child_idx = child->next_sibling_idx;
    }

    return best_move;
}
