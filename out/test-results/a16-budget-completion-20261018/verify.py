"""Check the A16 completion against exported production results and contracts."""
from pathlib import Path
import csv
import hashlib
import json
import re


ROOT = Path(__file__).resolve().parent
load = lambda p: json.loads(p.read_text())
config = lambda row: dict(p.split("=", 1) for p in row["workloadConfig"].split(";") if "=" in p)

before = load(ROOT / "phone-before/results/results.json")
after = load(ROOT / "phone-final/results/results.json")
report = load(ROOT / "phone-final/suite-runs/latest.json")
selected = load(ROOT / "selected-jobs.json")
audit = load(ROOT / "production-contract-audit.json")
assert len(before) == 24 and len(after) == 35 and after[:24] == before
assert len({r["id"] for r in after}) == len(after)
assert report["status"] in ("passed", "review") and report["finishedAt"] and not report.get("resumedFrom")
assert [j["id"] for j in report["jobs"]] == selected
assert len(audit) == 9 and all(j["contractPassed"] for j in audit)
by_id = {r["id"]: r for r in after}
used = set()
summary = []
for job in report["jobs"]:
    assert job["status"] in ("passed", "review")
    assert len(job["resultIDs"]) == job["expectedRecords"]
    assert job["thermalStateStart"] == 0
    assert not used.intersection(job["resultIDs"])
    used.update(job["resultIDs"])
    for rid in job["resultIDs"]:
        row = by_id[rid]
        assert row["deviceModel"] == "iPhone15,3" and row["platform"] == "iOS"
        assert row["score"] > 0 and not row["isSoftware"]
        summary.append({"job": job["id"], "resultID": rid, "workload": row["workload"],
                        "api": row["graphicsApi"], "score": row["score"], "unit": row["scoreUnit"],
                        "status": job["status"], "review": job["error"],
                        "thermalStart": job["thermalStateStart"], "thermalEnd": job["thermalStateEnd"]})
assert used == {r["id"] for r in after[24:]}

memory = next(by_id[j["resultIDs"][0]] for j in report["jobs"] if j["id"] == "vulkan/memory_sweep")
c = config(memory)
assert c["apiImplementation"] == "MoltenVK" and c["translatedTo"] == "Metal"
assert c["maxDatasetMiB"] == c["requestedMaxDatasetMiB"] == "512"
assert c["dataVerified"] == c["memoryPlateauReached"] == "1"
assert c["memoryBudgetPolicy"] == "ios_app_headroom_quarter_v1"
assert c["writeCopyChecks"] == "38" and c["timestampAnomalies"] == "0"
assert c["largeDatasetTraversal"] == "ordered_full_dataset_passes"
assert 2 * int(c["perBufferBudgetBytes"]) <= int(c["availableAppMemoryBytes"]) / 2
log = (ROOT / "phone-final/suite-runs" / (report["id"] + ".log")).read_text()
memory_points = [dict(p.split("=", 1) for p in line.split("\t")[1:])
                 for line in log.splitlines() if line.startswith("MEMBW_POINT\t")]
assert len(memory_points) == 83
tails = {}
for kernel in ("read", "write", "copy"):
    tail = [p for p in memory_points if p["kernel"] == kernel][-3:]
    assert [int(p["dataset_bytes"]) >> 20 for p in tail] == [256, 384, 512]
    assert all(int(p["passes"]) == int(p["dispatches_per_sample"]) >= 1 for p in tail)
    rates = [float(p["median_gbps"]) for p in tail]
    assert max(rates) / min(rates) <= 1.2
    tails[kernel] = rates

compute = [by_id[rid] for j in report["jobs"] if j["id"] == "cpu_compute" for rid in j["resultIDs"]]
core_summary = []
for kind in ("performance", "efficiency"):
    row = next(r for r in compute if config(r).get("coreType") == kind)
    c = config(row)
    residency = [float(c[f"round{i}RequestedLowerBound"]) for i in range(1, 4)]
    busy = [float(c[f"round{i}WorkerCpuSeconds"]) / float(c["roundSeconds"]) for i in range(1, 4)]
    assert c["residencyVerified"] == "1" and min(residency) >= .99 and min(busy) >= .90
    if kind == "efficiency":
        assert c["efficiencyPolicy"] == "foreground_with_performance_load"
        assert c["placementHelperThreads"] == "2"
        assert min(float(c[f"helper{i}RequestedLowerBound"]) for i in range(2)) >= .99
    core_summary.append({"coreType": kind, "score": row["score"],
                         "minimumResidencyLowerBound": min(residency),
                         "minimumWorkerBusyFraction": min(busy),
                         "roundEffectiveGHz": [float(c[f"round{i}EffectiveGHz"]) for i in range(1, 4)]})
all_core = next(r for r in compute if r["workload"] == "cpu_multi_core")
c = config(all_core)
assert c["threads"] == "6" and c["affinityMode"] == "scheduler_managed"
assert c["workerLabels"] == "unclassified_threads_not_numbered_cores" and c["pinnedThreads"] == "0"
workers = [float(v) for v in c["workerScores"].split(",")]
assert len(workers) == 6 and abs(sum(workers) - all_core["score"]) < .001

