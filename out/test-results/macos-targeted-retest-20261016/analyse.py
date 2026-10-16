"""Independent consistency checks and exports of this run's original records."""
from pathlib import Path
import csv
import hashlib
import json
import re
import statistics
import subprocess

root = Path(__file__).resolve().parent
repo = root.parents[2]
rows = json.loads((root / "data/results/results.json").read_text())
status = json.loads((root / "run-status.json").read_text())
assert len(rows) == 6 and len({r["id"] for r in rows}) == 6
assert len(status) == 5 and all(e["returnCode"] == 0 for e in status)
assert all(e[t]["thermalState"] == 0 for e in status for t in ["thermalBefore", "thermalAfter"])

def cfg(row):
    result = {}
    for item in row["workloadConfig"].split(";"):
        key, sep, value = item.partition("=")
        if sep and key not in result:
            result[key] = value
    return result

def records(file, prefix):
    return [dict(x.split("=", 1) for x in line.split("\t")[1:] if "=" in x)
            for line in (root / file).read_text().splitlines() if line.startswith(prefix + "\t")]

def save_csv(name, points):
    with (root / name).open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(points[0]))
        writer.writeheader()
        writer.writerows(points)

audit = {"sourceRevision": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
         "measurements": [], "scope": "Five targeted Mac executions, six saved records; no full suite or iOS device measurement."}
for row in rows:
    assert row["deviceName"] == "Apple M4 Pro" and row["platform"] == "macOS"
    assert row["score"] > 0

cpu = [r for r in rows if r["graphicsApi"] == "CPU"]
assert len(cpu) == 3
residency = records("cpu-kept-branch.log", "CPU_RESIDENCY")
assert len(residency) == 6
assert all(r["verified"] == "1" and float(r["requested_lower_bound"]) >= .99 for r in residency)
save_csv("cpu-residency.csv", residency)
core_protocol = records("cpu-kept-branch.log", "CPU_RESULT")
for row in cpu:
    c = cfg(row)
    assert "_kept_branch" in row["workloadVersion"] and c["scoreContract"] == "formal"
    assert c["totalMeasureSeconds"] == "15" and c["warmupSeconds"] == "0.2" and c["rounds"] == "3"
    entry = {"id": row["id"], "workload": row["workload"], "score": row["score"], "coreType": c.get("coreType", "all"), "status": "passed"}
    if row["workload"] == "cpu_single_core":
        assert c["residencyVerified"] == "1"
        protocol = next(p for p in core_protocol if p.get("kind") == "core" and p.get("core_type") == c["coreType"])
        median = protocol["median_round"]
        entry["medianRound"] = int(median)
        entry["effectiveGHz"] = float(c[f"round{median}EffectiveGHz"])
        entry["approxCyclesPerWorkUnit"] = entry["effectiveGHz"] * 1000 / row["score"]
        entry["minimumResidencyLowerBound"] = min(float(c[f"round{i}RequestedLowerBound"]) for i in range(1, 4))
    else:
        assert c["threads"] == "14"
        workers = records("cpu-kept-branch.log", "CPU_MULTI_WORKER")
        assert len(workers) == 42
        sums = [sum(float(w["score"]) for w in workers if w["round"] == str(i)) for i in range(1, 4)]
        assert abs(statistics.median(sums) - row["score"]) < .001
        entry["roundWorkerScoreSums"] = sums
        entry["workerLabels"] = "unclassified migrating threads, not numbered P/E cores"
    audit["measurements"].append(entry)

timing_pattern = re.compile(r"\[GPU Timing\] Compute: ([0-9.]+) ms \| Render: ([0-9.]+) ms \| Total: ([0-9.]+) ms \| FPS: (\d+) \| T: ([0-9.]+) s(.*)")
windows = []
for name, api, headless in [("metal-headless-1m", "Metal", True), ("vulkan-render-1m", "Vulkan", False)]:
    row = next(r for r in rows if r["workload"] == "stream" and r["graphicsApi"] == api)
    assert row["headless"] == headless and row["particleCount"] == 1048576
    assert row["warmupSec"] == 2 and row["durationSec"] >= 12.9 and row["timingSamples"] > 0
    assert row["avgComputeMs"] + row["avgRenderMs"] <= row["avgTotalGpuMs"] + .01
    assert row["avgTotalGpuMs"] <= row["avgFrameTimeMs"] * 1.1
    for line in (root / (name + ".log")).read_text().splitlines():
        m = timing_pattern.search(line)
        if not m:
            continue
        compute, render, total, fps, elapsed = map(float, m.groups()[:5])
        windows.append({"job": name, "seconds": elapsed, "warmup": "warm-up" in m.group(6),
                        "computeMs": compute, "renderMs": render, "totalGpuMs": total,
                        "fps": fps, "derivedGBps": 40 * row["particleCount"] / compute / 1e6})
    audit["measurements"].append({"id": row["id"], "job": name, "score": row["score"], "stableScore": row["stableScore"],
                                  "dropPct": row["throttlePct"], "gpuFrameRatio": row["avgTotalGpuMs"] / row["avgFrameTimeMs"],
                                  "status": "passed_this_run", "historicalCauseClosed": False})
