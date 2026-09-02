#include "move.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

t_cteerr init_move_list(struct s_cte_move_list *list, uint16_t initial_cap){
    if(!list) return e_null;
    if(initial_cap == 0) initial_cap = 16;
    list->moves = malloc(sizeof(struct s_cte_move) * initial_cap);
    if(!list->moves){
        list->size = 0;
        list->max = 0;
        return e_alloc;
    }
    list->size = 0;
    list->max = initial_cap;
    return e_ok;
}

void free_move(struct s_cte_move *move){
    if(!move) return;
    move->cards_picked.size = 0;
}

void free_move_list(struct s_cte_move_list *list){
    if(!list) return;
    if(list->moves){
        free(list->moves);
        list->moves = NULL;
    }
    list->size = 0;
    list->max = 0;
}

static t_cteerr move_list_push(struct s_cte_move_list *list, const struct s_cte_move *move){
    if(!list || !move) return e_null;
    if(list->size >= list->max){
        uint16_t new_cap = (list->max == 0) ? 16 : (list->max * 2);
        struct s_cte_move *new_arr = realloc(list->moves, sizeof(struct s_cte_move) * new_cap);
        if(!new_arr) return e_realloc;
        list->moves = new_arr;
        list->max = new_cap;
    }
    list->moves[list->size++] = *move;
    return e_ok;
}

static uint8_t count_card_aces(const uint8_t *cards, uint32_t mask, uint8_t n){
    uint8_t aces = 0;
    for(uint8_t i = 0; i < n; i++){
        if((mask & (1u << i)) && is_ace(cards[i])){
            aces++;
        }
    }
    return aces;
}

static uint16_t sum_card_values_as_11(const uint8_t *cards, uint32_t mask, uint8_t n){
    uint16_t total = 0;
    for(uint8_t i = 0; i < n; i++){
        if(mask & (1u << i)){
            total += get_value(cards[i]);
        }
    }
    return total;
}

static bool subset_sums_to(const uint8_t *cards, uint32_t mask, uint8_t n, uint8_t target_val){
    if(mask == 0) return false;
    uint16_t nominal_sum = sum_card_values_as_11(cards, mask, n);
    uint8_t aces = count_card_aces(cards, mask, n);

    if(nominal_sum < target_val) return false;
    uint16_t diff = nominal_sum - target_val;
    if(diff % 10 != 0) return false;
    uint8_t k = (uint8_t)(diff / 10);
    return (k <= aces);
}

static bool can_partition_rec(uint32_t mask, const uint8_t *valid_base, int8_t *memo){
    if(mask == 0) return true;
    if(memo[mask] != -1) return (bool)memo[mask];

    if(valid_base[mask]){
        memo[mask] = 1;
        return true;
    }

    for(uint32_t sub = (mask - 1) & mask; sub > 0; sub = (sub - 1) & mask){
        if(valid_base[sub]){
            if(can_partition_rec(mask ^ sub, valid_base, memo)){
                memo[mask] = 1;
                return true;
            }
        }
    }
    memo[mask] = 0;
    return false;
}

bool is_exact_partition(const uint8_t *cards, uint8_t n, uint8_t target_val){
    if(n == 0) return false;
    if(n > 16) return false;

    uint32_t num_masks = 1u << n;
    uint8_t valid_base[num_masks];
    int8_t memo[num_masks];

    for(uint32_t m = 0; m < num_masks; m++){
        valid_base[m] = (m > 0) ? (uint8_t)subset_sums_to(cards, m, n, target_val) : 0;
        memo[m] = -1;
    }

    return can_partition_rec(num_masks - 1, valid_base, memo);
}

t_cteerr is_legal(bool *ret, const struct table *table, const struct s_cte_move *move){
    if(!ret || !move) return e_null;
    *ret = false;

    if(move->cards_picked.size == 0){
        *ret = true;
        return e_ok;
    }

    uint8_t n = move->cards_picked.size;
    const uint8_t *cards = move->cards_picked.array;

    // If table is provided, verify cards exist on table
    if(table){
        for(uint8_t i = 0; i < n; i++){
            bool found = false;
            for(uint8_t j = 0; j < table->nb_cards_on_table; j++){
                if(table->cards_on_table[j] == cards[i]){
                    found = true;
                    break;
                }
            }
            if(!found) return e_ok; // Not legal because card not on table
        }
    }

    if(is_ace(move->card_played)){
        if(is_exact_partition(cards, n, 11) || is_exact_partition(cards, n, 1)){
            *ret = true;
        }
    } else {
        uint8_t val = get_value(move->card_played);
        if(is_exact_partition(cards, n, val)){
            *ret = true;
        }
    }

    return e_ok;
}

static void find_valid_capture_masks(const uint8_t *table_cards, uint8_t n, uint8_t target_val, uint8_t *is_valid_capture){
    if(n == 0 || n > 16) return;
    uint32_t num_masks = 1u << n;
    uint8_t valid_base[num_masks];
    int8_t memo[num_masks];

    for(uint32_t m = 0; m < num_masks; m++){
        valid_base[m] = (m > 0) ? (uint8_t)subset_sums_to(table_cards, m, n, target_val) : 0;
        memo[m] = -1;
    }

    for(uint32_t m = 1; m < num_masks; m++){
        if(can_partition_rec(m, valid_base, memo)){
            is_valid_capture[m] = 1;
        }
    }
}

