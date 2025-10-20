#ifndef DCT_CODEC_H
#define DCT_CODEC_H

#include <vector>
#include <string>

void encodeWav(const std::string &inputWav, const std::string &outputFile);
void decodeWav(const std::string &inputFile, const std::string &outputWav);

#endif
