"""Audit the single physical Metal pass, comparison, and immutable histories."""
from pathlib import Path
import csv
import hashlib
import json
import re

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
load = lambda p: json.loads(p.read_text())
config = lambda r: dict(p.split("=", 1) for p in r["workloadConfig"].split(";") if "=" in p)
before = load(ROOT / "phone-before/results/results.json")
after = load(ROOT / "phone-final/results/results.json")
report = load(ROOT / "manifest.json")
selected = load(ROOT / "selected-jobs.json")
old = {item["job"]: item["result"] for item in load(ROOT / "old-selected.json")}
assert len(before) == 141 and len(after) == 155 and after[:141] == before
assert len({r["id"] for r in after}) == len(after)
assert len(selected) == len(report["jobs"]) == 14 and [j["id"] for j in report["jobs"]] == selected
assert report["status"] in ["passed", "review"] and report["finishedAt"] and not report.get("resumedFrom")
by_id = {r["id"]: r for r in after}
audit = load(ROOT / "production-contract-audit.json")
assert len(audit) == 14 and all(item["contractPassed"] for item in audit)
new_ids = set()
comparison = []
memory_curves = []
cache_curves = []
for job in report["jobs"]:
    assert job["status"] in ["passed", "review"] and len(job["resultIDs"]) == 1
    assert job["thermalStateStart"] == 0
    row = by_id[job["resultIDs"][0]]
    assert row["id"] not in new_ids
    new_ids.add(row["id"])
    assert row["graphicsApi"] == "Metal" and row["platform"] == "iOS"
    assert row["deviceModel"] == "iPhone14,2" and not row["isSoftware"] and row["score"] > 0
    prior = old[job["id"]]
    assert row["scoreUnit"] == prior["scoreUnit"]
    c = config(row)
    if job["id"].startswith("stream-"):
        assert c["renderCoverage"] == "wrapped_ndc_v2"
    if job["id"] == "memory_sweep":
        assert c["memoryPlateauVerified"] == "0" and "preview" in row["workloadVersion"]
        assert next(a for a in audit if a["job"] == job["id"])["metalMemoryPoints"] > 0
        for key, value in c.items():
            point = re.fullmatch(r"((?:device_local|host_visible)_(?:read_only|read_modify_write))_(\d+)", key)
            if not point:
                continue
            memory_curves.append({"resultID": row["id"], "curve": point.group(1),
                                  "datasetKiB": int(point.group(2)) / 1024, "GBps": float(value)})
    if job["id"] == "gpu_cache-v2":
        assert c["cacheAttribution"] == "unverified" and c["checksumValid"] == "1"
        assert next(a for a in audit if a["job"] == job["id"])["metalCachePoints"] > 0
        for key, value in c.items():
            if not key.startswith("curve_") or not value:
                continue
            for pair in value.split(","):
                size, measured = pair.split(":")
                cache_curves.append({"resultID": row["id"], "condition": key, "datasetKiB": size,
                                     "value": measured, "unit": row["scoreUnit"]})
    comparison.append({"job": job["id"], "oldResultID": prior["id"], "newResultID": row["id"],
                       "unit": row["scoreUnit"], "oldScore": prior["score"], "newScore": row["score"],
                       "scoreChangePct": 100 * (row["score"] / prior["score"] - 1),
                       "status": job["status"], "earlyLateDropPct": row["throttlePct"],
                       "stableScore": row["stableScore"], "stableCVPct": row["stableVariancePct"],
                       "thermalStart": job["thermalStateStart"], "thermalEnd": job["thermalStateEnd"],
                       "review": job["error"], "oldVersion": prior["workloadVersion"], "newVersion": row["workloadVersion"]})
assert new_ids == {r["id"] for r in after[141:]}
coverage = load(ROOT / "coverage-after.json")
assert coverage["planCount"] == coverage["completeCount"] == 31 and coverage["historyRows"] == 155
assert sum(len(j["resultIDs"]) for j in coverage["jobs"]) == 33
for entry in load(ROOT / "histories-before.json"):
    path = Path(entry["path"])
    if not path.is_absolute():
        path = REPO / path
    if path.exists():
        assert hashlib.sha256(path.read_bytes()).hexdigest() == entry["sha256"], str(path)
assert load(ROOT / "run-exit.json")["returnCode"] == 0
assert load(ROOT / "stop-device.json")["info"]["outcome"] == "success"
for name, rows in [("comparison.csv", comparison), ("metal-memory-curves.csv", memory_curves),
                   ("metal-cache-curves.csv", cache_curves)]:
    assert rows, name
    with (ROOT / name).open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader(); writer.writerows(rows)
result = {"auditPassed": True, "device": "iPhone14,2 / A15", "newMetalJobs": 14,
          "passed": sum(j["status"] == "passed" for j in report["jobs"]),
          "review": sum(j["status"] == "review" for j in report["jobs"]), "executionFailures": 0,
          "historyRows": 155, "oldRowsUnchanged": 141, "currentPlanCoverage": "31/31 jobs, 33 records",
          "metalMemoryPoints": len(memory_curves), "metalCachePoints": len(cache_curves),
          "metalMemoryPlateauVerified": False, "cacheAttribution": "unverified",
          "oldCanonicalHistoriesAndLiveMacUnchanged": True, "comparison": comparison}
(ROOT / "verification.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
print(json.dumps(result, ensure_ascii=False, indent=2))
