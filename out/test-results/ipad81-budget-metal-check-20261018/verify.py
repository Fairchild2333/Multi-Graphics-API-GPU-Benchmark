"""Audit actual iPad8,1 Metal follow-up and 512MiB Vulkan memory platform."""
from pathlib import Path
import csv
import hashlib
import json
import re

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
load = lambda p: json.loads(p.read_text())
fields = lambda r: dict(p.split("=", 1) for p in r["workloadConfig"].split(";") if "=" in p)

before = load(ROOT / "phone-before/results/results.json")
after = load(ROOT / "phone-final/results/results.json")
assert len(before) == 51 and len(after) == 53 and after[:51] == before
original = REPO / "out/test-results/ipad81-design-review-20261016/phone-final/results/results.json"
assert original.read_bytes() == (ROOT / "phone-before/results/results.json").read_bytes()
assert len({r["id"] for r in after}) == len(after)
report = load(ROOT / "phone-final/suite-runs/latest.json")
assert report["status"] == "passed" and report["finishedAt"] and not report.get("resumedFrom")
assert [j["id"] for j in report["jobs"]] == load(ROOT / "selected-jobs.json")
assert all(j["status"] == "passed" and j["thermalStateStart"] == j["thermalStateEnd"] == 0 for j in report["jobs"])
assert {i for j in report["jobs"] for i in j["resultIDs"]} == {r["id"] for r in after[51:]}
audit = load(ROOT / "production-contract-audit.json")
assert len(audit) == 2 and all(j["contractPassed"] for j in audit)
metal, memory = after[-2:]
for row in (metal, memory):
    assert row["deviceModel"] == "iPad8,1" and row["platform"] == "iOS" and not row["isSoftware"]
    assert row["score"] > 0
assert metal["graphicsApi"] == "Metal" and metal["headless"] and metal["particleCount"] == 16777216
assert fields(metal)["timingMode"] == "command_buffer_gpu_time"
assert metal["stableScore"] > 0 and 0 <= metal["stableVariancePct"] <= 2
assert abs(metal["avgTotalGpuMs"] / metal["avgFrameTimeMs"] - 1) < 0.02
c = fields(memory)
assert memory["graphicsApi"] == "Vulkan" and memory["workloadVersion"].startswith("membw_v3_streaming_ios_preview_app_budget_v1")
assert c["maxDatasetMiB"] == c["requestedMaxDatasetMiB"] == "512"
assert c["memoryPlateauReached"] == c["dataVerified"] == "1"
assert c["writeCopyChecks"] == "38" and c["timestampAnomalies"] == "0"
assert c["memoryBudgetPolicy"] == "ios_app_headroom_quarter_v1"
assert 2 * int(c["perBufferBudgetBytes"]) <= int(c["availableAppMemoryBytes"]) / 2
assert c["largeDatasetTraversal"] == "ordered_full_dataset_passes"
log = (ROOT / "phone-final/suite-runs" / (report["id"] + ".log")).read_text()
points = [dict(p.split("=", 1) for p in line.split("\t")[1:]) for line in log.splitlines() if line.startswith("MEMBW_POINT\t")]
assert len(points) == 83
tails = {}
for kernel in ("read", "write", "copy"):
    tail = [p for p in points if p["kernel"] == kernel][-3:]
    assert [int(p["dataset_bytes"]) >> 20 for p in tail] == [256, 384, 512]
    assert all(int(p["passes"]) == int(p["dispatches_per_sample"]) >= 1 for p in tail)
    rates = [float(p["median_gbps"]) for p in tail]
    assert max(rates) / min(rates) <= 1.2
    tails[kernel] = rates
with (ROOT / "vulkan-memory-points.csv").open("w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=list(points[0])); writer.writeheader(); writer.writerows(points)
timeline = [dict(zip(["computeMs", "renderMs", "totalMs", "fps", "elapsedSec"], map(float, m.groups())))
            for m in re.finditer(r"\[GPU Timing\] Compute: ([\d.]+) ms \| Render: ([\d.]+) ms \| Total: ([\d.]+) ms \| FPS: ([\d.]+) \| T: ([\d.]+) s", log)]
assert timeline
with (ROOT / "metal-timeline.csv").open("w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=list(timeline[0])); writer.writeheader(); writer.writerows(timeline)
coverage = load(ROOT / "coverage-after.json")
assert coverage["completeCount"] == coverage["planCount"] == 31
assert sum(len(j["resultIDs"]) for j in coverage["jobs"]) == 33
for entry in load(ROOT / "histories-before.json"):
    p = Path(entry["path"])
    if not p.is_absolute(): p = REPO / p
    if p.exists(): assert hashlib.sha256(p.read_bytes()).hexdigest() == entry["sha256"], str(p)
assert load(ROOT / "stop-device.json")["info"]["outcome"] == "success"
result = {"auditPassed": True, "device": "iPad8,1 / A12X", "newJobs": 2, "passed": 2,
          "review": 0, "executionFailures": 0, "historyRows": 53, "restoredOldRowsUnchanged": 51,
          "currentPlanCoverage": "31/31 jobs, 33 selected records", "metal16MGBps": metal["score"],
          "metalStableCVPct": metal["stableVariancePct"], "vulkanReadMemoryGBps": memory["score"],
          "vulkanMemoryPoints": len(points), "writeCopyChecks": 38, "largeDatasetTailsGBps": tails,
          "allPriorSnapshotsAndLiveMacUnchanged": True}
(ROOT / "verification.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
print(json.dumps(result, ensure_ascii=False, indent=2))
