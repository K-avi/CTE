#include "game.h"
#include <stdlib.h>
#include <string.h>

t_cteerr init_game(s_cte_game *game, uint8_t nb_players, char *names[], bool is_team_mode){
    if(!game) return e_null;
    if(nb_players < 2 || nb_players > 4) return e_inval_val;

    memset(game, 0, sizeof(s_cte_game));
    game->is_team_mode = is_team_mode;
    game->last_captor_id = -1;
    game->current_player_id = 0;
    init_deck(&game->deck);
    game->table_bb = 0;

    return init_players(&game->players, nb_players, names);
}

void free_game(s_cte_game *game){
    if(!game) return;
    free_players(&game->players);
}

t_cteerr setup_round(s_cte_game *game){
    if(!game) return e_null;
    if(game->players.size < 2 || game->players.size > 4) return e_inval_val;

    init_deck(&game->deck);
    shuffle_deck(&game->deck);

    // Dynamic dealing: 12 / P cards per player (6 for 2p, 4 for 3p, 3 for 4p)
    uint8_t cards_per_player = (uint8_t)(12 / game->players.size);
    for(uint8_t p = 0; p < game->players.size; p++){
        for(uint8_t i = 0; i < cards_per_player; i++){
            game->players.players[p].hand.array[i] = game->deck.cards[p * cards_per_player + i];
        }
        game->players.players[p].hand.size = cards_per_player;
    }

    // Put 4 cards on the table
    game->table_bb = 0;
    for(int i = 0; i < 4; i++){
        game->table_bb |= (1ULL << game->deck.cards[12 + i]);
    }
    game->deck.cur_card = 16;
    game->last_captor_id = -1;

    return e_ok;
}

t_cteerr deal_next_hand(s_cte_game *game){
    if(!game) return e_null;
    if(game->players.size < 2 || game->players.size > 4) return e_inval_val;
    if(game->deck.cur_card + 12 > DECKSIZE) return e_inval_val;

    uint8_t cards_per_player = (uint8_t)(12 / game->players.size);
    for(uint8_t p = 0; p < game->players.size; p++){
        for(uint8_t i = 0; i < cards_per_player; i++){
            game->players.players[p].hand.array[i] = game->deck.cards[game->deck.cur_card + p * cards_per_player + i];
        }
        game->players.players[p].hand.size = cards_per_player;
    }
    game->deck.cur_card += 12;

    return e_ok;
}

t_cteerr award_remaining_table_cards(s_cte_game *game){
    if(!game) return e_null;
    if(game->last_captor_id < 0 || game->last_captor_id >= (int8_t)game->players.size) return e_inval_val;
    if(game->table_bb == 0) return e_ok;

    struct s_cte_player_data *captor = &game->players.players[game->last_captor_id];
    uint64_t temp = game->table_bb;
    while(temp > 0){
        t_card c = (t_card)__builtin_ctzll(temp);
        if(captor->won_cards.size < 52){
            captor->won_cards.array[captor->won_cards.size++] = c;
        }
        temp &= (temp - 1);
    }
    game->table_bb = 0;

    return e_ok;
}

t_cteerr run_round(s_cte_game *game, const s_cte_round_config *config){
    if(!game || !config) return e_null;
    if(game->players.size < 2 || game->players.size > 4) return e_inval_val;

    for(uint8_t i = 0; i < game->players.size; i++){
        t_evaluator eval_func = game->players.players[i].evaluator ? game->players.players[i].evaluator : config->evaluators[i];
        if(!eval_func) return e_null;
    }

    t_cteerr err = setup_round(game);
    if(err != e_ok) return err;

    if(config->callbacks && config->callbacks->on_deal){
        config->callbacks->on_deal(&game->players, config->ui_context);
    }

    uint8_t current = (uint8_t)(config->first_player % game->players.size);
    game->current_player_id = current;

    for(;;){
        bool all_hands_empty = true;
        for(uint8_t i = 0; i < game->players.size; i++){
            if(game->players.players[i].hand.size > 0){
                all_hands_empty = false;
                break;
            }
        }

        if(all_hands_empty){
            if(game->deck.cur_card >= DECKSIZE) break;
            err = deal_next_hand(game);
            if(err != e_ok) return err;

            if(config->callbacks && config->callbacks->on_deal){
                config->callbacks->on_deal(&game->players, config->ui_context);
            }
        }

        if(game->players.players[current].hand.size == 0){
            current = (uint8_t)((current + 1) % game->players.size);
            game->current_player_id = current;
            continue;
        }

        struct s_cte_move_list moves;
        err = init_move_list(&moves, 16);
        if(err != e_ok) return err;

        err = gen_all_moves(&moves, game->table_bb, &game->players.players[current].hand);
        if(err != e_ok){ free_move_list(&moves); return err; }

        s_cte_game_state state = {
            .table_bb          = game->table_bb,
            .players           = &game->players,
            .current_player_id = current,
            .is_team_mode      = game->is_team_mode,
        };

        if(config->callbacks && config->callbacks->on_turn_start){
            config->callbacks->on_turn_start(&state, &moves, config->ui_context);
        }

        t_evaluator eval_func = game->players.players[current].evaluator ? game->players.players[current].evaluator : config->evaluators[current];
        void *eval_ctx = game->players.players[current].eval_context ? game->players.players[current].eval_context : config->eval_contexts[current];

        uint16_t chosen_idx = eval_func(&state, &moves, eval_ctx);
        if(chosen_idx >= moves.size){ free_move_list(&moves); return e_inval_val; }

        struct s_cte_move chosen_move = moves.moves[chosen_idx];
        bool captured = false;
        err = play_move(&game->table_bb, &chosen_move, &game->players.players[current], &captured);

        if(config->callbacks && config->callbacks->on_move_played){
            config->callbacks->on_move_played(&game->players.players[current], &chosen_move, captured, config->ui_context);
        }

        free_move_list(&moves);
        if(err != e_ok) return err;

        if(captured) game->last_captor_id = (int8_t)current;

        current = (uint8_t)((current + 1) % game->players.size);
        game->current_player_id = current;
    }

    if(game->table_bb > 0 && game->last_captor_id >= 0){
        err = award_remaining_table_cards(game);
        if(err != e_ok) return err;
    }

    return e_ok;
}

