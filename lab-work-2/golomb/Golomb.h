#ifndef GOLOMB_H
#define GOLOMB_H

#include <vector>
#include <cstddef>

enum NegativeMode { SIGN_MAGNITUDE, INTERLEAVED };

class Golomb {
private:
    int m;
    NegativeMode mode;

    int mapNumber(int n) const;
    int unmapNumber(int n) const;

public:
    Golomb(int m, NegativeMode mode = INTERLEAVED);

    std::vector<bool> encode(int n) const;
    int decode(const std::vector<bool>& bits, size_t& index) const;
};

#endif
