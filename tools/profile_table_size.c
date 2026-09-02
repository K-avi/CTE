#include "cte.h"
#include <stdio.h>
#include <stdlib.h>

int main(){
    s_cte_game game;
    char *names[2] = { "Greedy_1", "Greedy_2" };
    init_game(&game, 2, names, false);

    s_cte_round_config config = {
        .first_player  = 0,
        .is_team_mode  = false,
        .evaluators    = { eval_greedy, eval_greedy, NULL, NULL },
        .eval_contexts = { NULL, NULL, NULL, NULL },
    };

    uint64_t total_turns = 0;
    uint64_t total_table_cards = 0;
    uint32_t max_table_seen = 0;
    uint64_t size_histogram[53] = {0};

    const uint32_t NUM_ROUNDS = 10000;

    for(uint32_t r = 0; r < NUM_ROUNDS; r++){
        setup_round(&game);
        reset_all_players(&game.players);

        // Track table size across every turn
        while(game.deck.cur_card < 52 || game.players.players[0].hand.size > 0 || game.players.players[1].hand.size > 0){
            bool all_empty = (game.players.players[0].hand.size == 0 && game.players.players[1].hand.size == 0);
            if(all_empty){
                if(game.deck.cur_card >= 52) break;
                deal_next_hand(&game);
            }

            uint8_t cur_p = game.current_player_id;
            if(game.players.players[cur_p].hand.size == 0){
                game.current_player_id = 1 - cur_p;
                continue;
            }

            uint8_t cur_sz = (uint8_t)__builtin_popcountll(game.table_bb);
            total_table_cards += cur_sz;
            total_turns++;
            size_histogram[cur_sz]++;
            if(cur_sz > max_table_seen) max_table_seen = cur_sz;

            struct s_cte_move_list moves;
            init_move_list(&moves, 16);
            gen_all_moves(&moves, game.table_bb, &game.players.players[cur_p].hand);

            s_cte_game_state st = { .table_bb = game.table_bb, .players = &game.players, .deck = &game.deck, .current_player_id = cur_p };
            uint16_t chosen = eval_greedy(&st, &moves, NULL);
            bool cap = false;
            play_move(&game.table_bb, &moves.moves[chosen], &game.players.players[cur_p], &cap);
            if(cap) game.last_captor_id = cur_p;
            free_move_list(&moves);

            game.current_player_id = 1 - cur_p;
        }
    }

    double mean_size = (double)total_table_cards / (double)total_turns;

    printf("=======================================================\n");
    printf("  EMPIRICAL TABLE SIZE DISTRIBUTION (10,000 Rounds)    \n");
    printf("=======================================================\n");
    printf(" Total Turns Sampled : %lu\n", (unsigned long)total_turns);
    printf(" Mean Table Size     : %.2f cards\n", mean_size);
    printf(" Maximum Table Size  : %u cards\n\n", max_table_seen);
    printf(" Histogram :\n");
    for(int s = 0; s <= (int)max_table_seen; s++){
        double pct = (double)size_histogram[s] * 100.0 / (double)total_turns;
        printf("   Size %2d : %8lu turns (%5.1f%%)\n", s, (unsigned long)size_histogram[s], pct);
    }
    printf("=======================================================\n");

    free_game(&game);
    return 0;
}
