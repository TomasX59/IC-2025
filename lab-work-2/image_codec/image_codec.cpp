#include <opencv2/opencv.hpp>
#include <cstdint>
#include <vector>
#include <string>
#include <cstring>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>

using namespace cv;
using namespace std;

// ------------------ Bit-level writer/reader ------------------

struct BitWriter {
    std::vector<uint8_t> data;
    uint8_t cur = 0;
    int pos = 0; // number bits filled in cur (0..7)

    void push_bit(bool b) {
        if (b) cur |= (1u << (7 - pos));
        pos++;
        if (pos == 8) {
            data.push_back(cur);
            cur = 0;
            pos = 0;
        }
    }

    void push_bits_from_uint(uint32_t val, int bits) {
        for (int i = bits - 1; i >= 0; --i) push_bit( (val >> i) & 1 );
    }

    void flush() {
        if (pos != 0) {
            data.push_back(cur);
            cur = 0;
            pos = 0;
        }
    }

    void write_to_file(const string& filename) {
        flush();
        ofstream ofs(filename, ios::binary);
        ofs.write((char*)data.data(), data.size());
        ofs.close();
    }

    void append_byte(uint8_t b) {
        flush();
        data.push_back(b);
    }

    void append_bytes(const uint8_t* ptr, size_t n) {
        flush();
        data.insert(data.end(), ptr, ptr + n);
    }
};

struct BitReader {
    const uint8_t* data;
    size_t size;
    size_t index = 0; // index of next byte to read into cur
    uint8_t cur = 0;
    int pos = 8; // next bit to read is pos (0..7). pos==8 => need to load new byte

    BitReader(const uint8_t* d, size_t s): data(d), size(s) {}

    bool read_bit(bool &out) {
        if (pos == 8) {
            if (index >= size) return false;
            cur = data[index++];
            pos = 0;
        }
        out = ((cur >> (7 - pos)) & 1);
        pos++;
        return true;
    }

    bool read_bits_uint(uint32_t &out, int bits) {
        out = 0;
        for (int i = 0; i < bits; ++i) {
            bool b;
            if (!read_bit(b)) return false;
            out = (out << 1) | (b ? 1u : 0u);
        }
        return true;
    }

    void align_to_byte() {
        if (pos != 8) {
            pos = 8;
        }
    }

    size_t bytes_remaining() const {
        if (index >= size) return 0;
        return size - index + (pos == 8 ? 0 : 1);
    }
};

// ------------------ Golomb (write/read) with truncated binary ------------------

// write unary q (q ones then zero)
static void write_unary(BitWriter &bw, uint32_t q) {
    for (uint32_t i = 0; i < q; ++i) bw.push_bit(true);
    bw.push_bit(false);
}

// read unary, return q and whether success
static bool read_unary(BitReader &br, uint32_t &q) {
    q = 0;
    bool b;
    while (true) {
        if (!br.read_bit(b)) return false;
        if (!b) break;
        ++q;
    }
    return true;
}

// write truncated binary for remainder r with parameter m
static void write_truncated_binary(BitWriter &bw, uint32_t r, uint32_t m) {
    if (m == 1) return; // no remainder bits
    int b = (int)std::ceil(std::log2((double)m));
    uint32_t cutoff = (1u << b) - m;
    if (r < cutoff) {
        // write r with b-1 bits
        bw.push_bits_from_uint(r, b - 1);
    } else {
        r += cutoff;
        bw.push_bits_from_uint(r, b);
    }
}

// read truncated binary: returns r and success
static bool read_truncated_binary(BitReader &br, uint32_t &r, uint32_t m) {
    r = 0;
    if (m == 1) { r = 0; return true; }
    int b = (int)std::ceil(std::log2((double)m));
    uint32_t cutoff = (1u << b) - m;
    uint32_t tmp = 0;
    // read b-1 bits first
    if (!br.read_bits_uint(tmp, b - 1)) return false;
    if (tmp < cutoff) {
        r = tmp;
        return true;
    } else {
        // need b bits, we already read b-1 as prefix; read one more bit and reconstruct
        uint32_t suffix = 0;
        if (!br.read_bits_uint(suffix, 1)) return false;
        tmp = (tmp << 1) | suffix;
        r = tmp - cutoff;
        return true;
    }
}

