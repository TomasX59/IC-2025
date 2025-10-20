//------------------------------------------------------------------------------
//
// Copyright 2025 University of Aveiro, Portugal, All Rights Reserved.
//
// These programs are supplied free of charge for research purposes only,
// and may not be sold or incorporated into any commercial product. There is
// ABSOLUTELY NO WARRANTY of any sort, nor any undertaking that they are
// fit for ANY PURPOSE WHATSOEVER. Use them at your own risk. If you do
// happen to find a bug, or have modifications to suggest, please report
// the same to Armando J. Pinho, ap@ua.pt. The copyright notice above
// and this statement of conditions must remain an integral part of each
// and every copy made of these files.
//
// Armando J. Pinho (ap@ua.pt)
// IEETA / DETI / University of Aveiro
//
#ifndef WAVHIST_H
#define WAVHIST_H

#include <iostream>
#include <vector>
#include <map>
#include <sndfile.hh>
#include <cmath>

static const size_t HISTOGRAM_BIN_POWER = 0; 

class WAVHist {
private:
    std::vector<std::map<int, size_t>> counts; // one map per channel (L, R, MID, SIDE)
    bool stereo;
    const int binSize; 

    // Proper floor division for negatives
    int binIndex(short value) const {
        int q = value / binSize;
        int r = value % binSize;
        if ((r != 0) && ((r < 0) != (binSize < 0))) q -= 1;
        return q;
    }

public:
    WAVHist(const SndfileHandle& sfh)
        : stereo(sfh.channels() == 2),
          binSize(1 << HISTOGRAM_BIN_POWER)
    {
        counts.resize(stereo ? 4 : 1); // mono = 1, stereo = 4 (L, R, MID, SIDE)
    }

    void update(const std::vector<short>& samples) {
        size_t nChannels = stereo ? 2 : 1;
        size_t nFrames = samples.size() / nChannels;

        for (size_t i = 0; i < nFrames; ++i) {
            short left = samples[i * nChannels];
            counts[0][binIndex(left)]++;

            if (stereo) {
                short right = samples[i * nChannels + 1];
                counts[1][binIndex(right)]++;

                short mid  = (left + right) / 2;
                short side = (left - right) / 2;
                counts[2][binIndex(mid)]++;
                counts[3][binIndex(side)]++;
            }
        }
    }

    void dump(const size_t channel) const {
        if (channel >= counts.size()) {
            std::cerr << "Error: invalid channel requested\n";
            return;
        }

        std::cout << "Bin size: " << binSize << "\n";
        std::cout << "Total bins: " << counts[channel].size() << "\n\n";

        for (auto [bin, count] : counts[channel]) {
            // represent bin by its *lower edge* (start of range)
            int start = bin * binSize;
            std::cout << start << "\t" << count << "\n";
        }

        // print MID/SIDE automatically after channel 1 (if stereo)
        if (stereo && channel == 1) {
            const char* labels[] = {"MID", "SIDE"};
            for (size_t c = 2; c < 4; ++c) {
                std::cout << "\n=== " << labels[c - 2] << " Channel ===\n";
                std::cout << "Bin size: " << binSize << "\n";
                std::cout << "Total bins: " << counts[c].size() << "\n\n";

                for (auto [bin, count] : counts[c]) {
                    int start = bin * binSize;
                    std::cout << start << "\t" << count << "\n";
                }
            }
        }
    }
};

#endif
