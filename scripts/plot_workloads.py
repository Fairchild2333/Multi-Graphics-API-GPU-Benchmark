#!/usr/bin/env python3
"""Cross-API charts for the four benchmark axes.

Reads the benchmark results JSON and produces one bar chart per workload:

  * stream    -> memory bandwidth   (GB/s)      by API
  * nbody     -> achievable compute (GFLOP/s)   by API
  * gpu_burn  -> versioned visual burn rate     by device and API
  * cinematic_liquid -> future 3D liquid score  by device and API

The historical ``fluid`` workload is a legacy 2D projected-dye prototype and
is deliberately excluded from formal charts.
  * stress    -> fill rate          (G-iter/s)  by API
  * synthpeak -> peak throughput    (GFLOPS/GIOPS) by precision, grouped by API

Simple workloads use the newest workload contract and keep device/API identity;
precision charts reduce repeated runs to the best score. Saves PNGs to the
output directory (default docs/images).

Usage:
    python scripts/plot_workloads.py                  # auto-find results, save to docs/images
    python scripts/plot_workloads.py --input r.json --out out_dir
"""
import argparse
import json
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

# Stable colour per API so charts are comparable.
API_COLORS = {
    "Vulkan": "#a41e22", "DX12": "#0a7d2c", "DirectX 12": "#0a7d2c",
    "DX11": "#1f6fb2", "DirectX 11": "#1f6fb2",
    "OpenGL": "#e08a00", "Metal": "#7d3fbf",
}
WORKLOAD_TITLES = {
    "stream":    "Memory Bandwidth (Stream)",
    "nbody":     "Achievable Compute (N-body)",
    "gpu_burn":  "GPU Burn (Mangekyo / Plasma Bloom)",
    "gpu_stress": "GraphicsBurn v1 / Component (Advanced)",
    "stress":    "Fill Rate (Fractal Stress)",
    "render3d":  "3D Render Throughput (Billboards)",
    "volumetric": "Volumetric Throughput (Experimental)",
    "cinematic_liquid": "Cinematic Liquid (3D)",
    "fluid":     "Legacy 2D Fluid (Unverified)",
    "synthpeak": "Peak Throughput (Synthetic)",
}
PRECISION_ORDER = ["FP16", "FP32", "FP64", "INT32"]


def find_results(explicit: str | None) -> Path:
    if explicit:
        return Path(explicit)
    here = Path(__file__).resolve().parent.parent
    candidates = [here / "results" / "results.json",
                  Path.home() / ".gpu_bench" / "results.json"]
    for c in candidates:
        if c.is_file():
            return c
    return candidates[0]


def load(path: Path) -> list[dict]:
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    return data if isinstance(data, list) else data.get("results", [])


def best_by(rows, key_fn):
    """Reduce rows to the max-score entry per key."""
    best: dict = {}
    for r in rows:
        score = float(r.get("score", 0) or 0)
        if score <= 0:
            continue
        k = key_fn(r)
        if k not in best or score > float(best[k].get("score", 0) or 0):
            best[k] = r
    return best


def color_for(api: str) -> str:
    return API_COLORS.get(api, "#666666")


