#!/usr/bin/env python3
"""
PoC Piste B - Fast Size Estimation (corrected BFS)
Bug fix: can[mask] must be initialized to valid[mask], not just can[0]=True.
Single-element valid blocks are terminal cases, not intermediate ones.
"""

import itertools, math

RANK_VALS = [2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14]

def card_val(c):   return RANK_VALS[c % 13]
def is_ace(c):     return (c % 13) == 9

def subset_sums_to(cards, target):
    s = sum(card_val(c) for c in cards)
    a = sum(1 for c in cards if is_ace(c))
    if s < target: return False
    diff = s - target
    if diff % 10 != 0: return False
    return (diff // 10) <= a

def can_partition_iter(cards, target):
    """
    Corrected BFS: can cards be split into k>=2 groups each summing to target?
    Key fix: can[mask] initialised to valid[mask] (a valid singleton IS a partition).
    Returns True only if the FULL set is NOT directly valid (that's a simple capture).
    """
    n = len(cards)
    if n < 2: return False
    N = 1 << n
    full = N - 1

    # valid[mask] = True if subset(mask) directly sums to target
    valid = [False] * N
    for mask in range(1, N):
        sub = [cards[i] for i in range(n) if mask >> i & 1]
        valid[mask] = subset_sums_to(sub, target)

    # can[mask] = True if mask can be partitioned into 1+ valid groups
    # Init: a directly valid subset can trivially partition into 1 group (itself)
    can = list(valid)
    can[0] = True

    for mask in range(1, N):
        if can[mask]: continue  # already True from valid[]
        sub = (mask - 1) & mask
        while sub > 0:
            if valid[sub] and can[mask ^ sub]:
                can[mask] = True
                break
            sub = (sub - 1) & mask

    # Multi-partition: full CAN be partitioned but is NOT a direct single-sum capture
    return can[full] and not valid[full]

def main():
    print("=== PoC Piste B: Corrected Partition LUT Size Estimation ===\n")

    multi_keys = {}  # (frozenset(cards), target) -> True
    TARGETS = list(range(1, 15))

    for n in range(2, 6):
        total = math.comb(52, n)
        print(f"  n={n}: C(52,{n}) = {total:,} subsets...", flush=True)
        count_n = 0
        for cards in itertools.combinations(range(52), n):
            s = sum(card_val(c) for c in cards)
            a = sum(1 for c in cards if is_ace(c))
            for target in TARGETS:
                # Fast pre-filter: can sum == k*target for k>=2 after Ace flex?
                found = False
                for ka in range(a + 1):
                    adj = s - 10 * ka
                    if adj >= 2 * target and adj % target == 0:
                        found = True; break
                if not found: continue
                if can_partition_iter(cards, target):
                    key = (frozenset(cards), target)
                    if key not in multi_keys:
                        multi_keys[key] = True
                        count_n += 1
        total_so_far = len(multi_keys)
        print(f"    n={n}: +{count_n:,} new keys  (total: {total_so_far:,})")

    print(f"\n=== Results (n=2..5) ===")
    print(f"  Distinct (subset, target) multi-partition pairs: {len(multi_keys):,}")
    print(f"  Distinct subset bitmasks: {len(set(k for k,_ in multi_keys)):,}")

    for n in range(2, 6):
        cnt = sum(1 for (c,_) in multi_keys if len(c)==n)
        print(f"    n={n}: {cnt:,}")

    print(f"\n  Extrapolation to n=6: C(52,6)={math.comb(52,6):,} subsets")
    n4 = sum(1 for (c,_) in multi_keys if len(c)==4)
    n5 = sum(1 for (c,_) in multi_keys if len(c)==5)
    ratio = (n5/n4) if n4 > 0 else "?"
    print(f"    n4={n4:,}  n5={n5:,}  growth ratio={ratio:.1f}x")
    if isinstance(ratio, float):
        print(f"    Estimated n=6: ~{int(n5*ratio):,} keys")

    total = len(multi_keys)
    if total < 50_000:
        print(f"\n  -> FEASIBLE global LUT: {total} entries (~{total*8//1024} KB)")
    elif total < 500_000:
        print(f"\n  -> MARGINAL: {total} entries (~{total*8//1024} KB)")
    else:
        print(f"\n  -> NOT FEASIBLE as flat global LUT")
    print(f"\n  Alternative Piste B: per-position precomputation (max 2^10=1024 entries)")

if __name__ == "__main__":
    main()
