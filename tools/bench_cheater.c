// tools/bench_cheater.c — Cheater AI optimization A/B benchmark harness
// Compares baseline (pos_evaluate_v0) vs candidate (pos_evaluate) head-to-head.
// Usage: ./build/bench_cheater [nb_rounds] [seed]
// Default: 200 rounds, seed from CTE_TEST_SEED or time-based
#include "cte.h"
#include "minmax.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv){
    uint32_t nb_rounds = 200;
    unsigned int seed = 0;

    if(argc >= 2) nb_rounds = (uint32_t)atoi(argv[1]);
    if(nb_rounds == 0) nb_rounds = 200;

    // Seed handling: CLI arg > env > entropy
    if(argc >= 3){
        seed = (unsigned int)strtoul(argv[2], NULL, 0);
    } else {
        const char *env = getenv("CTE_TEST_SEED");
        if(env && *env){
            seed = (unsigned int)strtoul(env, NULL, 0);
        } else {
            seed = (unsigned int)(time(NULL) ^ ((unsigned int)getpid() << 16));
        }
    }
    srand(seed);

    printf("====================================================================\n");
    printf("        CTE CHEATER AI A/B BENCHMARK\n");
    printf("====================================================================\n");
    printf("[SEED] %u (reproduce: CTE_TEST_SEED=%u make bench-cheater)\n", seed, seed);
    printf("[CONFIG] %u rounds, depth 6, UBP_NO_TABLIC\n\n", nb_rounds);

    // --- A/B Benchmark: Candidate (current pos_evaluate) vs Baseline (pos_evaluate_v0) ---
    s_cte_search_config candidate_cfg = {
        .max_depth     = 6,
        .timeout_ms    = 0,
        .ubp_model     = UBP_NO_TABLIC,
        .eval_fn       = NULL,  // NULL = use current pos_evaluate
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

    s_cte_bench_result res_ab;
    t_cteerr err = cte_run_ai_benchmark(
        AI_TYPE_CHEATER, &candidate_cfg,
        AI_TYPE_CHEATER, &baseline_cfg,
        nb_rounds, &res_ab);

    if(err != e_ok){
        fprintf(stderr, "A/B Benchmark failed with error %d\n", err);
        return 1;
    }

    // Override names for clarity
    snprintf(res_ab.name_a, sizeof(res_ab.name_a), "Candidate");
    snprintf(res_ab.name_b, sizeof(res_ab.name_b), "Baseline_v0");

    printf("--- Candidate (pos_evaluate) vs Baseline (pos_evaluate_v0) ---\n");
    cte_print_bench_result(&res_ab);

    printf("SUMMARY:\n");
    printf("  Candidate win rate : %.1f%%\n", res_ab.win_rate_a);
    printf("  Candidate avg pts  : %.2f\n", res_ab.avg_pts_a);
    printf("  Baseline  avg pts  : %.2f\n", res_ab.avg_pts_b);
    printf("  Delta Elo          : %+.1f (candidate over baseline)\n", res_ab.delta_elo_a);
    printf("  Candidate latency  : %.2f ms/move\n", res_ab.avg_latency_us_a / 1000.0);
    printf("  Baseline  latency  : %.2f ms/move\n", res_ab.avg_latency_us_b / 1000.0);
    printf("  Candidate nodes    : %lu total\n", (unsigned long)candidate_cfg.nodes_visited);
    printf("  Baseline  nodes    : %lu total\n", (unsigned long)baseline_cfg.nodes_visited);
    if(res_ab.total_moves_a > 0){
        printf("  Candidate nodes/mv : %.0f\n",
               (double)candidate_cfg.nodes_visited / (double)res_ab.total_moves_a);
    }
    if(res_ab.total_moves_b > 0){
        printf("  Baseline  nodes/mv : %.0f\n",
               (double)baseline_cfg.nodes_visited / (double)res_ab.total_moves_b);
    }
    printf("====================================================================\n");

    return 0;
}
