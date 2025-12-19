#include "huffman.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Huffman Coding Implementation
//
// Uses canonical Huffman codes for compact code storage.
// Two-pass algorithm: first count frequencies, then encode.
// Fast table-based decoding for codes up to 15 bits.

#define NUM_SYMBOLS 256
#define MAX_CODE_LENGTH 15
#define DECODE_TABLE_BITS 15 // Must be >= MAX_CODE_LENGTH to avoid prefix bugs

// Huffman tree node
typedef struct HuffmanNode
{
  uint64_t freq;
  int symbol;
  struct HuffmanNode *left;
  struct HuffmanNode *right;
} HuffmanNode;

// Code table entry
typedef struct
{
  uint32_t code;
  uint8_t length;
} HuffmanCode;

// Priority queue for building tree
typedef struct
{
  HuffmanNode **nodes;
  size_t size;
  size_t capacity;
} PriorityQueue;

//============ Priority Queue ============

static PriorityQueue *pq_create(size_t capacity)
{
  PriorityQueue *pq = malloc(sizeof(PriorityQueue));
  if (!pq) return NULL;  // check malloc
  pq->nodes = malloc(capacity * sizeof(HuffmanNode *));
  if (!pq->nodes)  // check malloc
  {
    free(pq);
    return NULL;
  }
  pq->size = 0;
  pq->capacity = capacity;
  return pq;
}

static void pq_destroy(PriorityQueue *pq)
{
  if (!pq) return;  // null check
  free(pq->nodes);
  free(pq);
}

static int pq_push(PriorityQueue *pq, HuffmanNode *node)  // return int for error
{
  if (pq->size >= pq->capacity) return -1;  // capacity check (issue #4)

  size_t idx = pq->size++;
  pq->nodes[idx] = node;

  while (idx > 0)
  {
    size_t parent = (idx - 1) / 2;
    if (pq->nodes[parent]->freq <= pq->nodes[idx]->freq) break;
    HuffmanNode *tmp = pq->nodes[parent];
    pq->nodes[parent] = pq->nodes[idx];
    pq->nodes[idx] = tmp;
    idx = parent;
  }
  return 0;  // return success
}

static HuffmanNode *pq_pop(PriorityQueue *pq)
{
  if (pq->size == 0) return NULL;  // empty check

  HuffmanNode *result = pq->nodes[0];
  pq->size--;

  if (pq->size > 0)
  {
    pq->nodes[0] = pq->nodes[pq->size];
    size_t idx = 0;

    while (1)
    {
      size_t smallest = idx;
      size_t left = 2 * idx + 1;
      size_t right = 2 * idx + 2;

      if (left < pq->size && pq->nodes[left]->freq < pq->nodes[smallest]->freq) smallest = left;
      if (right < pq->size && pq->nodes[right]->freq < pq->nodes[smallest]->freq) smallest = right;

      if (smallest == idx) break;

      HuffmanNode *tmp = pq->nodes[idx];
      pq->nodes[idx] = pq->nodes[smallest];
      pq->nodes[smallest] = tmp;
      idx = smallest;
    }
  }

  return result;
}

//============ Huffman Tree ============

static HuffmanNode *create_node(int symbol, uint64_t freq)
{
  HuffmanNode *node = malloc(sizeof(HuffmanNode));
  if (!node) return NULL;  // check malloc
  node->symbol = symbol;
  node->freq = freq;
  node->left = NULL;
  node->right = NULL;
  return node;
}

static void destroy_tree(HuffmanNode *root)
{
  if (!root) return;
  destroy_tree(root->left);
  destroy_tree(root->right);
  free(root);
}

