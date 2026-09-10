#include "test_common.h"

int main(void) {
    printf("===================================================\n");
    printf("              CTE TEST SUITE RUNNER                \n");
    printf("===================================================\n");
    test_get_seed();
    printf("\n");

    printf("[RUN] Running Core Game Logic tests...\n");
    run_test_core();
    printf("[PASS] Core Game Logic tests passed.\n");

    printf("[RUN] Running Move Generation & Legality tests...\n");
    run_test_moves();
    printf("[PASS] Move Generation & Legality tests passed.\n");

    printf("[RUN] Running Scoring & Invariance tests...\n");
    run_test_scoring();
    printf("[PASS] Scoring & Invariance tests passed.\n");

    printf("[RUN] Running AI, Minimax & Backend tests...\n");
    run_test_ai();
    printf("[PASS] AI, Minimax & Backend tests passed.\n");

    printf("[RUN] Running Tournament, Profiles & Elo tests...\n");
    run_test_tournament();
    printf("[PASS] Tournament, Profiles & Elo tests passed.\n");

    printf("===================================================\n");
    printf("       All 38 tests passed successfully!           \n");
    printf("===================================================\n");

    return 0;
}
