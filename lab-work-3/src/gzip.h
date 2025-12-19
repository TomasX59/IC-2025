#ifndef GZIP_H
#define GZIP_H

#include "stcompress.h"

// Compress file using gzip (via system call)
// Uses compression level 9 by default
int gzip_compress(CompressionContext *ctx);

// Decompress file compressed with gzip
int gzip_decompress(CompressionContext *ctx);

#endif
