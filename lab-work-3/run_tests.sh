#!/bin/bash

INPUT_FILE="./res/model.safetensors"
EXECUTABLE="./stcompress"

# Check if input file exists
if [ ! -f "$INPUT_FILE" ]; then
    echo "Error: Input file '$INPUT_FILE' not found"
    exit 1
fi

# Check if executable exists
if [ ! -f "$EXECUTABLE" ]; then
    echo "Building project..."
    make
fi

echo "=================================================="
echo "           Running all compression algorithms"
echo "=================================================="
echo ""

# --- Arithmetic ---
echo "=================================================="
echo "                    ARITHMETIC"
echo "=================================================="
$EXECUTABLE -a arithmetic -i $INPUT_FILE
echo ""

# --- Optimized Arithmetic ---
echo "=================================================="
echo "              OPTIMIZED ARITHMETIC"
echo "=================================================="
$EXECUTABLE -a optarithm -i $INPUT_FILE
echo ""

# --- Huffman ---
echo "=================================================="
echo "                     HUFFMAN"
echo "=================================================="
$EXECUTABLE -a huffman -i $INPUT_FILE
echo ""

# --- LZ77 ---
echo "=================================================="
echo "                      LZ77"
echo "=================================================="
$EXECUTABLE -a lz77 -i $INPUT_FILE
echo ""

# --- Gzip (level 9, default) ---
echo "=================================================="
echo "                   GZIP (level 9)"
echo "=================================================="
$EXECUTABLE -a gzip -i $INPUT_FILE
echo ""

# --- Gzip level 1 (fast) ---
echo "=================================================="
echo "                   GZIP (level 1)"
echo "=================================================="
$EXECUTABLE -a gzip -c 1 -i $INPUT_FILE
echo ""

echo "=================================================="
echo "                  ALL TESTS COMPLETE"
echo "=================================================="