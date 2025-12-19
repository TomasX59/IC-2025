#include "lz77.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple LZ77 Implementation
//
// Format: [flag byte] [8 items]
// - Flag bit 0 = literal (1 byte follows)
// - Flag bit 1 = match (2 bytes: dist_lo, (len-3)<<4 | dist_hi)
// Will produce negative compression on float16 data (expected).

#define WINDOW_BITS 12
#define WINDOW_SIZE (1 << WINDOW_BITS) // 4KB window for speed
#define WINDOW_MASK (WINDOW_SIZE - 1)
#define MIN_MATCH 3
#define MAX_MATCH 18
#define HASH_BITS 14
#define HASH_SIZE (1 << HASH_BITS)
#define HASH_MASK (HASH_SIZE - 1)

static inline uint32_t hash3(const uint8_t *p) { return ((p[0] << 8) ^ (p[1] << 4) ^ p[2]) & HASH_MASK; }

int lz77_compress(CompressionContext *ctx)
{
  FILE *in = fopen(ctx->input_path, "rb");
  FILE *out = fopen(ctx->compressed_path, "wb");
  if (!in || !out)
  {
    if (in) fclose(in);
    if (out) fclose(out);
    return -1;
  }

  // Read safetensors header size
  uint64_t st_hdr_size;
  if (fread(&st_hdr_size, sizeof(st_hdr_size), 1, in) != 1)  // check fread
  {
    fclose(in);
    fclose(out);
    return -1;
  }

  // Write our header
  CompressedFileHeader hdr = {0};
  memcpy(hdr.magic, STC_MAGIC, STC_MAGIC_SIZE);
  hdr.algorithm = STC_ALG_LZ77;
  hdr.flags = 0;
  hdr.original_size = ctx->input_size;
  hdr.header_size = st_hdr_size;
  fwrite(&hdr, sizeof(hdr), 1, out);

  // Copy safetensors header
  fwrite(&st_hdr_size, sizeof(st_hdr_size), 1, out);
  uint8_t *st_hdr = malloc(st_hdr_size);
  if (!st_hdr)  // check malloc
  {
    fclose(in);
    fclose(out);
    return -1;
  }
  if (fread(st_hdr, 1, st_hdr_size, in) != st_hdr_size)  // check fread
  {
    free(st_hdr);
    fclose(in);
    fclose(out);
    return -1;
  }
  fwrite(st_hdr, 1, st_hdr_size, out);
  free(st_hdr);

  // Data size
  size_t data_size = ctx->input_size - 8 - st_hdr_size;
  fwrite(&data_size, sizeof(data_size), 1, out);

  // Read all data into memory for simplicity
  uint8_t *data = malloc(data_size);
  if (!data)  // check malloc
  {
    fclose(in);
    fclose(out);
    return -1;
  }
  if (fread(data, 1, data_size, in) != data_size)  // check fread
  {
    free(data);
    fclose(in);
    fclose(out);
    return -1;
  }
  fclose(in);

  // Hash table: stores position of last occurrence
  uint32_t *hash_table = malloc(HASH_SIZE * sizeof(uint32_t));
  if (!hash_table)  // check malloc
  {
    free(data);
    fclose(out);
    return -1;
  }
  memset(hash_table, 0xFF, HASH_SIZE * sizeof(uint32_t)); // 0xFFFFFFFF = no entry

  // Compress
  size_t pos = 0;
  uint8_t flags;
  uint8_t pending[24]; // Max 8 items * 3 bytes
  int pending_len;
  int flag_bit;
  int last_percent = -1;

  while (pos < data_size)
  {
    flags = 0;
    pending_len = 0;
    flag_bit = 0;

    // Process up to 8 items
    while (flag_bit < 8 && pos < data_size)
    {
      int best_len = 0;
      int best_dist = 0;

      // Hash lookup for match
      if (pos + MIN_MATCH <= data_size)
      {
        uint32_t h = hash3(data + pos);
        uint32_t match_pos = hash_table[h];

        if (match_pos != 0xFFFFFFFF && pos > match_pos)
        {
          uint32_t dist = pos - match_pos;
          if (dist < WINDOW_SIZE)
          {
            // Found potential match, extend it
            int len = 0;
            while (len < MAX_MATCH && pos + len < data_size && data[match_pos + len] == data[pos + len]) { len++; }
            if (len >= MIN_MATCH)
            {
              best_len = len;
              best_dist = dist;
            }
          }
        }

        // Update hash table
        hash_table[h] = pos;
      }

      if (best_len >= MIN_MATCH)
      {
        // Output match
        flags |= (1 << flag_bit);
        pending[pending_len++] = best_dist & 0xFF;
        pending[pending_len++] = ((best_len - 3) << 4) | ((best_dist >> 8) & 0x0F);
        pos += best_len;
      }
      else
      {
        // Output literal
        pending[pending_len++] = data[pos++];
      }
      flag_bit++;
    }

    // Write flag byte and pending data
    fputc(flags, out);
    fwrite(pending, 1, pending_len, out);

    // Progress
    if (ctx->progress_callback)
    {
      int percent = (int)((pos * 100) / data_size);
      if (percent != last_percent)
      {
        ctx->progress_callback(percent, "Compressing (lz77)");
        last_percent = percent;
      }
    }
  }

  free(hash_table);

  free(data);
  fclose(out);

  if (ctx->progress_callback) { ctx->progress_callback(100, "Compression complete"); }
  return 0;
}

