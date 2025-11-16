#!/usr/bin/env python3
"""
Benchmark helper that encodes/decodes the WAV files in lab-work-1/data/audio/,
measures times, and reports compression ratios and bits per sample for the new
Golomb audio codec.
"""

import argparse
import csv
import subprocess
import sys
import tempfile
import time
import wave
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description="Benchmark the audio codec on the provided WAV samples."
    )
    parser.add_argument(
        "--block-sizes",
        nargs="+",
        type=int,
        default=[1024],
        help="Block sizes to test (default: 1024).",
    )
    parser.add_argument(
        "--fixed-m",
        nargs="*",
        type=int,
        default=[],
        help="List of fixed Golomb m parameters to try (adaptive if omitted).",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("benchmark_audio.csv"),
        help="Output CSV path (relative to script directory).",
    )
    parser.add_argument(
        "--samples-dir",
        type=Path,
        default=Path("../../lab-work-1/data/audio"),
        help="Directory that contains the WAV samples (relative to script directory).",
    )
    return parser.parse_args()


def run_command(cmd):
    start = time.perf_counter()
    subprocess.run(cmd, check=True)
    end = time.perf_counter()
    return (end - start) * 1000.0  # milliseconds


def main():
    args = parse_args()
    script_dir = Path(__file__).resolve().parent
    base_dir = script_dir.parent.parent
    codec_bin = base_dir / "lab-work-2/bin/audio_codec"
    if not codec_bin.exists():
        print("Build the codec first (see lab-work-2/audio_codec/README.md).", file=sys.stderr)
        sys.exit(1)

    samples_dir = (script_dir / args.samples_dir).resolve()
    if not samples_dir.is_dir():
        raise SystemExit(f"Samples directory not found: {samples_dir}")

    output_dir = script_dir / "benchmark_output"
    output_dir.mkdir(parents=True, exist_ok=True)
    args.output = script_dir / args.output

    modes = [("adaptive", None)]
    for m in args.fixed_m:
        if m <= 0:
            raise SystemExit("--fixed-m must be positive")
        modes.append((f"fixed_{m}", m))

    rows = []
    sample_files = sorted(samples_dir.glob("*.wav"))
    if not sample_files:
        raise SystemExit("No WAV samples found in " + str(samples_dir))

    for wav_path in sample_files:
        with wave.open(str(wav_path), "rb") as reader:
            frames = reader.getnframes()
            channels = reader.getnchannels()

        for block_size in args.block_sizes:
            if block_size <= 0 or block_size > 65535:
                raise SystemExit("block size must be between 1 and 65535")

            for mode_label, fixed_m in modes:
                gac_path = (
                    output_dir
                    / f"{wav_path.stem}_bs{block_size}_{mode_label}.gac"
                )
                encode_cmd = [
                    str(codec_bin),
                    "encode",
                    str(wav_path),
                    str(gac_path),
                    "--block-size",
                    str(block_size),
                ]
                if fixed_m is not None:
                    encode_cmd.extend(["--fixed-m", str(fixed_m)])

                try:
                    encode_time = run_command(encode_cmd)
                except subprocess.CalledProcessError as exc:
                    raise SystemExit(f"Encode failed: {exc}")

                if not gac_path.exists():
                    raise SystemExit("Encoded file missing: " + str(gac_path))

                with tempfile.TemporaryDirectory() as tmpdir:
                    decoded_path = Path(tmpdir) / f"{wav_path.stem}_decoded.wav"
                    decode_cmd = [
                        str(codec_bin),
                        "decode",
                        str(gac_path),
                        str(decoded_path),
                    ]
                    decode_time = run_command(decode_cmd)

                gac_bytes = gac_path.stat().st_size
                wav_bytes = wav_path.stat().st_size
                bits_per_sample = (
                    (gac_bytes * 8) / (frames * channels) if frames * channels > 0 else 0
                )

                rows.append(
                    {
                        "file": str(wav_path.relative_to(base_dir)),
                        "block_size": block_size,
                        "mode": mode_label,
                        "fixed_m": fixed_m if fixed_m is not None else "",
                        "encode_ms": f"{encode_time:.3f}",
                        "decode_ms": f"{decode_time:.3f}",
                        "wav_bytes": wav_bytes,
                        "gac_bytes": gac_bytes,
                        "frames": frames,
                        "channels": channels,
                        "bits_per_sample": f"{bits_per_sample:.4f}",
                    }
                )

    header = [
        "file",
        "block_size",
        "mode",
        "fixed_m",
        "encode_ms",
        "decode_ms",
        "wav_bytes",
        "gac_bytes",
        "frames",
        "channels",
        "bits_per_sample",
    ]
    with args.output.open("w", newline="") as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=header)
        writer.writeheader()
        writer.writerows(rows)

    print(f"Benchmark complete, results written to {args.output}")
    print(f"Encoded files stored under {output_dir}")


if __name__ == "__main__":
    main()
