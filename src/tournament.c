#include "tournament.h"
#include "eval.h"
#include "profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

t_cteerr init_tournament(s_cte_tournament *t, const s_cte_tournament_config *cfg){
    if(!t || !cfg) return e_null;
    if(cfg->nb_participants < 2 || cfg->nb_participants > CTE_MAX_TOURNAMENT_PLAYERS){
        return e_inval_val;
    }

    if(cfg->type == TOURNAMENT_KNOCKOUT){
        uint8_t n = cfg->nb_participants;
        if((n & (n - 1)) != 0){
            return e_inval_val; // Must be power of 2 (2, 4, 8, 16)
        }
    }

    for(uint8_t i = 0; i < cfg->nb_participants; i++){
        if(cfg->participants[i].name[0] == '\0'){
            return e_inval_val; // Empty name not allowed
        }
        for(uint8_t j = (uint8_t)(i + 1); j < cfg->nb_participants; j++){
            if(strcasecmp(cfg->participants[i].name, cfg->participants[j].name) == 0){
                return e_inval_val; // Duplicate name not allowed
            }
        }
    }

    memset(t, 0, sizeof(s_cte_tournament));
    t->config = *cfg;
    t->champion_idx = -1;

    for(uint8_t i = 0; i < cfg->nb_participants; i++){
        t->standings[i] = i;
        s_cte_tournament_participant *p = &t->config.participants[i];

        if(p->elo_start == 0){
            if(t->config.profile_db != NULL){
                s_cte_profile *prof = find_profile(t->config.profile_db, p->name);
                if(prof){
                    p->elo_start = prof->elo;
                } else if(p->is_human){
                    p->elo_start = CTE_DEFAULT_ELO;
                } else {
                    p->elo_start = cte_default_ai_elo(p->ai_type);
                }
            } else {
                if(p->is_human){
                    p->elo_start = CTE_DEFAULT_ELO;
                } else {
                    p->elo_start = cte_default_ai_elo(p->ai_type);
                }
            }
        }
        if(p->elo_current == 0){
            p->elo_current = p->elo_start;
        }
    }

    return e_ok;
}

static t_cteerr play_tournament_match(s_cte_tournament *t,
                                      uint8_t p1_idx,
                                      uint8_t p2_idx,
                                      uint8_t stage,
                                      bool force_decisive_winner,
                                      s_cte_tournament_match *out_match)
{
    s_cte_tournament_participant *p1 = &t->config.participants[p1_idx];
    s_cte_tournament_participant *p2 = &t->config.participants[p2_idx];

    s_cte_game game;
    char *names[2] = { p1->name, p2->name };
    t_cteerr err = init_game(&game, 2, names, false);
    if(err != e_ok) return err;

    game.players.players[0].is_human    = p1->is_human;
    game.players.players[0].evaluator   = p1->evaluator;
    game.players.players[0].eval_context= p1->eval_context;

    game.players.players[1].is_human    = p2->is_human;
    game.players.players[1].evaluator   = p2->evaluator;
    game.players.players[1].eval_context= p2->eval_context;

    struct s_cte_match match;
    err = init_match(&match, &game, t->config.winning_score > 0 ? t->config.winning_score : 101);
    if(err != e_ok){
        free_game(&game);
        return err;
    }
    match.max_rounds = t->config.max_rounds > 0 ? t->config.max_rounds : 10;

    bool has_human = p1->is_human || p2->is_human;
    bool match_silent = t->config.silent && !has_human;

    s_cte_round_config r_cfg = {
        .first_player  = (stage + p1_idx + p2_idx) % 2,
        .is_team_mode  = false,
        .evaluators    = { NULL, NULL, NULL, NULL },
        .eval_contexts = { NULL, NULL, NULL, NULL },
        .callbacks     = match_silent ? NULL : t->config.callbacks,
        .ui_context    = t->config.ui_context,
    };

    err = run_match(&match, &r_cfg);
    if(err != e_ok){
        free_game(&game);
        return err;
    }

    // In Knockout mode, if match ends in a tie, play sudden-death rounds until broken
    if(force_decisive_winner && match.match_scores[0] == match.match_scores[1]){
        uint8_t tiebreaker_rounds = 0;
        match.winning_score = UINT16_MAX; // Allow rounds to be played even if score >= original target
        while(match.match_scores[0] == match.match_scores[1] && tiebreaker_rounds < 3){
            match.max_rounds = match.round_nb + 1;
            err = run_match(&match, &r_cfg);
            if(err != e_ok){
                free_game(&game);
                return err;
            }
            tiebreaker_rounds++;
        }
        // Fallback tiebreak for completely passive bot pairings
        if(match.match_scores[0] == match.match_scores[1]){
            if(p1->total_points > p2->total_points){
                match.match_scores[0]++;
            } else if(p2->total_points > p1->total_points){
                match.match_scores[1]++;
            } else {
                match.match_scores[p1_idx % 2]++;
            }
        }
    }

    uint16_t s1 = match.match_scores[0];
    uint16_t s2 = match.match_scores[1];

    p1->matches_played++;
    p2->matches_played++;
    p1->total_points += s1;
    p2->total_points += s2;
    p1->total_tablics += match.match_tablics[0];
    p2->total_tablics += match.match_tablics[1];

    int8_t winner = -1;
    if(s1 > s2){
        p1->matches_won++;
        p2->matches_lost++;
        winner = 0;
    } else if(s2 > s1){
        p2->matches_won++;
        p1->matches_lost++;
        winner = 1;
    } else {
        p1->matches_tied++;
        p2->matches_tied++;
        winner = -1;
    }

    // In-tournament Elo rating update
    double score1 = (winner == 0) ? 1.0 : (winner == -1) ? 0.5 : 0.0;
    double score2 = 1.0 - score1;
    int16_t d1 = compute_elo_delta(p1->elo_current, p2->elo_current, score1, CTE_DEFAULT_K_FACTOR);
    int16_t d2 = compute_elo_delta(p2->elo_current, p1->elo_current, score2, CTE_DEFAULT_K_FACTOR);
    p1->elo_current += d1;
    p2->elo_current += d2;
    if(p1->elo_current < 100) p1->elo_current = 100;
    if(p2->elo_current < 100) p2->elo_current = 100;

    if(out_match){
        out_match->p1_idx        = p1_idx;
        out_match->p2_idx        = p2_idx;
        out_match->score_p1      = s1;
        out_match->score_p2      = s2;
        out_match->winner_idx    = winner;
        out_match->bracket_stage = stage;
    }

    free_game(&game);
    return e_ok;
}

