#include "optarithm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Optimized Arithmetic Coding with Preprocessing Pipeline
//
// Pipeline: Raw -> Byte Transpose -> Delta -> Bit-plane -> Arithmetic
//
// 1. Byte Transposition: Groups bytes by position within multi-byte elements
//    [B0 B1 B2 B3][B0 B1 B2 B3]... -> [B0 B0...][B1 B1...][B2 B2...][B3 B3...]
//    This groups similar bytes together (e.g., all exponent bytes of floats)
//
// 2. Delta Encoding: Stores differences between consecutive bytes
//    [x0, x1, x2, ...] -> [x0, x1-x0, x2-x1, ...]
//    Reduces entropy for smooth data sequences
//
// 3. Bit-plane Separation: Groups bits by significance level
//    Separates MSBs (more predictable) from LSBs (more random)
//
// 4. Arithmetic Coding: Adaptive order-0 entropy coding

//============ Constants ============

#define CODE_BITS 32
#define TOP_VALUE ((uint32_t)1 << (CODE_BITS - 1))
#define FIRST_QTR (TOP_VALUE / 2)
#define HALF (2 * FIRST_QTR)
#define THIRD_QTR (3 * FIRST_QTR)

#define NUM_SYMBOLS 257
#define EOF_SYMBOL 256
#define MAX_FREQUENCY 16383
#define IO_BUFFER_SIZE 65536

// Preprocessing header stored in compressed file
typedef struct
{
  uint32_t element_size;    // bytes per element (2 for float16, 4 for float32)
  uint32_t num_elements;    // number of elements
  uint32_t remainder_bytes; // leftover bytes not fitting element_size
  uint8_t flags;            // bit 0: transpose, bit 1: delta, bit 2: bitplane
} PreprocessHeader;

#define FLAG_TRANSPOSE 0x01
#define FLAG_DELTA 0x02
#define FLAG_BITPLANE 0x04

//============ Preprocessing: Byte Transposition ============

// Transpose bytes: group by position within element
// Input:  [A0 A1 A2 A3][B0 B1 B2 B3][C0 C1 C2 C3]...
// Output: [A0 B0 C0...][A1 B1 C1...][A2 B2 C2...][A3 B3 C3...]
static void byte_transpose(const uint8_t *in, uint8_t *out, size_t num_elements, int elem_size)
{
  for (size_t i = 0; i < num_elements; i++)
  {
    for (int b = 0; b < elem_size; b++) { out[b * num_elements + i] = in[i * elem_size + b]; }
  }
}

// Inverse transpose
static void byte_untranspose(const uint8_t *in, uint8_t *out, size_t num_elements, int elem_size)
{
  for (size_t i = 0; i < num_elements; i++)
  {
    for (int b = 0; b < elem_size; b++) { out[i * elem_size + b] = in[b * num_elements + i]; }
  }
}

//============ Preprocessing: Delta Encoding ============

// Delta encode: store differences
// Input:  [x0, x1, x2, x3, ...]
// Output: [x0, x1-x0, x2-x1, x3-x2, ...]
static void delta_encode(uint8_t *data, size_t size)
{
  if (size < 2) return;

  uint8_t prev = data[0];
  for (size_t i = 1; i < size; i++)
  {
    uint8_t curr = data[i];
    data[i] = curr - prev; // wraps naturally for uint8_t
    prev = curr;
  }
}

// Delta decode: reconstruct from differences
static void delta_decode(uint8_t *data, size_t size)
{
  if (size < 2) return;

  for (size_t i = 1; i < size; i++)
  {
    data[i] = data[i] + data[i - 1]; // wraps naturally for uint8_t
  }
}

//============ Preprocessing: Bit-plane Separation ============

