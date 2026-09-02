#include "engine.h"

static s_cte_pos backend_array_to_pos(const s_cte_game *game){
    if(!game){
        s_cte_pos empty = {0};
        return empty;
    }
    s_cte_game_state st = {
        .table_bb          = game->table_bb,
        .players           = &game->players,
        .deck              = &game->deck,
        .current_player_id = game->current_player_id,
        .is_team_mode      = game->is_team_mode,
    };
    return pos_from_state(&st);
}

static const s_cte_engine_backend g_backend_array = {
    .type            = CTE_BACKEND_ARRAY,
    .name            = "Array (Reference)",
    .init_game       = init_game,
    .free_game       = free_game,
    .setup_round     = setup_round,
    .deal_next_hand  = deal_next_hand,
    .award_remaining = award_remaining_table_cards,
    .run_round       = run_round,
    .is_legal        = is_legal,
    .gen_card_moves  = gen_card_moves,
    .gen_all_moves   = gen_all_moves,
    .play_move       = play_move,
    .score_move      = score_move,
    .to_pos          = backend_array_to_pos,
};

extern const s_cte_engine_backend g_backend_bitboard;
extern const s_cte_engine_backend g_backend_bitboard_table;
extern const s_cte_engine_backend g_backend_bitboard_rank;

const s_cte_engine_backend *cte_get_backend(e_cte_backend_type type){
    switch(type){
        case CTE_BACKEND_BITBOARD:
            return &g_backend_bitboard;
        case CTE_BACKEND_BITBOARD_TABLE:
            return &g_backend_bitboard_table;
        case CTE_BACKEND_BITBOARD_RANK:
            return &g_backend_bitboard_rank;
        case CTE_BACKEND_ARRAY:
        default:
            return &g_backend_array;
    }
}