static int compare_standings(const void *a, const void *b, void *thunk){
    const s_cte_tournament *t = (const s_cte_tournament*)thunk;
    uint8_t idx_a = *(const uint8_t*)a;
    uint8_t idx_b = *(const uint8_t*)b;

    const s_cte_tournament_participant *pa = &t->config.participants[idx_a];
    const s_cte_tournament_participant *pb = &t->config.participants[idx_b];

    // 1. Most wins
    if(pa->matches_won != pb->matches_won){
        return (int)pb->matches_won - (int)pa->matches_won;
    }
    // 2. Fewest losses
    if(pa->matches_lost != pb->matches_lost){
        return (int)pa->matches_lost - (int)pb->matches_lost;
    }
    // 3. Highest total points
    if(pa->total_points != pb->total_points){
        return (int)pb->total_points - (int)pa->total_points;
    }
    // 4. Most tablics
    if(pa->total_tablics != pb->total_tablics){
        return (int)pb->total_tablics - (int)pa->total_tablics;
    }
    return 0;
}

static void sort_standings(s_cte_tournament *t){
    uint8_t n = t->config.nb_participants;
    // Simple insertion sort using compare_standings logic
    for(uint8_t i = 1; i < n; i++){
        uint8_t key = t->standings[i];
        int j = (int)i - 1;
        while(j >= 0 && compare_standings(&t->standings[j], &key, t) > 0){
            t->standings[j + 1] = t->standings[j];
            j--;
        }
        t->standings[j + 1] = key;
    }
}