def plot_simple(rows, workload, out_dir):
    """One honest bar per device/API for the newest workload contract."""
    scored = [r for r in rows if float(r.get("score", 0) or 0) > 0]
    if not scored:
        return None

    # Never mix algorithm revisions into one chart. Schema-v1 history has no
    # workloadVersion; in that case retain the unversioned rows. Otherwise use
    # the contract attached to the newest result.
    newest = max(scored, key=lambda r: r.get("timestamp", ""))
    version = newest.get("workloadVersion", "") or ""
    compatible = [r for r in scored
                  if (r.get("workloadVersion", "") or "") == version]
    best = best_by(compatible, lambda r: (r.get("deviceName", "(unknown)"),
                                          r.get("graphicsApi", "?")))
    if not best:
        return None

    api_order = {name: index for index, name in enumerate(API_COLORS)}
    keys = sorted(best, key=lambda k: (k[0], api_order.get(k[1], 99), k[1]))
    scores = [float(best[k]["score"]) for k in keys]
    unit = best[keys[0]].get("scoreUnit", "")
    labels = []
    for device, api in keys:
        short_device = device if len(device) <= 28 else device[:25] + "..."
        labels.append(f"{api}\n{short_device}")

    fig_width = max(7.0, min(16.0, 1.25 * len(keys)))
    fig, ax = plt.subplots(figsize=(fig_width, 4.8))
    bars = ax.bar(range(len(keys)), scores,
                  color=[color_for(api) for _, api in keys])
    ax.set_xticks(range(len(keys)))
    ax.set_xticklabels(labels, rotation=20, ha="right", fontsize=8)
    ax.set_ylabel(unit)
    contract = f" — {version}" if version else ""
    ax.set_title(f"{WORKLOAD_TITLES.get(workload, workload)}{contract}\n"
                 "Best normalised score per device/API")
    ax.grid(axis="y", alpha=0.3)
    for b, s in zip(bars, scores):
        value = f"{s:,.1f}" if s < 1000 else f"{s:,.0f}"
        ax.text(b.get_x() + b.get_width() / 2, s, value,
                ha="center", va="bottom", fontsize=9)
    fig.tight_layout()
    out = out_dir / f"workload_{workload}.png"
    fig.savefig(out, dpi=120)
    plt.close(fig)
    return out


def plot_synthpeak(rows, out_dir):
    """Grouped bars: x = precision, series = API."""
    best = best_by(rows, lambda r: (r.get("graphicsApi", "?"),
                                    r.get("precision", "") or "FP32"))
    if not best:
        return None
    apis = sorted({k[0] for k in best})
    precs = [p for p in PRECISION_ORDER if any(k[1] == p for k in best)]
    if not precs:
        return None

    x = np.arange(len(precs))
    width = 0.8 / max(len(apis), 1)
    fig, ax = plt.subplots(figsize=(8, 4.5))
    for i, api in enumerate(apis):
        vals = [float(best[(api, p)]["score"]) if (api, p) in best else 0.0
                for p in precs]
        ax.bar(x + i * width, vals, width, label=api, color=color_for(api))

    dev = next(iter(best.values())).get("deviceName", "")
    ax.set_xticks(x + width * (len(apis) - 1) / 2)
    ax.set_xticklabels(precs)
    ax.set_ylabel("GFLOPS / GIOPS")
    ax.set_yscale("log")  # FP64 is ~1/60 of FP32 — log keeps all bars readable
    ax.set_title(f"{WORKLOAD_TITLES['synthpeak']} (log scale)\n{dev}")
    ax.legend()
    ax.grid(axis="y", alpha=0.3, which="both")
    fig.tight_layout()
    out = out_dir / "workload_synthpeak.png"
    fig.savefig(out, dpi=120)
    plt.close(fig)
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--input", help="Path to results.json (auto-detected if omitted)")
    ap.add_argument("--out", default=None, help="Output directory (default docs/images)")
    args = ap.parse_args()

    path = find_results(args.input)
    if not path.is_file():
        print(f"No results file found at {path}", file=sys.stderr)
        return 1
    rows = load(path)
    print(f"Loaded {len(rows)} result(s) from {path}")

    out_dir = Path(args.out) if args.out else (path.resolve().parent.parent / "docs" / "images")
    out_dir.mkdir(parents=True, exist_ok=True)

    by_wl: dict[str, list] = {}
    for r in rows:
        by_wl.setdefault(r.get("workload", "stream"), []).append(r)

    written = []
    for wl in ("stream", "gpu_burn", "cinematic_liquid", "gpu_stress", "nbody", "stress", "render3d", "volumetric"):
        if wl in by_wl:
            out = plot_simple(by_wl[wl], wl, out_dir)
            if out:
                written.append(out)
    if "synthpeak" in by_wl:
        out = plot_synthpeak(by_wl["synthpeak"], out_dir)
        if out:
            written.append(out)

    if not written:
        print("No scoreable results found (need entries with score > 0). "
              "Run some --benchmark workloads first.", file=sys.stderr)
        return 1
    for w in written:
        print(f"  wrote {w}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
