#include "arithmetic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Arithmetic Coding Implementation
//
// Uses a 32-bit range coder with adaptive probability model.
// The model tracks symbol frequencies and adjusts probabilities as it encodes.
//
// Optimizations:
// - 64-bit arithmetic for range calculations (prevents overflow)
// - Binary search for symbol lookup in decoder
// - Buffered I/O for speed

// Range coder constants - classic WMW style
#define CODE_BITS 32
#define TOP_VALUE ((uint32_t)1 << (CODE_BITS - 1))
#define FIRST_QTR (TOP_VALUE / 2)
#define HALF (2 * FIRST_QTR)
#define THIRD_QTR (3 * FIRST_QTR)

// Frequency table size (256 byte symbols + 1 EOF)
#define NUM_SYMBOLS 257
#define EOF_SYMBOL 256

// Maximum frequency before rescaling
#define MAX_FREQUENCY 16383

// I/O buffer size
#define IO_BUFFER_SIZE 65536

// Adaptive frequency model
typedef struct
{
  uint32_t freq[NUM_SYMBOLS + 1]; // Cumulative frequencies
  uint32_t total;
} FrequencyModel;

// Range encoder state
typedef struct
{
  uint32_t low;
  uint32_t high;
  uint32_t bits_to_follow;
  FILE *output;
  BitWriter bw;
} RangeEncoder;

// Range decoder state
typedef struct
{
  uint32_t low;
  uint32_t high;
  uint32_t value;
  FILE *input;
  BitReader br;
} RangeDecoder;

//============ Frequency Model ============

static void model_init(FrequencyModel *model)
{
  // Initialize with uniform distribution
  for (int i = 0; i <= NUM_SYMBOLS; i++) { model->freq[i] = i; }
  model->total = NUM_SYMBOLS;
}

static void model_update(FrequencyModel *model, int symbol)
{
  // Rescale if total gets too high
  if (model->total >= MAX_FREQUENCY)
  {
    uint32_t cum = 0;
    for (int i = 0; i < NUM_SYMBOLS; i++)
    {
      uint32_t freq = model->freq[i + 1] - model->freq[i];
      freq = (freq + 1) / 2; // Halve but keep at least 1
      if (freq < 1) freq = 1;
      model->freq[i] = cum;
      cum += freq;
    }
    model->freq[NUM_SYMBOLS] = cum;
    model->total = cum;
  }

  // Increment frequencies for symbols >= symbol
  for (int i = symbol + 1; i <= NUM_SYMBOLS; i++) { model->freq[i]++; }
  model->total++;
}

// Binary search to find symbol from cumulative frequency
static int model_find_symbol(FrequencyModel *model, uint32_t cum)
{
  int lo = 0, hi = NUM_SYMBOLS - 1;

  while (lo < hi)
  {
    int mid = (lo + hi + 1) / 2;
    if (model->freq[mid] <= cum) { lo = mid; }
    else
    {
      hi = mid - 1;
    }
  }

  return lo;
}

//============ Range Encoder ============
static void encoder_init(RangeEncoder *enc, FILE *output)
{
  enc->low = 0;
  enc->high = TOP_VALUE - 1;  // fix range initialization (issue #5)
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

  // Use 64-bit arithmetic to prevent overflow
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
  if (enc->low < FIRST_QTR) { encoder_output_bit(enc, 0); }
  else
  {
    encoder_output_bit(enc, 1);
  }
  bitwriter_flush(&enc->bw);
}

//============ Range Decoder ============
static void decoder_init(RangeDecoder *dec, FILE *input)
{
  dec->low = 0;
  dec->high = TOP_VALUE - 1;  // fix range initialization (issue #5)
  dec->value = 0;
  dec->input = input;
  bitreader_init(&dec->br, input);

  // Read initial bits
  for (int i = 0; i < CODE_BITS; i++) { dec->value = (dec->value << 1) | bitreader_read_bit(&dec->br); }
}

