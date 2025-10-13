#include <iostream>
#include <vector>
#include <sndfile.hh>
#include <cmath>

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536; // buffer frames

int main(int argc, char* argv[]) {
    if(argc < 4) {
        cerr << "Usage: wav_quant <input.wav> <output.wav> <bits>\n";
        return 1;
    }

    string inFile  = argv[1];
    string outFile = argv[2];
    int bits = stoi(argv[3]);

    if(bits <= 0 || bits > 16) {
        cerr << "Error: bits must be between 1 and 16\n";
        return 1;
    }

    SndfileHandle sfhIn { inFile };
    if(sfhIn.error() || (sfhIn.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        cerr << "Error: invalid input WAV file (must be PCM_16)\n";
        return 1;
    }

    SndfileHandle sfhOut { outFile, SFM_WRITE, sfhIn.format(), sfhIn.channels(), sfhIn.samplerate() };
    if(sfhOut.error()) {
        cerr << "Error: cannot create output file\n";
        return 1;
    }

    // Quantization parameters
    int nLevels = 1 << bits;
    int step = 65536 / nLevels; // step size
    vector<short> samples(FRAMES_BUFFER_SIZE * sfhIn.channels());

    size_t nFrames;
    while((nFrames = sfhIn.readf(samples.data(), FRAMES_BUFFER_SIZE))) {
        for(size_t i = 0; i < nFrames * sfhIn.channels(); ++i) {
            int s = samples[i] + 32768;         // shift to [0,65535]
            s = (s / step) * step + step/2;     // quantize
            s -= 32768;                         // shift back
            if(s > 32767) s = 32767;            // clamp
            if(s < -32768) s = -32768;
            samples[i] = static_cast<short>(s);
        }
        sfhOut.writef(samples.data(), nFrames);
    }

    cout << "Quantization complete: " << bits << " bits used.\n";
    return 0;
}
