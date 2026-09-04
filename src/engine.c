#include "engine.h"
#include "move.h"

static const s_cte_engine_backend g_backend_array = {
    .type           = CTE_BACKEND_ARRAY,
    .name           = "Array (Reference)",
    .is_legal       = is_legal,
    .gen_card_moves = gen_card_moves,
    .gen_all_moves  = gen_all_moves,
};

extern const s_cte_engine_backend g_backend_bitboard;

const s_cte_engine_backend *cte_get_backend(e_cte_backend_type type){
    switch(type){
        case CTE_BACKEND_BITBOARD:
            return &g_backend_bitboard;
        case CTE_BACKEND_ARRAY:
            return &g_backend_array;
        default:
            return &g_backend_bitboard; // Default = bitboard SWAR
    }
}