static int decoder_decode(RangeDecoder *dec, FrequencyModel *model)
{
  uint64_t range = (uint64_t)(dec->high - dec->low) + 1;

  // Calculate cumulative frequency for value
  uint32_t cum = (uint32_t)(((uint64_t)(dec->value - dec->low + 1) * model->total - 1) / range);

  // Binary search for symbol
  int symbol = model_find_symbol(model, cum);

  // Update range using 64-bit arithmetic
  dec->high = dec->low + (uint32_t)((range * model->freq[symbol + 1]) / model->total) - 1;
  dec->low = dec->low + (uint32_t)((range * model->freq[symbol]) / model->total);

  // Normalize
  for (;;)
  {
    if (dec->high < HALF)
    {
      // Nothing
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

//============ Public API ============

int arithmetic_compress(CompressionContext *ctx)
{
  FILE *input = fopen(ctx->input_path, "rb");
  FILE *output = fopen(ctx->compressed_path, "wb");

  if (!input || !output)
  {
    if (input) fclose(input);
    if (output) fclose(output);
    return -1;
  }

  // Read safetensors header size first if not already parsed
  uint64_t st_hdr_size;
  char *st_hdr_json = NULL;
  int allocated_hdr = 0;  // track if we allocated st_hdr_json (issue #7)

  if (ctx->header.header_size > 0 && ctx->header.header_json)
  {
    st_hdr_size = ctx->header.header_size;
    st_hdr_json = ctx->header.header_json;
  }
  else
  {
    // Read header from input
    if (fread(&st_hdr_size, sizeof(st_hdr_size), 1, input) != 1)  // check fread
    {
      fclose(input);
      fclose(output);
      return -1;
    }
    st_hdr_json = malloc(st_hdr_size);
    if (!st_hdr_json)  // check malloc
    {
      fclose(input);
      fclose(output);
      return -1;
    }
    if (fread(st_hdr_json, 1, st_hdr_size, input) != st_hdr_size)  // check fread
    {
      free(st_hdr_json);
      fclose(input);
      fclose(output);
      return -1;
    }
    allocated_hdr = 1;  // mark as allocated
  }

  // Write compressed file header
  CompressedFileHeader hdr = {0};
  memcpy(hdr.magic, STC_MAGIC, STC_MAGIC_SIZE);
  hdr.algorithm = STC_ALG_ARITHMETIC;
  hdr.flags = ctx->use_chunking ? STC_FLAG_CHUNKED : 0;
  hdr.original_size = ctx->input_size;
  hdr.header_size = st_hdr_size;
  fwrite(&hdr, sizeof(hdr), 1, output);

  // Write safetensors header uncompressed (flat)
  fwrite(&st_hdr_size, sizeof(st_hdr_size), 1, output);
  fwrite(st_hdr_json, 1, st_hdr_size, output);

  // Free if we allocated it
  if (allocated_hdr) { free(st_hdr_json); }  // use flag instead of pointer comparison

  // Initialize model and encoder
  FrequencyModel model;
  RangeEncoder encoder;
  model_init(&model);
  encoder_init(&encoder, output);

  // Compress tensor data with buffered I/O
  size_t data_start = 8 + st_hdr_size;
  fseek(input, data_start, SEEK_SET);

  size_t data_size = ctx->input_size - data_start;
  size_t processed = 0;
  int last_percent = -1;

  uint8_t *read_buf = malloc(IO_BUFFER_SIZE);
  if (!read_buf)  // check malloc
  {
    fclose(input);
    fclose(output);
    return -1;
  }
  size_t buf_pos = 0, buf_len = 0;

  while (processed < data_size)
  {
    // Refill buffer if needed
    if (buf_pos >= buf_len)
    {
      buf_len = fread(read_buf, 1, IO_BUFFER_SIZE, input);
      buf_pos = 0;
      if (buf_len == 0) break;
    }

    encoder_encode(&encoder, &model, read_buf[buf_pos++]);
    model_update(&model, read_buf[buf_pos - 1]);
    processed++;

    // Progress callback (check less frequently)
    if (ctx->progress_callback && (processed & 0xFFFFF) == 0)
    {
      int percent = (int)((processed * 100) / data_size);
      if (percent != last_percent)
      {
        ctx->progress_callback(percent, "Compressing (arithmetic)");
        last_percent = percent;
      }
    }
  }

  // Encode EOF symbol
  encoder_encode(&encoder, &model, EOF_SYMBOL);
  encoder_finish(&encoder);

  free(read_buf);
  fclose(input);
  fclose(output);

  if (ctx->progress_callback) { ctx->progress_callback(100, "Compression complete"); }

  return 0;
}

int arithmetic_decompress(CompressionContext *ctx)
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
  CompressedFileHeader hdr;
  if (fread(&hdr, sizeof(hdr), 1, input) != 1)  // check fread
  {
    fclose(input);
    fclose(output);
    return -1;
  }

  // Validate magic header (issue #6)
  if (memcmp(hdr.magic, STC_MAGIC, STC_MAGIC_SIZE) != 0)
  {
    fclose(input);
    fclose(output);
    return -1;
  }

  // Read and write safetensors header (uncompressed)
  uint64_t st_hdr_size;
  if (fread(&st_hdr_size, sizeof(st_hdr_size), 1, input) != 1)  // check fread
  {
    fclose(input);
    fclose(output);
    return -1;
  }
  fwrite(&st_hdr_size, sizeof(st_hdr_size), 1, output);

  char *st_hdr = malloc(st_hdr_size);
  if (!st_hdr)  // check malloc
  {
    fclose(input);
    fclose(output);
    return -1;
  }
  if (fread(st_hdr, 1, st_hdr_size, input) != st_hdr_size)  // check fread
  {
    free(st_hdr);
    fclose(input);
    fclose(output);
    return -1;
  }
  fwrite(st_hdr, 1, st_hdr_size, output);
  free(st_hdr);

  // Initialize model and decoder
  FrequencyModel model;
  RangeDecoder decoder;
  model_init(&model);
  decoder_init(&decoder, input);

  // Decompress with buffered output
  size_t data_size = hdr.original_size - 8 - st_hdr_size;
  size_t processed = 0;
  int last_percent = -1;

  uint8_t *write_buf = malloc(IO_BUFFER_SIZE);
  if (!write_buf)  // check malloc
  {
    fclose(input);
    fclose(output);
    return -1;
  }
  size_t buf_pos = 0;

  int symbol;
  while ((symbol = decoder_decode(&decoder, &model)) != EOF_SYMBOL)
  {
    write_buf[buf_pos++] = (uint8_t)symbol;
    model_update(&model, symbol);
    processed++;

    // Flush buffer if full
    if (buf_pos >= IO_BUFFER_SIZE)
    {
      fwrite(write_buf, 1, buf_pos, output);
      buf_pos = 0;
    }

    // Progress callback (check less frequently)
    if (ctx->progress_callback && (processed & 0xFFFFF) == 0)
    {
      int percent = (int)((processed * 100) / data_size);
      if (percent != last_percent)
      {
        ctx->progress_callback(percent, "Decompressing (arithmetic)");
        last_percent = percent;
      }
    }
  }

  // Flush remaining buffer
  if (buf_pos > 0) { fwrite(write_buf, 1, buf_pos, output); }

  free(write_buf);
  fclose(input);
  fclose(output);

  if (ctx->progress_callback) { ctx->progress_callback(100, "Decompression complete"); }

  return 0;
}
