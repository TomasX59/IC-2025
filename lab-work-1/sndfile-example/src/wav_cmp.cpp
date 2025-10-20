#include <iostream>
#include <vector>
#include <sndfile.hh>
#include <cmath>
#include <string>

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536;

// Compute MSE, max error, and SNR
int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: wav_cmp <original.wav> <processed.wav>\n";
        return 1;
    }

    string originalFile = argv[1];
    string processedFile = argv[2];

    SndfileHandle sfhOrig{originalFile};
    SndfileHandle sfhProc{processedFile};

    if (sfhOrig.error() || sfhProc.error()) {
        cerr << "Error: cannot open one of the files.\n";
        return 1;
    }

    if (sfhOrig.channels() != sfhProc.channels() ||
        sfhOrig.samplerate() != sfhProc.samplerate() ||
        sfhOrig.frames() != sfhProc.frames()) {
        cerr << "Error: input files differ in format or length.\n";
        return 1;
    }

    size_t nChannels = sfhOrig.channels();
    vector<double> mse(nChannels, 0.0);
    vector<short> bufferOrig(FRAMES_BUFFER_SIZE * nChannels);
    vector<short> bufferProc(FRAMES_BUFFER_SIZE * nChannels);
    vector<int> maxError(nChannels, 0);

    double mseAvg = 0.0;
    int maxErrorAvg = 0;
    size_t totalSamples = 0;

    // Read both files frame by frame
    size_t nRead;
    while ((nRead = sfhOrig.readf(bufferOrig.data(), FRAMES_BUFFER_SIZE)) > 0) {
        sfhProc.readf(bufferProc.data(), nRead);

        for (size_t i = 0; i < nRead; ++i) {
            double avgOrig = 0.0, avgProc = 0.0;

            for (size_t ch = 0; ch < nChannels; ++ch) {
                short o = bufferOrig[i * nChannels + ch];
                short p = bufferProc[i * nChannels + ch];
                int diff = o - p;

                mse[ch] += diff * diff;
                if (abs(diff) > maxError[ch])
                    maxError[ch] = abs(diff);

                avgOrig += o;
                avgProc += p;
            }

            // Average (mono) channel
            avgOrig /= nChannels;
            avgProc /= nChannels;
            int diffAvg = avgOrig - avgProc;
            mseAvg += diffAvg * diffAvg;
            if (abs(diffAvg) > maxErrorAvg)
                maxErrorAvg = abs(diffAvg);
        }

        totalSamples += nRead;
    }

    // Compute final averages
    for (size_t ch = 0; ch < nChannels; ++ch)
        mse[ch] /= totalSamples;
    mseAvg /= totalSamples;

    // Compute SNR
    sfhOrig.seek(0, SEEK_SET);
    vector<double> power(nChannels, 0.0);
    vector<double> powerAvg(nChannels, 0.0);
    totalSamples = 0;

    while ((nRead = sfhOrig.readf(bufferOrig.data(), FRAMES_BUFFER_SIZE)) > 0) {
        for (size_t i = 0; i < nRead; ++i) {
            double avg = 0.0;
            for (size_t ch = 0; ch < nChannels; ++ch) {
                short o = bufferOrig[i * nChannels + ch];
                power[ch] += o * o;
                avg += o;
            }
            avg /= nChannels;
            powerAvg[0] += avg * avg;
        }
        totalSamples += nRead;
    }

    for (size_t ch = 0; ch < nChannels; ++ch)
        power[ch] /= totalSamples;
    powerAvg[0] /= totalSamples;

    cout.setf(ios::fixed);
    cout.precision(4);

    cout << "\n=== WAV Comparison Results ===\n";
    for (size_t ch = 0; ch < nChannels; ++ch) {
        double snr = 10 * log10(power[ch] / mse[ch]);
        cout << "Channel " << ch << ":\n";
        cout << "  Mean Squared Error (L2): " << mse[ch] << '\n';
        cout << "  Max Abs Error (L∞): " << maxError[ch] << '\n';
        cout << "  SNR: " << snr << " dB\n";
    }

    double snrAvg = 10 * log10(powerAvg[0] / mseAvg);
    cout << "\nAverage (Mono):\n";
    cout << "  Mean Squared Error (L2): " << mseAvg << '\n';
    cout << "  Max Abs Error (L∞): " << maxErrorAvg << '\n';
    cout << "  SNR: " << snrAvg << " dB\n";
}