// Separate bits by plane (MSB to LSB)
// Groups all bit-7s together, then all bit-6s, etc.
// This separates high-entropy LSBs from low-entropy MSBs
static void bitplane_separate(const uint8_t *in, uint8_t *out, size_t size)
{
  // Output size is same as input
  // We pack 8 input bytes into 8 output bytes, but reorganized by bit plane

  size_t full_groups = size / 8;
  size_t remainder = size % 8;

  // Process full groups of 8 bytes
  for (size_t g = 0; g < full_groups; g++)
  {
    const uint8_t *src = in + g * 8;
    uint8_t *dst = out + g * 8;

    // Each output byte contains one bit from each of 8 input bytes
    for (int plane = 0; plane < 8; plane++)
    {
      uint8_t packed = 0;
      for (int i = 0; i < 8; i++) { packed |= ((src[i] >> (7 - plane)) & 1) << (7 - i); }
      dst[plane] = packed;
    }
  }

  // Handle remainder bytes (copy as-is, they'll still compress)
  if (remainder > 0) { memcpy(out + full_groups * 8, in + full_groups * 8, remainder); }
}

// Inverse bit-plane separation
static void bitplane_combine(const uint8_t *in, uint8_t *out, size_t size)
{
  size_t full_groups = size / 8;
  size_t remainder = size % 8;

  for (size_t g = 0; g < full_groups; g++)
  {
    const uint8_t *src = in + g * 8;
    uint8_t *dst = out + g * 8;

    // Initialize output bytes
    memset(dst, 0, 8);

    // Each input byte (plane) contributes one bit to each output byte
    for (int plane = 0; plane < 8; plane++)
    {
      uint8_t packed = src[plane];
      for (int i = 0; i < 8; i++) { dst[i] |= ((packed >> (7 - i)) & 1) << (7 - plane); }
    }
  }

  // Copy remainder
  if (remainder > 0) { memcpy(out + full_groups * 8, in + full_groups * 8, remainder); }
}

//============ Arithmetic Coding Core ============

// Order-0 model (single context)
typedef struct
{
  uint32_t freq[NUM_SYMBOLS + 1];
  uint32_t total;
} FrequencyModel;

// Order-1 model (256 contexts, one per previous byte)
typedef struct
{
  FrequencyModel contexts[256]; // one model per previous byte value
} Order1Model;

typedef struct
{
  uint32_t low;
  uint32_t high;
  uint32_t bits_to_follow;
  FILE *output;
  BitWriter bw;
} RangeEncoder;

typedef struct
{
  uint32_t low;
  uint32_t high;
  uint32_t value;
  FILE *input;
  BitReader br;
} RangeDecoder;

static void model_init(FrequencyModel *model)
{
  for (int i = 0; i <= NUM_SYMBOLS; i++) { model->freq[i] = i; }
  model->total = NUM_SYMBOLS;
}

static void model_update(FrequencyModel *model, int symbol)
{
  if (model->total >= MAX_FREQUENCY)
  {
    uint32_t cum = 0;
    for (int i = 0; i < NUM_SYMBOLS; i++)
    {
      uint32_t freq = model->freq[i + 1] - model->freq[i];
      freq = (freq + 1) / 2;
      if (freq < 1) freq = 1;
      model->freq[i] = cum;
      cum += freq;
    }
    model->freq[NUM_SYMBOLS] = cum;
    model->total = cum;
  }

  for (int i = symbol + 1; i <= NUM_SYMBOLS; i++) { model->freq[i]++; }
  model->total++;
}

static int model_find_symbol(FrequencyModel *model, uint32_t cum)
{
  int lo = 0, hi = NUM_SYMBOLS - 1;
  while (lo < hi)
  {
    int mid = (lo + hi + 1) / 2;
    if (model->freq[mid] <= cum) lo = mid;
    else hi = mid - 1;
  }
  return lo;
}

//============ Order-1 Context Model ============

static void order1_init(Order1Model *model)
{
  // Initialize all 256 context models
  for (int ctx = 0; ctx < 256; ctx++) { model_init(&model->contexts[ctx]); }
}

static inline FrequencyModel *order1_get_context(Order1Model *model, int prev_byte)
{
  return &model->contexts[prev_byte & 0xFF];
}

static void encoder_init(RangeEncoder *enc, FILE *output)
{
  enc->low = 0;
  enc->high = TOP_VALUE - 1;
  enc->bits_to_follow = 0;
  enc->output = output;
  bitwriter_init(&enc->bw, output);
}