save_csv("particle-timing-curves.csv", windows)

memory = next(r for r in rows if r["workload"] == "membw")
mc = cfg(memory)
assert memory["workloadVersion"].startswith("membw_v2_verified_preview")
assert mc["timing"] == "wall_clock" and mc["timingPolicy"] == "moltenvk_synchronized_sweep_v1"
assert mc["dataVerified"] == "1" and mc["writeCopyChecks"] == "46" and mc["timestampAnomalies"] == "0"
assert mc["maxDatasetMiB"] == "2048" and mc["memoryPlateauReached"] == "0"
points = records("vulkan-membw-v2.log", "MEMBW_POINT")
save_csv("vulkan-memory-points.csv", points)
audit["measurements"].append({"id": memory["id"], "status": "preview_memory_plateau_not_reached", "writeCopyChecks": 46,
                              "score": memory["score"], "curvePoints": len(points), "peakDramBandwidthConfirmed": False})

maths = records("metal-math-probe.log", "MATHPROBE")
assert len(maths) == 18
assert all(float(m["seconds"]) > 0 and float(m["rate"]) > 0 for m in maths)
for m in maths:
    fma = next(float(f["rate"]) for f in maths if f["maths"] == m["maths"] and f["function"] == "k_fma")
    m["relativeFmaRate"] = float(m["rate"]) / fma
save_csv("metal-math-rates.csv", maths)
audit["mathProbe"] = {"status": "completed", "sourceChanged": False,
                       "crossVendorProbeAvailable": False, "modes": maths}
(root / "audit.json").write_text(json.dumps(audit, indent=2) + "\n")

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

fig, axes = plt.subplots(2, 2, figsize=(12, 7), layout="constrained")
for i, name in enumerate(["metal-headless-1m", "vulkan-render-1m"]):
    ps = [p for p in windows if p["job"] == name]
    for key, label in [("computeMs", "Compute"), ("renderMs", "Render"), ("totalGpuMs", "Total GPU")]:
        axes[i, 0].plot([p["seconds"] for p in ps], [p[key] for p in ps], marker=".", label=label)
    axes[i, 1].plot([p["seconds"] for p in ps], [p["derivedGBps"] for p in ps], marker=".", color="#167964")
    axes[i, 0].set_title(name)
    axes[i, 0].set_ylabel("GPU time (ms)")
    axes[i, 0].legend(fontsize=8)
    axes[i, 1].set_title("Derived particle throughput, rounded window logs")
    axes[i, 1].set_ylabel("GB/s, 40 bytes per particle")
    for ax in axes[i]:
        ax.axvspan(0, 2, color="grey", alpha=.15)
        ax.set_xlabel("Elapsed seconds")
        ax.grid(alpha=.2)
fig.suptitle("M4 Pro targeted particle checks | 15 s including 2 s warm-up")
for ext in ["png", "pdf"]:
    fig.savefig(root / ("particle-timing-curves." + ext), dpi=160)
plt.close(fig)

fig, ax = plt.subplots(figsize=(9, 5), layout="constrained")
for kernel in ["read", "write", "copy"]:
    ps = [p for p in points if p["kernel"] == kernel]
    ax.plot([int(p["dataset_bytes"]) / 2**20 for p in ps], [float(p["median_gbps"]) for p in ps], marker=".", label=kernel)
ax.set_xscale("log", base=2)
ax.set_xlabel("Working set (MiB)")
ax.set_ylabel("Logical kernel traffic (GB/s)")
ax.set_title("M4 Pro Vulkan/MoltenVK v2 memory sweep | PREVIEW: read plateau absent")
ax.grid(alpha=.2)
ax.legend()
for ext in ["png", "pdf"]:
    fig.savefig(root / ("vulkan-memory-curves." + ext), dpi=160)
plt.close(fig)
print("Audit passed:", len(rows), "raw records;", len(residency), "CPU residency rounds;", len(windows), "particle windows;", len(points), "memory points;", len(maths), "math-probe rows")
