#ifndef __CTE_CARD_TRACKER_H
#define __CTE_CARD_TRACKER_H

#include "card.h"

// Bitmask of all 52 cards
#define CTE_ALL_52  ((1ULL << 52) - 1)

// Card tracker: maintains the set of cards whose location is unknown
// to a given observer player. Used by fair AI evaluators for determinization.
typedef struct {
    uint64_t unseen_bb;     // Cards not yet revealed to this observer
    uint8_t  unseen_count;  // popcount(unseen_bb)
    uint8_t  observer;      // Player ID this tracker belongs to
} s_cte_card_tracker;

struct s_cte_game_state;

// Initialize tracker directly from the current board and player state:
// seen_cards = own_hand | table | all_won_cards; unseen = CTE_ALL_52 & ~seen.
// This is stateless, exact, and handles deals, rounds, drops, and captures cleanly.
void tracker_init_from_state(s_cte_card_tracker *tracker, uint8_t observer,
                             const struct s_cte_game_state *state);

// Initialize tracker at the start of a round (manual bitboard inputs).
void tracker_init(s_cte_card_tracker *tracker, uint8_t observer,
                  uint64_t own_hand_bb, uint64_t table_bb);

// Update tracker when any card becomes visible (opponent plays a card).
// card_played: the card index (0..51)
// capture_mask: bitboard of cards captured (already known from table)
void tracker_observe_play(s_cte_card_tracker *tracker, t_card card_played);

// Update tracker after a new deal: remove the observer's new hand cards from unseen.
void tracker_observe_deal(s_cte_card_tracker *tracker, uint64_t own_new_hand_bb);

// Sample a plausible determinization: deal unseen cards into opponent hands.
// Fills opp_hands[0..nb_players-1] with bitboards. observer's slot is set to own_hand_bb.
// hand_sizes[i] = number of cards player i currently holds.
// seed: pointer to RNG state (modified in place for reproducibility).
void tracker_determinize(const s_cte_card_tracker *tracker,
                         uint64_t own_hand_bb,
                         const uint8_t hand_sizes[],
                         uint8_t nb_players,
                         uint64_t opp_hands[],
                         uint32_t *seed);

#endif