// write Golomb for non-negative integer mapped using parameter m
static void write_golomb(BitWriter &bw, uint32_t mapped, uint32_t m) {
    uint32_t q = mapped / m;
    uint32_t r = mapped % m;
    write_unary(bw, q);
    write_truncated_binary(bw, r, m);
}

// read Golomb -> mapped (non-negative) using parameter m
static bool read_golomb(BitReader &br, uint32_t &mapped, uint32_t m) {
    uint32_t q;
    if (!read_unary(br, q)) return false;
    uint32_t r;
    if (!read_truncated_binary(br, r, m)) return false;
    mapped = q * m + r;
    return true;
}

// map signed residual to non-negative (interleaving)
static uint32_t map_signed(int32_t val) {
    if (val >= 0) return (uint32_t)(2 * (uint32_t)val);
    else return (uint32_t)(-2 * (int32_t)val - 1);
}

static int32_t unmap_signed(uint32_t mapped) {
    if ((mapped & 1u) == 0u) return (int32_t)(mapped / 2);
    else return - (int32_t)((mapped + 1) / 2);
}

// ------------------ Predictors ------------------

inline int clamp8(int v) { return std::max(0, std::min(255, v)); }

// compute predictors using previously reconstructed pixels (img must be accessible)
void compute_neighbors(const Mat &img, int y, int x, int &L, int &T, int &TL) {
    if (x > 0) L = img.at<uint8_t>(y, x - 1);
    else L = 0;
    if (y > 0) T = img.at<uint8_t>(y - 1, x);
    else T = 0;
    if (x > 0 && y > 0) TL = img.at<uint8_t>(y - 1, x - 1);
    else TL = 0;
}


inline int predictor_val_id(int L, int T, int TL, int &predOut) {
    int P = L + T - TL;
    int M = std::max(std::min(L, T), std::min(std::max(L, T), P)); // median of (L,T,P)
    int preds[4] = { L, T, P, M };
    for (int i = 0; i < 4; ++i) preds[i] = clamp8(preds[i]);
    // selection to be done outside using current original pixel value
    predOut = preds[0];
    return 0;
}

// ------------------ Encoding / Decoding ------------------

// choose best predictor (min abs residual) using available neighbors; returns id and residual
inline void best_predictor_and_residual(const Mat &img, int y, int x, uint8_t origPixel, int &bestId, int &residual) {
    int L, T, TL;
    if (x > 0) L = img.at<uint8_t>(y, x - 1);
    else L = 0;
    if (y > 0) T = img.at<uint8_t>(y - 1, x);
    else T = 0;
    if (x > 0 && y > 0) TL = img.at<uint8_t>(y - 1, x - 1);
    else TL = 0;

    int P = L + T - TL;
    int M = std::max(std::min(L, T), std::min(std::max(L, T), P));
    int preds[4] = { clamp8(L), clamp8(T), clamp8(P), clamp8(M) };

    int best = 0;
    int bestErr = abs((int)origPixel - preds[0]);
    for (int i = 1; i < 4; ++i) {
        int e = abs((int)origPixel - preds[i]);
        if (e < bestErr) { bestErr = e; best = i; }
    }
    bestId = best;
    residual = (int)origPixel - preds[best];
}

