#include "eval.h"
#include <stdlib.h>

// Pure uniform-random move evaluator (zero I/O)
uint16_t eval_random(const s_cte_game_state *state,
                     const struct s_cte_move_list *moves,
                     void *ctx)
{
    (void)state;
    (void)ctx;
    if(!moves || moves->size == 0) return 0;
    return (uint16_t)(rand() % moves->size);
}
