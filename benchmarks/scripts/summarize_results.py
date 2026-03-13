#!/usr/bin/env python3
import argparse
import csv
import json
import math
import statistics
from collections import defaultdict
from pathlib import Path


def safe_mean(values):
    return statistics.fmean(values) if values else 0.0


def safe_median(values):
    return statistics.median(values) if values else 0.0


def safe_cv(values):
    if not values:
        return 0.0
    mean = safe_mean(values)
    if mean == 0.0:
        return 0.0
    return statistics.pstdev(values) / mean


def load_throughput(path: Path):
    data = json.loads(path.read_text())
    grouped = defaultdict(list)

    for row in data.get("benchmarks", []):
        if row.get("run_type") == "aggregate":
            continue
        name = row.get("name")
        if not name or "real_time" not in row:
            continue
        grouped[name].append(row)

    records = []
    for name, rows in sorted(grouped.items()):
        real_times = [float(r["real_time"]) for r in rows]
        eps = [float(r.get("events_per_sec", 0.0)) for r in rows]
        records.append(
            {
                "suite": path.stem,
                "name": name,
                "mean_ns": safe_mean(real_times),
                "median_ns": safe_median(real_times),
                "cv": safe_cv(real_times),
                "mean_events_per_sec": safe_mean(eps),
            }
        )
    return records