static void encoder_output_bit(RangeEncoder *enc, int bit)
{
  bitwriter_write_bit(&enc->bw, bit);
  while (enc->bits_to_follow > 0)
  {
    bitwriter_write_bit(&enc->bw, !bit);
    enc->bits_to_follow--;
  }
}

static void encoder_encode(RangeEncoder *enc, FrequencyModel *model, int symbol)
{
  uint64_t range = (uint64_t)(enc->high - enc->low) + 1;

  enc->high = enc->low + (uint32_t)((range * model->freq[symbol + 1]) / model->total) - 1;
  enc->low = enc->low + (uint32_t)((range * model->freq[symbol]) / model->total);

  for (;;)
  {
    if (enc->high < HALF) { encoder_output_bit(enc, 0); }
    else if (enc->low >= HALF)
    {
      encoder_output_bit(enc, 1);
      enc->low -= HALF;
      enc->high -= HALF;
    }
    else if (enc->low >= FIRST_QTR && enc->high < THIRD_QTR)
    {
      enc->bits_to_follow++;
      enc->low -= FIRST_QTR;
      enc->high -= FIRST_QTR;
    }
    else
    {
      break;
    }
    enc->low = 2 * enc->low;
    enc->high = 2 * enc->high + 1;
  }
}

static void encoder_finish(RangeEncoder *enc)
{
  enc->bits_to_follow++;
  if (enc->low < FIRST_QTR) encoder_output_bit(enc, 0);
  else encoder_output_bit(enc, 1);
  bitwriter_flush(&enc->bw);
}

static void decoder_init(RangeDecoder *dec, FILE *input)
{
  dec->low = 0;
  dec->high = TOP_VALUE - 1;
  dec->value = 0;
  dec->input = input;
  bitreader_init(&dec->br, input);

  for (int i = 0; i < CODE_BITS; i++) { dec->value = (dec->value << 1) | bitreader_read_bit(&dec->br); }
}

static int decoder_decode(RangeDecoder *dec, FrequencyModel *model)
{
  uint64_t range = (uint64_t)(dec->high - dec->low) + 1;
  uint32_t cum = (uint32_t)(((uint64_t)(dec->value - dec->low + 1) * model->total - 1) / range);

  int symbol = model_find_symbol(model, cum);

  dec->high = dec->low + (uint32_t)((range * model->freq[symbol + 1]) / model->total) - 1;
  dec->low = dec->low + (uint32_t)((range * model->freq[symbol]) / model->total);

  for (;;)
  {
    if (dec->high < HALF)
    {
      // nothing
    }
    else if (dec->low >= HALF)
    {
      dec->value -= HALF;
      dec->low -= HALF;
      dec->high -= HALF;
    }
    else if (dec->low >= FIRST_QTR && dec->high < THIRD_QTR)
    {
      dec->value -= FIRST_QTR;
      dec->low -= FIRST_QTR;
      dec->high -= FIRST_QTR;
    }
    else
    {
      break;
    }
    dec->low = 2 * dec->low;
    dec->high = 2 * dec->high + 1;
    dec->value = (dec->value << 1) | bitreader_read_bit(&dec->br);
  }

  return symbol;
}

//============ Dtype Detection ============

// Detect element size from SafeTensors dtype string
static int get_element_size(const char *dtype)
{
  if (!dtype) return 1;

  if (strcmp(dtype, "F16") == 0 || strcmp(dtype, "BF16") == 0) return 2;
  if (strcmp(dtype, "F32") == 0) return 4;
  if (strcmp(dtype, "F64") == 0) return 8;
  if (strcmp(dtype, "I8") == 0 || strcmp(dtype, "U8") == 0) return 1;
  if (strcmp(dtype, "I16") == 0 || strcmp(dtype, "U16") == 0) return 2;
  if (strcmp(dtype, "I32") == 0 || strcmp(dtype, "U32") == 0) return 4;
  if (strcmp(dtype, "I64") == 0 || strcmp(dtype, "U64") == 0) return 8;

  return 1; // default to byte-level
}

