#ifndef __CTE_CARD_H
#define __CTE_CARD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Error codes
typedef uint8_t t_cteerr;

enum cter_err_codes {
    e_ok = 0,
    e_null,
    e_inval_val,
    e_alloc,
    e_realloc
};

typedef uint8_t t_card;
typedef uint8_t t_points;

enum e_colors {
    clubs = 0,
    diamonds,
    hearts,
    spade
};

// Global card lookup tables
extern uint8_t __tab_points[52];
extern uint8_t values[13];

// Utility macros
#define get_value(card) (values[(card)%13])
#define get_color(card) ((card)/13)
#define is_ace(card) (get_value(card) == 11)

#define get_points_var(value, color) (__tab_points[(color)*13 + (value-2)])
#define get_points(card) (__tab_points[card])

#define DECKSIZE 52

// Global game components
extern struct deck {
    uint8_t cur_card;
    t_card cards[52];
} deck;

extern struct table {
    uint8_t nb_cards_on_table;
    t_card cards_on_table[52];
} table;

// Card render styles
typedef enum {
    CTE_RENDER_UNICODE = 0,
    CTE_RENDER_ASCII   = 1
} e_cte_render_style;

// Formatting and printing functions
void format_card(char *buf, size_t buf_size, t_card card, e_cte_render_style style);
void print_card(uint8_t card);
void print_table(void);

#endif
