#!/bin/bash

INPUT_DIR="input"
OUTPUT_DIR="output"
SCRIPT="Code/strict_2PL_with_wait_die.py"

# Ensure output directory exists
mkdir -p "$OUTPUT_DIR"

# Loop over input1.txt to input5.txt
for i in {1..5}; do
    INPUT_FILE="${INPUT_DIR}/input${i}.txt"
    OUTPUT_FILE="${OUTPUT_DIR}/output${i}.txt"

    echo "Processing $INPUT_FILE -> $OUTPUT_FILE"
    python3 "$SCRIPT" "$INPUT_FILE" "$OUTPUT_FILE"
done
