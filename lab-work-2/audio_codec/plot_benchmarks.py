#!/usr/bin/env python3
"""
Generates PNG figures from benchmark_audio.csv for the report.
"""

import csv
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt


def ensure_output_dir(report_dir: Path) -> Path:
    vis_dir = report_dir / "VISUAL_RESOURCES"
    vis_dir.mkdir(parents=True, exist_ok=True)
    return vis_dir


def load_data(csv_path: Path):
    rows = []
    with csv_path.open() as fh:
        reader = csv.DictReader(fh)
        for row in reader:
            row["encode_ms"] = float(row["encode_ms"])
            row["decode_ms"] = float(row["decode_ms"])
            row["wav_bytes"] = int(row["wav_bytes"])
            row["gac_bytes"] = int(row["gac_bytes"])
            row["frames"] = int(row["frames"])
            row["channels"] = int(row["channels"])
            row["bits_per_sample"] = float(row["bits_per_sample"])
            row["ratio"] = row["gac_bytes"] / row["wav_bytes"]
            rows.append(row)
    return rows


def plot_compression_ratio(rows, out_path: Path) -> None:
    files = []
    mode_ratios = defaultdict(dict)
    for row in rows:
        if row["file"] not in files:
            files.append(row["file"])
        mode_ratios[row["mode"]][row["file"]] = row["ratio"]

    fig, ax = plt.subplots(figsize=(10, 5))
    modes = sorted(mode_ratios.keys())
    width = 0.8 / max(1, len(modes))
    for idx, mode in enumerate(modes):
        ratios = [mode_ratios[mode].get(file, 0) for file in files]
        offsets = [i + idx * width for i in range(len(files))]
        ax.bar(offsets, ratios, width=width, label=mode)
    ticks = [i + width * (len(modes) - 1) / 2 for i in range(len(files))]
    ax.set_xticks(ticks)
    ax.set_xticklabels(files, rotation=45, ha="right")
    ax.set_ylabel("Compression ratio (gac / wav)")
    ax.set_title("Compression ratio por amostra e modo")
    ax.grid(axis="y", linestyle=":", color="#cccccc")
    ax.legend(title="Modo")
    plt.tight_layout()
    plt.savefig(out_path)
    plt.close()


def plot_times(rows, out_path: Path) -> None:
    files = []
    phase_data = defaultdict(dict)
    for row in rows:
        if row["file"] not in files:
            files.append(row["file"])
        phase_data[(row["mode"], "encode")][row["file"]] = row["encode_ms"]
        phase_data[(row["mode"], "decode")][row["file"]] = row["decode_ms"]

    fig, ax = plt.subplots(figsize=(10, 5))
    for (mode, phase), values in sorted(phase_data.items()):
        y = [values.get(file, 0) for file in files]
        label = f"{mode} {'encode' if phase == 'encode' else 'decode'}"
        ax.plot(files, y, marker="o" if phase == "encode" else "s", label=label)
    ax.set_ylabel("Tempo (ms)")
    ax.set_xlabel("Ficheiro WAV")
    ax.set_title("Tempo médio de codificação/decodificação")
    ax.grid(True, linestyle="--", alpha=0.5)
    ax.legend()
    plt.xticks(rotation=45, ha="right")
    plt.tight_layout()
    plt.savefig(out_path)
    plt.close()


def plot_bits_per_sample(rows, out_path: Path) -> None:
    files = []
    bp_data = defaultdict(dict)
    for row in rows:
        if row["file"] not in files:
            files.append(row["file"])
        bp_data[row["mode"]][row["file"]] = row["bits_per_sample"]

    fig, ax = plt.subplots(figsize=(10, 5))
    for mode in sorted(bp_data.keys()):
        y = [bp_data[mode].get(file, 0) for file in files]
        ax.plot(files, y, marker="s", label=mode)
    ax.set_ylabel("Bits por amostra")
    ax.set_xlabel("Ficheiro WAV")
    ax.set_title("Bits por amostra por modo")
    ax.grid(True, linestyle=":", alpha=0.5)
    ax.legend()
    plt.xticks(rotation=45, ha="right")
    plt.tight_layout()
    plt.savefig(out_path)
    plt.close()


def main():
    base = Path(__file__).resolve().parent
    csv_path = base / "benchmark_audio.csv"
    if not csv_path.exists():
        raise SystemExit("Execute benchmark_audio.py first to generate the CSV.")

    report_dir = base.parent / "report"
    vis_dir = ensure_output_dir(report_dir)

    rows = load_data(csv_path)
    plot_compression_ratio(rows, vis_dir / "bench_compression_ratio.png")
    plot_times(rows, vis_dir / "bench_encode_decode_times.png")
    plot_bits_per_sample(rows, vis_dir / "bench_bits_per_sample.png")

    print(f"Figures saved to {vis_dir}")


if __name__ == "__main__":
    main()
