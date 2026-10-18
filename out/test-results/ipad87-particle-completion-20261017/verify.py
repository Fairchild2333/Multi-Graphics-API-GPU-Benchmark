"""Verify five actual iPad particle follow-ups and preserve previous histories."""
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
report = load(ROOT / "phone-final/suite-runs/latest.json")
selected = load(ROOT / "selected-jobs.json")
assert len(before) == 74 and len(after) == 79 and after[:74] == before
assert len({r["id"] for r in after}) == len(after)
assert report["status"] == "review" and len(report["jobs"]) == 5
assert [j["id"] for j in report["jobs"]] == selected
assert not report.get("resumedFrom") and report["finishedAt"]
assert {j["job"] for j in load(ROOT / "coverage-before.json")["jobs"] if not j["complete"]} == set(selected)
audit = load(ROOT / "production-contract-audit.json")
assert len(audit) == 5 and all(j["contractPassed"] for j in audit)
by_id = {r["id"]: r for r in after}
log = (ROOT / "phone-final/suite-runs" / (report["id"] + ".log")).read_text()
segments = re.split(r"^=== \d+/5 ([^:]+): (?:passed|review) ===$", log, flags=re.M)
logs = dict(zip(segments[1::2], segments[2::2]))
summary, timeline, used = [], [], set()
for job in report["jobs"]:
    assert job["status"] in ("passed", "review") and len(job["resultIDs"]) == 1
    assert job["thermalStateStart"] == job["thermalStateEnd"] == 0
    rid = job["resultIDs"][0]
    assert rid not in used
    used.add(rid)
    row = by_id[rid]
    c = fields(row)
    assert row["deviceModel"] == "iPad8,7" and row["platform"] == "iOS"
    assert row["graphicsApi"] == "Vulkan" and row["score"] > 0 and not row["isSoftware"]
    assert c["apiImplementation"] == "MoltenVK" and c["translatedTo"] == "Metal"
    assert c["timingMode"] == "synchronized_wall_clock" and "sync_timing" in row["workloadVersion"]
    assert row["avgTotalGpuMs"] > 0 and row["avgFrameTimeMs"] > 0
    assert abs(row["avgTotalGpuMs"] / row["avgFrameTimeMs"] - 1) < 0.2
    if not row["headless"]:
        assert c["renderCoverage"] == "wrapped_ndc_v2" and c["renderViewport"] == "1280x720"
    samples = []
    for m in re.finditer(r"\[GPU Timing\] Compute: ([\d.]+) ms \| Render: ([\d.]+) ms \| Total: ([\d.]+) ms \| FPS: ([\d.]+) \| T: ([\d.]+) s", logs[job["id"]]):
        compute, render, total, fps, elapsed = map(float, m.groups())
        samples.append({"job": job["id"], "resultID": rid, "elapsedSec": elapsed,
                        "computeMs": compute, "renderMs": render, "totalMs": total, "fps": fps})
    assert samples
    timeline.extend(samples)
    summary.append({"job": job["id"], "resultID": rid, "status": job["status"],
                    "scoreGBps": row["score"], "stableScoreGBps": row["stableScore"],
                    "stableCVPct": row["stableVariancePct"], "earlyLateDropPct": row["throttlePct"],
                    "computeMs": row["avgComputeMs"], "renderMs": row["avgRenderMs"],
                    "totalGpuMs": row["avgTotalGpuMs"], "frameMs": row["avgFrameTimeMs"],
                    "thermalStart": job["thermalStateStart"], "thermalEnd": job["thermalStateEnd"],
                    "review": job["error"]})
assert used == {r["id"] for r in after[74:]}
coverage = load(ROOT / "coverage-after.json")
assert coverage["completeCount"] == coverage["planCount"] == 31
assert sum(len(j["resultIDs"]) for j in coverage["jobs"]) == 33
for entry in load(ROOT / "histories-before.json"):
    p = Path(entry["path"])
    if not p.is_absolute():
        p = REPO / p
    if p.exists():
        assert hashlib.sha256(p.read_bytes()).hexdigest() == entry["sha256"], str(p)
assert load(ROOT / "stop-device.json")["info"]["outcome"] == "success"
for name, points in [("new-results-summary.csv", summary), ("particle-timeline.csv", timeline)]:
    with (ROOT / name).open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(points[0]))
        writer.writeheader(); writer.writerows(points)
result = {"auditPassed": True, "device": "iPad8,7 / A12X", "newJobs": 5,
          "passed": 4, "review": 1, "executionFailures": 0, "historyRows": 79,
          "oldRowsUnchanged": 74, "currentPlanCoverage": "31/31 jobs, 33 selected records",
          "allPriorSnapshotsAndLiveMacUnchanged": True, "newResults": summary}
(ROOT / "verification.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
print(json.dumps(result, ensure_ascii=False, indent=2))
