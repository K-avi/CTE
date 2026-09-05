#include "profile.h"
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

static int ensure_dir_exists(const char *path){
    char temp[512];
    snprintf(temp, sizeof(temp), "%s", path);
    size_t len = strlen(temp);
    if(len == 0) return 0;

    // Find last slash to isolate directory
    char *last_slash = strrchr(temp, '/');
    if(!last_slash) return 0; // Local directory, already exists
    *last_slash = '\0';

    // Iteratively create directories
    for(char *p = temp + 1; *p; p++){
        if(*p == '/'){
            *p = '\0';
            if(mkdir(temp, 0755) != 0 && errno != EEXIST){
                return -1;
            }
            *p = '/';
        }
    }
    if(mkdir(temp, 0755) != 0 && errno != EEXIST){
        return -1;
    }
    return 0;
}

t_cteerr get_default_profile_path(char *out_path, size_t max_len){
    if(!out_path || max_len == 0) return e_null;

    const char *xdg_data = getenv("XDG_DATA_HOME");
    if(xdg_data && xdg_data[0] != '\0'){
        snprintf(out_path, max_len, "%s/cte/profiles.dat", xdg_data);
        return e_ok;
    }

    const char *home = getenv("HOME");
    if(home && home[0] != '\0'){
        snprintf(out_path, max_len, "%s/.local/share/cte/profiles.dat", home);
        return e_ok;
    }

    snprintf(out_path, max_len, "./profiles.dat");
    return e_ok;
}

t_cteerr init_profile_db(s_cte_profile_db *db, const char *custom_path){
    if(!db) return e_null;
    memset(db, 0, sizeof(s_cte_profile_db));

    if(custom_path && custom_path[0] != '\0'){
        snprintf(db->filepath, sizeof(db->filepath), "%s", custom_path);
    } else {
        t_cteerr err = get_default_profile_path(db->filepath, sizeof(db->filepath));
        if(err != e_ok) return err;
    }

    return load_profiles(db);
}

t_cteerr load_profiles(s_cte_profile_db *db){
    if(!db) return e_null;

    FILE *f = fopen(db->filepath, "rb");
    if(!f){
        // File doesn't exist yet: start fresh
        db->count = 0;
        return e_ok;
    }

    char magic[8];
    if(fread(magic, 1, 8, f) != 8){
        fclose(f);
        db->count = 0;
        return e_inval_val;
    }

    if(memcmp(magic, CTE_PROFILE_MAGIC, 8) != 0){
        fclose(f);
        db->count = 0;
        return e_inval_val;
    }

    uint16_t count = 0;
    if(fread(&count, sizeof(uint16_t), 1, f) != 1 || count > CTE_MAX_PROFILES){
        fclose(f);
        db->count = 0;
        return e_inval_val;
    }

    if(count > 0){
        if(fread(db->profiles, sizeof(s_cte_profile), count, f) != count){
            fclose(f);
            db->count = 0;
            return e_inval_val;
        }
    }

    db->count = count;
    fclose(f);
    return e_ok;
}

t_cteerr save_profiles(const s_cte_profile_db *db){
    if(!db) return e_null;

    ensure_dir_exists(db->filepath);

    char temp_path[540];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", db->filepath);

    FILE *f = fopen(temp_path, "wb");
    if(!f){
        return e_inval_val;
    }

    if(fwrite(CTE_PROFILE_MAGIC, 1, 8, f) != 8){
        fclose(f);
        unlink(temp_path);
        return e_inval_val;
    }

    if(fwrite(&db->count, sizeof(uint16_t), 1, f) != 1){
        fclose(f);
        unlink(temp_path);
        return e_inval_val;
    }

    if(db->count > 0){
        if(fwrite(db->profiles, sizeof(s_cte_profile), db->count, f) != db->count){
            fclose(f);
            unlink(temp_path);
            return e_inval_val;
        }
    }

    fflush(f);
    fclose(f);

    if(rename(temp_path, db->filepath) != 0){
        unlink(temp_path);
        return e_inval_val;
    }

    return e_ok;
}

s_cte_profile* find_profile(s_cte_profile_db *db, const char *name){
    if(!db || !name || name[0] == '\0') return NULL;

    for(uint16_t i = 0; i < db->count; i++){
        if(strcasecmp(db->profiles[i].name, name) == 0){
            return &db->profiles[i];
        }
    }
    return NULL;
}

s_cte_profile* find_or_create_profile(s_cte_profile_db *db, const char *name){
    if(!db || !name || name[0] == '\0') return NULL;

    s_cte_profile *p = find_profile(db, name);
    if(p) return p;

    if(db->count >= CTE_MAX_PROFILES){
        return NULL; // Database capacity reached
    }

    p = &db->profiles[db->count++];
    memset(p, 0, sizeof(s_cte_profile));
    snprintf(p->name, sizeof(p->name), "%s", name);
    p->elo = CTE_DEFAULT_ELO;
    uint64_t now = (uint64_t)time(NULL);
    p->created_at = now;
    p->last_played_at = now;

    return p;
}

int16_t compute_elo_delta(int16_t elo_self, int16_t elo_opponent,
                          double score, uint8_t k_factor){
    if(k_factor == 0) k_factor = CTE_DEFAULT_K_FACTOR;
    double expected = 1.0 / (1.0 + pow(10.0, (double)(elo_opponent - elo_self) / 400.0));
    return (int16_t)round((double)k_factor * (score - expected));
}