t_cteerr gen_card_moves(struct s_cte_move_list *moves, const struct table *table, t_card card){
    if(!moves) return e_null;
    if(!moves->moves && moves->max == 0){
        t_cteerr err = init_move_list(moves, 16);
        if(err != e_ok) return err;
    }

    // 1. Always add the drop move (cards_picked.size = 0)
    struct s_cte_move drop_move;
    drop_move.card_played = card;
    drop_move.cards_picked.size = 0;
    t_cteerr err = move_list_push(moves, &drop_move);
    if(err != e_ok) return err;

    if(!table) return e_ok;
    uint8_t n = table->nb_cards_on_table;
    if(n == 0) return e_ok;
    if(n > 16) n = 16;

    uint32_t num_masks = 1u << n;
    uint8_t *is_valid_capture = calloc(num_masks, sizeof(uint8_t));
    if(!is_valid_capture) return e_alloc;

    if(is_ace(card)){
        find_valid_capture_masks(table->cards_on_table, n, 11, is_valid_capture);
        find_valid_capture_masks(table->cards_on_table, n, 1, is_valid_capture);
    } else {
        uint8_t val = get_value(card);
        find_valid_capture_masks(table->cards_on_table, n, val, is_valid_capture);
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

            uint8_t idx = 0;
            for(uint8_t i = 0; i < n; i++){
                if(m & (1u << i)){
                    move.cards_picked.array[idx++] = table->cards_on_table[i];
                }
            }

            err = move_list_push(moves, &move);
            if(err != e_ok){
                free(is_valid_capture);
                return err;
            }
        }
    }

    free(is_valid_capture);
    return e_ok;
}

t_cteerr gen_all_moves(struct s_cte_move_list *moves, const struct table *table, const struct s_cte_hand *hand){
    if(!moves || !hand) return e_null;
    if(!moves->moves && moves->max == 0){
        t_cteerr err = init_move_list(moves, 32);
        if(err != e_ok) return err;
    }
    for(uint8_t i = 0 ; i < hand->size; i++){
        t_cteerr err = gen_card_moves(moves, table, hand->array[i]);
        if(err != e_ok) return err;
    }
    return e_ok;
}

t_cteerr play_move(struct table *table, const struct s_cte_move *move, struct s_cte_player_data *player, bool *captured){
    if(!table || !move || !player) return e_null;

    // 1. Remove card_played from player hand
    int hand_idx = -1;
    for(uint8_t i = 0; i < player->hand.size; i++){
        if(player->hand.array[i] == move->card_played){
            hand_idx = i;
            break;
        }
    }
    if(hand_idx == -1) return e_inval_val;

    for(uint8_t i = hand_idx; i + 1 < player->hand.size; i++){
        player->hand.array[i] = player->hand.array[i+1];
    }
    player->hand.size--;

    // 2. Drop move
    if(move->cards_picked.size == 0){
        if(table->nb_cards_on_table >= 52) return e_inval_val;
        table->cards_on_table[table->nb_cards_on_table++] = move->card_played;
        if(captured) *captured = false;
        return e_ok;
    }

    // 3. Capture move
    if(captured) *captured = true;

    for(uint8_t i = 0; i < move->cards_picked.size; i++){
        uint8_t target = move->cards_picked.array[i];
        int table_idx = -1;
        for(uint8_t j = 0; j < table->nb_cards_on_table; j++){
            if(table->cards_on_table[j] == target){
                table_idx = j;
                break;
            }
        }
        if(table_idx == -1) return e_inval_val;

        for(uint8_t j = table_idx; j + 1 < table->nb_cards_on_table; j++){
            table->cards_on_table[j] = table->cards_on_table[j+1];
        }
        table->nb_cards_on_table--;

        if(player->won_cards.size < 52){
            player->won_cards.array[player->won_cards.size++] = target;
        }
    }

    if(player->won_cards.size < 52){
        player->won_cards.array[player->won_cards.size++] = move->card_played;
    }

    if(table->nb_cards_on_table == 0){
        player->nb_tablic++;
    }

    return e_ok;
}

s_cte_move_score score_move(const struct s_cte_move *move, uint8_t nb_cards_on_table){
    s_cte_move_score res = {0};
    if(!move) return res;

    if(move->cards_picked.size == 0){
        return res;
    }

    res.card_points = get_points(move->card_played);
    for(uint8_t i = 0; i < move->cards_picked.size; i++){
        res.card_points += get_points(move->cards_picked.array[i]);
    }
    res.nb_cards = (uint8_t)(1 + move->cards_picked.size);

    if(nb_cards_on_table > 0 && move->cards_picked.size == nb_cards_on_table){
        res.is_tablic = true;
    }

    res.total_points = res.card_points + (res.is_tablic ? 1 : 0);
    return res;
}

void format_move(char *buf, size_t buf_size, const struct s_cte_move *move, e_cte_render_style style){
    if(!buf || buf_size == 0) return;
    if(!move){
        snprintf(buf, buf_size, "(null)");
        return;
    }

    char card_str[16];
    format_card(card_str, sizeof(card_str), move->card_played, style);

    if(move->cards_picked.size == 0){
        snprintf(buf, buf_size, "Drop %s", card_str);
        return;
    }

    char picked_str[128] = "";
    size_t offset = 0;
    for(uint8_t i = 0; i < move->cards_picked.size; i++){
        char picked_card[16];
        format_card(picked_card, sizeof(picked_card), move->cards_picked.array[i], style);
        if(i > 0){
            offset += snprintf(picked_str + offset, sizeof(picked_str) > offset ? sizeof(picked_str) - offset : 0, ", ");
        }
        offset += snprintf(picked_str + offset, sizeof(picked_str) > offset ? sizeof(picked_str) - offset : 0, "%s", picked_card);
    }

    snprintf(buf, buf_size, "Play %s -> Take [ %s ]", card_str, picked_str);
}

void print_move(const struct s_cte_move *move){
    if(!move) return;
    char buf[256];
    format_move(buf, sizeof(buf), move, CTE_RENDER_UNICODE);
    printf("%s\n", buf);
}
