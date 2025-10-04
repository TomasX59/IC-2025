#ifndef QUANTIZATION_H
#define QUANTIZATION_H

#include <vector>

// Quantização dos coeficientes DCT
std::vector<short> quantizeDCTCoefficients(const std::vector<short>& dctCoefficients);

// Dequantização dos coeficientes DCT
std::vector<short> dequantizeDCTCoefficients(const std::vector<short>& quantizedCoefficients);

#endif