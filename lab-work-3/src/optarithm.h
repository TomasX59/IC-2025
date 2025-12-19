#ifndef optarithm_H
#define optarithm_H

#include "stcompress.h"

// Optimized arithmetic coding with preprocessing pipeline:
// 1. Byte transposition - groups bytes by position within elements
// 2. Delta encoding - stores differences between consecutive bytes
// 3. Bit-plane separation - groups bits by significance level
// 4. Adaptive arithmetic coding
//
// Optimized for SafeTensors float16/float32 data.
// Slower but achieves significantly better compression.

int optarithm_compress(CompressionContext *ctx);
int optarithm_decompress(CompressionContext *ctx);

#endif