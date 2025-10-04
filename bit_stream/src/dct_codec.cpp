#include "dct_codec.h"
#include <sndfile.hh>
#include "bit_stream.h"
#include "quantization.h"
#include <iostream>
#include <vector>
#include <stdexcept>
#include <cmath>

// Tamanho do bloco para processamento (pode ajustar conforme necessário)
const int BLOCK_SIZE = 1024;
constexpr double PI = 3.14159265358979323846;

// Função para aplicar DCT Type-II em um bloco
void applyDCT(std::vector<short>& block) {
    std::vector<double> result(BLOCK_SIZE);
    
    for (int k = 0; k < BLOCK_SIZE; k++) {
        double sum = 0.0;
        double scale = (k == 0) ? 1.0 / sqrt(BLOCK_SIZE) : sqrt(2.0 / BLOCK_SIZE);
        
        for (int n = 0; n < BLOCK_SIZE; n++) {
            sum += block[n] * cos((PI * (2 * n + 1) * k) / (2 * BLOCK_SIZE));
        }
        
        result[k] = scale * sum;
    }
    
    // Copiar resultados de volta para o bloco
    for (int i = 0; i < BLOCK_SIZE; i++) {
        block[i] = static_cast<short>(std::round(result[i]));
    }
}

// Função para aplicar IDCT Type-III em um bloco
void applyIDCT(std::vector<short>& block) {
    std::vector<double> result(BLOCK_SIZE);
    
    for (int n = 0; n < BLOCK_SIZE; n++) {
        double sum = block[0] / sqrt(BLOCK_SIZE);
        
        for (int k = 1; k < BLOCK_SIZE; k++) {
            sum += sqrt(2.0 / BLOCK_SIZE) * block[k] * 
                  cos((PI * (2 * n + 1) * k) / (2 * BLOCK_SIZE));
        }
        
        result[n] = sum;
    }
    
    // Copiar resultados de volta para o bloco
    for (int i = 0; i < BLOCK_SIZE; i++) {
        block[i] = static_cast<short>(std::round(result[i]));
    }
}

void encodeWav(const std::string &inputWav, const std::string &outputFile) {
    // Abrir o arquivo WAV de entrada
    SndfileHandle sf(inputWav);
    if (sf.error()) {
        throw std::runtime_error("Erro ao abrir o arquivo WAV: " + inputWav);
    }

    int channels = sf.channels();
    if (channels > 2) {
        throw std::runtime_error("Apenas arquivos WAV mono ou estéreo são suportados");
    }

    // Criar arquivo de saída
    std::fstream fs(outputFile, std::ios::out | std::ios::binary);
    if (!fs) {
        throw std::runtime_error("Erro ao criar arquivo de saída: " + outputFile);
    }
    BitStream bs(fs, true);  // true para modo de escrita

    // Escrever cabeçalho
    // 1. Sample rate (32 bits)
    bs.write_n_bits(static_cast<uint64_t>(sf.samplerate()), 32);
    // 2. Número total de frames (32 bits)
    bs.write_n_bits(static_cast<uint64_t>(sf.frames()), 32);
    // 3. Tamanho do bloco (16 bits)
    bs.write_n_bits(static_cast<uint64_t>(BLOCK_SIZE), 16);

    std::cout << "Informações do arquivo:\n";
    std::cout << "Sample rate: " << sf.samplerate() << " Hz\n";
    std::cout << "Channels: " << sf.channels() << "\n";
    std::cout << "Frames: " << sf.frames() << "\n";

    // Criar buffers para as amostras
    std::vector<short> block(BLOCK_SIZE); // Buffer para o bloco mono final
    std::vector<short> readBuffer(BLOCK_SIZE * channels); // Buffer para leitura (mono ou estéreo)
    
    // Ler e processar o arquivo em blocos
    sf_count_t framesRead;
    int blockCount = 0;
    
    while ((framesRead = sf.readf(readBuffer.data(), BLOCK_SIZE)) > 0) {
        // Converter para mono se for estéreo
        if (channels == 2) {
            for (sf_count_t i = 0; i < framesRead; i++) {
                // Média dos canais esquerdo e direito
                block[i] = static_cast<short>((static_cast<int>(readBuffer[i*2]) + 
                                             static_cast<int>(readBuffer[i*2+1])) / 2);
            }
        } else {
            // Se já for mono, apenas copiar
            std::copy(readBuffer.begin(), readBuffer.begin() + framesRead, block.begin());
        }

        // Se o último bloco for menor, preenchemos com zeros
        if (framesRead < BLOCK_SIZE) {
            std::fill(block.begin() + framesRead, block.end(), 0);
        }

        // Aplicar DCT no bloco
        applyDCT(block);

        // Quantizar os coeficientes
        auto quantizedBlock = quantizeDCTCoefficients(block);

        // Escrever o tamanho do bloco (16 bits)
        bs.write_n_bits(static_cast<uint64_t>(framesRead), 16);

        // Escrever os coeficientes quantizados
        for (const auto &coef : quantizedBlock) {
            // Escrever cada coeficiente como 16 bits
            bs.write_n_bits(static_cast<uint64_t>(static_cast<uint16_t>(coef)), 16);
        }

        blockCount++;
        std::cout << "Processado bloco " << blockCount << " com " << framesRead << " frames\n";
    }

    bs.close();
    std::cout << "Total de blocos processados: " << blockCount << "\n";
}

