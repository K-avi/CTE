#include "game.h"
#include <stdlib.h>
#include <string.h>

t_cteerr setup_game(struct s_cte_players *players){
    if(!players) return e_null;
    if(players->size < 2 || players->size > 4) return e_inval_val;

    // Initialize unshuffled deck 0..51
    for(int i = 0 ; i < 52; i++){
        deck.cards[i] = i;
    }

    // Fisher-Yates shuffle
    for(int i = 51; i > 0; i--){
        int j = rand() % (i + 1);
        t_card temp = deck.cards[i];
        deck.cards[i] = deck.cards[j];
        deck.cards[j] = temp;
    }

    // Dynamic dealing: 12 / P cards per player (6 for 2p, 4 for 3p, 3 for 4p)
    uint8_t cards_per_player = (uint8_t)(12 / players->size);
    for(uint8_t p = 0; p < players->size; p++){
        for(uint8_t i = 0; i < cards_per_player; i++){
            players->players[p].hand.array[i] = deck.cards[p * cards_per_player + i];
        }
        players->players[p].hand.size = cards_per_player;
    }

    // Put 4 cards on the table
    for(int i = 0; i < 4; i++){
        table.cards_on_table[i] = deck.cards[12 + i];
    }
    table.nb_cards_on_table = 4;
    deck.cur_card = 16;
    
    return e_ok;
}

t_cteerr deal_next_hand(struct s_cte_players *players){
    if(!players) return e_null;
    if(players->size < 2 || players->size > 4) return e_inval_val;
    if(deck.cur_card + 12 > DECKSIZE) return e_inval_val;

    uint8_t cards_per_player = (uint8_t)(12 / players->size);
    for(uint8_t p = 0; p < players->size; p++){
        for(uint8_t i = 0; i < cards_per_player; i++){
            players->players[p].hand.array[i] = deck.cards[deck.cur_card + p * cards_per_player + i];
        }
        players->players[p].hand.size = cards_per_player;
    }
    deck.cur_card += 12;

    return e_ok;
}

t_cteerr award_remaining_table_cards(struct s_cte_players *players, uint8_t last_captor_id){
    if(!players) return e_null;
    if(last_captor_id >= players->size) return e_inval_val;
    if(table.nb_cards_on_table == 0) return e_ok;

    struct s_cte_player_data *captor = &players->players[last_captor_id];
    for(uint8_t i = 0; i < table.nb_cards_on_table; i++){
        if(captor->won_cards.size < 52){
            captor->won_cards.array[captor->won_cards.size++] = table.cards_on_table[i];
        }
    }
    table.nb_cards_on_table = 0;

    return e_ok;
}

t_cteerr run_round(struct s_cte_players *players, const s_cte_round_config *config){
    if(!players || !config) return e_null;
    if(players->size < 2 || players->size > 4) return e_inval_val;

    for(uint8_t i = 0; i < players->size; i++){
        t_evaluator eval_func = players->players[i].evaluator ? players->players[i].evaluator : config->evaluators[i];
        if(!eval_func) return e_null;
    }

    t_cteerr err = setup_game(players);
    if(err != e_ok) return err;

    if(config->callbacks && config->callbacks->on_deal){
        config->callbacks->on_deal(players, config->ui_context);
    }

    uint8_t current = config->first_player % players->size;
    int8_t last_captor_id = -1;

    for(;;){
        bool all_hands_empty = true;
        for(uint8_t i = 0; i < players->size; i++){
            if(players->players[i].hand.size > 0){
                all_hands_empty = false;
                break;
            }
        }

        if(all_hands_empty){
            if(deck.cur_card >= DECKSIZE) break;
            err = deal_next_hand(players);
            if(err != e_ok) return err;

            if(config->callbacks && config->callbacks->on_deal){
                config->callbacks->on_deal(players, config->ui_context);
            }
        }

        if(players->players[current].hand.size == 0){
            current = (uint8_t)((current + 1) % players->size);
            continue;
        }

        struct s_cte_move_list moves;
        err = init_move_list(&moves, 16);
        if(err != e_ok) return err;

        err = gen_all_moves(&moves, &players->players[current].hand);
        if(err != e_ok){ free_move_list(&moves); return err; }

        s_cte_game_state state = {
            .table             = &table,
            .players           = players,
            .current_player_id = current,
        };

        if(config->callbacks && config->callbacks->on_turn_start){
            config->callbacks->on_turn_start(&state, &moves, config->ui_context);
        }

        t_evaluator eval_func = players->players[current].evaluator ? players->players[current].evaluator : config->evaluators[current];
        void *eval_ctx = players->players[current].eval_context ? players->players[current].eval_context : config->eval_contexts[current];

        uint16_t chosen_idx = eval_func(&state, &moves, eval_ctx);
        if(chosen_idx >= moves.size){ free_move_list(&moves); return e_inval_val; }

        struct s_cte_move chosen_move = moves.moves[chosen_idx];
        bool captured = false;
        err = play_move(&chosen_move, &players->players[current], &captured);
        
        if(config->callbacks && config->callbacks->on_move_played){
            config->callbacks->on_move_played(&players->players[current], &chosen_move, captured, config->ui_context);
        }

        free_move_list(&moves);
        if(err != e_ok) return err;

        if(captured) last_captor_id = (int8_t)current;

        current = (uint8_t)((current + 1) % players->size);
    }

    if(table.nb_cards_on_table > 0 && last_captor_id >= 0){
        err = award_remaining_table_cards(players, (uint8_t)last_captor_id);
        if(err != e_ok) return err;
    }

    return e_ok;
}

