#!/usr/bin/env python3
"""Audit physical Ultra exports without mixing simulator or interrupted scores."""
from pathlib import Path
import hashlib
import json
import math

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]


def read(path):
    return json.loads(path.read_text())


def config(row):
    return dict(p.split("=", 1) for p in row["workloadConfig"].split(";") if "=" in p)


def main():
    data = ROOT / "watch-final"
    rows = read(data / "results/results.json")
    suite = read(data / "suite-runs/latest.json")
    by_id = {r["id"]: r for r in rows}
    test = read(ROOT / "physical-system-settings-summary.json")
    assert test["result"] == "Passed" and test["failedTests"] == 0
    assert test["devicesAndConfigurations"][0]["device"]["architecture"] == "arm64_32"
    assert len(by_id) == len(rows)
    assert suite["executionEnvironment"] == "device" and suite["profile"] == "standard"
    assert suite["status"] in ("passed", "review") and len(suite["jobs"]) == 2
    for index, job in enumerate(suite["jobs"]):
        assert job["status"] in ("passed", "review")
        assert len(job["resultIDs"]) == (2 if index == 0 else 1)
    selected = [by_id[i] for j in suite["jobs"] for i in j["resultIDs"]]
    assert {r["workload"] for r in selected} == {"cpu_single_core", "cpu_multi_core", "cpu_membw"}
    for row in selected:
        c = config(row)
        assert row["platform"] == "watchOS" and row["deviceModel"] == "Watch6,18"
        assert row["osVersion"].startswith("watchOS 26.5")
        # Original exporter missed Apple's __ILP32__ macro. Preserve its raw
        # ARM64 label; XCTest and native _32bit versions establish actual ABI.
        assert row["processArchitecture"] in ("ARM64", "ARM64_32") and row["isSoftware"] is False
        assert "watchos_preview" in row["workloadVersion"]
        assert "simulator" not in row["workloadVersion"] and "ui_smoke" not in row["workloadVersion"]
        assert c["watchRunProfile"] == "standard" and c["corePlacement"] == "scheduler_managed_unclassified"
        assert math.isfinite(row["score"]) and row["score"] > 0
        if row["workload"].startswith("cpu_") and row["workload"] != "cpu_membw":
            assert float(c["totalMeasureSeconds"]) == 15 and int(c["rounds"]) == 3
            assert int(c["logicalProcessors"]) == 2
            assert "_32bit_" in row["workloadVersion"]
    multi = next(r for r in selected if r["workload"] == "cpu_multi_core")
    c = config(multi)
    workers = list(map(float, c["workerScores"].split(",")))
    assert int(c["threads"]) == len(workers) == 2
    assert math.isclose(sum(workers), multi["score"], abs_tol=0.00051)
    memory = next(r for r in selected if r["workload"] == "cpu_membw")
    c = config(memory)
    assert c["pageFaultMeasured"] == c["storageMeasured"] == "1"
    assert int(c["storageFileMiB"]) == 32 and float(c["maxDatasetMiB"]) <= 16
    curves = {}
    for key, value in c.items():
        if not key.startswith("curve_") or not value:
            continue
        points = [tuple(map(float, p.split(":"))) for p in value.split(",")]
        assert points and all(0 < x <= 16384 and math.isfinite(y) and y >= 0 for x, y in points)
        assert all(b[0] > a[0] for a, b in zip(points, points[1:]))
        curves[key] = len(points)
    assert len(curves) == 6
    source = "single_memory" if c["score"] == "single_thread_memory_read" else "all_memory"
    if c[source + "_settled"] != "1":
        assert suite["status"] == suite["jobs"][1]["status"] == "review"
    histories = []
    for old in read(ROOT / "other-histories-before.json"):
        p = Path(old["path"])
        if not p.is_absolute():
            p = REPO / p
        if p.exists():
            assert hashlib.sha256(p.read_bytes()).hexdigest() == old["sha256"], str(p)
            histories.append(str(p))
    print(json.dumps({"result": "passed", "physicalDevice": "Watch6,18", "suiteID": suite["id"],
                      "actualArchitecture": "arm64_32", "rawArchitectureLabels": sorted({r["processArchitecture"] for r in selected}),
                      "suiteStatus": suite["status"], "historyRows": len(rows),
                      "selectedResults": [{"id": r["id"], "workload": r["workload"], "score": r["score"],
                                           "unit": r["scoreUnit"]} for r in selected],
                      "curvePoints": curves, "unchangedHistories": histories}, indent=2))


if __name__ == "__main__":
    main()
