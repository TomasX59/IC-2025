#include "stcompress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

// Platform-specific includes
#if defined(__APPLE__)
#include <mach/mach.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <sys/resource.h>
#include <sys/sysinfo.h>
#endif

// Para clock_gettime em alguns sistemas
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

//============ File utilities ============
int file_exists(const char *path)
{
  struct stat st;
  return stat(path, &st) == 0;
}

int ensure_directory(const char *path)
{
  struct stat st;
  if (stat(path, &st) == 0) { return S_ISDIR(st.st_mode) ? 0 : -1; }
  // Create directory with rwxr-xr-x permissions
  return mkdir(path, 0755);
}

size_t get_file_size(const char *path)
{
  struct stat st;
  if (stat(path, &st) != 0) { return 0; }
  return (size_t)st.st_size;
}

int verify_files(const char *path1, const char *path2)
{
  FILE *f1 = fopen(path1, "rb");
  FILE *f2 = fopen(path2, "rb");

  if (!f1 || !f2)
  {
    if (f1) fclose(f1);
    if (f2) fclose(f2);
    return 0;
  }

  // Compare file sizes first
  fseek(f1, 0, SEEK_END);
  fseek(f2, 0, SEEK_END);
  long size1 = ftell(f1);
  long size2 = ftell(f2);

  if (size1 != size2)
  {
    fclose(f1);
    fclose(f2);
    return 0;
  }

  fseek(f1, 0, SEEK_SET);
  fseek(f2, 0, SEEK_SET);

  // Compare contents in chunks
  const size_t CHUNK_SIZE = 65536;
  uint8_t *buf1 = malloc(CHUNK_SIZE);
  uint8_t *buf2 = malloc(CHUNK_SIZE);

  if (!buf1 || !buf2)
  {
    free(buf1);
    free(buf2);
    fclose(f1);
    fclose(f2);
    return 0;
  }

  int result = 1;
  size_t read1, read2;

  while ((read1 = fread(buf1, 1, CHUNK_SIZE, f1)) > 0)
  {
    read2 = fread(buf2, 1, CHUNK_SIZE, f2);
    if (read1 != read2 || memcmp(buf1, buf2, read1) != 0)
    {
      result = 0;
      break;
    }
  }

  free(buf1);
  free(buf2);
  fclose(f1);
  fclose(f2);

  return result;
}

//============ Memory utilities ============
size_t get_free_memory_mb(void)
{
#if defined(__APPLE__)
  // macOS: use sysctl for total memory, mach for free pages
  int64_t total_mem = 0;
  size_t len = sizeof(total_mem);
  if (sysctlbyname("hw.memsize", &total_mem, &len, NULL, 0) == 0)
  {
    // Get VM statistics
    vm_size_t page_size;
    vm_statistics64_data_t vm_stats;
    mach_port_t mach_port = mach_host_self();
    mach_msg_type_number_t count = sizeof(vm_stats) / sizeof(natural_t);

    if (host_page_size(mach_port, &page_size) == KERN_SUCCESS &&
        host_statistics64(mach_port, HOST_VM_INFO64, (host_info64_t)&vm_stats, &count) == KERN_SUCCESS)
    {
      // Include free + inactive + purgeable (all reclaimable)
      uint64_t available =
          (uint64_t)(vm_stats.free_count + vm_stats.inactive_count + vm_stats.purgeable_count) * page_size;
      return available / (1024 * 1024);
    }
    // Fallback: assume 50% free
    return (size_t)(total_mem / (2 * 1024 * 1024));
  }
#elif defined(__linux__)
  // Linux: read /proc/meminfo
  FILE *f = fopen("/proc/meminfo", "r");
  if (f)
  {
    char line[256];
    size_t mem_available = 0;
    while (fgets(line, sizeof(line), f))
    {
      if (strncmp(line, "MemAvailable:", 13) == 0)
      {
        sscanf(line + 13, "%zu", &mem_available);
        fclose(f);
        return mem_available / 1024; // kB to MB
      }
    }
    fclose(f);
  }
#endif
  // Portable fallback: assume 4GB available
  return 4096;
}

size_t estimate_memory_usage(int algorithm, size_t input_size)
{
  (void)input_size; // Not used directly in estimation

  switch (algorithm)
  {
  case STC_ALG_ARITHMETIC:
    // Frequency tables + range coder state + buffers
    return 50;
  case STC_ALG_HUFFMAN:
    // Huffman tree + frequency table + buffers
    return 100;
  case STC_ALG_LZ77:
    // Hash table + sliding window (64KB) + buffers
    return 70;
  default:
    return 100;
  }
}

