"""Audit this iPad's one actual V3 sweep; retain every earlier observation."""
from pathlib import Path
import csv
import hashlib
import json

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
load = lambda p: json.loads(p.read_text())
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
before_path = ROOT / "phone-before/results/results.json"
after_path = ROOT / "phone-final/results/results.json"
before, after = load(before_path), load(after_path)
assert len(before) == 72 and len(after) == 73 and after[:72] == before
assert len({r["id"] for r in after}) == 73
manifests = load(ROOT / "manifests.json")
assert len(manifests) == 1 and len(manifests[0]["jobs"]) == 1
job = manifests[0]["jobs"][0]
assert job["id"] == "vulkan/memory_sweep" and job["resultIDs"] == [after[-1]["id"]]
assert job["status"] == manifests[0]["status"] == "review"
row = after[-1]
c = dict(v.split("=", 1) for v in row["workloadConfig"].split(";") if "=" in v)
assert row["deviceModel"] == "iPad8,7" and row["graphicsApi"] == "Vulkan"
assert row["workloadVersion"] == "membw_v3_streaming_ios_preview_moltenvk"
assert c["dataVerified"] == "1" and c["writeCopyChecks"] == "34"
assert c["timestampAnomalies"] == "0" and c["memoryPlateauReached"] == "0"
assert c["maxDatasetMiB"] == "256" and c["largeDatasetTraversal"] == "ordered_full_dataset_passes"
assert c["apiImplementation"] == "MoltenVK" and c["translatedTo"] == "Metal"
assert c["timing"] == "wall_clock"
audit = load(ROOT / "production-contract-audit.json")
assert len(audit) == 1 and audit[0]["contractPassed"] and audit[0]["vulkanMemoryPoints"] == 77
log = (ROOT / "phone-final/suite-runs" / (manifests[0]["id"] + ".log")).read_text()
points = []
for line in log.splitlines():
    if line.startswith("MEMBW_POINT\t"):
        points.append(dict(v.split("=", 1) for v in line.split("\t")[1:]))
assert len(points) == 77
assert all(float(p["median_gbps"]) > 0 and float(p["median_ms"]) > 0 for p in points)
assert all(any(p["kernel"] == kind and p["dataset_bytes"] == str(256 << 20) for p in points)
           for kind in ["read", "write", "copy"])
with (ROOT / "vulkan-memory-points.csv").open("w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=list(points[0])); writer.writeheader(); writer.writerows(points)
prior = load(ROOT / "histories-before.json")
for device in prior["devices"]:
    p = REPO / "out/test-results" / device["path"]
    assert sha(p) == device["sha256"]
live = Path.home() / "Library/Application Support/GpuComputeBenchmark/results/results.json"
assert sha(live) == prior["devices"][0]["sha256"]
old = next(r for r in reversed(before) if r["graphicsApi"] == "Vulkan" and r["workload"] == "membw")
result = {"auditPassed": True, "deviceModel": "iPad8,7", "jobsExecuted": 1, "newRows": 1,
          "oldRowsUnchanged": 72, "historyRows": 73, "resultID": row["id"],
          "measurementStatus": "review", "reason": job["error"], "readReferenceGBps": row["score"],
          "writeReferenceGBps": float(c["writeMemoryGBps"]), "copyReferenceGBps": float(c["copyMemoryGBps"]),
          "workloadVersion": row["workloadVersion"], "durationSeconds": row["durationSec"],
          "memoryPlateauReached": False, "curvePoints": 77, "dataChecks": 34, "timestampAnomalies": 0,
          "maxDatasetMiB": 256, "limitReason": "Existing iOS software preview cap; not an established hardware buffer ceiling",
          "thermalStart": job["thermalStateStart"], "thermalEnd": job["thermalStateEnd"],
          "priorVulkanMemory": {"score": old["score"], "version": old["workloadVersion"], "id": old["id"]},
          "beforeSha256": sha(before_path), "afterSha256": sha(after_path),
          "allEarlierCanonicalSnapshotsUnchanged": True, "liveMacHistoryUnchanged": True,
          "productionCodeChanged": False}
(ROOT / "verification.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
print(json.dumps(result, ensure_ascii=False, indent=2))