// Encoding function: returns bytes in BitWriter and writes header bytes directly (in bw.data)
bool encode_image_to_gimg(const Mat &img, uint32_t blocksize, const string &out_filename) {
    if (img.empty() || img.type() != CV_8UC1) {
        cerr << "Image must be grayscale 8-bit\n";
        return false;
    }
    int rows = img.rows, cols = img.cols;
    size_t npixels = (size_t)rows * (size_t)cols;

    BitWriter bw;

    // header "GIMG"
    const char magic[4] = { 'G', 'I', 'M', 'G' };
    bw.append_bytes((const uint8_t*)magic, 4);

    // width, height, blocksize as 4-byte little-endian
    auto append_u32 = [&](uint32_t v){ uint8_t b[4]; b[0]=v&0xFF; b[1]=(v>>8)&0xFF; b[2]=(v>>16)&0xFF; b[3]=(v>>24)&0xFF; bw.append_bytes(b,4); };
    append_u32((uint32_t)cols);
    append_u32((uint32_t)rows);
    append_u32((uint32_t)blocksize);


    size_t pixels_done = 0;
    while (pixels_done < npixels) {
        size_t remaining = npixels - pixels_done;
        size_t this_block = std::min((size_t)blocksize, remaining);

        // vectors for this block
        vector<uint8_t> predictor_ids; predictor_ids.reserve(this_block);
        vector<int32_t> residuals; residuals.reserve(this_block);

        // compute positions
        for (size_t k = 0; k < this_block; ++k) {
            size_t idx = pixels_done + k;
            int y = (int)(idx / cols);
            int x = (int)(idx % cols);
            uint8_t orig = img.at<uint8_t>(y, x);
            int pid, res;
            best_predictor_and_residual(img, y, x, orig, pid, res);
            predictor_ids.push_back((uint8_t)pid);
            residuals.push_back((int32_t)res);
        }

        // compute avg abs residual
        double sumabs = 0.0;
        for (auto r : residuals) sumabs += std::abs((double)r);
        double avg = (this_block>0) ? (sumabs / this_block) : 0.0;

        // heuristic choose m: a small multiple of avg (ensure >=1)
        uint32_t m = std::max<uint32_t>(1u, (uint32_t)(std::floor(avg * 1.5) + 1.0));

        // write m as uint16 little-endian into raw bytes (flush bitwriter first)
        bw.flush();
        uint8_t mb[2]; mb[0] = m & 0xFF; mb[1] = (m >> 8) & 0xFF;
        bw.append_bytes(mb, 2);

        for (size_t k = 0; k < this_block; ++k) {
            uint8_t pid = predictor_ids[k] & 0x03;
            bw.push_bits_from_uint(pid, 2);

            // map residual
            uint32_t mapped = map_signed(residuals[k]);
            write_golomb(bw, mapped, m);
        }

        pixels_done += this_block;
    }

    bw.flush();

    // write to file
    ofstream ofs(out_filename, ios::binary);
    if (!ofs) { cerr << "Cannot open output file\n"; return false; }
    ofs.write((char*)bw.data.data(), bw.data.size());
    ofs.close();
    return true;
}

