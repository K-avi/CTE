#include "cte.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#ifdef DEBUG
void print_table(void);
void print_hand(struct s_cte_hand *hand);
void print_move(struct s_cte_move *move);
void print_card(uint8_t card);
#endif

/* order : 
* 2, 3, 4, 5, 6, 7, 8, 9, 10, ACE, JACK, QUEEN, KING
* Clubs, Diamonds, Hearts, Spade
*/

//Maybe useless, array with the existing values of each card. 
//This doesn't handle the ace bc ACE will be dealt with during calculation
uint8_t values[13] = {
    2,3,4,5,6,7,8,9,10,11,12,13,14
};

//Array of the value (in points) of each card in the game
uint8_t __tab_points[52] = {
   1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, //clubs (2 of clubs = 1, 10..King,Ace = 1)
   0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 1, 1, //diamonds (10 of diamonds = 2, Ace,Jack,Queen,King = 1)
   0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, //hearts (10..King,Ace = 1)
   0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, //spades (10..King,Ace = 1)
}; 

struct deck{
    uint8_t cur_card; 
    t_card cards[52];
} deck = {
    0,
    {
    0,1,2,3,4,5,6,7,8,9,10,11,12, //clubs
    13,14,15,16,17,18,19,20,21,22,23,24,25,//diamonds
    26,27,28,29,30,31,32,33,34,35,36,37,38, //hearts
    39,40,41,42,43,44,45,46,47,48,49,50,51 //spades
    }
};//nb : values start at 2 and ace is between 10 and jack.
#define DECKSIZE 52

//theoreticallly, there could be up to 52 cards on the table 
//the global table where cards are put on
struct table {
    uint8_t nb_cards_on_table;
    t_card cards_on_table[52]; 
}table = {0,{0}};
/******************************PLAYER DATA STUFF**************************************************/

static t_cteerr init_player_data(struct s_cte_player_data *player, uint8_t player_id, char *player_name){
    player->player_id = player_id;
    player->nb_tablic = 0;
    player->player_name = strndup(player_name, 20);
    if(!player->player_name) return e_alloc;
    player->hand.size = 0;
    player->won_cards.size = 0;
    return e_ok;
}//tested; ok

t_cteerr init_players(struct s_cte_players *players, uint8_t nb_players, char **player_names){
    if(nb_players < 2 || nb_players > 4) return e_inval_val;

    players->size = nb_players;
    players->players = malloc(sizeof(struct s_cte_player_data) * nb_players);
    if(!players->players) return e_alloc;

    for(uint8_t i = 0 ; i < nb_players; i++){
        t_cteerr err = init_player_data(&players->players[i], i, player_names[i]);
        if(err != e_ok) return err;
    }
    return e_ok;
}//tested; ok

static void free_player_data(struct s_cte_player_data *player){
    free(player->player_name);
}//tested; ok

void free_players(struct s_cte_players *players){
    for(uint8_t i = 0 ; i < players->size; i++){
        free_player_data(&players->players[i]);
    }
    free(players->players);
}//tested; ok

// Réinitialise les données d'un joueur entre deux manches :
// vide la main, le tas de cartes remportées et remet nb_tablic à 0.
// Ne touche pas au nom ni à l'id du joueur.
void reset_player_round(struct s_cte_player_data *player){
    player->hand.size = 0;
    player->won_cards.size = 0;
    player->nb_tablic = 0;
}//tested; ok

// Réinitialise tous les joueurs pour une nouvelle manche.
void reset_all_players(struct s_cte_players *players){
    for(uint8_t i = 0; i < players->size; i++){
        reset_player_round(&players->players[i]);
    }
}//tested; ok

/***********************DECK STUFF************************************************/

//this is the order of distribution of the cards for a game
static void shuffle_deck(void){
    //simple fisher-yates shuffle

    for(uint8_t i = DECKSIZE - 1 ; i > 0; i-- ){
        int idx = rand()%(i);
        uint8_t tmp = deck.cards[i];
        deck.cards[i] = deck.cards[idx]; 
        deck.cards[idx] = tmp;
    }
}//tested; ok

