"""Audit physical repair and boundary phases; never rewrite raw results."""
from pathlib import Path
import csv
import hashlib
import json
import re

root = Path(__file__).resolve().parent
old = root.parent / "macos-targeted-retest-20261016"

def config(row):
    fields = {}
    for entry in row["workloadConfig"].split(";"):
        key, separator, value = entry.partition("=")
        if separator and key not in fields:
            fields[key] = value
    return fields

def protocol(file, prefix):
    return [dict(x.split("=", 1) for x in line.split("\t")[1:] if "=" in x)
            for line in file.read_text().splitlines() if line.startswith(prefix + "\t")]

def save_csv(name, points):
    with (root / name).open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(points[0]))
        writer.writeheader()
        writer.writerows(points)

all_rows, checks, windows, memory_points = [], [], [], []
pattern = re.compile(r"\[GPU Timing\] Compute: ([0-9.]+) ms \| Render: ([0-9.]+) ms \| Total: ([0-9.]+) ms \| FPS: (\d+) \| T: ([0-9.]+) s(.*)")
for phase, directory in [("candidate", root), ("final", root / "final"), ("boundary", root / "boundary")]:
    rows = json.loads((directory / "data/results/results.json").read_text())
    runs = json.loads((directory / "run-status.json").read_text())
    expected = 2 if phase == "boundary" else 6
    assert len(rows) == expected and len(runs) == expected
    by_id = {r["id"]: r for r in rows}
    for run in runs:
        assert run["state"] == "completed" and run["returnCode"] == 0
        assert run["thermalBefore"]["thermalState"] == run["thermalAfter"]["thermalState"] == 0
        assert len(run["results"]) == 1
        row = by_id[run["results"][0]["id"]]
        c = config(row)
        assert row["deviceName"] == "Apple M4 Pro" and row["platform"] == "macOS"
        review = phase == "candidate" and row["graphicsApi"] == "Vulkan" and row["workload"] == "stream"
        preview = run["job"] == "vulkan-v3-256m-preview"
        item = {"phase": phase, "job": run["job"], "id": row["id"], "status": "diagnostic_legacy_query" if review else "expected_preview" if preview else "passed",
                "score": row["score"], "unit": row["scoreUnit"], "version": row["workloadVersion"], "dropPct": row["throttlePct"]}
        if row["workload"] == "membw":
            assert row["workloadVersion"] == ("membw_v3_streaming_preview_moltenvk" if preview else "membw_v3_streaming_moltenvk")
            assert c["memoryPlateauReached"] == ("0" if preview else "1") and c["dataVerified"] == "1"
            assert int(c["writeCopyChecks"]) > 0
            assert c["largeDatasetTraversal"] == "ordered_full_dataset_passes"
            assert c["timestampAnomalies"] == "0" and c["timing"] == "wall_clock"
            points = protocol(directory / (run["job"] + ".log"), "MEMBW_POINT")
            if not preview:
                assert len(points) == 95 and c["writeCopyChecks"] == "46"
            for point in points:
                size, passes = int(point["dataset_bytes"]), int(point["passes"])
                if point["kernel"] in ["read", "write", "copy"]:
                    assert int(point["dispatch_bytes"]) == size * passes * (2 if point["kernel"] == "copy" else 1)
                    if size >= 256 * 2**20:
                        assert int(point["dispatches_per_sample"]) == passes
                memory_points.append({"phase": phase, **point})
            read = [p for p in points if p["kernel"] == "read"]
            tail = [float(p["median_gbps"]) for p in read[-3:]]
            sizes = [int(p["dataset_bytes"]) for p in read[-3:]]
            if preview:
                assert sizes == [128 * 2**20, 192 * 2**20, 256 * 2**20]
            else:
                assert min(sizes) >= 256 * 2**20 and max(tail) / min(tail) <= 1.2
            item.update({"readTailGBps": tail, "writeGBps": float(c["writeMemoryGBps"]),
                         "readTailDatasetBytes": sizes, "writeCopyChecks": int(c["writeCopyChecks"]),
                         "copyGBps": float(c["copyMemoryGBps"]), "curvePoints": len(points)})
        else:
            assert row["avgTotalGpuMs"] <= row["avgFrameTimeMs"] * 1.1
            assert row["avgComputeMs"] + row["avgRenderMs"] <= row["avgTotalGpuMs"] + .01
            assert row["stableScore"] > 0 and row["throttlePct"] <= 5
            if not row["headless"]:
                assert c["renderCoverage"] == "wrapped_ndc_v2"
            if phase == "final" and row["graphicsApi"] == "Vulkan":
                assert c["timingMode"] == "synchronized_wall_clock" and "_sync_timing" in row["workloadVersion"]
            if row["headless"] and row["graphicsApi"] == "Metal":
                assert c["timingMode"] == "command_buffer_gpu_time"
            for line in (directory / (run["job"] + ".log")).read_text().splitlines():
                match = pattern.search(line)
                if match:
                    compute, render, total, fps, seconds = map(float, match.groups()[:5])
                    windows.append({"phase": phase, "job": run["job"], "id": row["id"], "seconds": seconds,
                                    "warmup": "warm-up" in match.group(6), "computeMs": compute, "renderMs": render,
                                    "totalGpuMs": total, "fps": fps, "derivedGBps": 40 * row["particleCount"] / compute / 1e6})
        checks.append(item)
    all_rows.extend(rows)
