// tools/bench_cheater.c — Cheater AI optimization A/B benchmark harness
// Compares candidate (pos_evaluate) vs frozen baseline (pos_evaluate_v0) head-to-head.
// Usage: ./build/bench_cheater [nb_rounds] [seed]
// Default: 1000 rounds, seed from CTE_TEST_SEED or default 42
#include "cte.h"
#include "minmax.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

int main(int argc, char **argv){
    uint32_t nb_rounds = 1000;
    unsigned int seed = 42;

    if(argc >= 2){
        nb_rounds = (uint32_t)atoi(argv[1]);
        if(nb_rounds == 0) nb_rounds = 1000;
    }

    if(argc >= 3){
        seed = (unsigned int)strtoul(argv[2], NULL, 0);
    } else {
        const char *env = getenv("CTE_TEST_SEED");
        if(env && *env) seed = (unsigned int)strtoul(env, NULL, 0);
    }

    srand(seed);

    printf("====================================================================\n");
    printf("        CTE CHEATER AI A/B BENCHMARK (HIGH ITERATION)\n");
    printf("====================================================================\n");
    printf("[CONFIG] %u rounds, depth 6, UBP_NO_TABLIC, master seed %u\n", nb_rounds, seed);
    printf("Reproduce: CTE_TEST_SEED=%u ./build/bench_cheater %u\n\n", seed, nb_rounds);

    s_cte_search_config candidate_cfg = {
        .max_depth     = 6,
        .timeout_ms    = 0,
        .ubp_model     = UBP_NO_TABLIC,
        .eval_fn       = NULL,  // Candidate pos_evaluate
        .nodes_visited = 0,
        .ubp_cutoffs   = 0
    };

    s_cte_search_config baseline_cfg = {
        .max_depth     = 6,
        .timeout_ms    = 0,
        .ubp_model     = UBP_NO_TABLIC,
        .eval_fn       = pos_evaluate_v0,  // Frozen baseline
        .nodes_visited = 0,
        .ubp_cutoffs   = 0
    };

    s_cte_bench_result res;
    t_cteerr err = cte_run_ai_benchmark(
        AI_TYPE_CHEATER, &candidate_cfg,
        AI_TYPE_CHEATER, &baseline_cfg,
        nb_rounds, &res
    );

    if(err != e_ok){
        fprintf(stderr, "Benchmark failed with error %d\n", err);
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
    printf("  Latency per Move     : Candidate %.2f ms vs Baseline %.2f ms\n",
           res.avg_latency_us_a / 1000.0, res.avg_latency_us_b / 1000.0);
    printf("  Total Search Nodes   : Candidate %lu vs Baseline %lu\n",
           (unsigned long)candidate_cfg.nodes_visited, (unsigned long)baseline_cfg.nodes_visited);
    if(res.total_moves_a > 0){
        printf("  Nodes per Move       : Candidate %.0f vs Baseline %.0f\n",
               (double)candidate_cfg.nodes_visited / (double)res.total_moves_a,
               (double)baseline_cfg.nodes_visited / (double)res.total_moves_b);
    }
    printf("====================================================================\n");

    return 0;
}