t_cteerr compute_round_score(struct s_cte_players *players, s_cte_round_score scores[], bool is_team_mode){
    if(!players || !scores) return e_null;

    uint8_t nb = players->size;

    for(uint8_t i = 0; i < nb; i++){
        scores[i].card_points   = 0;
        scores[i].majority_bonus = 0;
        scores[i].tablic_points = players->players[i].nb_tablic;
        for(uint8_t j = 0; j < players->players[i].won_cards.size; j++){
            scores[i].card_points += get_points(players->players[i].won_cards.array[j]);
        }
    }

    if(is_team_mode && nb == 4){
        // 2v2 Teams: Team 0 (P0 + P2) vs Team 1 (P1 + P3)
        uint8_t team0_cards = (uint8_t)(players->players[0].won_cards.size + players->players[2].won_cards.size);
        uint8_t team1_cards = (uint8_t)(players->players[1].won_cards.size + players->players[3].won_cards.size);

        if(team0_cards >= 27 && team0_cards != team1_cards){
            scores[0].majority_bonus = 3;
        } else if(team1_cards >= 27 && team0_cards != team1_cards){
            scores[1].majority_bonus = 3;
        }
    } else {
        // Individual mode: 2, 3, or 4 players
        uint8_t max_cards = 0;
        uint8_t max_count = 0;
        for(uint8_t i = 0; i < nb; i++){
            uint8_t nc = players->players[i].won_cards.size;
            if(nc > max_cards){
                max_cards = nc;
                max_count = 1;
            } else if(nc == max_cards){
                max_count++;
            }
        }
        if(max_cards >= 27 && max_count == 1){
            for(uint8_t i = 0; i < nb; i++){
                if(players->players[i].won_cards.size == max_cards){
                    scores[i].majority_bonus = 3;
                    break;
                }
            }
        }
    }

    for(uint8_t i = 0; i < nb; i++){
        scores[i].total = scores[i].card_points + scores[i].majority_bonus + scores[i].tablic_points;
    }

    return e_ok;
}

t_cteerr init_match(struct s_cte_match *match, struct s_cte_players *players, uint16_t winning_score){
    if(!match || !players) return e_null;
    if(winning_score == 0) return e_inval_val;

    match->players       = players;
    match->winning_score = winning_score;
    match->round_nb      = 0;
    match->max_rounds    = 0;
    match->is_team_mode  = false;
    for(uint8_t i = 0; i < 4; i++) match->match_scores[i] = 0;

    return e_ok;
}

bool match_is_over(const struct s_cte_match *match){
    if(!match) return false;
    if(match->max_rounds > 0 && match->round_nb >= match->max_rounds) return true;

    if(match->is_team_mode && match->players->size == 4){
        if(match->match_scores[0] >= match->winning_score || match->match_scores[1] >= match->winning_score){
            return true;
        }
    } else {
        for(uint8_t i = 0; i < match->players->size; i++){
            if(match->match_scores[i] >= match->winning_score) return true;
        }
    }
    return false;
}

int8_t match_winner(const struct s_cte_match *match){
    if(!match || !match_is_over(match)) return -1;

    if(match->is_team_mode && match->players->size == 4){
        if(match->match_scores[0] > match->match_scores[1]) return 0;
        if(match->match_scores[1] > match->match_scores[0]) return 1;
        return -1; // Tie
    }

    int8_t winner = -1;
    uint16_t best = 0;
    for(uint8_t i = 0; i < match->players->size; i++){
        if(match->match_scores[i] > best){
            best   = match->match_scores[i];
            winner = (int8_t)i;
        }
    }
    return winner;
}

t_cteerr run_match(struct s_cte_match *match, const s_cte_round_config *config){
    if(!match || !config) return e_null;

    match->is_team_mode = config->is_team_mode;

    while(!match_is_over(match)){
        if(config->callbacks && config->callbacks->on_round_start){
            config->callbacks->on_round_start(match->round_nb + 1, config->ui_context);
        }

        t_cteerr err = run_round(match->players, config);
        if(err != e_ok) return err;

        s_cte_round_score scores[4] = {0};
        err = compute_round_score(match->players, scores, match->is_team_mode);
        if(err != e_ok) return err;

        if(match->is_team_mode && match->players->size == 4){
            uint16_t team0_pts = (uint16_t)(scores[0].total + scores[2].total);
            uint16_t team1_pts = (uint16_t)(scores[1].total + scores[3].total);
            match->match_scores[0] += team0_pts;
            match->match_scores[1] += team1_pts;
            match->match_scores[2] = match->match_scores[0];
            match->match_scores[3] = match->match_scores[1];
        } else {
            for(uint8_t i = 0; i < match->players->size; i++){
                match->match_scores[i] += scores[i].total;
            }
        }

        if(config->callbacks && config->callbacks->on_round_end){
            config->callbacks->on_round_end(match->players, scores, config->ui_context);
        }

        match->round_nb++;
        reset_all_players(match->players);
    }

    if(config->callbacks && config->callbacks->on_match_end){
        int8_t winner = match_winner(match);
        config->callbacks->on_match_end(match, winner, config->ui_context);
    }

    return e_ok;
}
