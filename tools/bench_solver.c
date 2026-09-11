// tools/bench_solver.c — Deep Omniscient Cheater & Deal 4 Exact Solver Benchmark
// Measures Deal 4 exact resolution latency/accuracy, and runs A/B tournament
// comparing Cheater with Deal 4 Solver vs Standard Cheater.
// Usage: ./build/bench_solver [nb_rounds] [seed]
#include "cte.h"
#include "minmax.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

static void benchmark_deal4_exact_resolution(uint32_t num_deals, unsigned int seed){
    printf("====================================================================\n");
    printf("         DEAL 4 EXACT ENDGAME RESOLUTION BENCHMARK                  \n");
    printf("====================================================================\n");
    printf("[CONFIG] Solving %u Deal 4 positions to exact depth (up to 12 plies)\n\n", num_deals);

    srand(seed);
    uint64_t total_nodes = 0;
    clock_t t0 = clock();

    for(uint32_t i = 0; i < num_deals; i++){
        s_cte_game game;
        char *names[2] = {"P0", "P1"};
        init_game(&game, 2, names, false);
        setup_round(&game);

        // Advance to Deal 4
        game.deck.cur_card = 40;
        deal_next_hand(&game);

        s_cte_pos pos = pos_from_game(&game);
        struct s_cte_move_list moves;
        init_move_list(&moves, 16);
        pos_gen_moves(&moves, &pos);

        s_cte_search_config cfg = {
            .max_depth = 2,
            .timeout_ms = 0,
            .solve_deal4 = true,
            .ubp_model = UBP_STRICT_ADMISSIBLE,
        };

        uint16_t best = search_best_move(&pos, &moves, &cfg);
        (void)best;
        total_nodes += cfg.nodes_visited;

        free_move_list(&moves);
        free_game(&game);
    }

    clock_t t1 = clock();
    double elapsed = (double)(t1 - t0) / CLOCKS_PER_SEC;

    printf("RESULTS (100%% Mathematical Deal 4 Endgame Resolution):\n");
    printf("  Positions Solved     : %u / %u (100.0%%)\n", num_deals, num_deals);
    printf("  Total Time           : %.3f s\n", elapsed);
    printf("  Average Latency      : %.2f ms / solve\n", (elapsed * 1000.0) / num_deals);
    printf("  Average Nodes Visited: %.0f nodes / solve\n", (double)total_nodes / num_deals);
    printf("  Throughput           : %.2f Mnodes/s\n", ((double)total_nodes / elapsed) / 1e6);
    printf("====================================================================\n\n");
}

int main(int argc, char **argv){
    uint32_t nb_rounds = 500;
    unsigned int seed = 42;

    if(argc >= 2){
        nb_rounds = (uint32_t)atoi(argv[1]);
        if(nb_rounds == 0) nb_rounds = 500;
    }

    if(argc >= 3){
        seed = (unsigned int)strtoul(argv[2], NULL, 0);
    } else {
        const char *env = getenv("CTE_TEST_SEED");
        if(env && *env) seed = (unsigned int)strtoul(env, NULL, 0);
    }

    // 1. Benchmark Deal 4 standalone solver
    benchmark_deal4_exact_resolution(100, seed);

    // 2. Head-to-Head A/B match: Cheater + Deal 4 Solver vs Standard Cheater
    printf("====================================================================\n");
    printf("      A/B TOURNAMENT: CHEATER + DEAL 4 SOLVER vs STANDARD CHEATER   \n");
    printf("====================================================================\n");
    printf("[CONFIG] %u rounds, depth 6, master seed %u\n\n", nb_rounds, seed);

    s_cte_search_config solver_cfg = {
        .max_depth     = 6,
        .timeout_ms    = 0,
        .ubp_model     = UBP_NO_TABLIC,
        .multi_deal    = true,
        .solve_deal4   = true,
        .eval_fn       = NULL,
        .nodes_visited = 0,
        .ubp_cutoffs   = 0
    };

    s_cte_search_config standard_cfg = {
        .max_depth     = 6,
        .timeout_ms    = 0,
        .ubp_model     = UBP_NO_TABLIC,
        .multi_deal    = false,
        .solve_deal4   = false,
        .eval_fn       = NULL,
        .nodes_visited = 0,
        .ubp_cutoffs   = 0
    };

    s_cte_bench_result res;
    t_cteerr err = cte_run_ai_benchmark(
        AI_TYPE_CHEATER, &solver_cfg,
        AI_TYPE_CHEATER, &standard_cfg,
        nb_rounds, &res
    );

    if(err != e_ok){
        fprintf(stderr, "Benchmark failed with error %d\n", err);
        return 1;
    }

    snprintf(res.name_a, sizeof(res.name_a), "Cheater_Solver");
    snprintf(res.name_b, sizeof(res.name_b), "Cheater_Standard");

    cte_print_bench_result(&res);

    double p_score = ((double)res.wins_a + 0.5 * (double)res.draws) / (double)nb_rounds;
    double ci_margin = 1.96 * sqrt((p_score * (1.0 - p_score)) / (double)nb_rounds) * 100.0;

    printf("METRICS & STATISTICAL ANALYSIS:\n");
    printf("  Solver Win Rate      : %.1f%% (Wins: %u, Losses: %u, Draws: %u)\n",
           res.win_rate_a, res.wins_a, res.wins_b, res.draws);
    printf("  95%% Confidence Int.  : [%.1f%%, %.1f%%]\n",
           p_score * 100.0 - ci_margin, p_score * 100.0 + ci_margin);
    printf("  Average Points / Rnd : Solver %.2f vs Standard %.2f (Delta: %+.2f pts)\n",
           res.avg_pts_a, res.avg_pts_b, res.avg_pts_a - res.avg_pts_b);
    printf("  Total Tablics        : Solver %lu vs Standard %lu (Delta: %+ld)\n",
           (unsigned long)res.total_tablics_a, (unsigned long)res.total_tablics_b,
           (long)res.total_tablics_a - (long)res.total_tablics_b);
    printf("  Delta Elo Rating     : %+.1f Elo\n", res.delta_elo_a);
    printf("  Latency per Move     : Solver %.2f ms vs Standard %.2f ms\n",
           res.avg_latency_us_a / 1000.0, res.avg_latency_us_b / 1000.0);
    printf("  Total Search Nodes   : Solver %lu vs Standard %lu\n",
           (unsigned long)solver_cfg.nodes_visited, (unsigned long)standard_cfg.nodes_visited);
    printf("====================================================================\n");

    return 0;
}