// Try to detect dominant dtype from safetensors header
static int detect_element_size(const char *header_json, size_t header_len)
{
  // Count occurrences of common dtypes
  int count_f16 = 0, count_f32 = 0, count_bf16 = 0;

  const char *p = header_json;
  const char *end = header_json + header_len;

  while (p < end)
  {
    const char *found = strstr(p, "\"dtype\"");
    if (!found || found >= end) break;

    // Look for the dtype value
    const char *colon = strchr(found, ':');
    if (!colon || colon >= end) break;

    const char *quote1 = strchr(colon, '"');
    if (!quote1 || quote1 >= end) break;
    quote1++;

    if (strncmp(quote1, "F16", 3) == 0) count_f16++;
    else if (strncmp(quote1, "F32", 3) == 0) count_f32++;
    else if (strncmp(quote1, "BF16", 4) == 0) count_bf16++;

    p = quote1;
  }

  // Return most common element size
  if (count_f32 >= count_f16 && count_f32 >= count_bf16) return 4;
  if (count_f16 >= count_bf16) return 2;
  if (count_bf16 > 0) return 2;

  return 4; // default to float32
}

//============ Public API ============

int optarithm_compress(CompressionContext *ctx)
{
  FILE *input = fopen(ctx->input_path, "rb");
  FILE *output = fopen(ctx->compressed_path, "wb");

  if (!input || !output)
  {
    if (input) fclose(input);
    if (output) fclose(output);
    return -1;
  }

  // Read safetensors header
  uint64_t st_hdr_size;
  char *st_hdr_json = NULL;
  int allocated_hdr = 0;

  if (ctx->header.header_size > 0 && ctx->header.header_json)
  {
    st_hdr_size = ctx->header.header_size;
    st_hdr_json = ctx->header.header_json;
  }
  else
  {
    if (fread(&st_hdr_size, sizeof(st_hdr_size), 1, input) != 1)
    {
      fclose(input);
      fclose(output);
      return -1;
    }
    st_hdr_json = malloc(st_hdr_size + 1);
    if (!st_hdr_json)
    {
      fclose(input);
      fclose(output);
      return -1;
    }
    if (fread(st_hdr_json, 1, st_hdr_size, input) != st_hdr_size)
    {
      free(st_hdr_json);
      fclose(input);
      fclose(output);
      return -1;
    }
    st_hdr_json[st_hdr_size] = '\0';
    allocated_hdr = 1;
  }

  // Detect element size from header
  int elem_size = detect_element_size(st_hdr_json, st_hdr_size);

  // Write compressed file header
  CompressedFileHeader file_hdr = {0};
  memcpy(file_hdr.magic, STC_MAGIC, STC_MAGIC_SIZE);
  file_hdr.algorithm = STC_ALG_ARITHMETIC; // reuse ID, different format detected by preprocess header
  file_hdr.flags = ctx->use_chunking ? STC_FLAG_CHUNKED : 0;
  file_hdr.original_size = ctx->input_size;
  file_hdr.header_size = st_hdr_size;
  fwrite(&file_hdr, sizeof(file_hdr), 1, output);

  // Write safetensors header (uncompressed)
  fwrite(&st_hdr_size, sizeof(st_hdr_size), 1, output);
  fwrite(st_hdr_json, 1, st_hdr_size, output);

  if (allocated_hdr) free(st_hdr_json);

  // Read tensor data
  size_t data_start = 8 + st_hdr_size;
  fseek(input, data_start, SEEK_SET);

  size_t data_size = ctx->input_size - data_start;
  uint8_t *data = malloc(data_size);
  if (!data)
  {
    fclose(input);
    fclose(output);
    return -1;
  }

  if (fread(data, 1, data_size, input) != data_size)
  {
    free(data);
    fclose(input);
    fclose(output);
    return -1;
  }
  fclose(input);

  // Calculate preprocessing parameters
  size_t num_elements = data_size / elem_size;
  size_t aligned_size = num_elements * elem_size;
  size_t remainder = data_size - aligned_size;

  // Write preprocessing header
  // Note: Bit-plane disabled - it hurts compression after delta encoding
  PreprocessHeader pp_hdr = {0};
  pp_hdr.element_size = elem_size;
  pp_hdr.num_elements = (uint32_t)num_elements;
  pp_hdr.remainder_bytes = (uint32_t)remainder;
  pp_hdr.flags = FLAG_TRANSPOSE | FLAG_DELTA; // bitplane disabled
  fwrite(&pp_hdr, sizeof(pp_hdr), 1, output);

  // Allocate buffers for preprocessing
  uint8_t *buf1 = malloc(data_size);
  uint8_t *buf2 = malloc(data_size);
  if (!buf1 || !buf2)
  {
    free(data);
    free(buf1);
    free(buf2);
    fclose(output);
    return -1;
  }

  if (ctx->progress_callback) ctx->progress_callback(0, "Preprocessing (transpose)");

  // Step 1: Byte transposition on aligned portion
  if (num_elements > 0 && elem_size > 1)
  {
    byte_transpose(data, buf1, num_elements, elem_size);
    // Copy remainder bytes
    if (remainder > 0) { memcpy(buf1 + aligned_size, data + aligned_size, remainder); }
  }
  else
  {
    memcpy(buf1, data, data_size);
  }

  if (ctx->progress_callback) ctx->progress_callback(15, "Preprocessing (delta)");

  // Step 2: Delta encoding on each transposed stream
  if (num_elements > 1 && elem_size > 1)
  {
    for (int b = 0; b < elem_size; b++) { delta_encode(buf1 + b * num_elements, num_elements); }
  }
  else
  {
    delta_encode(buf1, data_size);
  }

  free(data);

  if (ctx->progress_callback) ctx->progress_callback(30, "Compressing (arithmetic o1)");

  // Step 3: Arithmetic coding with Order-1 context modeling
  // Use 256 frequency tables - one per previous byte value
  // This captures patterns like "0x00 often follows 0x00" after delta encoding
  Order1Model *model = malloc(sizeof(Order1Model));
  if (!model)
  {
    free(buf1);
    free(buf2);
    fclose(output);
    return -1;
  }
  order1_init(model);

  RangeEncoder encoder;
  encoder_init(&encoder, output);

  size_t processed = 0;
  int last_percent = -1;
  int prev_byte = 0; // context starts at 0

  for (size_t i = 0; i < data_size; i++)
  {
    int cur_byte = buf1[i];
    FrequencyModel *ctx_model = order1_get_context(model, prev_byte);

    encoder_encode(&encoder, ctx_model, cur_byte);
    model_update(ctx_model, cur_byte);

    prev_byte = cur_byte;
    processed++;

    if (ctx->progress_callback && (processed & 0xFFFFF) == 0)
    {
      int percent = 30 + (int)((processed * 70) / data_size);
      if (percent != last_percent)
      {
        ctx->progress_callback(percent, "Compressing (arithmetic o1)");
        last_percent = percent;
      }
    }
  }

  // Encode EOF using last context
  FrequencyModel *eof_ctx = order1_get_context(model, prev_byte);
  encoder_encode(&encoder, eof_ctx, EOF_SYMBOL);
  encoder_finish(&encoder);

  free(model);
  free(buf1);
  free(buf2);
  fclose(output);

  if (ctx->progress_callback) ctx->progress_callback(100, "Compression complete");

  return 0;
}

