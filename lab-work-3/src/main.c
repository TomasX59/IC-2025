#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arithmetic.h"
#include "gzip.h"
#include "huffman.h"
#include "lz77.h"
#include "optarithm.h"
#include "stcompress.h"

#define DEFAULT_INPUT_PATH "./res/model.safetensors"
#define TMP_DIR "tmp/"

typedef enum
{
  ALG_NONE = -1,
  ALG_ARITHMETIC = 0,
  ALG_HUFFMAN = 1,
  ALG_LZ77 = 2,
  ALG_OPTM = 3,
  ALG_GZIP = 4
} Algorithm;

typedef struct
{
  Algorithm algorithm;
  char *input_path;
  size_t memory_limit_mb;
  int memory_limit_set;
} ProgramArgs;

static void print_usage(const char *prog_name)
{
  printf("Usage: %s -a <algorithm> [-i <input_file>] [-m <memory_mb>]\n", prog_name);
  printf("\n");
  printf("Options:\n");
  printf("  -a <algorithm>   Compression algorithms: optarithm, arithmetic, huffman, lz77, gzip\n");
  printf("  -i <input_file>  Input file path (default: %s)\n", DEFAULT_INPUT_PATH);
  printf("  -m <memory_mb>   Memory limit in MB (optional)\n");
  printf("  -h               Show this help message\n");
  printf("\n");
  printf("Algorithms:\n");
  printf("  optarithm      - Optmized arithmetic, best compression (slowest)\n");
  printf("  arithmetic     - Best compression ratio (slow)\n");
  printf("  huffman        - Balanced compression and speed\n");
  printf("  lz77           - Best speed (lower compression)\n");
  printf("  gzip           - System gzip (level 9)\n");
}

static Algorithm parse_algorithm(const char *str)
{
  if (strcmp(str, "optarithm") == 0) return ALG_OPTM;
  if (strcmp(str, "arithmetic") == 0) return ALG_ARITHMETIC;
  if (strcmp(str, "huffman") == 0) return ALG_HUFFMAN;
  if (strcmp(str, "lz77") == 0) return ALG_LZ77;
  if (strcmp(str, "gzip") == 0) return ALG_GZIP;
  return ALG_NONE;
}

static const char *algorithm_name(Algorithm alg)
{
  switch (alg)
  {
  case ALG_ARITHMETIC:
    return "Arithmetic";
  case ALG_OPTM:
    return "Arithmetic Optimized";
  case ALG_HUFFMAN:
    return "Huffman";
  case ALG_LZ77:
    return "LZ77";
  case ALG_GZIP:
    return "Gzip";
  default:
    return "Unknown";
  }
}

static const char *algorithm_id(Algorithm alg)
{
  switch (alg)
  {
  case ALG_ARITHMETIC:
    return "arithmetic";
  case ALG_OPTM:
    return "optarithm";
  case ALG_HUFFMAN:
    return "huffman";
  case ALG_LZ77:
    return "lz77";
  case ALG_GZIP:
    return "gzip";
  default:
    return "unknown";
  }
}

static int parse_args(int argc, char *argv[], ProgramArgs *args)
{
  args->algorithm = ALG_NONE;
  args->input_path = DEFAULT_INPUT_PATH;
  args->memory_limit_mb = 0;
  args->memory_limit_set = 0;

  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "-a") == 0 && i + 1 < argc)
    {
      args->algorithm = parse_algorithm(argv[++i]);
      if (args->algorithm == ALG_NONE)
      {
        fprintf(stderr, "Error: Unknown algorithm '%s'\n", argv[i]);
        return -1;
      }
    }
    else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) { args->input_path = argv[++i]; }
    else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc)
    {
      args->memory_limit_mb = (size_t)atol(argv[++i]);
      args->memory_limit_set = 1;
    }
    else if (strcmp(argv[i], "-h") == 0)
    {
      print_usage(argv[0]);
      exit(0);
    }
    else
    {
      fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
      return -1;
    }
  }

  if (args->algorithm == ALG_NONE)
  {
    fprintf(stderr, "Error: Algorithm not specified. Use -a <algorithm>\n");
    return -1;
  }

  return 0;
}

static void progress_callback(int percent, const char *stage)
{
  printf("\r[");
  int filled = percent / 5;
  for (int i = 0; i < 20; i++)
  {
    if (i < filled) printf("#");
    else printf("-");
  }
  printf("] %3d%% %s", percent, stage);
  fflush(stdout);
}

