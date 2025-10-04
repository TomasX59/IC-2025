#include "quantization.h"
#include <vector>
#include <cmath>

// Tabela de quantização - os valores aumentam com a frequência para dar mais peso às baixas frequências
const std::vector<short> quantizationTable = {
    4, 4, 8, 8, 16, 16, 32, 32, 64, 64, 128, 128, 256, 256, 512, 512
};

std::vector<short> quantizeDCTCoefficients(const std::vector<short>& dctCoefficients) {
    std::vector<short> quantizedCoefficients(dctCoefficients.size());
    
    for (size_t i = 0; i < dctCoefficients.size(); i++) {
        // Usar o valor da tabela de quantização (repetir o último valor se necessário)
        short qValue = (i < quantizationTable.size()) ? 
                      quantizationTable[i] : 
                      quantizationTable.back();
                      
        quantizedCoefficients[i] = dctCoefficients[i] / qValue;
    }
    
    return quantizedCoefficients;
}

std::vector<short> dequantizeDCTCoefficients(const std::vector<short>& quantizedCoefficients) {
    std::vector<short> dequantizedCoefficients(quantizedCoefficients.size());
    
    for (size_t i = 0; i < quantizedCoefficients.size(); i++) {
        // Usar o valor da tabela de quantização (repetir o último valor se necessário)
        short qValue = (i < quantizationTable.size()) ? 
                      quantizationTable[i] : 
                      quantizationTable.back();
                      
        dequantizedCoefficients[i] = quantizedCoefficients[i] * qValue;
    }
    
    return dequantizedCoefficients;
}
