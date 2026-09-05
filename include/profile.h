#ifndef __CTE_PROFILE_H
#define __CTE_PROFILE_H

#include "card.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define CTE_MAX_PROFILES 128
#define CTE_PROFILE_NAME_MAX 32
#define CTE_PROFILE_MAGIC "CTEPRF01"
#define CTE_DEFAULT_ELO 1200
#define CTE_DEFAULT_K_FACTOR 32

typedef struct {
    char     name[CTE_PROFILE_NAME_MAX];
    int16_t  elo;
    uint32_t matches_played;
    uint32_t matches_won;
    uint32_t matches_lost;
    uint32_t matches_tied;
    uint32_t total_points;
    uint32_t total_tablics;
    uint64_t created_at;
    uint64_t last_played_at;
} s_cte_profile;

typedef struct {
    s_cte_profile profiles[CTE_MAX_PROFILES];
    uint16_t      count;
    char          filepath[512];
} s_cte_profile_db;

// Profile management & persistence
t_cteerr get_default_profile_path(char *out_path, size_t max_len);
t_cteerr init_profile_db(s_cte_profile_db *db, const char *custom_path);
t_cteerr load_profiles(s_cte_profile_db *db);
t_cteerr save_profiles(const s_cte_profile_db *db);

s_cte_profile* find_profile(s_cte_profile_db *db, const char *name);
s_cte_profile* find_or_create_profile(s_cte_profile_db *db, const char *name);

// Elo rating calculation & match update
int16_t compute_elo_delta(int16_t elo_self, int16_t elo_opponent,
                          double score, uint8_t k_factor);
void update_match_elo(s_cte_profile *p1, s_cte_profile *p2, int8_t winner_idx, uint8_t k_factor);

// Ranking & visualization
void sort_profiles_by_elo(s_cte_profile_db *db);
void print_leaderboard(const s_cte_profile_db *db);

#endif
