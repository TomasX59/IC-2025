# Audio Codec (Lab Work 2 — Part III)

This directory holds the lossless audio codec requested in Part III of Lab Work 2. It reads 16-bit PCM WAV files (mono or stereo), predicts residuals, and encodes them with Golomb coding using the shared `Golomb` class.

## Prerequisites

- `libsndfile` (development headers and library)

On Ubuntu/Debian:

```bash
sudo apt update
sudo apt install libsndfile1-dev
```

## Build

```bash
cd lab-work-2/audio_codec
mkdir -p build && cd build
cmake ..
cmake --build .
```

The executable `audio_codec` is emitted under `lab-work-2/bin`.

## Usage

### Encode

Encodes a WAV file into the custom Golomb-aware stream (`.gac` is suggested):

```bash
./audio_codec encode input.wav output.gac [--block-size N] [--fixed-m M]
```

- `block-size` controls how many frames are grouped for m-adaptation (default: 1024). Must be ≤ 65535.
- `fixed-m` forces a constant Golomb parameter (otherwise it is chosen adaptively per block).

### Decode

```bash
./audio_codec decode input.gac reconstructed.wav
```

## Format summary

- 4-byte header magic `GAC1` + version (1) + channel count + sample width (bits) + samplerate + total frames + block size.
- Each block stores frame count, the chosen `m`, a predictor bitmap for stereo, the number of Golomb bits, and the bitstream encoded as bytes.

## Notes

- Stereo blocks blend temporal prediction for the left channel with a per-sample choice between temporal and inter‑channel prediction for the right channel; the predictor bitmap is saved so the decoder can follow the same path.
- Golomb parameter `m` is recorded for each block so no adaptive metadata needs to be recomputed on decode.
- To gather metrics for the report, run `python3 benchmark_audio.py` from this directory. The script measures encode/decode times across the WAV samples and writes `benchmark_audio.csv` plus the generated `.gac` files to `benchmark_output/`.
- After benchmarking, render the report figures with `python3 plot_benchmarks.py`; the PNGs are stored at `../report/VISUAL_RESOURCES/bench_*.png`.
- The plotting script uses `matplotlib`, so install it if missing (`pip install matplotlib`) before running `plot_benchmarks.py`.