int main(int argc, char *argv[])
{
  ProgramArgs args;
  CompressionContext ctx = {0};
  CompressionResult result = {0};
  int ret = 0;

  // Parse arguments
  if (parse_args(argc, argv, &args) != 0)
  {
    print_usage(argv[0]);
    return 1;
  }

  printf("\nSafeTensors Compressor\n");
  printf("======================\n");
  printf("Algorithm: %s\n", algorithm_name(args.algorithm));
  printf("Input: %s\n", args.input_path);

  // Initialize context
  ctx.input_path = args.input_path;
  ctx.progress_callback = progress_callback;

  // Check if input file exists
  if (!file_exists(ctx.input_path))
  {
    fprintf(stderr, "Error: Input file '%s' not found\n", ctx.input_path);
    fprintf(stderr, "NOTE: put the model.safetensors file at the ./res/ folder!!!\n");
    return 1;
  }

  // Get input file size
  ctx.input_size = get_file_size(ctx.input_path);
  printf("Input size: %zu bytes (%.2f MB)\n", ctx.input_size, ctx.input_size / (1024.0 * 1024.0));

  // Memory estimation and strategy selection
  size_t estimated_memory = estimate_memory_usage(args.algorithm, ctx.input_size);
  size_t free_memory = get_free_memory_mb();

  if (!args.memory_limit_set)
  {
    printf("\nWarning: No memory limit specified (-m option)\n");
    printf("Estimated memory needed: %zu MB\n", estimated_memory);
    printf("Available free memory: %zu MB\n", free_memory);
    ctx.memory_limit_mb = free_memory > 0 ? free_memory : 4096;
  }
  else
  {
    ctx.memory_limit_mb = args.memory_limit_mb;
    printf("Memory limit: %zu MB\n", ctx.memory_limit_mb);
  }

  // Select processing strategy
  if (ctx.memory_limit_mb >= estimated_memory && free_memory >= estimated_memory)
  {
    ctx.use_chunking = 0;
    printf("Strategy: Streaming (sufficient memory)\n");
  }
  else
  {
    ctx.use_chunking = 1;
    printf("Strategy: Tensor-aware chunking (memory constrained)\n");

    // Parse safetensors header for chunking
    if (parse_safetensors_header(ctx.input_path, &ctx.header) != 0)
    {
      fprintf(stderr, "Error: Failed to parse safetensors header\n");
      return 1;
    }
    printf("Found %zu tensors in file\n", ctx.header.num_tensors);
  }

  // Ensure tmp directory exists
  if (ensure_directory(TMP_DIR) != 0)
  {
    fprintf(stderr, "Error: Failed to create tmp directory\n");
    cleanup_context(&ctx);
    return 1;
  }

  // Generate output paths
  char compressed_path[512];
  char decompressed_path[512];
  snprintf(compressed_path, sizeof(compressed_path), "%s%s_compressed.safetensor", TMP_DIR, algorithm_id(args.algorithm));
  snprintf(decompressed_path, sizeof(decompressed_path), "%s%s_decompressed.safetensor", TMP_DIR, algorithm_id(args.algorithm));
  ctx.compressed_path = compressed_path;
  ctx.decompressed_path = decompressed_path;

  printf("\n");

  // Start compression
  double compress_start = get_time_ms();

  switch (args.algorithm)
  {
  case ALG_OPTM:
    ret = optarithm_compress(&ctx);
    break;
  case ALG_ARITHMETIC:
    ret = arithmetic_compress(&ctx);
    break;
  case ALG_HUFFMAN:
    ret = huffman_compress(&ctx);
    break;
  case ALG_LZ77:
    ret = lz77_compress(&ctx);
    break;
  case ALG_GZIP:
    ret = gzip_compress(&ctx);
    break;
  default:
    ret = -1;
  }

  double compress_time = get_time_ms() - compress_start;

  if (ret != 0)
  {
    fprintf(stderr, "\nError: Compression failed\n");
    cleanup_context(&ctx);
    return 1;
  }

  result.compressed_size = get_file_size(ctx.compressed_path);
  result.compression_time_ms = compress_time;
  printf("\n");

  // Start decompression
  double decompress_start = get_time_ms();

  switch (args.algorithm)
  {
  case ALG_OPTM:
    ret = optarithm_decompress(&ctx);
    break;
  case ALG_ARITHMETIC:
    ret = arithmetic_decompress(&ctx);
    break;
  case ALG_HUFFMAN:
    ret = huffman_decompress(&ctx);
    break;
  case ALG_LZ77:
    ret = lz77_decompress(&ctx);
    break;
  case ALG_GZIP:
    ret = gzip_decompress(&ctx);
    break;
  default:
    ret = -1;
  }

  double decompress_time = get_time_ms() - decompress_start;

  if (ret != 0)
  {
    fprintf(stderr, "\nError: Decompression failed\n");
    cleanup_context(&ctx);
    return 1;
  }

  result.decompression_time_ms = decompress_time;
  printf("\n");

  // Verify decompressed file
  printf("Verifying...\n");
  int verified = verify_files(ctx.input_path, ctx.decompressed_path);
  result.verified = verified;

  // Get peak memory usage
  result.peak_memory_mb = get_peak_memory_mb();

  // Print results
  printf("\n");
  printf("Results\n");
  printf("=======\n");
  printf("Algorithm:          %s\n", algorithm_name(args.algorithm));
  printf("Original size:      %zu bytes\n", ctx.input_size);
  printf("Compressed size:    %zu bytes\n", result.compressed_size);
  printf("Compression ratio:  %.2f%%\n", ((double)result.compressed_size / ctx.input_size) * 100);
  printf("Compression time:   %.2f s\n", result.compression_time_ms / 1000.0);
  printf("Decompression time: %.2f s\n", result.decompression_time_ms / 1000.0);
  printf("Peak memory usage:  %zu MB\n", result.peak_memory_mb);
  printf("Verification:       %s\n", result.verified ? "PASSED" : "FAILED");
  printf("\n");

  // Cleanup
  cleanup_context(&ctx);

  // Remove temp files
  remove(compressed_path);
  remove(decompressed_path);

  return result.verified ? 0 : 1;
}