cpu_memory = next(by_id[j["resultIDs"][0]] for j in report["jobs"] if j["id"] == "cpu_memory")
c = config(cpu_memory)
assert c["residencyVerified"] == "1" and c["pageFaultMeasured"] == c["storageMeasured"] == "1"
assert float(c["pageFaultNsPerPage"]) > 0
assert all(float(c[key]) > 0 for key in ("storageSeqReadMBps", "storageSeqWriteMBps", "storageRandReadMBps", "storageRandWriteMBps"))
cpu_points = []
residency_points = []
for key, value in c.items():
    if key.startswith("curve_") and value:
        for pair in value.split(","):
            size, rate = pair.split(":")
            cpu_points.append({"series": key[6:], "datasetKiB": size, "GBps": rate})
    if key.startswith("residency_") and value:
        for item in value.split(","):
            size, kernel, lower, retries, ghz, pfraction, elapsed = item.split(":")
            assert float(lower) >= .99
            residency_points.append({"series": key, "datasetKiB": size, "kernel": kernel,
                                     "lowerBound": lower, "retries": retries,
                                     "effectiveGHz": ghz, "pFraction": pfraction, "elapsedMs": elapsed})
assert cpu_points and residency_points
cpu_memory_detail = {k: v for k, v in c.items() if k in (
    "single_memory_settled", "single_1_memory_settled", "all_memory_settled",
    "single_memory_read", "single_1_memory_read", "all_memory_read",
    "pageFaultNsPerPage", "pageBytes", "storageFileMiB", "storageSeqReadMBps",
    "storageSeqWriteMBps", "storageRandReadMBps", "storageRandWriteMBps", "storageDirectIo")}


cache = next(by_id[j["resultIDs"][0]] for j in report["jobs"] if j["id"] == "gpu_cache-v2")
c = config(cache)
assert c["cacheAttribution"] == "unverified" and c["checksumValid"] == "1"
cache_points = []
for key, value in c.items():
    if key.startswith("curve_") and value:
        for pair in value.split(","):
            size, measured = pair.split(":")
            cache_points.append({"condition": key[6:], "datasetKiB": size, "value": measured, "unit": cache["scoreUnit"]})
assert cache_points

timeline = []
sections = list(re.finditer(r"^=== \d+/\d+ (.+?): (?:passed|review) ===$", log, re.M))
for index, section in enumerate(sections):
    job_id = section.group(1)
    if "gpu_burn" not in job_id: continue
    block = log[section.end():sections[index + 1].start() if index + 1 < len(sections) else len(log)]
    for match in re.finditer(r"\[GPU Timing\] Compute: ([\d.]+) ms \| Render: ([\d.]+) ms \| Total: ([\d.]+) ms \| FPS: ([\d.]+) \| T: ([\d.]+) s", block):
        compute, render, total, fps, elapsed = map(float, match.groups())
        timeline.append({"job": job_id, "elapsedSec": elapsed, "computeMs": compute,
                         "renderMs": render, "totalMs": total, "fps": fps})
if timeline:
    with (ROOT / "gpu-burn-timeline.csv").open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(timeline[0])); writer.writeheader(); writer.writerows(timeline)

coverage = load(ROOT / "coverage-after.json")
assert coverage["planCount"] == coverage["completeCount"] == 31 and coverage["historyRows"] == 35
assert sum(len(j["resultIDs"]) for j in coverage["jobs"]) == 33
for entry in load(ROOT / "history-fingerprints-before.json"):
    assert hashlib.sha256(Path(entry["path"]).read_bytes()).hexdigest() == entry["sha256"], entry["path"]
assert load(ROOT / "stop-device.json")["info"]["outcome"] == "success"
for name, points in (("vulkan-memory-points.csv", memory_points), ("cpu-memory-curves.csv", cpu_points),
                     ("cpu-residency-points.csv", residency_points), ("gpu-cache-curves.csv", cache_points),
                     ("new-results-summary.csv", summary)):
    with (ROOT / name).open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(points[0])); writer.writeheader(); writer.writerows(points)
result = {"auditPassed": True, "device": "iPhone15,3 / A16", "newJobs": 9, "newRows": 11,
          "passed": sum(j["status"] == "passed" for j in report["jobs"]),
          "review": sum(j["status"] == "review" for j in report["jobs"]), "executionFailures": 0,
          "historyRows": 35, "oldRowsUnchanged": 24, "currentPlanCoverage": "31/31 jobs, 33 selected records",
          "readMemoryGBps": memory["score"], "vulkanMemoryPoints": len(memory_points),
          "largeDatasetTailsGBps": tails, "cpuSingleCores": core_summary,
          "cpuAllCore": {"score": all_core["score"], "workers": workers,
                         "typedPerWorkerParticipationVerified": False},
          "cpuMemoryGBps": cpu_memory["score"], "cpuMemoryPoints": len(cpu_points),
          "cpuMemoryDetails": cpu_memory_detail,
          "cpuResidencyPoints": len(residency_points), "gpuCachePoints": len(cache_points),
          "independentGpuCacheAttributionVerified": False,
          "oldCanonicalHistoriesAndLiveMacUnchanged": True, "newResults": summary}
(ROOT / "verification.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
print(json.dumps(result, ensure_ascii=False, indent=2))
