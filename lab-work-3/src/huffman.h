#ifndef HUFFMAN_H
#define HUFFMAN_H

#include "stcompress.h"

// Compress file using Huffman coding
int huffman_compress(CompressionContext *ctx);

// Decompress file compressed with Huffman coding
int huffman_decompress(CompressionContext *ctx);

#endif
