#include "gzip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_COMMAND_LEN 1024

int gzip_compress(CompressionContext *ctx, int level)
{
  char cmd[MAX_COMMAND_LEN];

  if (ctx->progress_callback) ctx->progress_callback(0, "Compressing with gzip...");

  // Use gzip -c to read file directly and output to stdout
  snprintf(cmd, sizeof(cmd), "gzip -%d -c \"%s\" > \"%s\"", level, ctx->input_path, ctx->compressed_path);

  if (system(cmd) != 0)
  {
    fprintf(stderr, "Error: gzip compression failed\n");
    return -1;
  }

  if (ctx->progress_callback) ctx->progress_callback(100, "Compression complete");

  return 0;
}

int gzip_decompress(CompressionContext *ctx)
{
  char cmd[MAX_COMMAND_LEN];

  if (ctx->progress_callback) ctx->progress_callback(0, "Decompressing with gzip...");

  // Use gzip -c to read file directly and output to stdout
  snprintf(cmd, sizeof(cmd), "gzip -d -c \"%s\" > \"%s\"", ctx->compressed_path, ctx->decompressed_path);

  if (system(cmd) != 0)
  {
    fprintf(stderr, "Error: gzip decompression failed\n");
    return -1;
  }

  if (ctx->progress_callback) ctx->progress_callback(100, "Decompression complete");

  return 0;
}
