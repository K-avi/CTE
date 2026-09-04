#!/bin/bash
# bench_csv.sh — Build, run benchmark, append results to bench.csv
# Usage: ./tools/bench_csv.sh [label]

set -e
cd "$(dirname "$0")/.."

LABEL="${1:-baseline}"
CSV="bench.csv"
BUILD="build/bench_backends"

# Build via Makefile
make bench

# Run and capture output
OUTPUT=$(./"$BUILD" 2>&1)

# Write CSV header if file doesn't exist
if [ ! -f "$CSV" ]; then
    echo "label,timestamp,array_ns,bb_vtable_ns,cpt_rnk_ns,array_mops,bb_vtable_mops,cpt_rnk_mops" > "$CSV"
fi

# Parse ns/pos and Mcoups/s from benchmark output
TS=$(date +%Y-%m-%dT%H:%M:%S)

arr_ns=$(echo "$OUTPUT" | grep "Array (Référence" | grep -oP '[\d.]+(?= ns)')
bb_ns=$(echo "$OUTPUT" | grep "Bitboard SWAR (Vtable" | grep -oP '[\d.]+(?= ns)')
crk_ns=$(echo "$OUTPUT" | grep "Bitboard Compact SWAR" | grep -oP '[\d.]+(?= ns)')

arr_mops=$(echo "$OUTPUT" | grep "Array (Référence" | grep -oP '[\d.]+(?= Mcoups)')
bb_mops=$(echo "$OUTPUT" | grep "Bitboard SWAR (Vtable" | grep -oP '[\d.]+(?= Mcoups)')
crk_mops=$(echo "$OUTPUT" | grep "Bitboard Compact SWAR" | grep -oP '[\d.]+(?= Mcoups)')

echo "${LABEL},${TS},${arr_ns},${bb_ns},${crk_ns},${arr_mops},${bb_mops},${crk_mops}" >> "$CSV"

echo "=== Résultat ajouté à $CSV (label: $LABEL) ==="
echo "$OUTPUT"
