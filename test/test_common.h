#ifndef __CTE_TEST_COMMON_H
#define __CTE_TEST_COMMON_H

#include "cte.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include <time.h>

// Master seed infrastructure for reproducible fuzzing
static inline unsigned int test_get_seed(void) {
    const char *logged = getenv("CTE_TEST_SEED_LOGGED");
    const char *env = getenv("CTE_TEST_SEED");
    unsigned int seed = 0;

    if (env && *env) {
        seed = (unsigned int)strtoul(env, NULL, 0);
        if (!logged) {
            printf("[SEED] Using fixed seed from CTE_TEST_SEED: %u\n", seed);
            setenv("CTE_TEST_SEED_LOGGED", "1", 1);
            srand(seed);
        }
        return seed;
    }

    seed = (unsigned int)(time(NULL) ^ ((unsigned int)getpid() << 16));
    char seed_str[32];
    snprintf(seed_str, sizeof(seed_str), "%u", seed);
    setenv("CTE_TEST_SEED", seed_str, 1);
    if (!logged) {
        printf("[SEED] Master PRNG Seed: %u (reproduce with: CTE_TEST_SEED=%u make test)\n",
               seed, seed);
        setenv("CTE_TEST_SEED_LOGGED", "1", 1);
    }
    srand(seed);
    return seed;
}

// Module runner prototypes
int run_test_core(void);
int run_test_moves(void);
int run_test_scoring(void);
int run_test_ai(void);
int run_test_tournament(void);

#endif
