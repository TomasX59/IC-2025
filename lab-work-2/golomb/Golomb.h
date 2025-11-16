#ifndef GOLOMB_H
#define GOLOMB_H

#include <vector>
#include <cstddef>
#include <cstdint>

enum NegativeMode { SIGN_MAGNITUDE, INTERLEAVED };

class Golomb {
private:
    uint32_t m;
    NegativeMode mode;

    uint64_t mapNumber(int value) const;
    int64_t unmapNumber(uint64_t mapped, bool negativeFlag) const;

public:
    Golomb(int m, NegativeMode mode = INTERLEAVED);

    std::vector<bool> encode(int n) const;
    int decode(const std::vector<bool>& bits, size_t& index) const;
};

#endif
