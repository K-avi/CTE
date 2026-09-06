#ifndef __CTE_TEST_COMMON_H
#define __CTE_TEST_COMMON_H

#include "cte.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

// Module runner prototypes
int run_test_core(void);
int run_test_moves(void);
int run_test_scoring(void);
int run_test_ai(void);
int run_test_tournament(void);

#endif