int optarithm_decompress(CompressionContext *ctx)
{
  FILE *input = fopen(ctx->compressed_path, "rb");
  FILE *output = fopen(ctx->decompressed_path, "wb");

  if (!input || !output)
  {
    if (input) fclose(input);
    if (output) fclose(output);
    return -1;
  }

  // Read compressed file header
  CompressedFileHeader file_hdr;
  if (fread(&file_hdr, sizeof(file_hdr), 1, input) != 1)
  {
    fclose(input);
    fclose(output);
    return -1;
  }

  if (memcmp(file_hdr.magic, STC_MAGIC, STC_MAGIC_SIZE) != 0)
  {
    fclose(input);
    fclose(output);
    return -1;
  }

  // Read and write safetensors header
  uint64_t st_hdr_size;
  if (fread(&st_hdr_size, sizeof(st_hdr_size), 1, input) != 1)
  {
    fclose(input);
    fclose(output);
    return -1;
  }
  fwrite(&st_hdr_size, sizeof(st_hdr_size), 1, output);

  char *st_hdr = malloc(st_hdr_size);
  if (!st_hdr)
  {
    fclose(input);
    fclose(output);
    return -1;
  }
  if (fread(st_hdr, 1, st_hdr_size, input) != st_hdr_size)
  {
    free(st_hdr);
    fclose(input);
    fclose(output);
    return -1;
  }
  fwrite(st_hdr, 1, st_hdr_size, output);
  free(st_hdr);

  // Read preprocessing header
  PreprocessHeader pp_hdr;
  if (fread(&pp_hdr, sizeof(pp_hdr), 1, input) != 1)
  {
    fclose(input);
    fclose(output);
    return -1;
  }

  size_t data_size = file_hdr.original_size - 8 - st_hdr_size;
  size_t num_elements = pp_hdr.num_elements;
  size_t elem_size = pp_hdr.element_size;
  size_t aligned_size = num_elements * elem_size;

  // Allocate buffers
  uint8_t *buf1 = malloc(data_size);
  uint8_t *buf2 = malloc(data_size);
  if (!buf1 || !buf2)
  {
    free(buf1);
    free(buf2);
    fclose(input);
    fclose(output);
    return -1;
  }

  if (ctx->progress_callback) ctx->progress_callback(0, "Decompressing (arithmetic o1)");

  // Step 1: Arithmetic decoding with Order-1 context modeling
  Order1Model *model = malloc(sizeof(Order1Model));
  if (!model)
  {
    free(buf1);
    free(buf2);
    fclose(input);
    fclose(output);
    return -1;
  }
  order1_init(model);

  RangeDecoder decoder;
  decoder_init(&decoder, input);

  size_t processed = 0;
  int last_percent = -1;
  int prev_byte = 0; // context starts at 0
  int symbol;

  while (processed < data_size)
  {
    FrequencyModel *ctx_model = order1_get_context(model, prev_byte);
    symbol = decoder_decode(&decoder, ctx_model);

    if (symbol == EOF_SYMBOL) break;

    buf1[processed++] = (uint8_t)symbol;
    model_update(ctx_model, symbol);
    prev_byte = symbol;

    if (ctx->progress_callback && (processed & 0xFFFFF) == 0)
    {
      int percent = (int)((processed * 40) / data_size);
      if (percent != last_percent)
      {
        ctx->progress_callback(percent, "Decompressing (arithmetic o1)");
        last_percent = percent;
      }
    }
  }

  free(model);
  fclose(input);

  if (ctx->progress_callback) ctx->progress_callback(50, "Postprocessing (delta)");

  // Step 3: Inverse delta encoding
  if (pp_hdr.flags & FLAG_DELTA)
  {
    if (num_elements > 1 && elem_size > 1)
    {
      for (size_t b = 0; b < elem_size; b++) { delta_decode(buf1 + b * num_elements, num_elements); }
    }
    else
    {
      delta_decode(buf1, data_size);
    }
  }

  if (ctx->progress_callback) ctx->progress_callback(70, "Postprocessing (transpose)");

  // Step 4: Inverse byte transposition
  if ((pp_hdr.flags & FLAG_TRANSPOSE) && num_elements > 0 && elem_size > 1)
  {
    byte_untranspose(buf1, buf2, num_elements, elem_size);
    // Copy remainder
    if (pp_hdr.remainder_bytes > 0) { memcpy(buf2 + aligned_size, buf1 + aligned_size, pp_hdr.remainder_bytes); }
    fwrite(buf2, 1, data_size, output);
  }
  else
  {
    fwrite(buf1, 1, data_size, output);
  }

  free(buf1);
  free(buf2);
  fclose(output);

  if (ctx->progress_callback) ctx->progress_callback(100, "Decompression complete");

  return 0;
}
