#include "player.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

t_cteerr init_players(struct s_cte_players *players, uint8_t nb_players, char **player_names){
    if(!players) return e_null;
    if(nb_players < 2 || nb_players > 4) return e_inval_val;

    players->size = nb_players;
    players->players = malloc(sizeof(struct s_cte_player_data) * nb_players);
    if(!players->players) return e_alloc;

    for(uint8_t i = 0 ; i < nb_players; i++){
        players->players[i].player_id = i;
        players->players[i].team_id = (uint8_t)(i % 2);
        players->players[i].nb_tablic = 0;
        players->players[i].hand.size = 0;
        players->players[i].won_cards.size = 0;
        players->players[i].player_name = strdup(player_names[i]);
        players->players[i].evaluator = NULL;
        players->players[i].eval_context = NULL;
        players->players[i].is_human = false;
    }

    return e_ok;
}

void free_players(struct s_cte_players *players){
    if(!players || !players->players) return;
    for(uint8_t i = 0 ; i < players->size; i++){
        if(players->players[i].player_name){
            free(players->players[i].player_name);
            players->players[i].player_name = NULL;
        }
    }
    free(players->players);
    players->players = NULL;
    players->size = 0;
}

void reset_player_round(struct s_cte_player_data *player){
    if(!player) return;
    player->hand.size = 0;
    player->won_cards.size = 0;
    player->nb_tablic = 0;
}

void reset_all_players(struct s_cte_players *players){
    if(!players || !players->players) return;
    for(uint8_t i = 0; i < players->size; i++){
        reset_player_round(&players->players[i]);
    }
}

void print_hand(struct s_cte_hand *hand){
    if(!hand) return;
    printf("Cards in hand : \n");
    for(uint8_t i = 0 ; i < hand->size; i++){
        uint8_t card = hand->array[i];
        print_card(card);
    }
    printf("\n");
}
