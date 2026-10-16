"""Verify actual iPad platform, simulator fallback and immutable histories."""
from pathlib import Path
import csv
import hashlib
import json

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
load = lambda p: json.loads(p.read_text())
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
fields = lambda row: dict(v.split("=", 1) for v in row["workloadConfig"].split(";") if "=" in v)
before = load(ROOT / "phone-before/results/results.json")
after_path = ROOT / "phone-final/results/results.json"
after = load(after_path)
assert len(before) == 73 and len(after) == 74 and after[:73] == before
assert len({r["id"] for r in after}) == 74
manifest = load(ROOT / "phone-final/suite-runs/latest.json")
assert manifest["status"] == "passed" and len(manifest["jobs"]) == 1
job = manifest["jobs"][0]
assert job["id"] == "vulkan/memory_sweep" and job["status"] == "passed"
assert job["thermalStateStart"] == job["thermalStateEnd"] == 0
row = after[-1]; c = fields(row)
assert job["resultIDs"] == [row["id"]] and row["deviceModel"] == "iPad8,7"
assert row["workloadVersion"] == "membw_v3_streaming_ios_preview_app_budget_v1_moltenvk"
assert c["dataVerified"] == "1" and c["writeCopyChecks"] == "38" and c["timestampAnomalies"] == "0"
assert c["memoryPlateauReached"] == "1" and c["score"] == "read_memory_plateau"
assert c["maxDatasetMiB"] == c["requestedMaxDatasetMiB"] == "512"
assert c["memoryBudgetPolicy"] == "ios_app_headroom_quarter_v1"
assert 2 * int(c["perBufferBudgetBytes"]) <= int(c["availableAppMemoryBytes"]) / 2
assert int(c["perBufferBudgetBytes"]) == 512 << 20
audit = load(ROOT / "production-contract-audit.json")
assert len(audit) == 1 and audit[0]["contractPassed"] and audit[0]["vulkanMemoryPoints"] == 83
assert audit[0]["status"] == "passed" and audit[0]["reviewReasons"] == []
log = (ROOT / "phone-final/suite-runs" / (manifest["id"] + ".log")).read_text()
points = [dict(v.split("=", 1) for v in line.split("\t")[1:]) for line in log.splitlines()
          if line.startswith("MEMBW_POINT\t")]
assert len(points) == 83
tails = {}
for kind in ["read", "write", "copy"]:
    tail = [p for p in points if p["kernel"] == kind][-3:]
    assert [int(p["dataset_bytes"]) >> 20 for p in tail] == [256, 384, 512]
    assert all(int(p["dispatches_per_sample"]) >= 1 and int(p["passes"]) == int(p["dispatches_per_sample"]) for p in tail)
    rates = [float(p["median_gbps"]) for p in tail]
    assert min(rates) > 0 and max(rates) / min(rates) <= 1.2
    tails[kind] = [{"datasetMiB": int(p["dataset_bytes"]) >> 20,
                    "GBps": float(p["median_gbps"]), "completePassesPerSample": int(p["dispatches_per_sample"])} for p in tail]
with (ROOT / "vulkan-memory-points.csv").open("w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=list(points[0])); writer.writeheader(); writer.writerows(points)
sim_docs = ROOT / "simulator/final-documents"
sim_manifest = load(sim_docs / "suite-runs/latest.json")
assert sim_manifest["status"] == sim_manifest["jobs"][0]["status"] == "review"
sim = next(r for r in load(sim_docs / "results/results.json") if r["id"] in sim_manifest["jobs"][0]["resultIDs"])
s = fields(sim)
assert sim["workloadVersion"] == row["workloadVersion"]
assert s["requestedMaxDatasetMiB"] == "512" and s["maxDatasetMiB"] == "256"
assert s["availableAppMemoryBytes"] == "0" and int(s["perBufferBudgetBytes"]) == 256 << 20
assert s["memoryPlateauReached"] == "0" and s["dataVerified"] == "1" and s["writeCopyChecks"] == "34"
assert all(b["returnCode"] == 0 for b in load(ROOT / "build-results.json"))
assert load(ROOT / "signed-build-exit.json")["returnCode"] == 0
assert "100% tests passed" in (ROOT / "native-ctest.log").read_text()
assert "boundaries verified" in (ROOT / "swift-regression/run-selected-fixture.log").read_text()
prior = load(ROOT / "histories-before.json")
for d in prior["devices"]:
    p = REPO / "out/test-results" / d["path"]
    assert sha(p) == d["sha256"]
live = Path.home() / "Library/Application Support/GpuComputeBenchmark/results/results.json"
assert sha(live) == prior["devices"][0]["sha256"]
assert load(ROOT / "stop-device.json")["info"]["outcome"] == "success"
result = {"auditPassed": True, "physicalDevice": "iPad8,7 / A12X", "historyRows": 74,
          "oldRowsUnchanged": 73, "resultID": row["id"], "status": "passed",
          "readMemoryGBps": row["score"], "writeMemoryGBps": float(c["writeMemoryGBps"]),
          "copyMemoryGBps": float(c["copyMemoryGBps"]), "curvePoints": 83, "dataChecks": 38,
          "requestedMaxDatasetMiB": 512, "actualMaxDatasetMiB": 512,
          "availableAppMemoryBytes": int(c["availableAppMemoryBytes"]),
          "perBufferBudgetBytes": int(c["perBufferBudgetBytes"]), "tails": tails,
          "thermal": "nominal -> nominal", "simulatorFallback": "unknown app headroom -> 256 MiB; preview/review retained",
          "nativeCTest": "7/7 passed", "iOSDeviceAndSimulatorReleaseBuilds": "passed",
          "swiftManifestRegression": "passed; old fixed-cap version and missing budget policy rejected",
          "beforeSha256": sha(ROOT / "phone-before/results/results.json"), "afterSha256": sha(after_path),
          "allEarlierCanonicalSnapshotsUnchanged": True, "liveMacHistoryUnchanged": True,
          "A15NewProfileRetested": False, "otherIPadNewProfileRetested": False}
(ROOT / "verification.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
print(json.dumps(result, ensure_ascii=False, indent=2))
