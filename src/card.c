#include "card.h"
#include <stdio.h>
#include <stdlib.h>

// Point values of each card in the game
uint8_t __tab_points[52] = {
   1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, // clubs (2 of clubs = 1, 10..King,Ace = 1)
   0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 1, 1, // diamonds (10 of diamonds = 2, Ace,Jack,Queen,King = 1)
   0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, // hearts (10..King,Ace = 1)
   0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, // spades (10..King,Ace = 1)
};

// Nominal numeric values (Ace = 11, Jack = 12, Queen = 13, King = 14)
uint8_t values[13] = {
    2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
};

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

void init_deck(struct deck *d){
    if(!d) return;
    d->cur_card = 0;
    for(uint8_t i = 0; i < 52; i++){
        d->cards[i] = i;
    }
}

void shuffle_deck(struct deck *d){
    if(!d) return;
    for(int i = 51; i > 0; i--){
        int j = rand() % (i + 1);
        t_card tmp = d->cards[i];
        d->cards[i] = d->cards[j];
        d->cards[j] = tmp;
    }
}

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

void print_card(uint8_t card){
    uint8_t value = get_value(card);
    uint8_t color = get_color(card);
    printf("%s of %s\n", value_str[value-2], color_str[color]);
}

void print_table(uint64_t table_bb){
    uint8_t count = (uint8_t)__builtin_popcountll(table_bb);
    printf("Cards on table (%u): \n", (unsigned)count);
    uint64_t temp = table_bb;
    while(temp > 0){
        t_card card = (t_card)__builtin_ctzll(temp);
        uint8_t value = get_value(card);
        uint8_t color = get_color(card);
        printf("%s of %s\n", value_str[value-2], color_str[color]);
        temp &= (temp - 1);
    }
    printf("\n");
}
