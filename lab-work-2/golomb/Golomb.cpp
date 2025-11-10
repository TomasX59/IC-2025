#include "Golomb.h"
#include <cmath>
#include <stdexcept>

// Constructor
Golomb::Golomb(int m, NegativeMode mode) : m(m), mode(mode) {
    if (m <= 0) throw std::invalid_argument("Parameter m must be positive");
}

// --- Mapping for negatives ---
int Golomb::mapNumber(int n) const {
    if (mode == SIGN_MAGNITUDE) {
        return std::abs(n);
    } else { // INTERLEAVED
        if (n >= 0) return 2 * n;
        else return -2 * n - 1;
    }
}

int Golomb::unmapNumber(int n) const {
    if (mode == SIGN_MAGNITUDE) {
        return n;
    } else {
        if (n % 2 == 0) return n / 2;
        else return -(n + 1) / 2;
    }
}

// --- Encoding ---
std::vector<bool> Golomb::encode(int n) const {
    std::vector<bool> bits;

    bool negative = false;
    if (mode == SIGN_MAGNITUDE && n < 0) {
        negative = true;
        n = -n;
    }

    int mapped = mapNumber(n);

    int q = mapped / m;
    int r = mapped % m;

    // Unary code for q: q times '1' followed by '0'
    for (int i = 0; i < q; i++) bits.push_back(true);
    bits.push_back(false);

    // Truncated binary code for r
    int b = std::ceil(std::log2(m));
    int cutoff = (1 << b) - m;

    if (r < cutoff) {
        // Represent r with b-1 bits
        for (int i = b - 2; i >= 0; i--) bits.push_back((r >> i) & 1);
    } else {
        r += cutoff;
        for (int i = b - 1; i >= 0; i--) bits.push_back((r >> i) & 1);
    }

    // Add sign bit if using sign-magnitude
    if (mode == SIGN_MAGNITUDE) bits.insert(bits.begin(), negative);

    return bits;
}

// --- Decoding ---
int Golomb::decode(const std::vector<bool>& bits, size_t& index) const {
    bool negative = false;
    if (mode == SIGN_MAGNITUDE) {
        negative = bits[index++];
    }

    // Decode unary part (count 1s until 0)
    int q = 0;
    while (index < bits.size() && bits[index]) {
        q++;
        index++;
    }
    index++; // skip the 0

    // Decode remainder
    int b = std::ceil(std::log2(m));
    int cutoff = (1 << b) - m;

    int r = 0;
    int temp = 0;
    for (int i = 0; i < b - 1 && index + i < bits.size(); i++)
        temp = (temp << 1) | bits[index + i];

    if (temp < cutoff) {
        r = temp;
        index += b - 1;
    } else {
        temp = 0;
        for (int i = 0; i < b && index + i < bits.size(); i++)
            temp = (temp << 1) | bits[index + i];
        r = temp - cutoff;
        index += b;
    }

    int mapped = q * m + r;
    int value = unmapNumber(mapped);

    if (mode == SIGN_MAGNITUDE && negative)
        value = -value;

    return value;
}
