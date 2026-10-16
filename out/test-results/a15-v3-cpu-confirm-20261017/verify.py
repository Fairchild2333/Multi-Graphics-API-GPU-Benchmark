"""Audit the two actual A15 runs without altering any benchmark record."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]


def load(path):
    return json.loads(path.read_text())


def config(row):
    return dict(x.split("=", 1) for x in row["workloadConfig"].split(";") if "=" in x)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


before = load(ROOT / "phone-before/results/results.json")
after_path = ROOT / "phone-final/results/results.json"
after = load(after_path)
assert len(before) == 128 and len(after) == 132
assert after[:len(before)] == before
assert len({r["id"] for r in after}) == len(after)
manifests = load(ROOT / "manifests.json")
assert [r["jobs"][0]["id"] for r in manifests] == ["vulkan/memory_sweep", "cpu_compute"]
assert all(r["jobs"][0]["thermalStateStart"] == r["jobs"][0]["thermalStateEnd"] == 0 for r in manifests)
audit = load(ROOT / "production-contract-audit.json")
assert all(a["contractPassed"] for a in audit)
new = {r["id"]: r for r in after[len(before):]}
assert set(new) == {i for m in manifests for i in m["jobs"][0]["resultIDs"]}
mem = next(r for r in new.values() if r["workload"] == "membw")
mc = config(mem)
assert mem["workloadVersion"] == "membw_v3_streaming_ios_preview_moltenvk"
assert mc["dataVerified"] == "1" and mc["writeCopyChecks"] == "34"
assert mc["timestampAnomalies"] == "0" and mc["timing"] == "wall_clock"
assert mc["apiImplementation"] == "MoltenVK" and mc["translatedTo"] == "Metal"
assert mc["maxDatasetMiB"] == "256" and mc["memoryPlateauReached"] == "0"
assert mc["largeDatasetTraversal"] == "ordered_full_dataset_passes"
assert audit[0]["status"] == "review" and audit[0]["vulkanMemoryPoints"] == 77
cpu = [r for r in new.values() if r["graphicsApi"] == "CPU"]
assert len(cpu) == 3 and audit[1]["status"] == "passed"
cores = []
for kind in ["performance", "efficiency"]:
    row = next(r for r in cpu if config(r).get("coreType") == kind)
    c = config(row)
    residency = [float(c[f"round{i}RequestedLowerBound"]) for i in range(1, 4)]
    busy = [float(c[f"round{i}WorkerCpuSeconds"]) / float(c["roundSeconds"]) for i in range(1, 4)]
    assert c["residencyVerified"] == "1" and min(residency) >= .99 and min(busy) >= .90
    old = next(r for r in reversed(before) if r["graphicsApi"] == "CPU" and config(r).get("coreType") == kind)
    if kind == "efficiency":
        assert c["efficiencyPolicy"] == "foreground_with_performance_load" and c["placementHelperThreads"] == "2"
        assert min(float(c[f"helper{i}RequestedLowerBound"]) for i in range(2)) >= .99
    cores.append({"coreType": kind, "resultID": row["id"], "score": row["score"],
                  "priorScore": old["score"], "changePercent": (row["score"] / old["score"] - 1) * 100,
                  "minimumResidencyLowerBound": min(residency), "minimumWorkerBusyFraction": min(busy),
                  "roundEffectiveGHz": [float(c[f"round{i}EffectiveGHz"]) for i in range(1, 4)]})
all_core = next(r for r in cpu if r["workload"] == "cpu_multi_core")
c = config(all_core)
assert c["threads"] == "6"
# A15 intentionally keeps the scheduler-managed path. The typed scheduling
# contract is enabled for A12X-class accounting, not this device. Do not invent
# per-worker type/occupancy validation that this run did not save.
assert c["workerLabels"] == "unclassified_threads_not_numbered_cores"
assert c["affinityMode"] == "scheduler_managed" and c["pinnedThreads"] == "0"
assert "allCoreParticipationVerified" not in c
workers = [float(v) for v in c["workerScores"].split(",")]
assert len(workers) == 6 and abs(sum(workers) - all_core["score"]) < .0005
others = []
for device in load(ROOT / "hardware-histories-before.json"):
    p = REPO / "out/test-results" / device["path"]
    assert sha(p) == device["sha256"]
    others.append({"device": device["device"], "sha256": sha(p), "unchanged": True})
assert sha(Path.home() / "Library/Application Support/GpuComputeBenchmark/results/results.json") == others[0]["sha256"]
old_all = next(r for r in reversed(before) if r["workload"] == "cpu_multi_core")
verification = {"status": "passed", "jobs": 2, "newRows": 4, "historyRows": len(after),
    "oldRowsUnchanged": len(before), "physicalDevice": "iPhone14,2 / A15", "productionSourceChanged": False,
    "memory": {"score": mem["score"], "unit": "GB/s", "status": "review", "platformReached": False,
               "maxDatasetMiB": 256, "scanLimitReason": "Existing iOS software preview cap, not hardware buffer limit",
               "curvePoints": 77, "dataChecks": 34, "timestampAnomalies": 0},
    "singleCore": cores, "allCore": {"score": all_core["score"], "priorScore": old_all["score"],
        "changePercent": (all_core["score"] / old_all["score"] - 1) * 100,
        "workerScores": workers, "threads": 6, "scheduling": "scheduler_managed",
        "typedPerWorkerParticipationVerified": False,
        "processCpuSecondsSelectedRound": float(c["processCpuSeconds"]),
        "performanceCpuSecondsSelectedRound": float(c["performanceCpuSeconds"]),
        "accountingIncludesWarmupSeconds": float(c["accountingIncludesWarmup"])},
    "thermal": "nominal -> nominal for each job", "previousCanonicalArchives": others,
    "beforeSha256": sha(ROOT / "phone-before/results/results.json"), "afterSha256": sha(after_path)}
(ROOT / "verification.json").write_text(json.dumps(verification, indent=2) + "\n")
print(json.dumps(verification, indent=2))
