#ifndef LZ77_H
#define LZ77_H

#include "stcompress.h"

// Compress file using LZ77
int lz77_compress(CompressionContext *ctx);

// Decompress file compressed with LZ77
int lz77_decompress(CompressionContext *ctx);

#endif
