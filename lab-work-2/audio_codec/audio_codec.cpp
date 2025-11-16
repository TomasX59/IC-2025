#include "Golomb.h"

#include <sndfile.hh>

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {

template <typename T>
void write_le(std::ostream &stream, T value) {
    using UIntType = typename std::make_unsigned<T>::type;
    UIntType uvalue = static_cast<UIntType>(value);
    for (size_t i = 0; i < sizeof(T); ++i) {
        stream.put(static_cast<char>((uvalue >> (i * 8)) & 0xFF));
    }
}

template <typename T>
T read_le(std::istream &stream) {
    using UIntType = typename std::make_unsigned<T>::type;
    UIntType result = 0;
    for (size_t i = 0; i < sizeof(T); ++i) {
        int byte = stream.get();
        if (byte == EOF) {
            throw std::runtime_error("Unexpected end of file while reading header");
        }
        result |= static_cast<UIntType>(static_cast<unsigned char>(byte)) << (i * 8);
    }
    return static_cast<T>(result);
}

inline uint32_t compute_adaptive_m(uint64_t sum_abs, size_t count) {
    if (count == 0) return 1;
    double average = static_cast<double>(sum_abs) / static_cast<double>(count);
    uint32_t chosen = static_cast<uint32_t>(std::max(1.0, std::round(average)));
    return std::min<uint32_t>(chosen, 65535u);
}

inline int16_t clamp_to_int16(int32_t value) {
    if (value < std::numeric_limits<int16_t>::min()) {
        return std::numeric_limits<int16_t>::min();
    }
    if (value > std::numeric_limits<int16_t>::max()) {
        return std::numeric_limits<int16_t>::max();
    }
    return static_cast<int16_t>(value);
}

std::vector<uint8_t> pack_bits(const std::vector<bool> &bits) {
    if (bits.empty()) return {};
    const size_t byte_count = (bits.size() + 7) / 8;
    std::vector<uint8_t> bytes(byte_count, 0);
    for (size_t i = 0; i < bits.size(); ++i) {
        if (bits[i]) {
            bytes[i / 8] |= static_cast<uint8_t>(1u << (7 - (i % 8)));
        }
    }
    return bytes;
}

std::vector<bool> unpack_bits(const std::vector<uint8_t> &bytes, size_t bit_count) {
    std::vector<bool> bits(bit_count);
    for (size_t i = 0; i < bit_count; ++i) {
        uint8_t byte = bytes[i / 8];
        bits[i] = (byte >> (7 - (i % 8))) & 1u;
    }
    return bits;
}

int parse_positive_int(const char *token, const char *name) {
    char *end = nullptr;
    long value = std::strtol(token, &end, 10);
    if (*token == '\0' || *end != '\0' || value <= 0) {
        throw std::invalid_argument(std::string("Invalid value for ") + name + ": " + token);
    }
    return static_cast<int>(value);
}

void encode_audio(const std::string &input_wav, const std::string &output_bin,
                  uint16_t block_size, const std::optional<int> &fixed_m) {
    SndfileHandle reader(input_wav);
    if (reader.error()) {
        throw std::runtime_error("Failed to open WAV file: " + input_wav);
    }
    if ((reader.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        throw std::runtime_error("Only PCM_16 WAV files are supported");
    }
    int channels = reader.channels();
    if (channels != 1 && channels != 2) {
        throw std::runtime_error("Codec supports only mono or stereo WAV files");
    }

    std::ofstream out(output_bin, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Cannot create output file: " + output_bin);
    }

    const char magic[4] = {'G', 'A', 'C', '1'};
    out.write(magic, 4);
    write_le<uint8_t>(out, 1); // version
    write_le<uint8_t>(out, static_cast<uint8_t>(channels));
    write_le<uint8_t>(out, 16); // sample width bits
    write_le<uint32_t>(out, static_cast<uint32_t>(reader.samplerate()));
    write_le<uint32_t>(out, static_cast<uint32_t>(reader.frames()));
    write_le<uint16_t>(out, block_size);

    std::vector<short> buffer(static_cast<size_t>(block_size) * channels);
    std::vector<int32_t> previous(channels, 0);
    sf_count_t frames_read = 0;

    while ((frames_read = reader.readf(buffer.data(), block_size)) > 0) {
        const size_t frames = static_cast<size_t>(frames_read);
        std::vector<int32_t> residuals;
        residuals.reserve(frames * channels);
        std::vector<bool> predictor_bits;
        uint64_t sum_abs = 0;

        for (size_t f = 0; f < frames; ++f) {
            size_t offset = f * channels;
            int32_t left = static_cast<int32_t>(buffer[offset]);
            int32_t res_left = left - previous[0];
            residuals.push_back(res_left);
            sum_abs += static_cast<uint64_t>(std::abs(static_cast<int64_t>(res_left)));
            previous[0] = left;

            if (channels == 2) {
                int32_t right = static_cast<int32_t>(buffer[offset + 1]);
                int32_t temp_res = right - previous[1];
                int32_t inter_res = right - left;
                bool use_inter = std::llabs(static_cast<long long>(inter_res)) <
                                 std::llabs(static_cast<long long>(temp_res));
                int32_t chosen = use_inter ? inter_res : temp_res;
                predictor_bits.push_back(use_inter);
                residuals.push_back(chosen);
                sum_abs += static_cast<uint64_t>(std::abs(static_cast<int64_t>(chosen)));
                previous[1] = right;
            }
        }

        const uint32_t adaptive_m = compute_adaptive_m(sum_abs, residuals.size());
        uint32_t m_value = fixed_m.has_value()
            ? static_cast<uint32_t>(std::clamp(fixed_m.value(), 1, 65535))
            : adaptive_m;

        Golomb coder(static_cast<int>(m_value));
        std::vector<bool> golomb_bits;
        golomb_bits.reserve(residuals.size() * 10);
        for (int32_t value : residuals) {
            auto bits = coder.encode(value);
            golomb_bits.insert(golomb_bits.end(), bits.begin(), bits.end());
        }

        const uint16_t predictor_bytes =
            channels == 2 ? static_cast<uint16_t>((predictor_bits.size() + 7) / 8) : 0;
        const auto predictor_payload = pack_bits(predictor_bits);
        const uint32_t bit_count = static_cast<uint32_t>(golomb_bits.size());
        const auto golomb_payload = pack_bits(golomb_bits);

        write_le<uint16_t>(out, static_cast<uint16_t>(frames));
        write_le<uint16_t>(out, static_cast<uint16_t>(m_value));
        if (channels == 2) {
            write_le<uint16_t>(out, predictor_bytes);
            if (!predictor_payload.empty()) {
                out.write(reinterpret_cast<const char *>(predictor_payload.data()), predictor_payload.size());
            }
        }
        write_le<uint32_t>(out, bit_count);
        if (!golomb_payload.empty()) {
            out.write(reinterpret_cast<const char *>(golomb_payload.data()), golomb_payload.size());
        }
    }
}

void decode_audio(const std::string &input_bin, const std::string &output_wav) {
    std::ifstream in(input_bin, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot open encoded file: " + input_bin);
    }

    char magic[4];
    in.read(magic, 4);
    if (in.gcount() != 4 || std::strncmp(magic, "GAC1", 4) != 0) {
        throw std::runtime_error("Input file is not a Golomb audio codec stream");
    }

    const uint8_t version = read_le<uint8_t>(in);
    if (version != 1) {
        throw std::runtime_error("Unsupported codec version");
    }

    const uint8_t channels = read_le<uint8_t>(in);
    const uint8_t sample_width = read_le<uint8_t>(in);
    const uint32_t sample_rate = read_le<uint32_t>(in);
    const uint32_t total_frames = read_le<uint32_t>(in);
    const uint16_t block_size = read_le<uint16_t>(in);

    if (channels != 1 && channels != 2) {
        throw std::runtime_error("Codec file contains unsupported number of channels");
    }
    if (sample_width != 16) {
        throw std::runtime_error("Codec assumes 16-bit PCM samples");
    }

    SndfileHandle writer(output_wav, SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_PCM_16,
                        channels, sample_rate);
    if (writer.error()) {
        throw std::runtime_error("Failed to create WAV file: " + output_wav);
    }

    std::vector<int32_t> previous(channels, 0);
    uint32_t frames_decoded = 0;

    while (frames_decoded < total_frames) {
        const uint16_t frames_in_block = read_le<uint16_t>(in);
        const uint16_t m_value = read_le<uint16_t>(in);
        std::vector<bool> predictor_bits;
        if (channels == 2) {
            const uint16_t predictor_bytes = read_le<uint16_t>(in);
            std::vector<uint8_t> predictor_payload(predictor_bytes);
            if (predictor_bytes > 0) {
                in.read(reinterpret_cast<char *>(predictor_payload.data()), predictor_bytes);
                if (in.gcount() != predictor_bytes) {
                    throw std::runtime_error("Truncated predictor stream");
                }
            }
            predictor_bits = unpack_bits(predictor_payload, frames_in_block);
        }

        const uint32_t bit_count = read_le<uint32_t>(in);
        const size_t byte_count = (bit_count + 7) / 8;
        std::vector<uint8_t> golomb_payload(byte_count);
        if (byte_count > 0) {
            in.read(reinterpret_cast<char *>(golomb_payload.data()), byte_count);
            if (static_cast<size_t>(in.gcount()) != byte_count) {
                throw std::runtime_error("Truncated Golomb bitstream");
            }
        }

        const std::vector<bool> golomb_bits = unpack_bits(golomb_payload, bit_count);
        Golomb coder(static_cast<int>(m_value));
        size_t bit_index = 0;
        std::vector<short> output_buffer(static_cast<size_t>(frames_in_block) * channels);

        for (size_t f = 0; f < frames_in_block; ++f) {
            size_t offset = f * channels;
            int32_t left_residual = coder.decode(golomb_bits, bit_index);
            int32_t sample_left = previous[0] + left_residual;
            previous[0] = sample_left;
            output_buffer[offset] = clamp_to_int16(sample_left);

            if (channels == 2) {
                bool use_inter = predictor_bits[f];
                int32_t right_residual = coder.decode(golomb_bits, bit_index);
                int32_t predictor = use_inter ? sample_left : previous[1];
                int32_t sample_right = predictor + right_residual;
                previous[1] = sample_right;
                output_buffer[offset + 1] = clamp_to_int16(sample_right);
            }
        }

        writer.writef(output_buffer.data(), frames_in_block);
        frames_decoded += frames_in_block;
    }
}

void print_usage(const char *binary_name) {
    std::cout << "Usage:\n";
    std::cout << "  " << binary_name << " encode <input.wav> <output.gac> [--block-size N] [--fixed-m M]\n";
    std::cout << "  " << binary_name << " decode <input.gac> <output.wav>\n";
    std::cout << "Defaults: block size 1024, adaptive Golomb parameter.\n";
}

} // namespace

int main(int argc, char *argv[]) {
    try {
        if (argc < 4) {
            print_usage(argv[0]);
            return 1;
        }

        const std::string command = argv[1];
        if (command == "encode") {
            if (argc < 4) {
                print_usage(argv[0]);
                return 1;
            }
            const std::string input_wav = argv[2];
            const std::string output_bin = argv[3];
            uint16_t block_size = 1024;
            std::optional<int> fixed_m;

            for (int i = 4; i < argc; ++i) {
                const std::string token = argv[i];
                if (token == "--block-size") {
                    if (i + 1 >= argc) {
                        throw std::invalid_argument("Missing value for --block-size");
                    }
                    int parsed = parse_positive_int(argv[++i], "block-size");
                    if (parsed > std::numeric_limits<uint16_t>::max()) {
                        throw std::invalid_argument("Block size cannot exceed 65535");
                    }
                    block_size = static_cast<uint16_t>(parsed);
                } else if (token == "--fixed-m") {
                    if (i + 1 >= argc) {
                        throw std::invalid_argument("Missing value for --fixed-m");
                    }
                    fixed_m = parse_positive_int(argv[++i], "fixed-m");
                } else {
                    throw std::invalid_argument("Unknown option: " + token);
                }
            }

            encode_audio(input_wav, output_bin, block_size, fixed_m);
        } else if (command == "decode") {
            if (argc != 4) {
                print_usage(argv[0]);
                return 1;
            }
            decode_audio(argv[2], argv[3]);
        } else {
            print_usage(argv[0]);
            return 1;
        }
    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