t_cteerr setup_game(struct s_cte_players *players){
    
    shuffle_deck();

    if(players->size == 2){
        for(int i = 0 ; i < 6; i++){//distribute 6 cards to each player 
            players->players[0].hand.array[i] = deck.cards[i];
            players->players[1].hand.array[i] = deck.cards[i+6];
        }
        players->players[0].hand.size = 6;
        players->players[1].hand.size = 6;
    }else if (players->size == 4) {
        return e_inval_val;
    }else if (players->size == 3) {
        return e_inval_val;
    }else{
        return e_inval_val;
    }

    //put 4 cards on the table
    for(int i = 0; i < 4; i++){
        table.cards_on_table[i] = deck.cards[i+12];
    }
    table.nb_cards_on_table = 4;
    deck.cur_card = 16;
    
    return e_ok;
}//tested; ok

// Redistribue 6 cartes à chaque joueur (2 joueurs) depuis le paquet restant.
// À appeler quand les mains des deux joueurs sont vides mais que deck.cur_card < 52.
// Ne remet AUCUNE carte sur la table (contrairement à setup_game).
// Retourne e_inval_val si le paquet est épuisé ou s'il n'y a pas 2 joueurs.
t_cteerr deal_next_hand(struct s_cte_players *players){
    if(!players) return e_null;
    if(players->size != 2) return e_inval_val;
    if(deck.cur_card + 12 > DECKSIZE) return e_inval_val; // plus assez de cartes

    for(int i = 0; i < 6; i++){
        players->players[0].hand.array[i] = deck.cards[deck.cur_card + i];
        players->players[1].hand.array[i] = deck.cards[deck.cur_card + i + 6];
    }
    players->players[0].hand.size = 6;
    players->players[1].hand.size = 6;
    deck.cur_card += 12;

    return e_ok;
}//tested; ok

t_cteerr init_move_list(struct s_cte_move_list *list, uint16_t initial_cap){
    if(!list) return e_null;
    list->size = 0;
    list->max = (initial_cap > 0) ? initial_cap : 16;
    list->moves = malloc(sizeof(struct s_cte_move) * list->max);
    if(!list->moves) return e_alloc;
    return e_ok;
}

static t_cteerr move_list_push(struct s_cte_move_list *list, const struct s_cte_move *move){
    if(!list) return e_null;
    if(list->size >= list->max){
        uint16_t new_max = list->max ? (uint16_t)(list->max * 2) : 16;
        struct s_cte_move *new_moves = realloc(list->moves, sizeof(struct s_cte_move) * new_max);
        if(!new_moves) return e_realloc;
        list->moves = new_moves;
        list->max = new_max;
    }
    list->moves[list->size++] = *move;
    return e_ok;
}

void free_move(struct s_cte_move *move){
    if(move && move->cards_picked.array){
        free(move->cards_picked.array);
        move->cards_picked.array = NULL;
        move->cards_picked.size = 0;
        move->cards_picked.max = 0;
    }
}

void free_move_list(struct s_cte_move_list *list){
    if(!list) return;
    if(list->moves){
        for(uint16_t i = 0 ; i < list->size; i++){
            free_move(&list->moves[i]);
        }
        free(list->moves);
        list->moves = NULL;
    }
    list->size = 0;
    list->max = 0;
}

