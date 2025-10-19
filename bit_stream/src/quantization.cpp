#include "quantization.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

// Tabela simples de fatores de quantização para os primeiros coeficientes.
// Os restantes usam o último valor.
const std::vector<double> kQuantizationTable = {
    4.0,   4.0,   8.0,    8.0,    16.0,   16.0,   32.0,   32.0,
    64.0,  64.0,  128.0,  128.0,  256.0,  256.0,  512.0,  512.0
};

double quantization_step_for_index(std::size_t idx) {
    return (idx < kQuantizationTable.size())
               ? kQuantizationTable[idx]
               : kQuantizationTable.back();
}


std::vector<int32_t> quantizeDCTCoefficients(const std::vector<double>& dctCoefficients) {
    std::vector<int32_t> quantizedCoefficients(dctCoefficients.size());

    for (std::size_t i = 0; i < dctCoefficients.size(); ++i) {
        const double step = quantization_step_for_index(i);
        const long long rawValue = std::llround(dctCoefficients[i] / step);
        const long long clamped = std::clamp(
            rawValue,
            static_cast<long long>(std::numeric_limits<int32_t>::min()),
            static_cast<long long>(std::numeric_limits<int32_t>::max()));
        quantizedCoefficients[i] = static_cast<int32_t>(clamped);
    }

    return quantizedCoefficients;
}

std::vector<double> dequantizeDCTCoefficients(const std::vector<int32_t>& quantizedCoefficients) {
    std::vector<double> dequantizedCoefficients(quantizedCoefficients.size());

    for (std::size_t i = 0; i < quantizedCoefficients.size(); ++i) {
        const double step = quantization_step_for_index(i);
        dequantizedCoefficients[i] = static_cast<double>(quantizedCoefficients[i]) * step;
    }

    return dequantizedCoefficients;
}

