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
#define get_value(card) ((card) < 52 ? values[(card)%13] : values[0])
#define get_color(card) ((card) < 52 ? ((card)/13) : 0)
#define is_ace(card) (get_value(card) == 11)

#define get_points_var(value, color) (__tab_points[(color)*13 + (value-2)])
#define get_points(card) ((card) < 52 ? __tab_points[card] : 0)

#define DECKSIZE 52

// Reentrant deck and table structures
struct deck {
    uint8_t cur_card;
    t_card cards[52];
};

// Card render styles
typedef enum {
    CTE_RENDER_UNICODE = 0,
    CTE_RENDER_ASCII   = 1
} e_cte_render_style;

// Deck management
void init_deck(struct deck *d);
void shuffle_deck(struct deck *d);

// Formatting and printing functions
void format_card(char *buf, size_t buf_size, t_card card, e_cte_render_style style);
void print_card(uint8_t card);
void print_table(uint64_t table_bb);

#endif