def load_latency(path: Path):
    rows = []
    with path.open(newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(
                {
                    "suite": path.stem,
                    "name": row["benchmark"],
                    "events": int(row["events"]),
                    "p50_cycles": int(row["p50_cycles"]),
                    "p95_cycles": int(row["p95_cycles"]),
                    "p99_cycles": int(row["p99_cycles"]),
                    "mean_cycles": float(row["mean_cycles"]),
                }
            )
    return rows


def write_csv(out_path: Path, throughput, latency):
    with out_path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(
            [
                "type",
                "suite",
                "name",
                "mean_ns",
                "median_ns",
                "cv",
                "mean_events_per_sec",
                "events",
                "p50_cycles",
                "p95_cycles",
                "p99_cycles",
                "mean_cycles",
            ]
        )

        for row in throughput:
            writer.writerow(
                [
                    "throughput",
                    row["suite"],
                    row["name"],
                    f"{row['mean_ns']:.6f}",
                    f"{row['median_ns']:.6f}",
                    f"{row['cv']:.6f}",
                    f"{row['mean_events_per_sec']:.6f}",
                    "",
                    "",
                    "",
                    "",
                    "",
                ]
            )

        for row in latency:
            writer.writerow(
                [
                    "latency",
                    row["suite"],
                    row["name"],
                    "",
                    "",
                    "",
                    "",
                    row["events"],
                    row["p50_cycles"],
                    row["p95_cycles"],
                    row["p99_cycles"],
                    f"{row['mean_cycles']:.6f}",
                ]
            )


def write_markdown(out_path: Path, throughput, latency):
    lines = []
    lines.append("# Synthetic Benchmark Summary")
    lines.append("")
    lines.append("## Throughput")
    lines.append("")
    lines.append("| Suite | Benchmark | Mean ns | Median ns | CV | Mean events/s |")
    lines.append("|---|---|---:|---:|---:|---:|")

    for row in throughput:
        cv_flag = " ⚠" if row["cv"] > 0.05 else ""
        lines.append(
            f"| {row['suite']} | {row['name']} | {row['mean_ns']:.3f} | {row['median_ns']:.3f} | {row['cv']:.4f}{cv_flag} | {row['mean_events_per_sec']:.0f} |"
        )

    lines.append("")
    lines.append("## Latency")
    lines.append("")
    lines.append("| Suite | Benchmark | Events | p50 cycles | p95 cycles | p99 cycles | Mean cycles |")
    lines.append("|---|---|---:|---:|---:|---:|---:|")

    for row in latency:
        lines.append(
            f"| {row['suite']} | {row['name']} | {row['events']} | {row['p50_cycles']} | {row['p95_cycles']} | {row['p99_cycles']} | {row['mean_cycles']:.3f} |"
        )

    out_path.write_text("\n".join(lines) + "\n")


def normalized_name(name: str) -> str:
    return name.split("/", 1)[0]


def parse_wildcard_rank(variant: str):
    if variant.startswith("W") and variant[1:].isdigit():
        return int(variant[1:])
    if variant in {"none", "one", "two", "three", "four", "five", "seven", "eight", "sixteen", "thirtytwo"}:
        mapping = {
            "none": 0,
            "one": 1,
            "two": 2,
            "three": 3,
            "four": 4,
            "five": 5,
            "seven": 7,
            "eight": 8,
            "sixteen": 16,
            "thirtytwo": 32,
        }
        return mapping[variant]
    return None


def parse_stream_size_token(token: str):
    if token.endswith("M"):
        return int(token[:-1]) * 1_000_000
    if token.endswith("K"):
        return int(token[:-1]) * 1_000
    if token.isdigit():
        return int(token)
    return None


def parse_throughput_case(case_name: str):
    base = normalized_name(case_name)
    if not base.startswith("BM_"):
        return None
    core = base[3:]
    parts = core.split("_")

    if core.startswith("S") and len(parts) >= 3:
        return {
            "group": "S",
            "variant": parts[0],
            "mode": parts[1],
            "selectivity": int(parts[2]),
        }

    if core.startswith("K") and len(parts) >= 3:
        return {
            "group": "K",
            "variant": parts[0],
            "mode": parts[1],
            "selectivity": int(parts[2]),
        }

    if core.startswith("W"):
        if parts[0] == "W" and len(parts) >= 4:
            variant = f"W{parse_wildcard_rank(parts[1])}" if parse_wildcard_rank(parts[1]) is not None else None
            if variant is None:
                return None
            return {
                "group": "W",
                "variant": variant,
                "mode": parts[2],
                "selectivity": int(parts[3]),
            }
        if len(parts) >= 3:
            return {
                "group": "W",
                "variant": parts[0],
                "mode": parts[1],
                "selectivity": int(parts[2]),
            }

    if parts[0] == "P" and len(parts) >= 3:
        return {
            "group": "P",
            "variant": "_".join(parts[1:-1]),
            "mode": parts[-1],
            "selectivity": None,
        }

    if parts[0] == "R" and len(parts) >= 3:
        if len(parts) >= 4 and parts[2] in {"jitter", "nojitter"}:
            return {
                "group": "R",
                "variant": parts[1],
                "mode": parts[2],
                "stream_size": parse_stream_size_token(parts[3]),
            }
        return {
            "group": "R",
            "variant": parts[1],
            "mode": "jitter",
            "stream_size": parse_stream_size_token(parts[2]),
        }

    return None


def parse_latency_case(case_name: str):
    parts = case_name.split("_")
    if not parts:
        return None

    if parts[0].startswith("K") and len(parts) >= 3:
        return {
            "group": "K",
            "variant": parts[0],
            "mode": parts[1],
            "selectivity": int(parts[2]),
        }

    if parts[0].startswith("W") and len(parts) >= 3:
        if parts[0] == "W" and len(parts) >= 4:
            rank = parse_wildcard_rank(parts[1])
            if rank is None:
                return None
            return {
                "group": "W",
                "variant": f"W{rank}",
                "mode": parts[2],
                "selectivity": int(parts[3]),
            }
        return {
            "group": "W",
            "variant": parts[0],
            "mode": parts[1],
            "selectivity": int(parts[2]),
        }

    return None


def order_by_numeric_suffix(values, prefix):
    return sorted(values, key=lambda x: int(x[len(prefix):]))


def maybe_import_matplotlib():
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt

        return plt
    except ModuleNotFoundError:
        return None


def plot_s_throughput(plt, out_dir: Path, rows):
    s_rows = [r for r in rows if r.get("group") == "S"]
    if not s_rows:
        return

    variants = order_by_numeric_suffix(sorted({r["variant"] for r in s_rows}), "S")
    selectivities = sorted({r["selectivity"] for r in s_rows})
    fig, axes = plt.subplots(1, len(selectivities), figsize=(7 * len(selectivities), 5), sharey=True)
    if len(selectivities) == 1:
        axes = [axes]

    width = 0.35
    for ax, sel in zip(axes, selectivities):
        generated = {(r["variant"], r["mode"]): r["mean_events_per_sec"] / 1e6 for r in s_rows if r["selectivity"] == sel}
        x = list(range(len(variants)))
        y_gen = [generated.get((v, "generated"), math.nan) for v in variants]
        y_hand = [generated.get((v, "hand"), math.nan) for v in variants]
        ax.bar([i - width / 2 for i in x], y_gen, width, label="generated")
        ax.bar([i + width / 2 for i in x], y_hand, width, label="hand")
        ax.set_xticks(x)
        ax.set_xticklabels(variants)
        ax.set_title(f"S group @ {sel}%")
        ax.set_xlabel("Pattern")
        ax.grid(axis="y", linestyle=":", alpha=0.4)

    axes[0].set_ylabel("Events/s (millions)")
    axes[0].legend()
    fig.tight_layout()
    fig.savefig(out_dir / "throughput_s_generated_vs_hand.png", dpi=160)
    plt.close(fig)


def plot_s_jitter_vs_nojitter(plt, out_dir: Path, rows):
    s_rows = [r for r in rows if r.get("group") == "S"]
    if not s_rows:
        return

    variants = order_by_numeric_suffix(sorted({r["variant"] for r in s_rows}), "S")
    selectivities = sorted({r["selectivity"] for r in s_rows})
    fig, axes = plt.subplots(1, len(selectivities), figsize=(7 * len(selectivities), 5), sharey=True)
    if len(selectivities) == 1:
        axes = [axes]

    for ax, sel in zip(axes, selectivities):
        jitter = {
            r["variant"]: r["mean_events_per_sec"] / 1e6
            for r in s_rows
            if r["selectivity"] == sel and r["mode"] == "generated"
        }
        nojitter = {
            r["variant"]: r["mean_events_per_sec"] / 1e6
            for r in s_rows
            if r["selectivity"] == sel and r["mode"] == "nojitter"
        }

        x = list(range(len(variants)))
        y_j = [jitter.get(v, math.nan) for v in variants]
        y_nj = [nojitter.get(v, math.nan) for v in variants]
        ax.plot(x, y_j, marker="o", label="jitter")
        ax.plot(x, y_nj, marker="o", label="nojitter")
        ax.set_xticks(x)
        ax.set_xticklabels(variants)
        ax.set_title(f"S group @ {sel}%")
        ax.set_xlabel("Pattern")
        ax.grid(True, linestyle=":", alpha=0.4)

    axes[0].set_ylabel("Events/s (millions)")
    axes[0].legend()
    fig.tight_layout()
    fig.savefig(out_dir / "throughput_s_jitter_vs_nojitter.png", dpi=160)
    plt.close(fig)


def plot_k_or_w_throughput(plt, out_dir: Path, rows, group: str):
    g_rows = [r for r in rows if r.get("group") == group]
    if not g_rows:
        return

    prefix = "K" if group == "K" else "W"
    variants = order_by_numeric_suffix(sorted({r["variant"] for r in g_rows}), prefix)
    selectivities = sorted({r["selectivity"] for r in g_rows})
    fig, axes = plt.subplots(1, len(selectivities), figsize=(7 * len(selectivities), 5), sharey=True)
    if len(selectivities) == 1:
        axes = [axes]

    for ax, sel in zip(axes, selectivities):
        jitter = {r["variant"]: r["mean_events_per_sec"] / 1e6 for r in g_rows if r["selectivity"] == sel and r["mode"] == "jitter"}
        nojitter = {
            r["variant"]: r["mean_events_per_sec"] / 1e6 for r in g_rows if r["selectivity"] == sel and r["mode"] == "nojitter"
        }
        x = list(range(len(variants)))
        y_j = [jitter.get(v, math.nan) for v in variants]
        y_nj = [nojitter.get(v, math.nan) for v in variants]
        ax.plot(x, y_j, marker="o", label="jitter")
        ax.plot(x, y_nj, marker="o", label="nojitter")
        ax.set_xticks(x)
        ax.set_xticklabels(variants)
        ax.set_title(f"{group} group @ {sel}%")
        ax.set_xlabel("Pattern")
        ax.grid(True, linestyle=":", alpha=0.4)

    axes[0].set_ylabel("Events/s (millions)")
    axes[0].legend()
    fig.tight_layout()
    fig.savefig(out_dir / f"throughput_{group.lower()}_jitter_vs_nojitter.png", dpi=160)
    plt.close(fig)


def plot_p_throughput(plt, out_dir: Path, rows):
    p_rows = [r for r in rows if r.get("group") == "P"]
    if not p_rows:
        return

    order = ["simple", "moderate", "medium", "compound", "very_compound"]
    levels = [x for x in order if any(r["variant"] == x for r in p_rows)]
    x = list(range(len(levels)))
    width = 0.35
    generated = {r["variant"]: r["mean_events_per_sec"] / 1e6 for r in p_rows if r["mode"] == "generated"}
    hand = {r["variant"]: r["mean_events_per_sec"] / 1e6 for r in p_rows if r["mode"] == "hand"}

    fig, ax = plt.subplots(figsize=(8, 5))
    ax.bar([i - width / 2 for i in x], [generated.get(v, math.nan) for v in levels], width, label="generated")
    ax.bar([i + width / 2 for i in x], [hand.get(v, math.nan) for v in levels], width, label="hand")
    ax.set_xticks(x)
    ax.set_xticklabels(levels, rotation=15)
    ax.set_ylabel("Events/s (millions)")
    ax.set_title("P group throughput by predicate complexity")
    ax.grid(axis="y", linestyle=":", alpha=0.4)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_dir / "throughput_p_complexity.png", dpi=160)
    plt.close(fig)