static HuffmanNode *build_tree(uint64_t *frequencies)
{
  PriorityQueue *pq = pq_create(NUM_SYMBOLS + 1);
  if (!pq) return NULL;  // check malloc

  int num_symbols = 0;
  for (int i = 0; i < NUM_SYMBOLS; i++)
  {
    if (frequencies[i] > 0)
    {
      HuffmanNode *node = create_node(i, frequencies[i]);
      if (!node)  // check malloc
      {
        pq_destroy(pq);
        return NULL;
      }
      if (pq_push(pq, node) != 0)  // check push result
      {
        free(node);
        pq_destroy(pq);
        return NULL;
      }
      num_symbols++;
    }
  }

  if (num_symbols == 0)
  {
    pq_destroy(pq);
    return create_node(0, 0);
  }

  if (num_symbols == 1)
  {
    HuffmanNode *single = pq_pop(pq);
    HuffmanNode *root = create_node(-1, single->freq);
    if (!root)  // check malloc
    {
      free(single);
      pq_destroy(pq);
      return NULL;
    }
    root->left = single;
    root->right = create_node(-1, 0);
    if (!root->right)  // check malloc
    {
      free(root);
      free(single);
      pq_destroy(pq);
      return NULL;
    }
    pq_destroy(pq);
    return root;
  }

  while (pq->size > 1)
  {
    HuffmanNode *left = pq_pop(pq);
    HuffmanNode *right = pq_pop(pq);
    HuffmanNode *parent = create_node(-1, left->freq + right->freq);
    if (!parent)  // check malloc
    {
      destroy_tree(left);
      destroy_tree(right);
      pq_destroy(pq);
      return NULL;
    }
    parent->left = left;
    parent->right = right;
    if (pq_push(pq, parent) != 0)  // check push
    {
      destroy_tree(parent);
      pq_destroy(pq);
      return NULL;
    }
  }

  HuffmanNode *root = pq_pop(pq);
  pq_destroy(pq);
  return root;
}

//============ Code Generation ============

static void generate_codes_recursive(HuffmanNode *node, uint32_t code, uint8_t length, HuffmanCode *codes)
{
  if (!node) return;

  if (node->symbol >= 0)
  {
    codes[node->symbol].code = code;
    codes[node->symbol].length = length > 0 ? length : 1;
    return;
  }

  generate_codes_recursive(node->left, code << 1, length + 1, codes);
  generate_codes_recursive(node->right, (code << 1) | 1, length + 1, codes);
}

static void generate_codes(HuffmanNode *root, HuffmanCode *codes)
{
  memset(codes, 0, NUM_SYMBOLS * sizeof(HuffmanCode));
  generate_codes_recursive(root, 0, 0, codes);
}

// Make canonical codes from lengths
static void make_canonical(HuffmanCode *codes, uint8_t *lengths)
{
  // Extract and clamp lengths
  for (int i = 0; i < NUM_SYMBOLS; i++)
  {
    lengths[i] = codes[i].length;
    if (lengths[i] > MAX_CODE_LENGTH) lengths[i] = MAX_CODE_LENGTH;
  }

  // Count codes of each length
  int count[MAX_CODE_LENGTH + 1] = {0};
  for (int i = 0; i < NUM_SYMBOLS; i++)
  {
    if (lengths[i] > 0) count[lengths[i]]++;
  }

  // Calculate starting code for each length
  uint32_t next_code[MAX_CODE_LENGTH + 1] = {0};
  uint32_t code = 0;
  for (int len = 1; len <= MAX_CODE_LENGTH; len++)
  {
    code = (code + count[len - 1]) << 1;
    next_code[len] = code;
  }

  // Assign canonical codes in symbol order
  for (int i = 0; i < NUM_SYMBOLS; i++)
  {
    if (lengths[i] > 0)
    {
      codes[i].code = next_code[lengths[i]]++;
      codes[i].length = lengths[i];
    }
  }
}

//============ Fast Decoding ============

typedef struct
{
  int16_t symbol; // Symbol or -1 if need more bits
  uint8_t length; // Code length
} DecodeEntry;

static DecodeEntry *build_decode_table(uint8_t *lengths, HuffmanCode *codes)
{
  size_t table_size = 1 << DECODE_TABLE_BITS;
  DecodeEntry *table = malloc(table_size * sizeof(DecodeEntry));
  if (!table) return NULL;  // check malloc

  // Initialize all entries as invalid
  for (size_t i = 0; i < table_size; i++)
  {
    table[i].symbol = -1;
    table[i].length = 0;
  }

  // Fill table for each symbol
  for (int sym = 0; sym < NUM_SYMBOLS; sym++)
  {
    if (lengths[sym] == 0) continue;
    if (lengths[sym] > DECODE_TABLE_BITS) continue;

    uint32_t code = codes[sym].code;
    int len = lengths[sym];
    int pad_bits = DECODE_TABLE_BITS - len;

    // Fill all entries that match this code
    uint32_t base = code << pad_bits;
    uint32_t count = 1 << pad_bits;

    for (uint32_t j = 0; j < count; j++)
    {
      table[base + j].symbol = sym;
      table[base + j].length = len;
    }
  }

  return table;
}

//============ Buffered Bit I/O ============

typedef struct
{
  FILE *file;
  uint32_t buffer;
  int bits_in_buffer;
} FastBitReader;

