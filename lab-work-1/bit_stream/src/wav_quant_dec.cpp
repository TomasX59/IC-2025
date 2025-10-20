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

    SF_INFO sfinfo ;
    sfinfo.channels = 1;
    sfinfo.samplerate = 44100;
    sfinfo.format = SF_FORMAT_WAV;

    // file output handler
    SNDFILE* outfile = sf_open(argv[2], SFM_WRITE, &sfinfo);
    if(sf_error(outfile)) {
        cerr << "Error: cannot create output file\n" << sf_error(outfile);
        return 1;
    }

    
	  BitStream ibs { ifs, STREAM_READ };
    // vector<short> samples(FRAMES_BUFFER_SIZE * nChannels);
    vector<short> samples(FRAMES_BUFFER_SIZE);
    sf_count_t count = ibs.tell();

    int c;
    int i = 0;
    while ((c = ibs.read_n_bits(bits)) != EOF)
    {
      samples[i] = c;
      i++;
    }

    sf_writef_short(outfile, samples.data(), count);
    
    cout << "Decodification complete: " << bits << " bits of resolution used.\n";
    return 0;
}
