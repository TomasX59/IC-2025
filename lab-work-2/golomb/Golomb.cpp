#include "Golomb.h"

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {
int ceilLog2(uint32_t value) {
    if (value <= 1) return 0;

    int bits = 0;
    uint64_t power = 1ull;
    while (power < value) {
        power <<= 1;
        ++bits;
    }
    return bits;
}
} // namespace

Golomb::Golomb(int m, NegativeMode mode) : m(static_cast<uint32_t>(m)), mode(mode) {
    if (m <= 0)
        throw std::invalid_argument("Golomb parameter m must be positive");
}

uint64_t Golomb::mapNumber(int value) const {
    int64_t signedValue = static_cast<int64_t>(value);

    if (mode == SIGN_MAGNITUDE) {
        return static_cast<uint64_t>(signedValue < 0 ? -signedValue : signedValue);
    }

    if (signedValue >= 0) {
        return static_cast<uint64_t>(signedValue) * 2ull;
    }
    return static_cast<uint64_t>(-signedValue) * 2ull - 1ull;
}

int64_t Golomb::unmapNumber(uint64_t mapped, bool negativeFlag) const {
    if (mode == SIGN_MAGNITUDE) {
        int64_t magnitude = static_cast<int64_t>(mapped);
        return negativeFlag ? -magnitude : magnitude;
    }

    if ((mapped & 1ull) == 0ull) {
        return static_cast<int64_t>(mapped / 2ull);
    }

    uint64_t magnitude = (mapped + 1ull) / 2ull;
    return -static_cast<int64_t>(magnitude);
}

std::vector<bool> Golomb::encode(int n) const {
    std::vector<bool> bits;

    bool negative = (mode == SIGN_MAGNITUDE) && (n < 0);
    if (mode == SIGN_MAGNITUDE) bits.push_back(negative);

    uint64_t mapped = mapNumber(n);
    uint64_t q = mapped / m;
    uint32_t r = static_cast<uint32_t>(mapped % m);

    for (uint64_t i = 0; i < q; ++i) bits.push_back(true);
    bits.push_back(false);

    if (m > 1) {
        int b = ceilLog2(m);
        uint64_t base = 1ull << b;
        uint32_t cutoff = static_cast<uint32_t>(base - m);

        if (r < cutoff) {
            for (int i = b - 2; i >= 0; --i) bits.push_back((r >> i) & 1);
        } else {
            r += cutoff;
            for (int i = b - 1; i >= 0; --i) bits.push_back((r >> i) & 1);
        }
    }

    return bits;
}

int Golomb::decode(const std::vector<bool>& bits, size_t& index) const {
    if (index >= bits.size())
        throw std::invalid_argument("Golomb decode: empty bit sequence");

    bool negative = false;
    if (mode == SIGN_MAGNITUDE) {
        negative = bits[index++];
    }

    uint64_t q = 0;
    while (true) {
        if (index >= bits.size()) throw std::invalid_argument("Golomb decode: missing unary terminator");
        if (!bits[index]) {
            ++index;
            break;
        }
        ++q;
        ++index;
    }

    uint64_t r = 0;
    if (m > 1) {
        int b = ceilLog2(m);
        uint64_t base = 1ull << b;
        uint32_t cutoff = static_cast<uint32_t>(base - m);

        uint32_t prefix = 0;
        for (int i = 0; i < b - 1; ++i) {
            if (index >= bits.size())
                throw std::invalid_argument("Golomb decode: truncated remainder");
            prefix = (prefix << 1) | (bits[index] ? 1u : 0u);
            ++index;
        }

        if (prefix < cutoff) {
            r = prefix;
        } else {
            if (index >= bits.size())
                throw std::invalid_argument("Golomb decode: truncated remainder");
            prefix = (prefix << 1) | (bits[index] ? 1u : 0u);
            ++index;
            r = prefix - cutoff;
        }
    }

    uint64_t mapped = q * static_cast<uint64_t>(m) + r;
    int64_t value = unmapNumber(mapped, negative);

    if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
        throw std::overflow_error("Golomb decode: decoded value out of int range");

    return static_cast<int>(value);
}
