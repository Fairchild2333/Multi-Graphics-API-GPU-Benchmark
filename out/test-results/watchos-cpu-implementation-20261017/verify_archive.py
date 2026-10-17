#!/usr/bin/env python3
"""Check real Watch simulator exports, provenance, and untouched device histories."""
from pathlib import Path
import hashlib
import json
import math

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]


def read(path):
    return json.loads(path.read_text())


def config(row):
    return dict(part.split("=", 1) for part in row["workloadConfig"].split(";") if "=" in part)


def main():
    data = ROOT / "watch-documents"
    rows = read(data / "results/results.json")
    by_id = {row["id"]: row for row in rows}
    assert len(by_id) == len(rows), "duplicate result IDs"
    for row in rows:
        c = config(row)
        assert row["platform"] == "watchOS" and row["osVersion"].startswith("watchOS ")
        assert row["isSoftware"] is True and "_simulator" in row["workloadVersion"]
        assert "_watchos_preview" in row["workloadVersion"]
        assert c["watchProfile"] == "cpu_watch_v1"
        assert c["corePlacement"] == "scheduler_managed_unclassified"
        assert c["performanceScope"] == "host_cpu_not_watch"
        assert math.isfinite(row["score"]) and row["score"] > 0
        assert ("_ui_smoke" in row["workloadVersion"]) == (c["watchRunProfile"] == "smoke")

    suites = [read(p) for p in (data / "suite-runs").glob("*.json") if p.name != "latest.json"]
    completed = [s for s in suites if s["status"] in ("passed", "review")]
    assert any(s["profile"] == "standard" for s in completed), "normal profile not completed"
    assert any(s["profile"] == "smoke" for s in completed), "UI profile not completed"
    assert any(s["status"] == "cancelled" for s in suites), "cancelled run not persisted"
    memory_summary = []
    for suite in completed:
        assert suite["executionEnvironment"] == "simulator"
        assert len(suite["jobs"]) == 2
        for index, job in enumerate(suite["jobs"]):
            assert job["status"] in ("passed", "review")
            assert len(job["resultIDs"]) == (2 if index == 0 else 1)
        selected = [by_id[rid] for j in suite["jobs"] for rid in j["resultIDs"]]
        assert {r["workload"] for r in selected} == {"cpu_single_core", "cpu_multi_core", "cpu_membw"}
        memory = next(r for r in selected if r["workload"] == "cpu_membw")
        c = config(memory)
        assert c["pageFaultMeasured"] == "1" and c["storageMeasured"] == "1"
        assert int(c["storageFileMiB"]) == 32
        curve_count = 0
        max_kib = 0
        for key, value in c.items():
            if not key.startswith("curve_") or not value:
                continue
            for point in value.split(","):
                kib, rate = map(float, point.split(":"))
                assert kib > 0 and math.isfinite(rate) and rate >= 0
                max_kib = max(max_kib, kib)
                curve_count += 1
        assert curve_count > 0
        assert max_kib <= (1024 if suite["profile"] == "smoke" else 16 * 1024)
        source = "single_memory" if c["score"] == "single_thread_memory_read" else "all_memory"
        if c.get(source + "_settled") != "1":
            assert suite["jobs"][1]["status"] == "review" and suite["status"] == "review"
        memory_summary.append({"suite": suite["id"], "profile": suite["profile"],
                               "curvePoints": curve_count, "maxDatasetKiB": max_kib,
                               "memoryStatus": suite["jobs"][1]["status"]})

    for name in ["build-final-watchos", "build-final-watchsimulator", "native-build", "native-tests",
                 "test-standard", "test-verified"]:
        assert read(ROOT / (name + "-exit.json"))["returnCode"] == 0, name
    for entry in read(ROOT / "histories-before.json"):
        # Canonical histories can be checked on another computer; live user data is Mac-local.
        old = Path(entry["path"])
        marker = "/Vulkan-GPU-Compute-Microbenchmark/"
        path = REPO / str(old).split(marker, 1)[1] if marker in str(old) else old
        if path.exists():
            assert hashlib.sha256(path.read_bytes()).hexdigest() == entry["sha256"], str(path)

    print(json.dumps({"result": "passed", "simulatorOnly": True, "resultRows": len(rows),
                      "completedSuites": memory_summary,
                      "cancelledSuites": sum(s["status"] == "cancelled" for s in suites)}, indent=2))


if __name__ == "__main__":
    main()
