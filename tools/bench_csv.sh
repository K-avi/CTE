#!/bin/bash
# bench_csv.sh — Build, run benchmark, append results to bench.csv
# Usage: ./tools/bench_csv.sh [label]

set -e
cd "$(dirname "$0")/.."

LABEL="${1:-baseline}"
CSV="bench.csv"
BUILD="build/bench_backends"

# Build
mkdir -p build
gcc -std=gnu2x -Wall -Wextra -Wpedantic -Wno-unused-parameter -march=native \
    -O3 -funroll-loops -I./include \
    -o "$BUILD" \
    src/card.c src/player.c src/move.c src/game.c src/eval.c src/minmax.c \
    src/front_cli.c src/front_tui.c src/engine.c src/backend_bitboard.c \
    src/bitboard_tables.c src/bitboard_rank_tables.c tools/bench_backends.c -lncursesw

# Run and capture output
OUTPUT=$(./"$BUILD" 2>&1)

# Write CSV header if file doesn't exist
if [ ! -f "$CSV" ]; then
    echo "label,timestamp,array_ns,dyn_ns,tbl_ns,cpt_dyn_ns,cpt_tbl_ns,cpt_rnk_ns,array_mops,dyn_mops,tbl_mops,cpt_dyn_mops,cpt_tbl_mops,cpt_rnk_mops" > "$CSV"
fi

# Parse ns/pos and Mcoups/s from benchmark output
TS=$(date +%Y-%m-%dT%H:%M:%S)

arr_ns=$(echo "$OUTPUT" | grep "Array" | grep -oP '[\d.]+(?= ns)')
dyn_ns=$(echo "$OUTPUT" | grep "Dynamique (Carry" | grep -oP '[\d.]+(?= ns)')
tbl_ns=$(echo "$OUTPUT" | grep "1D Pivot Tables (Opt" | grep -oP '[\d.]+(?= ns)')
cdy_ns=$(echo "$OUTPUT" | grep "Compact Dynamique" | grep -oP '[\d.]+(?= ns)')
ctb_ns=$(echo "$OUTPUT" | grep "Compact 1D Pivot" | grep -oP '[\d.]+(?= ns)')
crk_ns=$(echo "$OUTPUT" | grep "Compact SWAR Rank" | grep -oP '[\d.]+(?= ns)')

arr_mops=$(echo "$OUTPUT" | grep "Array" | grep -oP '[\d.]+(?= Mcoups)')
dyn_mops=$(echo "$OUTPUT" | grep "Dynamique (Carry" | grep -oP '[\d.]+(?= Mcoups)')
tbl_mops=$(echo "$OUTPUT" | grep "1D Pivot Tables (Opt" | grep -oP '[\d.]+(?= Mcoups)')
cdy_mops=$(echo "$OUTPUT" | grep "Compact Dynamique" | grep -oP '[\d.]+(?= Mcoups)')
ctb_mops=$(echo "$OUTPUT" | grep "Compact 1D Pivot" | grep -oP '[\d.]+(?= Mcoups)')
crk_mops=$(echo "$OUTPUT" | grep "Compact SWAR Rank" | grep -oP '[\d.]+(?= Mcoups)')

echo "${LABEL},${TS},${arr_ns},${dyn_ns},${tbl_ns},${cdy_ns},${ctb_ns},${crk_ns},${arr_mops},${dyn_mops},${tbl_mops},${cdy_mops},${ctb_mops},${crk_mops}" >> "$CSV"

echo "=== Résultat ajouté à $CSV (label: $LABEL) ==="
echo "$OUTPUT"