t_cteerr play_move(struct s_cte_move *move, struct s_cte_player_data *player, bool *captured){
    if(!move || !player) return e_null;

    // Remove card played from hand
    for(uint8_t i = 0 ; i < player->hand.size; i++){
        if(move->card_played == player->hand.array[i]){
            player->hand.array[i] = player->hand.array[player->hand.size - 1];
            player->hand.size--;
            break;
        }
    }

    // If no cards were picked, the card is dropped onto the table
    if(move->cards_picked.size == 0){
        if(table.nb_cards_on_table < 52){
            table.cards_on_table[table.nb_cards_on_table++] = move->card_played;
        }
        if(captured) *captured = false;
        return e_ok;
    }

    // Remove picked cards from table
    for(uint8_t i = 0 ; i < move->cards_picked.size; i++){
        for(uint8_t j = 0; j < table.nb_cards_on_table; j++){
            if(table.cards_on_table[j] == move->cards_picked.array[i]){
                table.cards_on_table[j] = table.cards_on_table[table.nb_cards_on_table - 1];
                table.nb_cards_on_table--;
                break;
            }
        }
    }

    // Score a Tablic if table was emptied
    if(table.nb_cards_on_table == 0) player->nb_tablic++;

    // Add cards to player's won cards pile
    player->won_cards.array[player->won_cards.size++] = move->card_played;
    for(uint8_t i = 0; i < move->cards_picked.size; i++){
        player->won_cards.array[player->won_cards.size++] = move->cards_picked.array[i]; 
    }

    if(captured) *captured = true;
    return e_ok;
}

// Attribue les cartes restantes sur la table au joueur qui a fait la dernière prise.
// À appeler en fin de manche si table.nb_cards_on_table > 0.
// last_captor_id : player_id du dernier joueur ayant effectué une prise (pas une pose).
t_cteerr award_remaining_table_cards(struct s_cte_players *players, uint8_t last_captor_id){
    if(!players) return e_null;
    if(last_captor_id >= players->size) return e_inval_val;
    if(table.nb_cards_on_table == 0) return e_ok; // rien à faire

    struct s_cte_player_data *captor = &players->players[last_captor_id];
    for(uint8_t i = 0; i < table.nb_cards_on_table; i++){
        captor->won_cards.array[captor->won_cards.size++] = table.cards_on_table[i];
    }
    table.nb_cards_on_table = 0;

    return e_ok;
}

/***********************GAME LOOP ************************************************/

// Exécute une manche complète de Tablić.
//
// Séquence :
//   1. Mélange + distribution initiale (6 cartes/joueur + 4 sur table) via setup_game().
//   2. Boucle de jeu :
//      a. Si les deux mains sont vides et le paquet non épuisé : deal_next_hand().
//      b. Génération des coups légaux pour le joueur courant.
//      c. Sélection d'un coup via config->evaluators[current_player_id].
//      d. Exécution du coup ; mise à jour de last_captor_id si capture.
//      e. Passage au joueur suivant.
//   3. Attribution des cartes restantes sur la table au last_captor.
//
// Précondition : config->evaluators[i] != NULL pour tous les joueurs actifs.
// Retourne e_inval_val si players->size != 2 (seul le mode 2 joueurs est supporté).
t_cteerr run_round(struct s_cte_players *players, const s_cte_round_config *config){
    if(!players || !config) return e_null;
    if(players->size != 2) return e_inval_val;

    // Vérifier que chaque joueur a bien un évaluateur
    for(uint8_t i = 0; i < players->size; i++){
        if(!config->evaluators[i]) return e_null;
    }

    t_cteerr err = setup_game(players);
    if(err != e_ok) return err;

    uint8_t current = config->first_player % players->size;
    int8_t last_captor_id = -1; // -1 = aucune prise effectuée

    for(;;){
        // Vérifier la condition de fin : mains vides ET paquet épuisé
        bool all_hands_empty = true;
        for(uint8_t i = 0; i < players->size; i++){
            if(players->players[i].hand.size > 0){
                all_hands_empty = false;
                break;
            }
        }

        if(all_hands_empty){
            if(deck.cur_card >= DECKSIZE) break; // fin de manche
            err = deal_next_hand(players);
            if(err != e_ok) return err;
        }

        // Sauter les joueurs à main vide (ne devrait pas arriver si deal est correct)
        if(players->players[current].hand.size == 0){
            current = (uint8_t)((current + 1) % players->size);
            continue;
        }

        // Générer les coups légaux
        struct s_cte_move_list moves;
        err = init_move_list(&moves, 16);
        if(err != e_ok) return err;

        err = gen_all_moves(&moves, &players->players[current].hand);
        if(err != e_ok){ free_move_list(&moves); return err; }

        // Construire le snapshot d'état pour l'évaluateur
        s_cte_game_state state = {
            .table             = &table,
            .players           = players,
            .current_player_id = current,
        };

        // Appel à l'évaluateur : retourne l'index du coup choisi
        uint16_t chosen_idx = config->evaluators[current](&state, &moves, config->eval_contexts[current]);
        if(chosen_idx >= moves.size){ free_move_list(&moves); return e_inval_val; }

        // Jouer le coup
        bool captured = false;
        err = play_move(&moves.moves[chosen_idx], &players->players[current], &captured);
        free_move_list(&moves);
        if(err != e_ok) return err;

        if(captured) last_captor_id = (int8_t)current;

        current = (uint8_t)((current + 1) % players->size);
    }

    // Attribuer les cartes restantes sur la table
    if(table.nb_cards_on_table > 0 && last_captor_id >= 0){
        err = award_remaining_table_cards(players, (uint8_t)last_captor_id);
        if(err != e_ok) return err;
    }

    return e_ok;
}