size_t get_peak_memory_mb(void)
{
#if defined(__APPLE__) || defined(__linux__)
  struct rusage usage;
  if (getrusage(RUSAGE_SELF, &usage) == 0)
  {
#if defined(__APPLE__)
    // macOS reports in bytes
    return (size_t)(usage.ru_maxrss / (1024 * 1024));
#else
    // Linux reports in kilobytes
    return (size_t)(usage.ru_maxrss / 1024);
#endif
  }
#endif
  return 0;
}

//============ Timer utilities ============
double get_time_ms(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

//============ SafeTensors parsing ============
// Simple JSON string extraction helper
static int json_get_string(const char *json, const char *key, char *out, size_t out_size)
{
  char search[512];
  snprintf(search, sizeof(search), "\"%s\"", key);

  const char *pos = strstr(json, search);
  if (!pos) return -1;

  pos = strchr(pos + strlen(search), ':');
  if (!pos) return -1;

  // Skip whitespace
  while (*pos && (*pos == ':' || *pos == ' ' || *pos == '\t')) pos++;

  if (*pos != '"') return -1;
  pos++; // Skip opening quote

  size_t i = 0;
  while (*pos && *pos != '"' && i < out_size - 1) { out[i++] = *pos++; }
  out[i] = '\0';

  return 0;
}

// Extract array of integers from JSON (for shape)
static int json_get_int_array(const char *json, const char *key, size_t *out, size_t max_size, size_t *count)
{
  char search[512];
  snprintf(search, sizeof(search), "\"%s\"", key);

  const char *pos = strstr(json, search);
  if (!pos) return -1;

  pos = strchr(pos + strlen(search), '[');
  if (!pos) return -1;
  pos++; // Skip '['

  *count = 0;
  while (*pos && *pos != ']' && *count < max_size)
  {
    while (*pos && (*pos == ' ' || *pos == ',')) pos++;
    if (*pos == ']') break;

    out[*count] = (size_t)strtol(pos, (char **)&pos, 10);
    (*count)++;
  }

  return 0;
}

// Extract data_offsets [start, end] from JSON
static int json_get_offsets(const char *json, const char *key, size_t *start, size_t *end)
{
  char search[512];
  snprintf(search, sizeof(search), "\"%s\"", key);

  const char *pos = strstr(json, search);
  if (!pos) return -1;

  pos = strchr(pos + strlen(search), '[');
  if (!pos) return -1;
  pos++; // Skip '['

  *start = (size_t)strtol(pos, (char **)&pos, 10);

  while (*pos && *pos != ',') pos++;
  if (*pos == ',') pos++;

  *end = (size_t)strtol(pos, NULL, 10);

  return 0;
}

int parse_safetensors_header(const char *path, SafeTensorsHeader *header)
{
  FILE *f = fopen(path, "rb");
  if (!f) return -1;

  memset(header, 0, sizeof(SafeTensorsHeader));

  // Read header size (first 8 bytes, little-endian uint64)
  uint64_t header_size;
  if (fread(&header_size, sizeof(header_size), 1, f) != 1)
  {
    fclose(f);
    return -1;
  }
  header->header_size = (size_t)header_size;
  header->data_offset = 8 + header->header_size;

  // Read JSON header
  header->header_json = malloc(header->header_size + 1);
  if (!header->header_json)
  {
    fclose(f);
    return -1;
  }

  if (fread(header->header_json, 1, header->header_size, f) != header->header_size)
  {
    free(header->header_json);
    fclose(f);
    return -1;
  }
  header->header_json[header->header_size] = '\0';
  fclose(f);

  // Count tensors (count occurrences of "dtype")
  const char *p = header->header_json;
  size_t count = 0;
  while ((p = strstr(p, "\"dtype\"")) != NULL)
  {
    count++;
    p++;
  }
  header->num_tensors = count;

  // Allocate tensor info array
  header->tensors = calloc(count, sizeof(TensorInfo));
  if (!header->tensors)
  {
    free(header->header_json);
    return -1;
  }

  // Parse each tensor - find tensor names (keys with dtype inside)
  p = header->header_json;
  size_t tensor_idx = 0;

  // Skip the opening brace and any __metadata__
  if (*p == '{') p++;

  while (tensor_idx < count && *p)
  {
    // Find next key
    while (*p && *p != '"') p++;
    if (!*p) break;
    p++; // Skip opening quote

    // Check if this is __metadata__
    if (strncmp(p, "__metadata__", 12) == 0)
    {
      // Skip this entry - find the closing brace
      int brace_count = 0;
      while (*p && !(*p == '{')) p++;
      if (*p == '{')
      {
        brace_count = 1;
        p++;
        while (*p && brace_count > 0)
        {
          if (*p == '{') brace_count++;
          else if (*p == '}') brace_count--;
          p++;
        }
      }
      continue;
    }

    // Extract tensor name
    char *name_start = (char *)p;
    while (*p && *p != '"') p++;
    size_t name_len = p - name_start;
    if (name_len >= sizeof(header->tensors[tensor_idx].name))
    {
      name_len = sizeof(header->tensors[tensor_idx].name) - 1;
    }
    strncpy(header->tensors[tensor_idx].name, name_start, name_len);
    header->tensors[tensor_idx].name[name_len] = '\0';

    // Find the value object for this tensor
    while (*p && *p != '{') p++;
    if (!*p) break;

    // Find end of this object
    char *obj_start = (char *)p;
    int brace_count = 1;
    p++;
    while (*p && brace_count > 0)
    {
      if (*p == '{') brace_count++;
      else if (*p == '}') brace_count--;
      p++;
    }
    char *obj_end = (char *)p;

    // Temporarily null-terminate this object for parsing
    char saved = *obj_end;
    *obj_end = '\0';

    // Parse dtype
    json_get_string(obj_start, "dtype", header->tensors[tensor_idx].dtype, sizeof(header->tensors[tensor_idx].dtype));

    // Parse shape
    json_get_int_array(obj_start, "shape", header->tensors[tensor_idx].shape, 8, &header->tensors[tensor_idx].num_dims);

    // Parse data_offsets
    size_t start, end;
    if (json_get_offsets(obj_start, "data_offsets", &start, &end) == 0)
    {
      header->tensors[tensor_idx].data_offset = start;
      header->tensors[tensor_idx].data_size = end - start;
    }

    *obj_end = saved;
    tensor_idx++;
  }

  return 0;
}

void free_safetensors_header(SafeTensorsHeader *header)
{
  if (header->header_json)
  {
    free(header->header_json);
    header->header_json = NULL;
  }
  if (header->tensors)
  {
    free(header->tensors);
    header->tensors = NULL;
  }
}

//============ Bit I/O utilities ============
void bitwriter_init(BitWriter *bw, FILE *file)
{
  bw->file = file;
  bw->buffer = 0;
  bw->bit_count = 0;
}

void bitwriter_write_bit(BitWriter *bw, int bit)
{
  bw->buffer = (bw->buffer << 1) | (bit & 1);
  bw->bit_count++;

  if (bw->bit_count == 8)
  {
    fputc(bw->buffer, bw->file);
    bw->buffer = 0;
    bw->bit_count = 0;
  }
}

void bitwriter_write_bits(BitWriter *bw, uint32_t value, int num_bits)
{
  for (int i = num_bits - 1; i >= 0; i--) { bitwriter_write_bit(bw, (value >> i) & 1); }
}

void bitwriter_flush(BitWriter *bw)
{
  if (bw->bit_count > 0)
  {
    bw->buffer <<= (8 - bw->bit_count);
    fputc(bw->buffer, bw->file);
    bw->buffer = 0;
    bw->bit_count = 0;
  }
}

void bitreader_init(BitReader *br, FILE *file)
{
  br->file = file;
  br->buffer = 0;
  br->bit_count = 0;
  br->eof = 0;
}

int bitreader_read_bit(BitReader *br)
{
  if (br->bit_count == 0)
  {
    int c = fgetc(br->file);
    if (c == EOF)
    {
      br->eof = 1;
      return 0;
    }
    br->buffer = (uint8_t)c;
    br->bit_count = 8;
  }

  br->bit_count--;
  return (br->buffer >> br->bit_count) & 1;
}

uint32_t bitreader_read_bits(BitReader *br, int num_bits)
{
  uint32_t value = 0;
  for (int i = 0; i < num_bits; i++) { value = (value << 1) | bitreader_read_bit(br); }
  return value;
}

//============ Context cleanup ============
void cleanup_context(CompressionContext *ctx) { free_safetensors_header(&ctx->header); }
