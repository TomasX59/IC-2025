#include <iostream>
#include <vector>
#include <sndfile.hh>
#include <cmath>
#include <string>

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536;

// Simple DSP effects for WAV files
int main(int argc, char* argv[]) {
    if (argc < 5) {
        cerr << "Usage:\n";
        cerr << "  wav_effects <input.wav> <output.wav> <effect> <param1> [param2]\n\n";
        cerr << "Effects:\n";
        cerr << "  echo <delay_ms> <decay>           - Single echo\n";
        cerr << "  multiecho <delay_ms> <decay>      - Multiple echoes\n";
        cerr << "  am <freq_Hz>                      - Amplitude modulation\n";
        cerr << "  delaymod <max_delay_ms> <freq_Hz> - Time-varying delay (flanger-like)\n";
        return 1;
    }

    string inFile = argv[1];
    string outFile = argv[2];
    string effect = argv[3];

    SndfileHandle sfhIn{inFile};
    if (sfhIn.error()) {
        cerr << "Error: cannot open input file\n";
        return 1;
    }

    SndfileHandle sfhOut{outFile, SFM_WRITE, sfhIn.format(), sfhIn.channels(), sfhIn.samplerate()};
    if (sfhOut.error()) {
        cerr << "Error: cannot create output file\n";
        return 1;
    }

    int sampleRate = sfhIn.samplerate();
    int nChannels = sfhIn.channels();
    vector<short> samples(FRAMES_BUFFER_SIZE * nChannels);

    // Load entire file into memory
    vector<short> allSamples;
    size_t nRead;
    while ((nRead = sfhIn.readf(samples.data(), FRAMES_BUFFER_SIZE)) > 0)
        allSamples.insert(allSamples.end(), samples.begin(), samples.begin() + nRead * nChannels);

    size_t totalSamples = allSamples.size();
    vector<short> output = allSamples;

    // --- Echo ---
    if (effect == "echo") {
        if (argc < 6) { cerr << "Usage: wav_effects input.wav output.wav echo <delay_ms> <decay>\n"; return 1; }
        float delay_ms = stof(argv[4]);
        float decay = stof(argv[5]);
        size_t delaySamples = (size_t)((delay_ms / 1000.0f) * sampleRate);

        for (size_t i = delaySamples * nChannels; i < totalSamples; ++i)
            output[i] += static_cast<short>(decay * allSamples[i - delaySamples * nChannels]);
    }

    // --- Multiple echoes ---
    else if (effect == "multiecho") {
        if (argc < 6) { cerr << "Usage: wav_effects input.wav output.wav multiecho <delay_ms> <decay>\n"; return 1; }
        float delay_ms = stof(argv[4]);
        float decay = stof(argv[5]);
        size_t delaySamples = (size_t)((delay_ms / 1000.0f) * sampleRate);

        for (size_t echo = 1; echo <= 5; ++echo) {
            float gain = pow(decay, echo);
            for (size_t i = echo * delaySamples * nChannels; i < totalSamples; ++i)
                output[i] += static_cast<short>(gain * allSamples[i - echo * delaySamples * nChannels]);
        }
    }

    // --- Amplitude modulation ---
    else if (effect == "am") {
        if (argc < 5) { cerr << "Usage: wav_effects input.wav output.wav am <freq_Hz>\n"; return 1; }
        float freq = stof(argv[4]);
        for (size_t i = 0; i < totalSamples / nChannels; ++i) {
            float mod = 0.5f * (1.0f + sin(2 * M_PI * freq * i / sampleRate));
            for (int ch = 0; ch < nChannels; ++ch)
                output[i * nChannels + ch] = static_cast<short>(allSamples[i * nChannels + ch] * mod);
        }
    }

    // --- Time-varying delay (flanger / vibrato style) ---
    else if (effect == "delaymod") {
        if (argc < 6) { cerr << "Usage: wav_effects input.wav output.wav delaymod <max_delay_ms> <freq_Hz>\n"; return 1; }
        float maxDelayMs = stof(argv[4]);
        float freq = stof(argv[5]);
        size_t maxDelaySamples = (size_t)((maxDelayMs / 1000.0f) * sampleRate);

        for (size_t i = 0; i < totalSamples / nChannels; ++i) {
            float mod = (sin(2 * M_PI * freq * i / sampleRate) + 1.0f) / 2.0f; // [0,1]
            size_t delay = (size_t)(mod * maxDelaySamples);
            for (int ch = 0; ch < nChannels; ++ch) {
                size_t idx = i * nChannels + ch;
                if (i > delay)
                    output[idx] += static_cast<short>(0.7f * allSamples[idx - delay * nChannels]);
            }
        }
    }

    else {
        cerr << "Error: unknown effect '" << effect << "'\n";
        return 1;
    }

    // Write result
    sfhOut.writef(output.data(), totalSamples / nChannels);
    cout << "Effect '" << effect << "' applied successfully.\n";
    return 0;
}
