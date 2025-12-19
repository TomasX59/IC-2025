#ifndef GZIP_H
#define GZIP_H

#include "stcompress.h"

// Compress file using gzip (via system call)
// level: compression level 1-9 (1=fastest, 9=best compression)
int gzip_compress(CompressionContext *ctx, int level);

// Decompress file compressed with gzip
int gzip_decompress(CompressionContext *ctx);

#endif