static void fbr_init(FastBitReader *fbr, FILE *file)
{
  fbr->file = file;
  fbr->buffer = 0;
  fbr->bits_in_buffer = 0;
}

static void fbr_fill(FastBitReader *fbr)
{
  while (fbr->bits_in_buffer <= 24)
  {
    int c = fgetc(fbr->file);
    if (c == EOF) break;
    fbr->buffer |= ((uint32_t)c << (24 - fbr->bits_in_buffer));
    fbr->bits_in_buffer += 8;
  }
}

static inline uint32_t fbr_peek(FastBitReader *fbr, int n) { return fbr->buffer >> (32 - n); }

static inline void fbr_consume(FastBitReader *fbr, int n)
{
  fbr->buffer <<= n;
  fbr->bits_in_buffer -= n;
}

static inline int fbr_read_bit(FastBitReader *fbr)
{
  if (fbr->bits_in_buffer == 0) fbr_fill(fbr);
  int bit = (fbr->buffer >> 31) & 1;
  fbr->buffer <<= 1;
  fbr->bits_in_buffer--;
  return bit;
}

//============ Public API ============

int huffman_compress(CompressionContext *ctx)
{
  FILE *input = fopen(ctx->input_path, "rb");
  if (!input) return -1;

  // First pass: count frequencies
  uint64_t frequencies[NUM_SYMBOLS] = {0};

  if (ctx->progress_callback) { ctx->progress_callback(0, "Counting frequencies"); }

  uint64_t hdr_size;
  if (fread(&hdr_size, sizeof(hdr_size), 1, input) != 1)  // check fread
  {
    fclose(input);
    return -1;
  }
  fseek(input, 8 + hdr_size, SEEK_SET);

  size_t data_size = ctx->input_size - 8 - hdr_size;
  size_t processed = 0;
  int last_percent = -1;

  // Use buffered reading for speed
  uint8_t *read_buf = malloc(65536);
  if (!read_buf)  // check malloc
  {
    fclose(input);
    return -1;
  }
  size_t buf_pos = 0, buf_len = 0;

  while (processed < data_size)
  {
    if (buf_pos >= buf_len)
    {
      buf_len = fread(read_buf, 1, 65536, input);
      buf_pos = 0;
      if (buf_len == 0) break;
    }

    frequencies[read_buf[buf_pos++]]++;
    processed++;

    if (ctx->progress_callback && (processed & 0xFFFFF) == 0)
    {
      int percent = (int)((processed * 50) / data_size);
      if (percent != last_percent)
      {
        ctx->progress_callback(percent, "Counting frequencies");
        last_percent = percent;
      }
    }
  }

  // Build tree and generate canonical codes
  HuffmanNode *tree = build_tree(frequencies);
  if (!tree)  // check build_tree result
  {
    free(read_buf);
    fclose(input);
    return -1;
  }
  HuffmanCode codes[NUM_SYMBOLS];
  generate_codes(tree, codes);

  uint8_t lengths[NUM_SYMBOLS];
  make_canonical(codes, lengths);
  destroy_tree(tree);

  // Open output
  FILE *output = fopen(ctx->compressed_path, "wb");
  if (!output)
  {
    fclose(input);
    free(read_buf);
    return -1;
  }

  // Write compressed file header
  CompressedFileHeader hdr = {0};
  memcpy(hdr.magic, STC_MAGIC, STC_MAGIC_SIZE);
  hdr.algorithm = STC_ALG_HUFFMAN;
  hdr.flags = ctx->use_chunking ? STC_FLAG_CHUNKED : 0;
  hdr.original_size = ctx->input_size;
  hdr.header_size = hdr_size;
  fwrite(&hdr, sizeof(hdr), 1, output);

  // Write safetensors header
  fseek(input, 0, SEEK_SET);
  if (fread(&hdr_size, sizeof(hdr_size), 1, input) != 1)  // check fread
  {
    free(read_buf);
    fclose(input);
    fclose(output);
    return -1;
  }
  fwrite(&hdr_size, sizeof(hdr_size), 1, output);

  char *st_hdr = malloc(hdr_size);
  if (!st_hdr)  // check malloc
  {
    free(read_buf);
    fclose(input);
    fclose(output);
    return -1;
  }
  if (fread(st_hdr, 1, hdr_size, input) != hdr_size)  // check fread
  {
    free(st_hdr);
    free(read_buf);
    fclose(input);
    fclose(output);
    return -1;
  }
  fwrite(st_hdr, 1, hdr_size, output);
  free(st_hdr);

  // Write code lengths and data size
  fwrite(lengths, 1, NUM_SYMBOLS, output);
  fwrite(&data_size, sizeof(data_size), 1, output);

  // Second pass: encode
  BitWriter bw;
  bitwriter_init(&bw, output);

  processed = 0;
  buf_pos = 0;
  buf_len = 0;
  last_percent = -1;

  while (processed < data_size)
  {
    if (buf_pos >= buf_len)
    {
      buf_len = fread(read_buf, 1, 65536, input);
      buf_pos = 0;
      if (buf_len == 0) break;
    }

    uint8_t sym = read_buf[buf_pos++];
    bitwriter_write_bits(&bw, codes[sym].code, codes[sym].length);
    processed++;

    if (ctx->progress_callback && (processed & 0xFFFFF) == 0)
    {
      int percent = 50 + (int)((processed * 50) / data_size);
      if (percent != last_percent)
      {
        ctx->progress_callback(percent, "Encoding (huffman)");
        last_percent = percent;
      }
    }
  }

  bitwriter_flush(&bw);

  free(read_buf);
  fclose(input);
  fclose(output);

  if (ctx->progress_callback) { ctx->progress_callback(100, "Compression complete"); }

  return 0;
}

