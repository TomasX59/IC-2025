#include <iostream>
#include <fstream>
#include <vector>
#include <sndfile.hh>
#include <cmath>

#include "bit_stream.h"

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536; // buffer frames

// Binary encoding of the provided audio file
int main(int argc, char* argv[]) {
    // argument handling
    if(argc < 4) {
        cerr << "Usage: wav2bin <input.wav> <encoded_file> <bits>\n";
        return 1;
    }

    string inFile  = argv[1];
    string outFile = argv[2];
    int bits = stoi(argv[3]);

    if(bits <= 0 || bits > 16) {
        cerr << "Error: bits must be between 1 and 16\n";
        return 1;
    }

    // file input handler
    SndfileHandle sfhIn { inFile };
    if(sfhIn.error() || (sfhIn.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        cerr << "Error: invalid input WAV file (must be PCM_16)\n";
        return 1;
    }
    
    // file output handler
    fstream ofs { outFile };
    if(not ofs.is_open()) {
        cerr << "Error opening bin file " << argv[argc-1] << endl;
        return 1;
    }
    
    BitStream obs { ofs, STREAM_WRITE };

    // Quantization parameters
    int nLevels = 1 << bits; // number of levels for resolution
    int step = 65536 / nLevels; // resolution step size
    
    vector<short> samples(FRAMES_BUFFER_SIZE * sfhIn.channels());

    // Quantization + encoding loop
    size_t nFrames;
    while((nFrames = sfhIn.readf(samples.data(), FRAMES_BUFFER_SIZE))) {
        for(size_t i = 0; i < nFrames * sfhIn.channels(); ++i) {
            int s = samples[i] + 32768;         // shift to [0,65535]
            s = (s / step) * step + step/2;     // quantize
            s -= 32768;                         // shift back
            if(s > 32767) s = 32767;            // clamp
            if(s < -32768) s = -32768;
            obs.write_n_bits(static_cast<short>(s), bits);
        }
    }

    // write format channels and samplerate

    cout << "Quantization complete: " << bits << " bits of resolution used.\n";
    return 0;
}