assert len({r["id"] for r in all_rows}) == 14
selected = {}
by_check = {c["id"]: c for c in checks}
for row in sorted(all_rows, key=lambda r: r["id"]):
    if by_check[row["id"]]["status"] == "passed":
        selected[(row["graphicsApi"], row["workload"], row["particleCount"], row["headless"])] = row
(root / "all-new-results.json").write_text(json.dumps(all_rows, ensure_ascii=False, indent=2) + "\n")
(root / "curated-current-targets.json").write_text(json.dumps(list(selected.values()), ensure_ascii=False, indent=2) + "\n")
assert len(selected) == 6
(root / "verification.json").write_text(json.dumps({"status": "passed", "rawRecords": 14, "currentPolicyPassedRecords": 10,
    "queryControlsForReview": 3, "expectedBoundaryPreview": 1, "currentTargetConfigurations": 6, "selection": "latest valid per configuration, not maximum score",
    "runs": checks}, indent=2) + "\n")
save_csv("particle-windows.csv", windows)
save_csv("vulkan-memory-points.csv", memory_points)

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

fig, axes = plt.subplots(1, 3, figsize=(14, 4.6), layout="constrained")
for axis, kernel in zip(axes, ["read", "write", "copy"]):
    for label, path, style in [("v2: old", old / "vulkan-membw-v2.log", "--"),
                               ("v3: first", root / "vulkan-membw-v3.log", ":"),
                               ("v3: particle-stage build", root / "final/vulkan-membw-v3-repeat.log", "-"),
                               ("v3: final boundary guard", root / "boundary/vulkan-v3-final-default.log", "-.")]:
        points = [p for p in protocol(path, "MEMBW_POINT") if p["kernel"] == kernel and int(p["dataset_bytes"]) >= 64 * 2**20]
        axis.plot([int(p["dataset_bytes"]) / 2**20 for p in points], [float(p["median_gbps"]) for p in points],
                  style, marker=".", label=label)
    axis.axvspan(256, 2048, alpha=.06, color="green")
    axis.set_xscale("log", base=2)
    axis.set_title(kernel)
    axis.set_xlabel("Data set (MiB)")
    axis.set_ylabel("Logical bandwidth (GB/s)")
    axis.grid(alpha=.2)
    axis.legend(fontsize=8)
fig.suptitle("M4 Pro memory sweep | V3 orders complete passes from 256 MiB")
for extension in ["png", "pdf"]:
    fig.savefig(root / ("memory-fix-comparison." + extension), dpi=150)
plt.close(fig)

def log_windows(path):
    points = []
    for line in path.read_text().splitlines():
        match = pattern.search(line)
        if match:
            comp, rend, total, fps, seconds = map(float, match.groups()[:5])
            points.append((seconds, comp, rend, total))
    return points

fig, axes = plt.subplots(1, 2, figsize=(12, 4.5), layout="constrained")
for label, path in [("legacy visible coverage", old / "vulkan-render-1m.log"),
                    ("fixed coverage query A", root / "vulkan-fixed-render-1m-a.log"),
                    ("fixed coverage query B", root / "vulkan-fixed-render-1m-b.log")]:
    points = log_windows(path)
    for axis, column in zip(axes, [1, 2]):
        axis.plot([p[0] for p in points], [p[column] for p in points], marker=".", label=label)
for axis, title in zip(axes, ["Compute query interval", "Render query interval"]):
    axis.set_title(title)
    axis.set_xlabel("Elapsed seconds")
    axis.set_ylabel("ms")
    axis.axvspan(0, 2, color="grey", alpha=.1)
    axis.grid(alpha=.2)
    axis.legend(fontsize=8)
fig.suptitle("1M particle coverage control | same query timing, different visible workload")
for extension in ["png", "pdf"]:
    fig.savefig(root / ("particle-coverage-control." + extension), dpi=150)
plt.close(fig)

fig, axes = plt.subplots(1, 2, figsize=(12, 4.5), layout="constrained")
for job, label in [("vulkan-fixed-1m-a", "Vulkan sync A"), ("vulkan-fixed-1m-b", "Vulkan sync B"), ("metal-fixed-1m", "Metal command buffer")]:
    points = [p for p in windows if p["phase"] == "final" and p["job"] == job]
    axes[0].plot([p["seconds"] for p in points], [p["renderMs"] for p in points], marker=".", label=label)
    measured = [p for p in points if not p["warmup"]]
    mean = sum(p["derivedGBps"] for p in measured) / len(measured)
    axes[1].plot([p["seconds"] for p in measured], [p["derivedGBps"] / mean * 100 for p in measured], marker=".", label=label)
axes[0].set_title("Fixed coverage: rendering remains constant")
axes[0].set_ylabel("Render phase time (ms)")
axes[1].set_title("Each run normalized to its own mean")
axes[1].set_ylabel("Relative throughput (%)")
for axis in axes:
    axis.set_xlabel("Elapsed seconds")
    axis.grid(alpha=.2)
    axis.legend(fontsize=8)
fig.suptitle("Final production checks | API timing scopes differ; no absolute score ranking")
for extension in ["png", "pdf"]:
    fig.savefig(root / ("final-particle-stability." + extension), dpi=150)
plt.close(fig)
print("Verified 14 real records: 10 current-policy passes, 3 query controls, 1 expected boundary preview; 6 target configurations")