t_cteerr run_tournament(s_cte_tournament *t){
    if(!t) return e_null;
    t->nb_matches = 0;

    if(t->config.type == TOURNAMENT_ROUND_ROBIN){
        uint8_t n = t->config.nb_participants;
        // Each pair plays once
        for(uint8_t i = 0; i < n; i++){
            for(uint8_t j = (uint8_t)(i + 1); j < n; j++){
                if(t->nb_matches >= CTE_MAX_TOURNAMENT_MATCHES) break;

                s_cte_tournament_match m;
                t_cteerr err = play_tournament_match(t, i, j, 0, false, &m);
                if(err != e_ok) return err;

                t->matches[t->nb_matches++] = m;
            }
        }
        sort_standings(t);
        t->champion_idx = (int8_t)t->standings[0];
        return e_ok;
    }

    if(t->config.type == TOURNAMENT_KNOCKOUT){
        uint8_t pool[CTE_MAX_TOURNAMENT_PLAYERS];
        uint8_t pool_size = t->config.nb_participants;
        for(uint8_t i = 0; i < pool_size; i++) pool[i] = i;

        uint8_t stage = 0;
        while(pool_size > 1){
            uint8_t next_pool[CTE_MAX_TOURNAMENT_PLAYERS];
            uint8_t next_size = 0;

            for(uint8_t k = 0; k < pool_size; k += 2){
                if(t->nb_matches >= CTE_MAX_TOURNAMENT_MATCHES) break;

                uint8_t p1 = pool[k];
                uint8_t p2 = pool[k + 1];

                s_cte_tournament_match m;
                t_cteerr err = play_tournament_match(t, p1, p2, stage, true, &m);
                if(err != e_ok) return err;

                t->matches[t->nb_matches++] = m;

                uint8_t winner_id = (m.winner_idx == 0) ? p1 : p2;
                next_pool[next_size++] = winner_id;
            }

            pool_size = next_size;
            for(uint8_t i = 0; i < pool_size; i++) pool[i] = next_pool[i];
            stage++;
        }

        t->champion_idx = (int8_t)pool[0];
        sort_standings(t);
        return e_ok;
    }

    return e_inval_val;
}

t_cteerr sync_tournament_profiles(const s_cte_tournament *t){
    if(!t) return e_null;
    s_cte_profile_db *db = t->config.profile_db;
    if(!db) return e_ok;

    for(uint8_t i = 0; i < t->config.nb_participants; i++){
        const s_cte_tournament_participant *part = &t->config.participants[i];
        if(!part->is_human && !t->config.persist_ai){
            continue;
        }
        s_cte_profile *p = find_profile(db, part->name);
        if(!p){
            p = find_or_create_profile(db, part->name);
        }
        if(p){
            p->elo = part->elo_current;
            p->matches_played += part->matches_played;
            p->matches_won += part->matches_won;
            p->matches_lost += part->matches_lost;
            p->matches_tied += part->matches_tied;
            p->total_points += part->total_points;
            p->total_tablics += part->total_tablics;
            p->last_played_at = (uint64_t)time(NULL);
        }
    }

    return save_profiles(db);
}

void print_tournament_standings(const s_cte_tournament *t, e_cte_render_style style){
    if(!t) return;
    (void)style;

    printf("\n===================================================================================================\n");
    if(t->config.type == TOURNAMENT_ROUND_ROBIN){
        printf("                             CTE TOURNAMENT — ROUND ROBIN STANDINGS                                \n");
    } else {
        printf("                             CTE TOURNAMENT — KNOCKOUT CUP STANDINGS                               \n");
    }
    printf("===================================================================================================\n");
    printf(" Rank | %-20s | Elo Bef | Elo Aft | Delta | Won  | Lost | Tied | Pts   | Tablics | Win%%  \n", "Participant");
    printf("------|----------------------|---------|---------|-------|------|------|------|-------|---------|-------\n");

    for(uint8_t r = 0; r < t->config.nb_participants; r++){
        uint8_t idx = t->standings[r];
        const s_cte_tournament_participant *p = &t->config.participants[idx];

        double win_rate = (p->matches_played > 0)
            ? ((double)p->matches_won / (double)p->matches_played) * 100.0
            : 0.0;

        char rank_str[16];
        if(idx == t->champion_idx){
            snprintf(rank_str, sizeof(rank_str), " [1] *");
        } else {
            snprintf(rank_str, sizeof(rank_str), "  %u   ", (unsigned)(r + 1));
        }

        printf("%s| %-20s |  %5d  |  %5d  | %+5d |  %2u  |  %2u  |  %2u  | %5u |   %3u   | %5.1f%%\n",
               rank_str,
               p->name,
               (int)p->elo_start,
               (int)p->elo_current,
               (int)(p->elo_current - p->elo_start),
               (unsigned)p->matches_won,
               (unsigned)p->matches_lost,
               (unsigned)p->matches_tied,
               (unsigned)p->total_points,
               (unsigned)p->total_tablics,
               win_rate);
    }
    printf("===================================================================================================\n");

    if(t->champion_idx >= 0 && t->champion_idx < t->config.nb_participants){
        printf(" >>> TOURNAMENT CHAMPION: %s <<<\n", t->config.participants[t->champion_idx].name);
        printf("===================================================================================================\n\n");
    }
}

void free_tournament(s_cte_tournament *t){
    if(!t) return;
    t->nb_matches = 0;
}