// Decoding
bool decode_gimg_to_image(const string &in_filename, Mat &out_img) {
    // read entire file
    ifstream ifs(in_filename, ios::binary | ios::ate);
    if (!ifs) { cerr << "Cannot open input file\n"; return false; }
    streamsize size = ifs.tellg();
    ifs.seekg(0, ios::beg);
    vector<uint8_t> buffer((size_t)size);
    if (!ifs.read((char*)buffer.data(), size)) { cerr << "Cannot read file\n"; return false; }

    // parse header
    if (size < 16) { cerr << "File too small\n"; return false; }
    if (buffer[0] != 'G' || buffer[1] != 'I' || buffer[2] != 'M' || buffer[3] != 'G') { cerr << "Not a GIMG file\n"; return false; }
    auto read_u32 = [&](size_t off)->uint32_t {
        return (uint32_t)buffer[off] | ((uint32_t)buffer[off+1] << 8) | ((uint32_t)buffer[off+2] << 16) | ((uint32_t)buffer[off+3] << 24);
    };
    uint32_t cols = read_u32(4);
    uint32_t rows = read_u32(8);
    uint32_t blocksize = read_u32(12);

    // data pointer begins at offset 16
    const uint8_t* data_ptr = buffer.data() + 16;
    size_t data_size = buffer.size() - 16;
    BitReader br(data_ptr, data_size);

    out_img = Mat((int)rows, (int)cols, CV_8UC1);
    size_t npixels = (size_t)rows * (size_t)cols;
    size_t pixels_done = 0;

    while (pixels_done < npixels) {
        br.align_to_byte();
        if (br.index + 2 > br.size) { cerr << "Unexpected EOF reading m\n"; return false; }
        uint16_t m = (uint16_t)data_ptr[br.index] | ((uint16_t)data_ptr[br.index + 1] << 8);
        br.index += 2;

        size_t remaining = npixels - pixels_done;
        size_t this_block = std::min((size_t)blocksize, remaining);

        for (size_t k = 0; k < this_block; ++k) {
            // read 2 bits predictor id
            uint32_t pid;
            if (!br.read_bits_uint(pid, 2)) { cerr << "Unexpected EOF reading predictor id\n"; return false; }
            uint32_t mapped;
            if (!read_golomb(br, mapped, m)) { cerr << "Unexpected EOF reading golomb code\n"; return false; }
            int32_t residual = unmap_signed(mapped);

            // compute coords
            size_t idx = pixels_done + k;
            int y = (int)(idx / cols);
            int x = (int)(idx % cols);

            // compute neighbors from already reconstructed image
            int L = 0, T = 0, TL = 0;
            if (x > 0) L = out_img.at<uint8_t>(y, x - 1);
            if (y > 0) T = out_img.at<uint8_t>(y - 1, x);
            if (x > 0 && y > 0) TL = out_img.at<uint8_t>(y - 1, x - 1);
            int P = clamp8(L + T - TL);
            int M = std::max(std::min(L, T), std::min(std::max(L, T), P));
            int preds[4] = { clamp8(L), clamp8(T), P, M };
            int pred = preds[pid & 0x03];
            int pix = pred + residual;
            pix = clamp8(pix);
            out_img.at<uint8_t>(y, x) = (uint8_t)pix;
        }
        pixels_done += this_block;
    }

    return true;
}

// ------------------ Command-line interface ------------------

void print_usage() {
    cout << "Usage:\n";
    cout << "  Encode: image_codec encode input.png output.gimg [blocksize]\n";
    cout << "  Decode: image_codec decode input.gimg output.png\n";
}

int main(int argc, char** argv) {
    if (argc < 2) { print_usage(); return 0; }
    string mode = argv[1];
    if (mode == "encode") {
        if (argc < 4) { print_usage(); return 0; }
        string in = argv[2];
        string out = argv[3];
        uint32_t blocksize = 4096;
        if (argc >= 5) blocksize = (uint32_t)stoi(argv[4]);

        Mat img = imread(in, IMREAD_GRAYSCALE);
        if (img.empty()) { cerr << "Cannot read image " << in << "\n"; return -1; }
        cout << "Encoding " << in << " (" << img.cols << "x" << img.rows << "), blocksize=" << blocksize << "\n";
        if (!encode_image_to_gimg(img, blocksize, out)) { cerr << "Encode failed\n"; return -1; }
        cout << "Wrote " << out << "\n";
        return 0;
    } else if (mode == "decode") {
        if (argc < 4) { print_usage(); return 0; }
        string in = argv[2];
        string out = argv[3];
        Mat img;
        cout << "Decoding " << in << "\n";
        if (!decode_gimg_to_image(in, img)) { cerr << "Decode failed\n"; return -1; }
        imwrite(out, img);
        cout << "Wrote " << out << "\n";
        return 0;
    } else {
        print_usage();
        return 0;
    }
}
