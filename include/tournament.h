#ifndef __CTE_TOURNAMENT_H
#define __CTE_TOURNAMENT_H

#include "game.h"
#include "profile.h"

#define CTE_MAX_TOURNAMENT_PLAYERS 16
#define CTE_MAX_TOURNAMENT_MATCHES 128

typedef enum {
    TOURNAMENT_ROUND_ROBIN = 0, // Every participant plays every other participant
    TOURNAMENT_KNOCKOUT    = 1, // Single-elimination bracket tournament (powers of 2: 2, 4, 8, 16)
} e_cte_tournament_type;

// Participant record in tournament
typedef struct {
    char          name[32];
    t_evaluator   evaluator;
    void         *eval_context;
    bool          is_human;
    e_cte_ai_type ai_type;
    uint16_t      matches_played;
    uint16_t      matches_won;
    uint16_t      matches_lost;
    uint16_t      matches_tied;
    uint32_t      total_points;
    uint16_t      total_tablics;
    int16_t       elo_start;
    int16_t       elo_current;
} s_cte_tournament_participant;

// Single match result inside tournament
typedef struct {
    uint8_t  p1_idx;
    uint8_t  p2_idx;
    uint16_t score_p1;
    uint16_t score_p2;
    int8_t   winner_idx; // 0 = p1, 1 = p2, -1 = tie
    uint8_t  bracket_stage; // Stage/round index in tournament
} s_cte_tournament_match;

// Configuration for tournament launch
typedef struct {
    e_cte_tournament_type        type;
    uint8_t                      nb_participants;
    s_cte_tournament_participant participants[CTE_MAX_TOURNAMENT_PLAYERS];
    uint16_t                     winning_score; // Match target points (e.g. 51 or 101)
    uint8_t                      max_rounds;    // Max deck cycles per match (0 = until winning_score)
    e_cte_render_style           style;
    bool                         silent;        // If true, suppress move-by-move prints
    const s_cte_ui_callbacks    *callbacks;
    void                        *ui_context;
    s_cte_profile_db            *profile_db;    // Optional: NULL if no profile tracking
    bool                         persist_ai;    // If true, persist AI profiles to DB
} s_cte_tournament_config;

// Tournament state structure
typedef struct {
    s_cte_tournament_config config;
    s_cte_tournament_match  matches[CTE_MAX_TOURNAMENT_MATCHES];
    uint16_t                nb_matches;
    uint8_t                 standings[CTE_MAX_TOURNAMENT_PLAYERS]; // Sorted indices (1st to last)
    int8_t                  champion_idx;                          // Index of winner, -1 if ongoing/tie
} s_cte_tournament;

// Statistical benchmark between two AIs
typedef struct {
    char     name_a[32];
    char     name_b[32];
    uint32_t nb_games;
    uint32_t wins_a;
    uint32_t wins_b;
    uint32_t draws;
    double   win_rate_a;       // (wins_a + 0.5 * draws) / nb_games
    double   avg_pts_a;
    double   avg_pts_b;
    double   delta_elo_a;      // Relative Elo difference of A over B
    uint64_t total_tablics_a;
    uint64_t total_tablics_b;
    uint64_t total_moves_a;
    uint64_t total_moves_b;
    double   avg_latency_us_a; // Average move latency in microseconds
    double   avg_latency_us_b;
} s_cte_bench_result;

// Tournament API
t_cteerr init_tournament(s_cte_tournament *t, const s_cte_tournament_config *cfg);
t_cteerr run_tournament(s_cte_tournament *t);
t_cteerr sync_tournament_profiles(const s_cte_tournament *t);
void     print_tournament_standings(const s_cte_tournament *t, e_cte_render_style style);
void     free_tournament(s_cte_tournament *t);

// Headless AI Benchmark API
t_cteerr cte_run_ai_benchmark(e_cte_ai_type type_a, void *ctx_a,
                             e_cte_ai_type type_b, void *ctx_b,
                             uint32_t nb_games,
                             s_cte_bench_result *out);

void cte_print_bench_result(const s_cte_bench_result *res);

#endif

