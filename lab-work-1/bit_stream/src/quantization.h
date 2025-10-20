#ifndef QUANTIZATION_H
#define QUANTIZATION_H

#include <cstdint>
#include <vector>

// Quantização dos coeficientes DCT
std::vector<int32_t> quantizeDCTCoefficients(const std::vector<double>& dctCoefficients);

// Dequantização dos coeficientes DCT
std::vector<double> dequantizeDCTCoefficients(const std::vector<int32_t>& quantizedCoefficients);

#endif
