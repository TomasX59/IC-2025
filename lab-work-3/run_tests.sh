#!/bin/bash

FILENAME="model.safetensors"
COMPRESSED_FILENAME="${FILENAME}.gz"
EXECUTABLE="./gzip_benchmarking"

# --- Teste de Nível -1 (Velocidade) ---
echo "=================================================="
echo "           PONTO DE OPERAÇÃO: Nível -1 (Velocidade)"
echo "=================================================="
echo "1. MEDIÇÃO DE TEMPO (COMPRESSÃO):"
time $EXECUTABLE -1
echo "--------------------------------------------------"
echo "2. MEDIÇÃO DE TEMPO (DESCOMPRESSÃO):"
# O ficheiro original foi restaurado pelo passo anterior, então comprimimos de novo apenas para o tempo
gzip -f -1 $FILENAME # Cria o .gz para o teste de descompressão
time gzip -f -d $COMPRESSED_FILENAME
echo ""

# --- Teste de Nível -9 (Taxa Máxima) ---
echo "=================================================="
echo "           PONTO DE OPERAÇÃO: Nível -9 (Taxa Máxima)"
echo "=================================================="
echo "1. MEDIÇÃO DE TEMPO (COMPRESSÃO):"
time $EXECUTABLE -9
echo "--------------------------------------------------"
echo "2. MEDIÇÃO DE TEMPO (DESCOMPRESSÃO):"
# O ficheiro original foi restaurado pelo passo anterior, então comprimimos de novo apenas para o tempo
gzip -f -9 $FILENAME # Cria o .gz para o teste de descompressão
time gzip -f -d $COMPRESSED_FILENAME
echo ""