int lz77_decompress(CompressionContext *ctx)
{
  FILE *in = fopen(ctx->compressed_path, "rb");
  FILE *out = fopen(ctx->decompressed_path, "wb");
  if (!in || !out)
  {
    if (in) fclose(in);
    if (out) fclose(out);
    return -1;
  }

  // Read header
  CompressedFileHeader hdr;
  if (fread(&hdr, sizeof(hdr), 1, in) != 1)  // check fread
  {
    fclose(in);
    fclose(out);
    return -1;
  }

  // Validate magic header (issue #6)
  if (memcmp(hdr.magic, STC_MAGIC, STC_MAGIC_SIZE) != 0)
  {
    fclose(in);
    fclose(out);
    return -1;
  }

  // Read and write safetensors header
  uint64_t st_hdr_size;
  if (fread(&st_hdr_size, sizeof(st_hdr_size), 1, in) != 1)  // check fread
  {
    fclose(in);
    fclose(out);
    return -1;
  }
  fwrite(&st_hdr_size, sizeof(st_hdr_size), 1, out);

  uint8_t *st_hdr = malloc(st_hdr_size);
  if (!st_hdr)  // check malloc
  {
    fclose(in);
    fclose(out);
    return -1;
  }
  if (fread(st_hdr, 1, st_hdr_size, in) != st_hdr_size)  // check fread
  {
    free(st_hdr);
    fclose(in);
    fclose(out);
    return -1;
  }
  fwrite(st_hdr, 1, st_hdr_size, out);
  free(st_hdr);

  size_t data_size;
  if (fread(&data_size, sizeof(data_size), 1, in) != 1)  // check fread
  {
    fclose(in);
    fclose(out);
    return -1;
  }

  // Allocate output buffer
  uint8_t *data = malloc(data_size);
  if (!data)  // check malloc
  {
    fclose(in);
    fclose(out);
    return -1;
  }
  size_t pos = 0;
  int last_percent = -1;

  while (pos < data_size)
  {
    int flags = fgetc(in);
    if (flags == EOF) break;

    for (int bit = 0; bit < 8 && pos < data_size; bit++)
    {
      if ((flags >> bit) & 1)
      {
        // Match
        int b1 = fgetc(in);
        int b2 = fgetc(in);
        if (b2 == EOF) goto done;

        int dist = b1 | ((b2 & 0x0F) << 8);
        int len = (b2 >> 4) + 3;

        // Validate dist to prevent buffer underflow (issue #3)
        if (dist == 0 || (size_t)dist > pos)
        {
          // Invalid distance - corrupted data
          free(data);
          fclose(in);
          fclose(out);
          return -1;
        }

        for (int i = 0; i < len && pos < data_size; i++)
        {
          data[pos] = data[pos - dist];
          pos++;
        }
      }
      else
      {
        // Literal
        int b = fgetc(in);
        if (b == EOF) goto done;
        data[pos++] = b;
      }
    }

    if (ctx->progress_callback)
    {
      int percent = (int)((pos * 100) / data_size);
      if (percent != last_percent)
      {
        ctx->progress_callback(percent, "Decompressing (lz77)");
        last_percent = percent;
      }
    }
  }

done:
  fwrite(data, 1, pos, out);
  free(data);
  fclose(in);
  fclose(out);

  if (ctx->progress_callback) { ctx->progress_callback(100, "Decompression complete"); }
  return 0;
}