int huffman_decompress(CompressionContext *ctx)
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

  // Read and write safetensors header
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

  // Read code lengths and data size
  uint8_t lengths[NUM_SYMBOLS];
  if (fread(lengths, 1, NUM_SYMBOLS, input) != NUM_SYMBOLS)  // check fread
  {
    fclose(input);
    fclose(output);
    return -1;
  }

  size_t data_size;
  if (fread(&data_size, sizeof(data_size), 1, input) != 1)  // check fread
  {
    fclose(input);
    fclose(output);
    return -1;
  }

  // Build canonical codes and decode table
  HuffmanCode codes[NUM_SYMBOLS] = {0};
  for (int i = 0; i < NUM_SYMBOLS; i++) { codes[i].length = lengths[i]; }
  make_canonical(codes, lengths);

  DecodeEntry *decode_table = build_decode_table(lengths, codes);
  if (!decode_table)  // check malloc
  {
    fclose(input);
    fclose(output);
    return -1;
  }

  // Decode using fast bit reader
  FastBitReader fbr;
  fbr_init(&fbr, input);

  // Output buffer for speed
  uint8_t *out_buf = malloc(65536);
  if (!out_buf)  // check malloc
  {
    free(decode_table);
    fclose(input);
    fclose(output);
    return -1;
  }
  size_t out_pos = 0;

  size_t decoded = 0;
  int last_percent = -1;

  while (decoded < data_size)
  {
    fbr_fill(&fbr);

    // Try fast table lookup
    uint32_t bits = fbr_peek(&fbr, DECODE_TABLE_BITS);
    DecodeEntry entry = decode_table[bits];

    if (entry.symbol >= 0)
    {
      // Fast path
      out_buf[out_pos++] = (uint8_t)entry.symbol;
      fbr_consume(&fbr, entry.length);
    }
    else
    {
      // Slow path for long codes
      uint32_t code = 0;
      int len = 0;

      while (len < MAX_CODE_LENGTH)
      {
        code = (code << 1) | fbr_read_bit(&fbr);
        len++;

        for (int sym = 0; sym < NUM_SYMBOLS; sym++)
        {
          if (codes[sym].length == len && codes[sym].code == code)
          {
            out_buf[out_pos++] = (uint8_t)sym;
            goto found;
          }
        }
      }
      // Code not found - error
      break;
    found:;
    }

    decoded++;

    // Flush output buffer
    if (out_pos >= 65536)
    {
      fwrite(out_buf, 1, out_pos, output);
      out_pos = 0;
    }

    if (ctx->progress_callback && (decoded & 0xFFFFF) == 0)
    {
      int percent = (int)((decoded * 100) / data_size);
      if (percent != last_percent)
      {
        ctx->progress_callback(percent, "Decompressing (huffman)");
        last_percent = percent;
      }
    }
  }

  // Flush remaining output
  if (out_pos > 0) { fwrite(out_buf, 1, out_pos, output); }

  free(out_buf);
  free(decode_table);
  fclose(input);
  fclose(output);

  if (ctx->progress_callback) { ctx->progress_callback(100, "Decompression complete"); }

  return 0;
}
