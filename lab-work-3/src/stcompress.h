#ifndef STCOMPRESS_H
#define STCOMPRESS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Magic bytes for compressed file format
#define STC_MAGIC "STC"
#define STC_MAGIC_SIZE 4

// Algorithm identifiers
#define STC_ALG_ARITHMETIC 0
#define STC_ALG_HUFFMAN 1
#define STC_ALG_LZ77 2

// Flags
#define STC_FLAG_CHUNKED 0x01

// Tensor information from safetensors header
typedef struct
{
  char name[256];
  char dtype[16];
  size_t shape[8];
  size_t num_dims;
  size_t data_offset;
  size_t data_size;
} TensorInfo;

// Safetensors header information
typedef struct
{
  size_t header_size;
  char *header_json;
  TensorInfo *tensors;
  size_t num_tensors;
  size_t data_offset; // Where tensor data starts in file
} SafeTensorsHeader;

// Progress callback type
typedef void (*ProgressCallback)(int percent, const char *stage);

// Compression context passed to algorithms
typedef struct
{
  const char *input_path;
  const char *compressed_path;
  const char *decompressed_path;
  size_t input_size;
  size_t memory_limit_mb;
  int use_chunking;
  SafeTensorsHeader header;
  ProgressCallback progress_callback;
} CompressionContext;

// Compression results
typedef struct
{
  size_t compressed_size;
  double compression_time_ms;
  double decompression_time_ms;
  size_t peak_memory_mb;
  int verified;
} CompressionResult;

// Compressed file header structure
typedef struct
{
  char magic[4];          //"STC\0"
  uint8_t algorithm;      // Algorithm ID
  uint8_t flags;          // Flags (chunked, etc.)
  uint64_t original_size; // Original file size
  uint64_t header_size;   // Safetensors JSON header size
} __attribute__((packed)) CompressedFileHeader;

//============ File utilities ============
// Check if file exists
int file_exists(const char *path);

// Ensure directory exists (creates if needed)
int ensure_directory(const char *path);

// Get file size in bytes
size_t get_file_size(const char *path);

// Compare two files byte-by-byte
int verify_files(const char *path1, const char *path2);

//============ Memory utilities ============
// Get free system memory in MB (portable)
size_t get_free_memory_mb(void);

// Estimate memory usage for algorithm
size_t estimate_memory_usage(int algorithm, size_t input_size);

// Get peak memory usage of current process in MB
size_t get_peak_memory_mb(void);

//============ Timer utilities ============
// Get current time in milliseconds
double get_time_ms(void);

//============ SafeTensors parsing ============
// Parse safetensors file header
int parse_safetensors_header(const char *path, SafeTensorsHeader *header);

// Free safetensors header resources
void free_safetensors_header(SafeTensorsHeader *header);

//============ Bit I/O utilities ============
typedef struct
{
  FILE *file;
  uint8_t buffer;
  int bit_count;
} BitWriter;

typedef struct
{
  FILE *file;
  uint8_t buffer;
  int bit_count;
  int eof;
} BitReader;

// Initialize bit writer
void bitwriter_init(BitWriter *bw, FILE *file);

// Write a single bit
void bitwriter_write_bit(BitWriter *bw, int bit);

// Write multiple bits (up to 32)
void bitwriter_write_bits(BitWriter *bw, uint32_t value, int num_bits);

// Flush remaining bits
void bitwriter_flush(BitWriter *bw);

// Initialize bit reader
void bitreader_init(BitReader *br, FILE *file);

// Read a single bit
int bitreader_read_bit(BitReader *br);

// Read multiple bits (up to 32)
uint32_t bitreader_read_bits(BitReader *br, int num_bits);

//============ Context cleanup ============
// Cleanup compression context resources
void cleanup_context(CompressionContext *ctx);

#endif // STCOMPRESS_H