def plot_r_throughput(plt, out_dir: Path, rows):
    r_rows = [r for r in rows if r.get("group") == "R"]
    if not r_rows:
        return

    levels = ["low", "medium", "high"]
    stream_sizes = sorted({r["stream_size"] for r in r_rows if r.get("stream_size") is not None})
    modes = sorted({r.get("mode", "jitter") for r in r_rows})
    if not stream_sizes:
        return

    fig, axes = plt.subplots(1, len(modes), figsize=(8 * len(modes), 5), sharey=True)
    if len(modes) == 1:
        axes = [axes]

    x = list(range(len(stream_sizes)))
    for ax, mode in zip(axes, modes):
        for level in levels:
            y = []
            for size in stream_sizes:
                val = next(
                    (
                        r["mean_events_per_sec"] / 1e6
                        for r in r_rows
                        if r["variant"] == level and r["stream_size"] == size and r.get("mode", "jitter") == mode
                    ),
                    math.nan,
                )
                y.append(val)
            ax.plot(x, y, marker="o", label=level)
        ax.set_xticks(x)
        ax.set_xticklabels([f"{s//1_000_000}M" for s in stream_sizes])
        ax.set_xlabel("Stream size")
        ax.set_title(f"R_real throughput ({mode})")
        ax.grid(True, linestyle=":", alpha=0.4)
        ax.legend()
    axes[0].set_ylabel("Events/s (millions)")
    fig.tight_layout()
    fig.savefig(out_dir / "throughput_r_stream_scaling.png", dpi=160)
    plt.close(fig)


