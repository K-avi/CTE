#include "card_tracker.h"
#include "eval.h"
#include <string.h>

void tracker_init_from_state(s_cte_card_tracker *tracker, uint8_t observer,
                             const s_cte_game_state *state)
{
    if(!tracker || !state) return;
    tracker->observer = observer;
    uint64_t seen_bb = state->table_bb;
    if(state->players){
        // Observer's own hand is seen
        if(observer < state->players->size){
            const struct s_cte_player_data *me = &state->players->players[observer];
            for(uint8_t i = 0; i < me->hand.size; i++){
                if(me->hand.array[i] < 52) seen_bb |= (1ULL << me->hand.array[i]);
            }
        }
        // All won/captured cards by all players are seen
        for(uint8_t p = 0; p < state->players->size; p++){
            const struct s_cte_player_data *pl = &state->players->players[p];
            for(uint8_t i = 0; i < pl->won_cards.size; i++){
                if(pl->won_cards.array[i] < 52) seen_bb |= (1ULL << pl->won_cards.array[i]);
            }
        }
    }
    tracker->unseen_bb = CTE_ALL_52 & ~seen_bb;
    tracker->unseen_count = (uint8_t)__builtin_popcountll(tracker->unseen_bb);
}

void tracker_init(s_cte_card_tracker *tracker, uint8_t observer,
                  uint64_t own_hand_bb, uint64_t table_bb)
{
    if(!tracker) return;
    tracker->observer = observer;
    // Everything except what the observer can see is unseen
    tracker->unseen_bb = CTE_ALL_52 & ~(own_hand_bb | table_bb);
    tracker->unseen_count = (uint8_t)__builtin_popcountll(tracker->unseen_bb);
}

void tracker_observe_play(s_cte_card_tracker *tracker, t_card card_played)
{
    if(!tracker || card_played >= 52) return;
    uint64_t bit = 1ULL << card_played;
    if(tracker->unseen_bb & bit){
        tracker->unseen_bb &= ~bit;
        if(tracker->unseen_count > 0) tracker->unseen_count--;
    }
}

void tracker_observe_deal(s_cte_card_tracker *tracker, uint64_t own_new_hand_bb)
{
    if(!tracker) return;
    // The observer now sees their new cards — remove from unseen
    tracker->unseen_bb &= ~own_new_hand_bb;
    tracker->unseen_count = (uint8_t)__builtin_popcountll(tracker->unseen_bb);
}

// xorshift32 — fast deterministic PRNG for sampling
static inline uint32_t xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

void tracker_determinize(const s_cte_card_tracker *tracker,
                         uint64_t own_hand_bb,
                         const uint8_t hand_sizes[],
                         uint8_t nb_players,
                         uint64_t opp_hands[],
                         uint32_t *seed)
{
    if(!tracker || !hand_sizes || !opp_hands || !seed) return;

    // Collect unseen cards into a flat array
    uint8_t unseen[52];
    uint8_t n_unseen = 0;
    uint64_t temp = tracker->unseen_bb;
    while(temp > 0){
        unseen[n_unseen++] = (uint8_t)__builtin_ctzll(temp);
        temp &= (temp - 1);
    }

    // Fisher-Yates partial shuffle: we only need sum(opp_hand_sizes) cards
    uint8_t total_needed = 0;
    for(uint8_t p = 0; p < nb_players; p++){
        if(p == tracker->observer) continue;
        total_needed += hand_sizes[p];
    }

    // Clamp to available unseen cards
    if(total_needed > n_unseen) total_needed = n_unseen;

    for(uint8_t i = 0; i < total_needed && i < n_unseen; i++){
        uint8_t remaining = (uint8_t)(n_unseen - i);
        uint32_t j = i + (xorshift32(seed) % remaining);
        // Swap
        uint8_t tmp = unseen[i];
        unseen[i] = unseen[j];
        unseen[j] = tmp;
    }

    // Deal shuffled unseen cards round-robin to opponents
    for(uint8_t p = 0; p < nb_players; p++){
        opp_hands[p] = 0;
    }
    opp_hands[tracker->observer] = own_hand_bb;

    uint8_t deal_idx = 0;
    for(uint8_t p = 0; p < nb_players; p++){
        if(p == tracker->observer) continue;
        for(uint8_t c = 0; c < hand_sizes[p] && deal_idx < total_needed; c++){
            opp_hands[p] |= (1ULL << unseen[deal_idx++]);
        }
    }
}
