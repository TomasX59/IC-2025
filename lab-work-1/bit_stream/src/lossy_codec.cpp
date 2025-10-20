#include "dct_codec.h"
#include <iostream>
#include <stdexcept>

int main(int argc, char *argv[]) {
    try {
        if (argc != 4 || (argv[1][0] != 'e' && argv[1][0] != 'd')) {
            std::cerr << "Uso: " << argv[0] << " <e|d> <arquivo_entrada> <arquivo_saida>\n";
            std::cerr << "  e: codificar WAV para arquivo comprimido\n";
            std::cerr << "  d: decodificar arquivo comprimido para WAV\n";
            return 1;
        }

        if (argv[1][0] == 'e') {
            // Modo de codificação
            encodeWav(argv[2], argv[3]);
            std::cout << "Arquivo WAV codificado com sucesso para " << argv[3] << std::endl;
        } else {
            // Modo de decodificação
            decodeWav(argv[2], argv[3]);
            std::cout << "Arquivo decodificado com sucesso para " << argv[3] << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Erro: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
