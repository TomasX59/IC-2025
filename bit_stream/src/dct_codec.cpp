#include "dct_codec.h"

#include <sndfile.hh>

#include "bit_stream.h"
#include "quantization.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

constexpr std::size_t BLOCK_SIZE = 1024;
constexpr double PI = 3.14159265358979323846;

namespace {

void applyDCT(const std::vector<double>& input, std::vector<double>& output) {
    for (std::size_t k = 0; k < BLOCK_SIZE; ++k) {
        double sum = 0.0;
        const double blockSizeAsDouble = static_cast<double>(BLOCK_SIZE);
        const double scale = (k == 0) ? 1.0 / std::sqrt(blockSizeAsDouble)
                                      : std::sqrt(2.0 / blockSizeAsDouble);

        for (std::size_t n = 0; n < BLOCK_SIZE; ++n) {
            const double angleNumerator =
                PI * (2.0 * static_cast<double>(n) + 1.0) * static_cast<double>(k);
            sum += input[n] * std::cos(angleNumerator / (2.0 * blockSizeAsDouble));
        }

        output[k] = scale * sum;
    }
}

void applyIDCT(const std::vector<double>& input, std::vector<double>& output) {
    const double blockSizeAsDouble = static_cast<double>(BLOCK_SIZE);
    const double dcScale = 1.0 / std::sqrt(blockSizeAsDouble);
    const double acScale = std::sqrt(2.0 / blockSizeAsDouble);

    for (std::size_t n = 0; n < BLOCK_SIZE; ++n) {
        double sum = input[0] * dcScale;

        for (std::size_t k = 1; k < BLOCK_SIZE; ++k) {
            const double angleNumerator =
                PI * (2.0 * static_cast<double>(n) + 1.0) * static_cast<double>(k);
            sum += acScale * input[k] * std::cos(angleNumerator / (2.0 * blockSizeAsDouble));
        }

        output[n] = sum;
    }
}

short clampToInt16(double sample) {
    const long long rounded = std::llround(sample);
    const long long clamped = std::clamp(
        rounded,
        static_cast<long long>(std::numeric_limits<int16_t>::min()),
        static_cast<long long>(std::numeric_limits<int16_t>::max()));
    return static_cast<short>(clamped);
}

uint32_t magnitudeFromCoefficient(int32_t coef) {
    return static_cast<uint32_t>(std::abs(static_cast<long long>(coef)));
}

} // namespace

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
    fs.seekp(0); // Ir para o início do arquivo para escrita
    BitStream bs(fs, false);  // false para modo de escrita

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
    std::vector<short> readBuffer(BLOCK_SIZE * static_cast<std::size_t>(channels));
    std::vector<double> monoBlock(BLOCK_SIZE);
    std::vector<double> dctCoefficients(BLOCK_SIZE);

    sf_count_t framesRead;
    int blockCount = 0;

    while ((framesRead = sf.readf(readBuffer.data(), BLOCK_SIZE)) > 0) {
        // Converter para mono (média simples) e preparar o bloco em double
        if (channels == 2) {
            for (sf_count_t i = 0; i < framesRead; ++i) {
                const int left = static_cast<int>(readBuffer[2 * i]);
                const int right = static_cast<int>(readBuffer[2 * i + 1]);
                monoBlock[static_cast<std::size_t>(i)] = static_cast<double>((left + right) / 2);
            }
        } else {
            for (sf_count_t i = 0; i < framesRead; ++i) {
                monoBlock[static_cast<std::size_t>(i)] = static_cast<double>(readBuffer[static_cast<std::size_t>(i)]);
            }
        }

        // Zero-pad do bloco caso não esteja completo
        for (std::size_t i = static_cast<std::size_t>(framesRead); i < BLOCK_SIZE; ++i) {
            monoBlock[i] = 0.0;
        }

        // Aplicar DCT no bloco
        applyDCT(monoBlock, dctCoefficients);

        // Quantizar os coeficientes
        const auto quantizedBlock = quantizeDCTCoefficients(dctCoefficients);

        // Determinar o número de bits necessários para representar o valor absoluto máximo
        uint32_t maxMagnitude = 0;
        for (const auto coef : quantizedBlock) {
            maxMagnitude = std::max(maxMagnitude, magnitudeFromCoefficient(coef));
        }
        const uint8_t magnitudeBits =
            (maxMagnitude == 0) ? 0 : static_cast<uint8_t>(std::bit_width(maxMagnitude));

        // Escrever o tamanho do bloco (16 bits) e os bits dedicados à magnitude (6 bits)
        bs.write_n_bits(static_cast<uint64_t>(framesRead), 16);
        bs.write_n_bits(static_cast<uint64_t>(magnitudeBits), 6);

        // Escrever os coeficientes quantizados (bit de sinal + magnitude)
        for (const auto coef : quantizedBlock) {
            const bool isNegative = coef < 0;
            bs.write_bit(isNegative ? 1 : 0);

            if (magnitudeBits > 0) {
                const uint32_t magnitude =
                    isNegative ? static_cast<uint32_t>(-static_cast<long long>(coef))
                               : static_cast<uint32_t>(coef);
                bs.write_n_bits(static_cast<uint64_t>(magnitude), magnitudeBits);
            }
        }

        blockCount++;
        if (blockCount <= 5 || blockCount % 200 == 0) {
            std::cout << "Processado bloco " << blockCount << " com " << framesRead << " frames\n";
        }
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
    fs.seekg(0); // Ir para o início do arquivo para leitura
    BitStream bs(fs, true);  // true para modo de leitura

    std::cout << "\nIniciando decodificação...\n";
    std::cout << "Lendo cabeçalho do arquivo...\n";

    // Ler cabeçalho
    // 1. Sample rate (32 bits)
    int sampleRate = static_cast<int>(bs.read_n_bits(32));
    // 2. Número total de frames (32 bits)
    sf_count_t totalFrames = static_cast<sf_count_t>(bs.read_n_bits(32));
    // 3. Tamanho do bloco (16 bits)
    int blockSize = static_cast<int>(bs.read_n_bits(16));

    std::cout << "Informações do arquivo:\n";
    std::cout << "Sample rate: " << sampleRate << " Hz\n";
    std::cout << "Total frames: " << totalFrames << "\n";
    std::cout << "Tamanho do bloco: " << blockSize << "\n";

    if (blockSize != BLOCK_SIZE) {
        throw std::runtime_error("Tamanho do bloco incompatível");
    }

    // Criar arquivo WAV de saída
    SndfileHandle sf(outputWav, SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 1, sampleRate);
    if (sf.error()) {
        throw std::runtime_error("Erro ao criar arquivo WAV: " + outputWav);
    }

    std::vector<int32_t> quantizedBlock(BLOCK_SIZE);
    std::vector<double> spectralBlock(BLOCK_SIZE);
    std::vector<double> timeDomainBlock(BLOCK_SIZE);
    std::vector<short> pcmBlock(BLOCK_SIZE);

    int blockCount = 0;
    sf_count_t totalFramesProcessed = 0;

    try {
        std::cout << "\nIniciando leitura dos blocos...\n";
        while (totalFramesProcessed < totalFrames) {
            const int framesInBlock = static_cast<int>(bs.read_n_bits(16));
            if (framesInBlock <= 0 || framesInBlock > static_cast<int>(BLOCK_SIZE)) {
                throw std::runtime_error("Tamanho de bloco inválido ou corrompido no fluxo codificado");
            }

            const uint8_t magnitudeBits = static_cast<uint8_t>(bs.read_n_bits(6));
            if (magnitudeBits > 32) {
                throw std::runtime_error("Número de bits da magnitude inválido no fluxo codificado");
            }

            for (std::size_t i = 0; i < BLOCK_SIZE; ++i) {
                const uint64_t signBit = bs.read_n_bits(1);
                int32_t value = 0;

                if (magnitudeBits > 0) {
                    const uint64_t magnitude = bs.read_n_bits(magnitudeBits);
                    if (magnitude > static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
                        throw std::runtime_error("Magnitude de coeficiente excede o intervalo suportado");
                    }

                    value = signBit ? -static_cast<int32_t>(magnitude)
                                    : static_cast<int32_t>(magnitude);
                }

                quantizedBlock[i] = value;
            }

            spectralBlock = dequantizeDCTCoefficients(quantizedBlock);
            applyIDCT(spectralBlock, timeDomainBlock);

            for (std::size_t i = 0; i < BLOCK_SIZE; ++i) {
                pcmBlock[i] = clampToInt16(timeDomainBlock[i]);
            }

            sf.writef(pcmBlock.data(), framesInBlock);
            totalFramesProcessed += framesInBlock;
            blockCount++;
        }
    } catch (const std::exception& e) {
        std::cout << "Erro durante a leitura: " << e.what() << "\n";
        throw;
    }

    std::cout << "\nResumo da decodificação:\n";
    std::cout << "Total de blocos processados: " << blockCount << "\n";
    std::cout << "Total de frames processados: " << totalFramesProcessed << "\n";
    std::cout << "Frames esperados: " << totalFrames << "\n";

    bs.close();
    std::cout << "Total de blocos decodificados: " << blockCount << "\n";
}