void decodeWav(const std::string &inputFile, const std::string &outputWav) {
    // Abrir arquivo binário de entrada
    std::fstream fs(inputFile, std::ios::in | std::ios::binary);
    if (!fs) {
        throw std::runtime_error("Erro ao abrir arquivo de entrada: " + inputFile);
    }
    BitStream bs(fs, false);  // false para modo de leitura

    // Ler cabeçalho
    // 1. Sample rate (32 bits)
    int sampleRate = static_cast<int>(bs.read_n_bits(32));
    // 2. Número total de frames (32 bits)
    sf_count_t totalFrames = static_cast<sf_count_t>(bs.read_n_bits(32));
    // 3. Tamanho do bloco (16 bits)
    int blockSize = static_cast<int>(bs.read_n_bits(16));

    if (blockSize != BLOCK_SIZE) {
        throw std::runtime_error("Tamanho do bloco incompatível");
    }

    // Criar arquivo WAV de saída
    SndfileHandle sf(outputWav, SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 1, sampleRate);
    if (sf.error()) {
        throw std::runtime_error("Erro ao criar arquivo WAV: " + outputWav);
    }

    // Buffers para processamento
    std::vector<short> block(BLOCK_SIZE);
    std::vector<short> quantizedBlock(BLOCK_SIZE);
    int blockCount = 0;

    try {
        while (true) {
            // Ler tamanho do bloco (16 bits)
            int framesInBlock = static_cast<int>(bs.read_n_bits(16));
            if (framesInBlock <= 0 || framesInBlock > BLOCK_SIZE) break;

            // Ler coeficientes quantizados
            for (int i = 0; i < BLOCK_SIZE; i++) {
                quantizedBlock[i] = static_cast<short>(bs.read_n_bits(16));
            }

            // Dequantizar os coeficientes
            block = dequantizeDCTCoefficients(quantizedBlock);

            // Aplicar IDCT
            applyIDCT(block);

            // Escrever no arquivo WAV
            sf.writef(block.data(), framesInBlock);

            blockCount++;
            std::cout << "Decodificado bloco " << blockCount << " com " << framesInBlock << " frames\n";
        }
    } catch (...) {
        // Fim do arquivo ou erro de leitura
    }

    bs.close();
    std::cout << "Total de blocos decodificados: " << blockCount << "\n";
}
