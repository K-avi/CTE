// tools/bench_fair.c — Fair AI (PIMC) optimization A/B benchmark harness
// Compares candidate Fair AI configuration vs frozen baseline Fair AI (fair_v0) head-to-head.
// Usage: ./build/bench_fair [nb_rounds] [seed] [candidate_opt] [baseline_opt]
// Default: 200 rounds, seed from CTE_TEST_SEED or default 42, candidate=ALL, baseline=NONE
#include "cte.h"
#include "pimc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

static uint32_t parse_single_flag(const char *token){
    if(!token) return 0;
    if(strcmp(token, "none") == 0 || strcmp(token, "base") == 0 || strcmp(token, "fair_v0") == 0){
        return PIMC_OPT_NONE;
    }
    if(strcmp(token, "d4") == 0 || strcmp(token, "deal4") == 0){
        return PIMC_OPT_SOLVE_DEAL4;
    }
    if(strcmp(token, "depth4") == 0){
        return PIMC_OPT_DEPTH4;
    }
    if(strcmp(token, "adaptive") == 0){
        return PIMC_OPT_ADAPTIVE;
    }
    if(strcmp(token, "borda") == 0 || strcmp(token, "voting") == 0){
        return PIMC_OPT_BORDA;
    }
    if(strcmp(token, "neg") == 0 || strcmp(token, "negative") == 0){
        return PIMC_OPT_NEG_INFERENCE;
    }
    if(strcmp(token, "iso") == 0 || strcmp(token, "isomorphism") == 0){
        return PIMC_OPT_ISO_DEDUP;
    }
    if(strcmp(token, "all") == 0){
        return PIMC_OPT_ALL;
    }
    char *endptr = NULL;
    unsigned long val = strtoul(token, &endptr, 0);
    if(endptr && *endptr == '\0') return (uint32_t)val;
    return 0;
}