t_cteerr compute_round_score(struct s_cte_players *players, s_cte_round_score scores[], bool is_team_mode){
    if(!players || !scores) return e_null;

    uint8_t nb = players->size;

    for(uint8_t i = 0; i < nb; i++){
        scores[i].card_points    = 0;
        scores[i].majority_bonus = 0;
        scores[i].tablic_points  = players->players[i].nb_tablic;
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

t_cteerr init_match(struct s_cte_match *match, s_cte_game *game, uint16_t winning_score){
    if(!match || !game) return e_null;
    if(winning_score == 0) return e_inval_val;

    match->game          = game;
    match->winning_score = winning_score;
    match->round_nb      = 0;
    match->max_rounds    = 0;
    match->is_team_mode  = game->is_team_mode;
    for(uint8_t i = 0; i < 4; i++) match->match_scores[i] = 0;

    return e_ok;
}

bool match_is_over(const struct s_cte_match *match){
    if(!match || !match->game) return false;
    if(match->max_rounds > 0 && match->round_nb >= match->max_rounds) return true;

    if(match->is_team_mode && match->game->players.size == 4){
        if(match->match_scores[0] >= match->winning_score || match->match_scores[1] >= match->winning_score){
            return true;
        }
    } else {
        for(uint8_t i = 0; i < match->game->players.size; i++){
            if(match->match_scores[i] >= match->winning_score) return true;
        }
    }
    return false;
}

int8_t match_winner(const struct s_cte_match *match){
    if(!match || !match_is_over(match) || !match->game) return -1;

    if(match->is_team_mode && match->game->players.size == 4){
        if(match->match_scores[0] > match->match_scores[1]) return 0;
        if(match->match_scores[1] > match->match_scores[0]) return 1;
        return -1; // Tie
    }

    int8_t winner = -1;
    uint16_t best = 0;
    for(uint8_t i = 0; i < match->game->players.size; i++){
        if(match->match_scores[i] > best){
            best   = match->match_scores[i];
            winner = (int8_t)i;
        }
    }
    return winner;
}

t_cteerr run_match(struct s_cte_match *match, const s_cte_round_config *config){
    if(!match || !match->game || !config) return e_null;

    match->is_team_mode = config->is_team_mode;
    match->game->is_team_mode = config->is_team_mode;

    while(!match_is_over(match)){
        if(config->callbacks && config->callbacks->on_round_start){
            config->callbacks->on_round_start(match->round_nb + 1, config->ui_context);
        }

        t_cteerr err = run_round(match->game, config);
        if(err != e_ok) return err;

        s_cte_round_score scores[4] = {0};
        err = compute_round_score(&match->game->players, scores, match->is_team_mode);
        if(err != e_ok) return err;

        if(match->is_team_mode && match->game->players.size == 4){
            uint16_t team0_pts = (uint16_t)(scores[0].total + scores[2].total);
            uint16_t team1_pts = (uint16_t)(scores[1].total + scores[3].total);
            match->match_scores[0] += team0_pts;
            match->match_scores[1] += team1_pts;
            match->match_scores[2] = match->match_scores[0];
            match->match_scores[3] = match->match_scores[1];
        } else {
            for(uint8_t i = 0; i < match->game->players.size; i++){
                match->match_scores[i] += scores[i].total;
            }
        }

        if(config->callbacks && config->callbacks->on_round_end){
            config->callbacks->on_round_end(&match->game->players, scores, config->ui_context);
        }

        match->round_nb++;
        reset_all_players(&match->game->players);
    }

    if(config->callbacks && config->callbacks->on_match_end){
        int8_t winner = match_winner(match);
        config->callbacks->on_match_end(match, winner, config->ui_context);
    }

    return e_ok;
}