/***********************SCORING & MATCH MANAGEMENT *******************************/

// Calcule le score d'une manche pour chaque joueur.
//
// Règles :
//   - card_points  : somme de __tab_points[card] pour chaque carte remportée.
//   - majority_bonus : +3 si le joueur a >= 27 cartes (0 si égalité 26/26).
//   - tablic_points : = nb_tablic du joueur (1 point par Tablić).
//   - total         : somme des trois.
//
// scores[] doit être un tableau alloué par l'appelant de taille >= players->size.
t_cteerr compute_round_score(struct s_cte_players *players, s_cte_round_score scores[]){
    if(!players || !scores) return e_null;

    uint8_t nb = players->size;

    // Compter le nombre de cartes et les points de chaque joueur
    for(uint8_t i = 0; i < nb; i++){
        scores[i].card_points  = 0;
        scores[i].tablic_points = players->players[i].nb_tablic;
        for(uint8_t j = 0; j < players->players[i].won_cards.size; j++){
            scores[i].card_points += get_points(players->players[i].won_cards.array[j]);
        }
    }

    // Bonus de majorité (uniquement si un joueur a strictement >= 27 cartes)
    // En cas d'égalité 26/26, personne ne reçoit le bonus.
    for(uint8_t i = 0; i < nb; i++) scores[i].majority_bonus = 0;

    if(nb == 2){
        uint8_t n0 = players->players[0].won_cards.size;
        uint8_t n1 = players->players[1].won_cards.size;
        if(n0 >= 27 && n0 != n1) scores[0].majority_bonus = 3;
        if(n1 >= 27 && n0 != n1) scores[1].majority_bonus = 3;
    } else {
        // Mode > 2 joueurs : le joueur avec le plus de cartes reçoit +3,
        // sauf en cas d'égalité au sommet.
        uint8_t max_cards = 0;
        uint8_t max_count = 0;
        for(uint8_t i = 0; i < nb; i++){
            uint8_t nc = players->players[i].won_cards.size;
            if(nc > max_cards){ max_cards = nc; max_count = 1; }
            else if(nc == max_cards) max_count++;
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

    // Total
    for(uint8_t i = 0; i < nb; i++){
        scores[i].total = scores[i].card_points + scores[i].majority_bonus + scores[i].tablic_points;
    }

    return e_ok;
}//tested; ok

// Initialise une structure de match.
t_cteerr init_match(struct s_cte_match *match, struct s_cte_players *players, uint16_t winning_score){
    if(!match || !players) return e_null;
    if(winning_score == 0) return e_inval_val;

    match->players       = players;
    match->winning_score = winning_score;
    match->round_nb      = 0;
    match->max_rounds    = 0;
    for(uint8_t i = 0; i < 4; i++) match->match_scores[i] = 0;

    return e_ok;
}//tested; ok

// Retourne true si le nombre max de manches est atteint ou si un joueur a atteint winning_score.
bool match_is_over(const struct s_cte_match *match){
    if(!match) return false;
    if(match->max_rounds > 0 && match->round_nb >= match->max_rounds) return true;
    for(uint8_t i = 0; i < match->players->size; i++){
        if(match->match_scores[i] >= match->winning_score) return true;
    }
    return false;
}//tested; ok

// Retourne l'indice du joueur gagnant, ou -1 si le match n'est pas encore terminé.
// En cas d'égalité au-dessus du seuil, retourne l'indice le plus bas (à raffiner).
int8_t match_winner(const struct s_cte_match *match){
    if(!match || !match_is_over(match)) return -1;
    int8_t winner = -1;
    uint16_t best = 0;
    for(uint8_t i = 0; i < match->players->size; i++){
        if(match->match_scores[i] > best){
            best   = match->match_scores[i];
            winner = (int8_t)i;
        }
    }
    return winner;
}//tested; ok

// Exécute un match complet (plusieurs manches) jusqu'à ce qu'un joueur atteigne
// match->winning_score. Chaque manche : run_round → compute_round_score →
// mise à jour des scores cumulatifs → reset des joueurs.
t_cteerr run_match(struct s_cte_match *match, const s_cte_round_config *config){
    if(!match || !config) return e_null;

    while(!match_is_over(match)){
        t_cteerr err = run_round(match->players, config);
        if(err != e_ok) return err;

        s_cte_round_score scores[4] = {0};
        err = compute_round_score(match->players, scores);
        if(err != e_ok) return err;

        for(uint8_t i = 0; i < match->players->size; i++){
            match->match_scores[i] += scores[i].total;
        }

        match->round_nb++;
        reset_all_players(match->players);
    }

    return e_ok;
}//tested; ok

/*********************** EXACT PARTITION & MOVE VALIDATION (DP) ********************/

static bool subset_sums_to(const uint8_t *cards, uint32_t mask, uint8_t n, uint8_t target_val){
    uint32_t sum_max = 0;
    uint8_t nb_aces = 0;
    for(uint8_t i = 0; i < n; i++){
        if(mask & (1u << i)){
            uint8_t v = get_value(cards[i]);
            if(v == 11){
                nb_aces++;
            }
            sum_max += v;
        }
    }
    // Each ace counted as 1 (instead of 11) reduces the sum by 10
    if(sum_max >= target_val && ((sum_max - target_val) % 10 == 0)){
        uint32_t k = (sum_max - target_val) / 10;
        if(k <= nb_aces){
            return true;
        }
    }
    return false;
}

static bool can_partition_rec(uint32_t mask, const uint8_t *valid_base, int8_t *memo){
    if(mask == 0) return true;
    if(memo[mask] != -1) return (bool)memo[mask];

    uint32_t lsb = mask & (-mask); // lowest set bit to avoid permutations
    for(uint32_t sub = mask; sub > 0; sub = (sub - 1) & mask){
        if((sub & lsb) && valid_base[sub]){
            if(can_partition_rec(mask ^ sub, valid_base, memo)){
                memo[mask] = 1;
                return true;
            }
        }
    }
    memo[mask] = 0;
    return false;
}

static bool is_exact_partition(const uint8_t *cards, uint8_t n, uint8_t target_val){
    if(n == 0) return false;
    if(n > 20) return false;

    uint32_t num_masks = 1u << n;
    uint8_t *valid_base = malloc(num_masks * sizeof(uint8_t));
    int8_t *memo = malloc(num_masks * sizeof(int8_t));
    if(!valid_base || !memo){
        free(valid_base);
        free(memo);
        return false;
    }

    for(uint32_t m = 0; m < num_masks; m++){
        valid_base[m] = (m > 0) ? (uint8_t)subset_sums_to(cards, m, n, target_val) : 0;
        memo[m] = -1;
    }

    bool result = can_partition_rec(num_masks - 1, valid_base, memo);

    free(valid_base);
    free(memo);
    return result;
}

t_cteerr is_legal(bool *ret, struct s_cte_move *move){
    if(!ret || !move) return e_null;
    *ret = false;

    // Laying down a card on the table (taking 0 cards) is always legal
    if(move->cards_picked.size == 0){
        *ret = true;
        return e_ok;
    }

    uint8_t n = move->cards_picked.size;
    const uint8_t *cards = move->cards_picked.array;

    if(is_ace(move->card_played)){
        // An Ace played can represent either value 11 or value 1
        if(is_exact_partition(cards, n, 11) || is_exact_partition(cards, n, 1)){
            *ret = true;
        }
    } else {
        uint8_t target_val = get_value(move->card_played);
        if(is_exact_partition(cards, n, target_val)){
            *ret = true;
        }
    }

    return e_ok;
}

/*********************** MOVE GENERATION ******************************************/

static void find_valid_capture_masks(const uint8_t *table_cards, uint8_t n, uint8_t target_val, uint8_t *is_valid_capture){
    if(n == 0 || n > 20) return;
    uint32_t num_masks = 1u << n;
    uint8_t *valid_base = malloc(num_masks * sizeof(uint8_t));
    int8_t *memo = malloc(num_masks * sizeof(int8_t));
    if(!valid_base || !memo){
        free(valid_base);
        free(memo);
        return;
    }

    for(uint32_t m = 0; m < num_masks; m++){
        valid_base[m] = (m > 0) ? (uint8_t)subset_sums_to(table_cards, m, n, target_val) : 0;
        memo[m] = -1;
    }

    for(uint32_t m = 1; m < num_masks; m++){
        if(can_partition_rec(m, valid_base, memo)){
            is_valid_capture[m] = 1;
        }
    }

    free(valid_base);
    free(memo);
}

t_cteerr gen_card_moves(struct s_cte_move_list *moves, t_card card){
    if(!moves) return e_null;
    if(!moves->moves && moves->max == 0){
        t_cteerr err = init_move_list(moves, 16);
        if(err != e_ok) return err;
    }

    // 1. Always add the drop move (cards_picked.size = 0)
    struct s_cte_move drop_move;
    drop_move.card_played = card;
    drop_move.cards_picked.size = 0;
    drop_move.cards_picked.max = 0;
    drop_move.cards_picked.array = NULL;
    t_cteerr err = move_list_push(moves, &drop_move);
    if(err != e_ok) return err;

    uint8_t n = table.nb_cards_on_table;
    if(n == 0) return e_ok;
    if(n > 20) n = 20;

    uint32_t num_masks = 1u << n;
    uint8_t *is_valid_capture = calloc(num_masks, sizeof(uint8_t));
    if(!is_valid_capture) return e_alloc;

    if(is_ace(card)){
        find_valid_capture_masks(table.cards_on_table, n, 11, is_valid_capture);
        find_valid_capture_masks(table.cards_on_table, n, 1, is_valid_capture);
    } else {
        uint8_t val = get_value(card);
        find_valid_capture_masks(table.cards_on_table, n, val, is_valid_capture);
    }

    for(uint32_t m = 1; m < num_masks; m++){
        if(is_valid_capture[m]){
            uint8_t count = 0;
            for(uint8_t i = 0; i < n; i++){
                if(m & (1u << i)) count++;
            }

            struct s_cte_move move;
            move.card_played = card;
            move.cards_picked.size = count;
            move.cards_picked.max = count;
            move.cards_picked.array = malloc(sizeof(uint8_t) * count);
            if(!move.cards_picked.array){
                free(is_valid_capture);
                return e_alloc;
            }

            uint8_t idx = 0;
            for(uint8_t i = 0; i < n; i++){
                if(m & (1u << i)){
                    move.cards_picked.array[idx++] = table.cards_on_table[i];
                }
            }

            err = move_list_push(moves, &move);
            if(err != e_ok){
                free(move.cards_picked.array);
                free(is_valid_capture);
                return err;
            }
        }
    }

    free(is_valid_capture);
    return e_ok;
}

t_cteerr gen_all_moves(struct s_cte_move_list *moves, struct s_cte_hand *hand){
    if(!moves || !hand) return e_null;
    if(!moves->moves && moves->max == 0){
        t_cteerr err = init_move_list(moves, 32);
        if(err != e_ok) return err;
    }
    for(uint8_t i = 0 ; i < hand->size; i++){
        t_cteerr err = gen_card_moves(moves, hand->array[i]);
        if(err != e_ok) return err;
    }
    return e_ok;
}



static const char * const value_str[] = {
    "2", "3", "4", "5", "6", "7", "8", "9", "10", "ACE", "JACK", "QUEEN", "KING"
};
static const char * const color_str[] = {
    "Clubs", "Diamonds", "Hearts", "Spades"
};

static const char * const value_short_str[] = {
    "2", "3", "4", "5", "6", "7", "8", "9", "10", "A", "J", "Q", "K"
};

static const char * const suit_unicode_str[] = {
    "♣", "♦", "♥", "♠"
};
static const char * const suit_ascii_str[] = {
    "C", "D", "H", "S"
};

void format_card(char *buf, size_t buf_size, t_card card, e_cte_render_style style){
    if(!buf || buf_size == 0) return;
    if(card >= 52){
        snprintf(buf, buf_size, "??");
        return;
    }
    uint8_t v_idx = (uint8_t)(card % 13);
    uint8_t c_idx = (uint8_t)(card / 13);

    if(style == CTE_RENDER_UNICODE){
        snprintf(buf, buf_size, "%s%s", value_short_str[v_idx], suit_unicode_str[c_idx]);
    } else {
        snprintf(buf, buf_size, "%s%s", value_short_str[v_idx], suit_ascii_str[c_idx]);
    }
}

void format_move(char *buf, size_t buf_size, const struct s_cte_move *move, e_cte_render_style style){
    if(!buf || buf_size == 0) return;
    if(!move){
        snprintf(buf, buf_size, "None");
        return;
    }
    char card_buf[16];
    format_card(card_buf, sizeof(card_buf), move->card_played, style);

    if(move->cards_picked.size == 0){
        snprintf(buf, buf_size, "Drop %s", card_buf);
        return;
    }

    size_t written = (size_t)snprintf(buf, buf_size, "Play %s -> Take [ ", card_buf);
    for(uint8_t i = 0; i < move->cards_picked.size; i++){
        char picked_buf[16];
        format_card(picked_buf, sizeof(picked_buf), move->cards_picked.array[i], style);
        if(i > 0 && written < buf_size){
            written += (size_t)snprintf(buf + written, buf_size - written, ", ");
        }
        if(written < buf_size){
            written += (size_t)snprintf(buf + written, buf_size - written, "%s", picked_buf);
        }
    }
    if(written < buf_size){
        snprintf(buf + written, buf_size - written, " ]");
    }
}


void print_card(uint8_t card){
    uint8_t value = get_value(card);
    uint8_t color = get_color(card);

    printf("%s of %s\n", value_str[value-2], color_str[color]);
}//tested; ok

void print_hand(struct s_cte_hand *hand){
    printf("Cards in hand : \n");
    for(uint8_t i = 0 ; i < hand->size; i++){
        uint8_t card = hand->array[i];
        uint8_t value = get_value(card);

        printf("%s of %s\n", value_str[value-2], color_str[get_color(card)]);
    }
    printf("\n");
}//tested; ok

void print_table(){
    printf("Cards on table : \n");
    for(uint8_t i = 0 ; i < table.nb_cards_on_table; i++){
        uint8_t card = table.cards_on_table[i];
        uint8_t value = get_value(card);
        uint8_t color = get_color(card);

        printf("%s of %s\n", value_str[value-2], color_str[color]);
    }
    printf("\n");
}//tested; ok

void print_move(struct s_cte_move *move){
    printf("Card played : %s of %s\n", value_str[get_value(move->card_played)-2], color_str[get_color(move->card_played)]);
    printf("Cards picked up : \n");
    for(uint8_t i = 0 ; i < move->cards_picked.size; i++){
        uint8_t card = move->cards_picked.array[i];
        uint8_t value = get_value(card);
        uint8_t color = get_color(card);

        printf("%s of %s\n", value_str[value-2], color_str[color]);
    }
    printf("\n");
}//tested; ok

t_cteerr print_won_cards();
//t_cteerr count_points();

/***********************EVALUATORS***********************************************/

// Stub aléatoire : choisit un coup unifo-aléatoirement parmi les coups légaux.
// ctx est ignoré. moves->size doit être > 0 (garanti par gen_all_moves
// qui génère toujours au moins le coup de pose).
uint16_t eval_random(const s_cte_game_state *state,
                     const struct s_cte_move_list *moves,
                     void *ctx)
{
    (void)state;
    (void)ctx;
    return (uint16_t)(rand() % moves->size);
}//tested; ok

uint16_t eval_human_cli(const s_cte_game_state *state,
                        const struct s_cte_move_list *moves,
                        void *ctx)
{
    if(!moves || moves->size == 0) return 0;

    e_cte_render_style style = CTE_RENDER_UNICODE;
    if(ctx != NULL){
        style = *(const e_cte_render_style *)ctx;
    }

    const struct s_cte_player_data *cur_player = &state->players->players[state->current_player_id];

    printf("\n=======================================================\n");
    // Summary of captured cards & points this round for all players
    printf(" [Round Status] (Deck: %u cards left)\n", (unsigned)(52 - deck.cur_card));
    for(uint8_t p = 0; p < state->players->size; p++){
        const struct s_cte_player_data *pl = &state->players->players[p];
        uint8_t pts = 0;
        for(uint8_t j = 0; j < pl->won_cards.size; j++){
            pts += get_points(pl->won_cards.array[j]);
        }
        printf("   * %-10s : %2u cards captured (%2u card pts, %u tablic)\n",
               pl->player_name,
               (unsigned)pl->won_cards.size,
               (unsigned)pts,
               (unsigned)pl->nb_tablic);
    }
    printf("-------------------------------------------------------\n");

    printf(" [Table (%u cards)] : ", (unsigned)state->table->nb_cards_on_table);
    if(state->table->nb_cards_on_table == 0){
        printf("(empty)\n");
    } else {
        for(uint8_t i = 0; i < state->table->nb_cards_on_table; i++){
            char card_str[16];
            format_card(card_str, sizeof(card_str), state->table->cards_on_table[i], style);
            printf("%s ", card_str);
        }
        printf("\n");
    }

    printf(" [%s's Hand (%u cards)] : ", cur_player->player_name, (unsigned)cur_player->hand.size);
    for(uint8_t i = 0; i < cur_player->hand.size; i++){
        char card_str[16];
        format_card(card_str, sizeof(card_str), cur_player->hand.array[i], style);
        printf("%s ", card_str);
    }
    printf("\n");

    printf(" Available moves (%u):\n", (unsigned)moves->size);
    for(uint16_t i = 0; i < moves->size; i++){
        char move_str[128];
        format_move(move_str, sizeof(move_str), &moves->moves[i], style);
        printf("   [%u] %s\n", (unsigned)i, move_str);
    }

    for(;;){
        printf(" Select move [0-%u]: ", (unsigned)(moves->size - 1));
        fflush(stdout);

        char input_buf[64];
        if(!fgets(input_buf, sizeof(input_buf), stdin)){
            printf("\n");
            return 0;
        }

        char *endptr = NULL;
        long val = strtol(input_buf, &endptr, 10);
        if(endptr != input_buf && val >= 0 && val < (long)moves->size){
            return (uint16_t)val;
        }
        printf(" Invalid input. Please enter a valid number between 0 and %u.\n", (unsigned)(moves->size - 1));
    }
}

uint16_t eval_ai_cli(const s_cte_game_state *state,
                     const struct s_cte_move_list *moves,
                     void *ctx)
{
    if(!moves || moves->size == 0) return 0;

    e_cte_render_style style = CTE_RENDER_UNICODE;
    if(ctx != NULL){
        style = *(const e_cte_render_style *)ctx;
    }

    uint16_t chosen = (uint16_t)(rand() % moves->size);
    const struct s_cte_player_data *cur_player = &state->players->players[state->current_player_id];

    char move_str[128];
    format_move(move_str, sizeof(move_str), &moves->moves[chosen], style);
    printf(" [%s (AI)] played : %s\n", cur_player->player_name, move_str);

    return chosen;
}