"""Verify the physical A15 follow-ups and coverage using exported production data."""
from pathlib import Path
import csv
import hashlib
import json
import re

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
load = lambda p: json.loads(p.read_text())
fields = lambda row: dict(p.split("=", 1) for p in row["workloadConfig"].split(";") if "=" in p)

before = load(ROOT / "phone-before/results/results.json")
after = load(ROOT / "phone-final/results/results.json")
reports = load(ROOT / "manifests.json")
pending = load(ROOT / "pending-jobs.json")
assert len(before) == 132 and len(after) == 141 and after[:132] == before
assert len({r["id"] for r in after}) == len(after)
assert len(reports) == len(pending) == 9
assert {j["job"] for j in load(ROOT / "coverage-before.json")["jobs"] if not j["complete"]} == set(pending)
audit = load(ROOT / "production-contract-audit.json")
assert len(audit) == 9 and all(j["contractPassed"] for j in audit)
by_id = {r["id"]: r for r in after}
used = set()
summary = []
timeline = []
memory_points = []
for index, (report, job_id) in enumerate(zip(reports, pending), 1):
    assert len(report["jobs"]) == 1 and not report.get("resumedFrom")
    job = report["jobs"][0]
    assert job["id"] == job_id and len(job["resultIDs"]) == 1
    assert report["status"] == job["status"] in ("passed", "review")
    assert job["thermalStateStart"] == job["thermalStateEnd"] == 0
    assert not used.intersection(job["resultIDs"])
    used.update(job["resultIDs"])
    row = by_id[job["resultIDs"][0]]
    c = fields(row)
    assert row["deviceModel"] == "iPhone14,2" and row["platform"] == "iOS"
    assert row["graphicsApi"] == "Vulkan" and not row["isSoftware"] and row["score"] > 0
    assert c["apiImplementation"] == "MoltenVK" and c["translatedTo"] == "Metal"
    folder = ROOT / (f"{index:02d}-" + job_id.replace("/", "-"))
    log = (folder / "documents/suite-runs" / (report["id"] + ".log")).read_text()
    if job_id == "vulkan/memory_sweep":
        assert job["status"] == "passed" and c["memoryPlateauReached"] == "1"
        assert c["memoryBudgetPolicy"] == "ios_app_headroom_quarter_v1"
        assert c["maxDatasetMiB"] == c["requestedMaxDatasetMiB"] == "512"
        assert c["streamingMinMiB"] == "256" and c["largeDatasetTraversal"] == "ordered_full_dataset_passes"
        assert c["dataVerified"] == "1" and c["writeCopyChecks"] == "38" and c["timestampAnomalies"] == "0"
        assert 2 * int(c["perBufferBudgetBytes"]) <= int(c["availableAppMemoryBytes"]) / 2
        memory_points = [dict(p.split("=", 1) for p in line.split("\t")[1:])
                         for line in log.splitlines() if line.startswith("MEMBW_POINT\t")]
        assert len(memory_points) == audit[index - 1]["vulkanMemoryPoints"] == 83
        for kernel in ("read", "write", "copy"):
            tail = [p for p in memory_points if p["kernel"] == kernel][-3:]
            assert [int(p["dataset_bytes"]) >> 20 for p in tail] == [256, 384, 512]
            assert all(int(p["passes"]) == int(p["dispatches_per_sample"]) >= 1 for p in tail)
            rates = [float(p["median_gbps"]) for p in tail]
            assert max(rates) / min(rates) <= 1.2
    else:
        assert c["timingMode"] == "synchronized_wall_clock"
        assert "sync_timing" in row["workloadVersion"]
        assert row["avgTotalGpuMs"] > 0 and row["avgFrameTimeMs"] > 0
        if not row["headless"]:
            assert c["renderCoverage"] == "wrapped_ndc_v2" and c["renderViewport"] == "1280x720"
        for match in re.finditer(r"\[GPU Timing\] Compute: ([\d.]+) ms \| Render: ([\d.]+) ms \| Total: ([\d.]+) ms \| FPS: ([\d.]+) \| T: ([\d.]+) s", log):
            compute, render, total, fps, elapsed = map(float, match.groups())
            timeline.append({"job": job_id, "resultID": row["id"], "elapsedSec": elapsed,
                             "computeMs": compute, "renderMs": render, "totalMs": total, "fps": fps})
    summary.append({"job": job_id, "resultID": row["id"], "status": job["status"],
                    "scoreGBps": row["score"], "stableScoreGBps": row["stableScore"],
                    "stableCVPct": row["stableVariancePct"], "earlyLateDropPct": row["throttlePct"],
                    "gpuMs": row["avgTotalGpuMs"], "frameMs": row["avgFrameTimeMs"],
                    "thermalStart": job["thermalStateStart"], "thermalEnd": job["thermalStateEnd"],
                    "review": job["error"]})
assert used == {r["id"] for r in after[132:]}
coverage = load(ROOT / "coverage-after.json")
assert coverage["planCount"] == coverage["completeCount"] == 31 and coverage["historyRows"] == 141
assert sum(len(j["resultIDs"]) for j in coverage["jobs"]) == 33
for entry in load(ROOT / "histories-before.json"):
    path = Path(entry["path"])
    if not path.is_absolute():
        path = REPO / path
    if path.exists():
        assert hashlib.sha256(path.read_bytes()).hexdigest() == entry["sha256"], str(path)
for name in ("build-ios", "signed-build", "run", "stream-run"):
    assert load(ROOT / (name + "-exit.json"))["returnCode"] == 0
assert load(ROOT / "stop-device.json")["info"]["outcome"] == "success"
for filename, points in [("vulkan-memory-points.csv", memory_points), ("particle-timeline.csv", timeline),
                         ("new-results-summary.csv", summary)]:
    with (ROOT / filename).open("w", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=list(points[0]))
        writer.writeheader(); writer.writerows(points)
result = {"auditPassed": True, "device": "iPhone14,2 / A15", "newJobs": 9,
          "newPassed": sum(j["status"] == "passed" for j in summary),
          "newReview": sum(j["status"] == "review" for j in summary), "executionFailures": 0,
          "historyRows": len(after), "oldRowsUnchanged": len(before), "currentPlanCoverage": "31/31 jobs, 33 selected records",
          "readMemoryGBps": after[132]["score"], "memoryCurvePoints": 83, "memoryDataChecks": 38,
          "oldCanonicalHistoriesAndLiveMacUnchanged": True, "newResults": summary}
(ROOT / "verification.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
print(json.dumps(result, ensure_ascii=False, indent=2))