def plot_k_or_w_latency(plt, out_dir: Path, rows, group: str):
    g_rows = [r for r in rows if r.get("group") == group]
    if not g_rows:
        return

    prefix = "K" if group == "K" else "W"
    variants = order_by_numeric_suffix(sorted({r["variant"] for r in g_rows}), prefix)
    selectivities = sorted({r["selectivity"] for r in g_rows})
    fig, axes = plt.subplots(1, len(selectivities), figsize=(7 * len(selectivities), 5), sharey=True)
    if len(selectivities) == 1:
        axes = [axes]

    for ax, sel in zip(axes, selectivities):
        jitter = {r["variant"]: r["p99_cycles"] for r in g_rows if r["selectivity"] == sel and r["mode"] == "jitter"}
        nojitter = {r["variant"]: r["p99_cycles"] for r in g_rows if r["selectivity"] == sel and r["mode"] == "nojitter"}
        x = list(range(len(variants)))
        y_j = [jitter.get(v, math.nan) for v in variants]
        y_nj = [nojitter.get(v, math.nan) for v in variants]
        ax.plot(x, y_j, marker="o", label="jitter")
        ax.plot(x, y_nj, marker="o", label="nojitter")
        ax.set_xticks(x)
        ax.set_xticklabels(variants)
        ax.set_title(f"{group} latency p99 @ {sel}%")
        ax.set_xlabel("Pattern")
        ax.grid(True, linestyle=":", alpha=0.4)

    axes[0].set_ylabel("p99 cycles")
    axes[0].legend()
    fig.tight_layout()
    fig.savefig(out_dir / f"latency_{group.lower()}_p99.png", dpi=160)
    plt.close(fig)


