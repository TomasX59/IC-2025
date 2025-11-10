#include "Golomb.h"
#include <iostream>
#include <sstream>
#include <string>
#include <limits>

int main() {
    int m;
    int modeChoice;

    std::cout << "=== Golomb Coding Test ===\n";
    std::cout << "Enter parameter m (positive integer): ";
    std::cin >> m;

    std::cout << "Choose negative number mode:\n";
    std::cout << "  1 - Sign and Magnitude\n";
    std::cout << "  2 - Interleaving\n";
    std::cout << "Choice: ";
    std::cin >> modeChoice;

    NegativeMode mode;
    if (modeChoice == 1)
        mode = SIGN_MAGNITUDE;
    else
        mode = INTERLEAVED;

    Golomb coder(m, mode);

    std::cout << "\nEnter integers to encode (separated by spaces), or 'q' to quit:\n";

    // Limpa o buffer de entrada
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::string line;
    while (true) {
        std::cout << "\n> ";
        std::getline(std::cin, line);

        if (line == "q" || line == "Q") break;

        std::istringstream iss(line);
        int n;
        while (iss >> n) {
            auto bits = coder.encode(n);

            std::cout << "Original: " << n << " | Encoded bits: ";
            for (bool b : bits) std::cout << b;
            std::cout << " | ";

            size_t idx = 0;
            int decoded = coder.decode(bits, idx);
            std::cout << "Decoded: " << decoded << "\n";
        }
    }

    std::cout << "\nProgram terminated.\n";
    return 0;
}
