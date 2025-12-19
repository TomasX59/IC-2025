#ifndef ARITHMETIC_H
#define ARITHMETIC_H

#include "stcompress.h"

// Compress file using arithmetic coding
int arithmetic_compress(CompressionContext *ctx);

// Decompress file compressed with arithmetic coding
int arithmetic_decompress(CompressionContext *ctx);

#endif