void update_match_elo(s_cte_profile *p1, s_cte_profile *p2, int8_t winner_idx, uint8_t k_factor){
    if(!p1 || !p2) return;
    if(k_factor == 0) k_factor = CTE_DEFAULT_K_FACTOR;

    double s1, s2;
    if(winner_idx == 0){
        s1 = 1.0;
        s2 = 0.0;
    } else if(winner_idx == 1){
        s1 = 0.0;
        s2 = 1.0;
    } else {
        s1 = 0.5;
        s2 = 0.5;
    }

    int16_t d1 = compute_elo_delta(p1->elo, p2->elo, s1, k_factor);
    int16_t d2 = compute_elo_delta(p2->elo, p1->elo, s2, k_factor);

    p1->elo += d1;
    if(p1->elo < CTE_MIN_ELO) p1->elo = CTE_MIN_ELO;

    p2->elo += d2;
    if(p2->elo < CTE_MIN_ELO) p2->elo = CTE_MIN_ELO;

    p1->matches_played++;
    p2->matches_played++;

    if(winner_idx == 0){
        p1->matches_won++;
        p2->matches_lost++;
    } else if(winner_idx == 1){
        p2->matches_won++;
        p1->matches_lost++;
    } else {
        p1->matches_tied++;
        p2->matches_tied++;
    }

    uint64_t now = (uint64_t)time(NULL);
    p1->last_played_at = now;
    p2->last_played_at = now;
}

static int compare_profiles_elo(const void *a, const void *b){
    const s_cte_profile *pa = (const s_cte_profile*)a;
    const s_cte_profile *pb = (const s_cte_profile*)b;

    if(pa->elo != pb->elo){
        return (int)pb->elo - (int)pa->elo;
    }
    if(pa->matches_won != pb->matches_won){
        return (int)pb->matches_won - (int)pa->matches_won;
    }
    return (int)pb->total_points - (int)pa->total_points;
}

void sort_profiles_by_elo(s_cte_profile_db *db){
    if(!db || db->count <= 1) return;
    qsort(db->profiles, db->count, sizeof(s_cte_profile), compare_profiles_elo);
}

void print_leaderboard(const s_cte_profile_db *db){
    if(!db) return;

    printf("\n=========================================================================================\n");
    printf("                               CTE PLAYER LEADERBOARD (ELO)                              \n");
    printf("=========================================================================================\n");
    printf(" Rank | %-20s |  Elo  | Played |  Won | Lost | Tied | Win Rate | Tablics \n", "Player Name");
    printf("------|----------------------|-------|--------|------|------|------|----------|---------\n");

    if(db->count == 0){
        printf("      | No profiles recorded |       |        |      |      |      |          |         \n");
    } else {
        for(uint16_t i = 0; i < db->count; i++){
            const s_cte_profile *p = &db->profiles[i];
            double win_rate = (p->matches_played > 0)
                ? ((double)p->matches_won / (double)p->matches_played) * 100.0
                : 0.0;

            printf("  %2u  | %-20s | %5d |   %4u | %4u | %4u | %4u |  %5.1f%%  |  %5u  \n",
                   (unsigned)(i + 1),
                   p->name,
                   (int)p->elo,
                   (unsigned)p->matches_played,
                   (unsigned)p->matches_won,
                   (unsigned)p->matches_lost,
                   (unsigned)p->matches_tied,
                   win_rate,
                   (unsigned)p->total_tablics);
        }
    }
    printf("=========================================================================================\n\n");
}

t_cteerr record_match_result_in_profile_path(const char *profile_name,
                                             const struct s_cte_match *match,
                                             const struct s_cte_players *players,
                                             const e_cte_ai_type *ai_types,
                                             uint8_t nb_ai_types,
                                             s_cte_profile *out_profile,
                                             int16_t *out_delta,
                                             const char *custom_path)
{
    if(!profile_name || profile_name[0] == '\0' || !match || !players) return e_null;

    s_cte_profile_db db;
    t_cteerr err = init_profile_db(&db, custom_path);
    if(err != e_ok) return err;

    s_cte_profile *p = find_or_create_profile(&db, profile_name);
    if(!p) return e_inval_val;

    p->total_points += match->match_scores[0];
    p->total_tablics += match->match_tablics[0];
    p->matches_played++;

    int16_t opp_elo = CTE_DEFAULT_ELO;
    if(players->size >= 2){
        if(!players->players[1].is_human && nb_ai_types > 0 && ai_types != NULL){
            opp_elo = cte_default_ai_elo(ai_types[0]);
        } else {
            s_cte_profile *opp_p = find_profile(&db, players->players[1].player_name);
            if(opp_p) opp_elo = opp_p->elo;
        }
    }

    uint16_t s0 = match->match_scores[0];
    uint16_t s1 = (players->size >= 2) ? match->match_scores[1] : 0;
    double score = 0.5;
    if(s0 > s1){
        p->matches_won++;
        score = 1.0;
    } else if(s1 > s0){
        p->matches_lost++;
        score = 0.0;
    } else {
        p->matches_tied++;
        score = 0.5;
    }

    int16_t delta = compute_elo_delta(p->elo, opp_elo, score, CTE_DEFAULT_K_FACTOR);
    p->elo += delta;
    if(p->elo < CTE_MIN_ELO) p->elo = CTE_MIN_ELO;
    p->last_played_at = (uint64_t)time(NULL);

    if(out_delta) *out_delta = delta;
    if(out_profile) *out_profile = *p;

    return save_profiles(&db);
}

t_cteerr record_match_result_in_profile(const char *profile_name,
                                        const struct s_cte_match *match,
                                        const struct s_cte_players *players,
                                        const e_cte_ai_type *ai_types,
                                        uint8_t nb_ai_types,
                                        s_cte_profile *out_profile,
                                        int16_t *out_delta)
{
    return record_match_result_in_profile_path(profile_name, match, players,
                                               ai_types, nb_ai_types,
                                               out_profile, out_delta, NULL);
}