static uint32_t parse_opt_flags(const char *token){
    if(!token) return PIMC_OPT_ALL;
    if(strchr(token, '+') || strchr(token, ',')){
        char buf[256];
        strncpy(buf, token, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *saveptr = NULL;
        char *p = strtok_r(buf, "+,", &saveptr);
        uint32_t flags = 0;
        while(p){
            flags |= parse_single_flag(p);
            p = strtok_r(NULL, "+,", &saveptr);
        }
        return flags;
    }
    return parse_single_flag(token);
}


static const char *opt_flags_to_string(uint32_t flags, char *buf, size_t buf_sz){
    if(flags == PIMC_OPT_NONE){
        snprintf(buf, buf_sz, "Baseline (fair_v0, depth 2, no opts)");
        return buf;
    }
    if(flags == PIMC_OPT_ALL){
        snprintf(buf, buf_sz, "All Optimizations Enabled");
        return buf;
    }
    buf[0] = '\0';
    size_t len = 0;
    if(flags & PIMC_OPT_SOLVE_DEAL4){
        len += snprintf(buf + len, buf_sz - len, "+Deal4 ");
    }
    if(flags & PIMC_OPT_DEPTH4){
        len += snprintf(buf + len, buf_sz - len, "+Depth4 ");
    }
    if(flags & PIMC_OPT_ADAPTIVE){
        len += snprintf(buf + len, buf_sz - len, "+Adaptive ");
    }
    if(flags & PIMC_OPT_BORDA){
        len += snprintf(buf + len, buf_sz - len, "+Borda ");
    }
    if(flags & PIMC_OPT_NEG_INFERENCE){
        len += snprintf(buf + len, buf_sz - len, "+NegInference ");
    }
    if(flags & PIMC_OPT_ISO_DEDUP){
        len += snprintf(buf + len, buf_sz - len, "+IsoDedup ");
    }
    if(len > 0 && buf[len - 1] == ' ') buf[len - 1] = '\0';
    return buf;
}

int main(int argc, char **argv){
    uint32_t nb_rounds = 200;
    unsigned int seed = 42;
    uint32_t cand_opts = CTE_PIMC_DEFAULT_OPTS;
    uint32_t base_opts = PIMC_OPT_NONE;
    bool base_is_greedy = false;

    if(argc >= 2){
        nb_rounds = (uint32_t)atoi(argv[1]);
        if(nb_rounds == 0) nb_rounds = 200;
    }

    if(argc >= 3){
        seed = (unsigned int)strtoul(argv[2], NULL, 0);
    } else {
        const char *env = getenv("CTE_TEST_SEED");
        if(env && *env) seed = (unsigned int)strtoul(env, NULL, 0);
    }

    if(argc >= 4){
        if(strcmp(argv[3], "default") == 0){
            cand_opts = CTE_PIMC_DEFAULT_OPTS;
        } else {
            cand_opts = parse_opt_flags(argv[3]);
        }
    }

    if(argc >= 5){
        if(strcmp(argv[4], "greedy") == 0){
            base_is_greedy = true;
        } else {
            base_opts = parse_opt_flags(argv[4]);
        }
    }

    srand(seed);

    char cand_desc[128], base_desc[128];
    opt_flags_to_string(cand_opts, cand_desc, sizeof(cand_desc));
    if(base_is_greedy){
        snprintf(base_desc, sizeof(base_desc), "Greedy Heuristic AI (AI_TYPE_GREEDY)");
    } else {
        opt_flags_to_string(base_opts, base_desc, sizeof(base_desc));
    }

    printf("====================================================================\n");
    printf("        CTE FAIR AI (PIMC) A/B BENCHMARK (HEAD-TO-HEAD)\n");
    printf("====================================================================\n");
    printf("[CONFIG] %u rounds, master seed %u\n", nb_rounds, seed);
    printf("  Candidate : %s (flags: 0x%02X)\n", cand_desc, cand_opts);
    printf("  Baseline  : %s\n", base_desc);
    printf("Reproduce: CTE_TEST_SEED=%u ./build/bench_fair %u %u 0x%02X %s\n\n",
           seed, nb_rounds, seed, cand_opts, base_is_greedy ? "greedy" : "base");

    s_cte_pimc_config candidate_cfg;
    pimc_config_init(&candidate_cfg, CTE_PIMC_DEFAULT_WORLDS, CTE_PIMC_DEFAULT_DEPTH, seed + 1);
    pimc_config_set_opts(&candidate_cfg, cand_opts);

    s_cte_pimc_config baseline_cfg;
    pimc_config_init(&baseline_cfg, CTE_PIMC_DEFAULT_WORLDS, CTE_PIMC_DEFAULT_DEPTH, seed + 2);
    pimc_config_set_opts(&baseline_cfg, base_opts);

    s_cte_bench_result res;
    t_cteerr err = cte_run_ai_benchmark(
        AI_TYPE_FAIR, &candidate_cfg,
        base_is_greedy ? AI_TYPE_GREEDY : AI_TYPE_FAIR,
        base_is_greedy ? NULL : &baseline_cfg,
        nb_rounds, &res
    );

    if(err != e_ok){
        fprintf(stderr, "Benchmark failed with error code %d\n", err);
        return 1;
    }

    snprintf(res.name_a, sizeof(res.name_a), "Candidate");
    snprintf(res.name_b, sizeof(res.name_b), "Baseline_v0");

    cte_print_bench_result(&res);

    double p_score = ((double)res.wins_a + 0.5 * (double)res.draws) / (double)nb_rounds;
    double ci_margin = 1.96 * sqrt((p_score * (1.0 - p_score)) / (double)nb_rounds) * 100.0;

    printf("METRICS & STATISTICAL ANALYSIS:\n");
    printf("  Candidate Win Rate   : %.1f%% (Wins: %u, Losses: %u, Draws: %u)\n",
           res.win_rate_a, res.wins_a, res.wins_b, res.draws);
    printf("  95%% Confidence Int.  : [%.1f%%, %.1f%%]\n",
           p_score * 100.0 - ci_margin, p_score * 100.0 + ci_margin);
    printf("  Average Points / Rnd : Candidate %.2f vs Baseline %.2f (Delta: %+.2f pts)\n",
           res.avg_pts_a, res.avg_pts_b, res.avg_pts_a - res.avg_pts_b);
    printf("  Total Tablics        : Candidate %lu vs Baseline %lu (Delta: %+ld)\n",
           (unsigned long)res.total_tablics_a, (unsigned long)res.total_tablics_b,
           (long)res.total_tablics_a - (long)res.total_tablics_b);
    printf("  Delta Elo Rating     : %+.1f Elo\n", res.delta_elo_a);
    printf("  Latency per Move     : Candidate %.1f us vs Baseline %.1f us\n",
           res.avg_latency_us_a, res.avg_latency_us_b);
    printf("  Total Search Nodes   : Candidate %lu vs Baseline %lu\n",
           (unsigned long)candidate_cfg.total_nodes, (unsigned long)baseline_cfg.total_nodes);
    if(res.total_moves_a > 0 && res.total_moves_b > 0){
        printf("  Nodes per Move       : Candidate %.0f vs Baseline %.0f\n",
               (double)candidate_cfg.total_nodes / (double)res.total_moves_a,
               (double)baseline_cfg.total_nodes / (double)res.total_moves_b);
    }
    printf("====================================================================\n");

    return 0;
}