def generate_plots(out_dir: Path, throughput, latency):
    plt = maybe_import_matplotlib()
    if plt is None:
        print("plot generation skipped: matplotlib is not installed")
        return

    plot_dir = out_dir / "plots"
    plot_dir.mkdir(parents=True, exist_ok=True)

    parsed_throughput = []
    for row in throughput:
        parsed = parse_throughput_case(row["name"])
        if parsed is None:
            continue
        parsed_throughput.append({**row, **parsed})

    parsed_latency = []
    for row in latency:
        parsed = parse_latency_case(row["name"])
        if parsed is None:
            continue
        parsed_latency.append({**row, **parsed})

    plot_s_throughput(plt, plot_dir, parsed_throughput)
    plot_s_jitter_vs_nojitter(plt, plot_dir, parsed_throughput)
    plot_k_or_w_throughput(plt, plot_dir, parsed_throughput, "K")
    plot_k_or_w_throughput(plt, plot_dir, parsed_throughput, "W")
    plot_p_throughput(plt, plot_dir, parsed_throughput)
    plot_r_throughput(plt, plot_dir, parsed_throughput)
    plot_k_or_w_latency(plt, plot_dir, parsed_latency, "K")
    plot_k_or_w_latency(plt, plot_dir, parsed_latency, "W")
    print(f"plots: {plot_dir}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--throughput-jitter", type=Path, required=True)
    parser.add_argument("--throughput-nojitter", type=Path, required=True)
    parser.add_argument("--latency-jitter", type=Path, required=True)
    parser.add_argument("--latency-nojitter", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)

    throughput = load_throughput(args.throughput_jitter) + load_throughput(args.throughput_nojitter)
    latency = load_latency(args.latency_jitter) + load_latency(args.latency_nojitter)

    throughput.sort(key=lambda x: (x["suite"], x["name"]))
    latency.sort(key=lambda x: (x["suite"], x["name"]))

    write_csv(args.out_dir / "summary.csv", throughput, latency)
    write_markdown(args.out_dir / "summary.md", throughput, latency)
    generate_plots(args.out_dir, throughput, latency)


if __name__ == "__main__":
    main()
