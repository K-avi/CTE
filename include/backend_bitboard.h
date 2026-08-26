#ifndef __CTE_BACKEND_BITBOARD_H
#define __CTE_BACKEND_BITBOARD_H

#include "engine.h"

// 64-bit Bitboard representation of game state
typedef struct {
    uint64_t table_bb;          // 52-bit mask of cards on the table
    uint64_t hand_bb[4];        // 52-bit mask of cards in each player's hand
    uint64_t won_bb[4];         // 52-bit mask of captured cards per player
    uint8_t  hand_count[4];     // Number of cards in each player's hand
    uint8_t  won_count[4];      // Number of captured cards per player
    uint8_t  card_points[4];    // Cumulative card points per player
    uint8_t  tablic_count[4];   // Cumulative tablic count per player
    int8_t   last_captor_id;    // Last player to capture (-1 if none)
    uint8_t  current_player_id; // Active player
    uint8_t  nb_players;        // 2, 3, or 4
    bool     is_team_mode;      // True for 4-player 2v2
} s_cte_bitboard_state;

// Compact Bitboard move (pure registers / stack, 0 malloc)
typedef struct {
    t_card   card_played;
    uint64_t capture_mask; // 0 for drop, or 64-bit mask of captured cards
} s_cte_bitboard_move;

typedef struct {
    uint16_t size;
    s_cte_bitboard_move moves[1024];
} s_cte_bitboard_move_list;

// Convert s_cte_game to s_cte_bitboard_state
void bitboard_from_game(s_cte_bitboard_state *bb_state, const s_cte_game *game);

// High-throughput 1-pass compact bitboard move generator (Dynamic Carry-Rippler, 100% stack)
void bitboard_gen_all_compact_moves(s_cte_bitboard_move_list *out_list, uint64_t table_bb, const struct s_cte_hand *hand);

// Ultra-fast 1-pass compact bitboard move generator (1D Pivot Tables, 0 recursion, 100% stack)
void bitboard_gen_all_compact_moves_table(s_cte_bitboard_move_list *out_list, uint64_t table_bb, const struct s_cte_hand *hand);

// Dynamic Carry-Rippler bitboard move generation (0 KB RAM)
t_cteerr bitboard_gen_card_moves_dynamic(struct s_cte_move_list *moves, uint64_t table_bb, t_card card);
t_cteerr bitboard_gen_all_moves_dynamic(struct s_cte_move_list *moves, uint64_t table_bb, const struct s_cte_hand *hand);

// Table Lookup bitboard move generation (225 KB RAM)
t_cteerr bitboard_gen_card_moves_table(struct s_cte_move_list *moves, uint64_t table_bb, t_card card);
t_cteerr bitboard_gen_all_moves_table(struct s_cte_move_list *moves, uint64_t table_bb, const struct s_cte_hand *hand);

// Registered backends
extern const s_cte_engine_backend g_backend_bitboard;        // Dynamic (Default)
extern const s_cte_engine_backend g_backend_bitboard_table;  // Lookup Tables

#endif
