#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define FILENAME "model.safetensors"
#define COMPRESSED_FILENAME FILENAME ".gz"
#define MAX_COMMAND_LEN 256

// Função para obter o tamanho de um ficheiro
long get_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

// Função principal
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <nivel_compressao> (e.g., -1 ou -9)\n", argv[0]);
        return 1;
    }

    char *compression_level = argv[1];
    long original_size, compressed_size;
    char cmd_compress[MAX_COMMAND_LEN];
    char cmd_decompress[MAX_COMMAND_LEN];

    printf("--- Teste de Gzip (Nível: %s) ---\n", compression_level);

    // 1. COMPRESSÃO
    original_size = get_file_size(FILENAME);
    if (original_size <= 0) {
        fprintf(stderr, "ERRO: Ficheiro '%s' não encontrado ou vazio.\n", FILENAME);
        return 1;
    }
    printf("Tamanho Original: %ld bytes (%.2f MB)\n", original_size, original_size / (1024.0 * 1024.0));

    // Comando de Compressão: gzip -f <nível> <ficheiro> (sem -k, o original é apagado)
    sprintf(cmd_compress, "gzip -f %s %s", compression_level, FILENAME);
    printf("A executar: %s\n", cmd_compress);
    if (system(cmd_compress) != 0) {
        fprintf(stderr, "ERRO: Falha na compressão.\n");
        return 1;
    }

    // 2. CÁLCULO DA CR
    compressed_size = get_file_size(COMPRESSED_FILENAME);
    if (compressed_size <= 0) {
        fprintf(stderr, "ERRO: Ficheiro comprimido '%s' não encontrado.\n", COMPRESSED_FILENAME);
        return 1;
    }

    double cr = (double)original_size / compressed_size;
    printf("Tamanho Comprimido: %ld bytes (%.2f MB)\n", compressed_size, compressed_size / (1024.0 * 1024.0));
    printf("Taxa de Compressão (CR): %.4f:1\n", cr);
    
    // 3. DESCOMPRESSÃO (para restaurar o ficheiro original)
    sprintf(cmd_decompress, "gzip -f -d %s", COMPRESSED_FILENAME);
    printf("A restaurar ficheiro original...\n");
    if (system(cmd_decompress) != 0) {
        fprintf(stderr, "ERRO: Falha na descompressão.\n");
        return 1;
    }

    printf("Ficheiro restaurado. Teste Gzip (%s) concluído com sucesso.\n", compression_level);
    return 0;
}