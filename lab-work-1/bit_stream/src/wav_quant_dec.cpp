#include <iostream>
#include <fstream>
#include <vector>
#include <sndfile.hh>
#include <cmath>

#include "bit_stream.h"

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536; // buffer frames

int main(int argc, char* argv[]) {
      // argument handling
    if(argc < 3) {
        cerr << "Usage: bin2wav <input.wav> <encoded_file> <bits> \n";
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
    fstream ifs { inFile, ios::in | ios::binary };
    if(not ifs.is_open()) {
      cerr << "Error opening bin file " << inFile << endl;
      return 1;
    }

    // audio file input handler
    SndfileHandle sfhIn { outFile };
    if(sfhIn.error() || (sfhIn.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        cerr << "Error: invalid input WAV file (must be PCM_16)\n";
        return 1;
    }

    // file output handler
    SndfileHandle sfhOut { outFile, SFM_WRITE, sfhIn.format(), sfhIn.channels(), sfhIn.samplerate() };
    if(sfhOut.error()) {
        cerr << "Error: cannot create output file\n" << sfhOut.error();
        return 1;
    }
    
	  BitStream ibs { ifs, STREAM_READ };
    // vector<short> samples(FRAMES_BUFFER_SIZE * nChannels);
    vector<short> samples(FRAMES_BUFFER_SIZE);

    int c;
    int i = 0;
    while ((c = ibs.read_n_bits(bits)) != EOF)
    {
      samples[i] = c;
      sfhOut.write(&c, i);
      i++;
    }

    sfhOut.writef(samples.data(), i);

    cout << "Decodification complete: " << bits << " bits of resolution used.\n";
    return 0;
}

