"""Verify isolated research data and preserve all production histories."""
from pathlib import Path
import hashlib
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
load = lambda p: json.loads(p.read_text())
gb = load(ROOT / "geekbench-result.json")
assert load(ROOT / "geekbench-exit.json")["returnCode"] == 0
assert gb["singleCoreScore"] == 3675 and gb["multiCoreScore"] == 22129
assert len(gb["singleCoreWorkloads"]) == len(gb["multiCoreWorkloads"]) == 16
assert "19350647" in (ROOT / "geekbench-cpu.log").read_text()
assert not re.search(r"/claim\?key=\d+", (ROOT / "geekbench-cpu.log").read_text())
diagnostic = load(ROOT / "diagnostic-results.json")
measurements = diagnostic["measurements"]
assert len(measurements) == 4 and diagnostic["fixedChecksumsMatchAcrossTargets"]
assert all(len(r["rounds"]) == 3 and all(x["valid"] == 1 and x["p_lower_bound"] >= .99 and
           x["busy"] >= .90 for x in r["rounds"]) for r in measurements)
assembly_audit = []
for target in ["default", "apple-m4"]:
    text = (ROOT / ("diagnostic-" + target + "-assembly.txt")).read_text()
    for name in ["RunKernelBatch", "RunAlternatingBranchBatch"]:
        match = re.search(r"(?ms)^[0-9a-f]+ <" + name + r"\(KernelState&\)>:\n(.*?)(?=\n[0-9a-f]+ <)", text)
        assert match, (target, name)
        body = match.group(1)
        assert re.search(r"\b(?:tbz|tbnz)\s+\w+, #0x0,", body)
        assert not re.search(r"\b(?:fcsel|csel)\s", body)
        assert re.search(r"\bfmadd\s+s", body) and re.search(r"\bfmadd\s+d", body)
        (ROOT / (target + "-" + name + ".txt")).write_text(body)
        assembly_audit.append({"target": target, "function": name,
                               "realBit0Branch": True, "conditionalSelectCount": 0,
                               "FP32AndFP64FMA": True})
by = {(r["target"], r["mode"]): r for r in measurements}
random = by["default", "random"]
tuned = by["apple-m4", "random"]
alternating = by["default", "alternating"]
histories = load(ROOT / "histories-before.json")
unchanged = []
for d in histories["devices"]:
    p = REPO / "out/test-results" / d["path"]
    b = p.read_bytes()
    assert hashlib.sha256(b).hexdigest() == d["sha256"] and len(json.loads(b)) == d["rows"]
    unchanged.append({"device": d["device"], "rows": d["rows"], "sha256": d["sha256"]})
assert load(REPO / "out/test-results/archive-index.json") == histories
live = Path.home() / "Library/Application Support/GpuComputeBenchmark/results/results.json"
assert hashlib.sha256(live.read_bytes()).hexdigest() == histories["devices"][0]["sha256"]
diff = subprocess.check_output(["git", "diff", "--", "src", "shaders", "ios-gui", "macos-gui"], text=True, cwd=REPO)
assert diff == ""
result = {
    "auditPassed": True,
    "geekbench": {"single": gb["singleCoreScore"], "multi": gb["multiCoreScore"],
                  "url": gb["sourceUrl"], "runs": 1,
                  "singleVs3900SamplePercent": (gb["singleCoreScore"] / 3900 - 1) * 100,
                  "singleVs3916SamplePercent": (gb["singleCoreScore"] / 3916 - 1) * 100,
                  "multiVs22688SamplePercent": (gb["multiCoreScore"] / 22688 - 1) * 100,
                  "vsA15Public2423SamplePercent": (gb["singleCoreScore"] / 2423 - 1) * 100,
                  "A15GeekbenchMeasuredThisTurn": False},
    "diagnostic": {"defaultRandomMedian": random["medianScore"],
                   "m4TargetRandomMedian": tuned["medianScore"],
                   "m4TargetChangePercent": (tuned["medianScore"] / random["medianScore"] - 1) * 100,
                   "defaultAlternatingMedian": alternating["medianScore"],
                   "alternatingChangePercent": (alternating["medianScore"] / random["medianScore"] - 1) * 100,
                   "randomCyclesPerWork": random["medianRound"]["cycles_per_work"],
                   "alternatingCyclesPerWork": alternating["medianRound"]["cycles_per_work"],
                   "actualHardwareBranchMissRateMeasured": False,
                   "notProductionScores": True, "assembly": assembly_audit},
    "existingAppSingleCoreRatioPercent": (216.737 / 147 - 1) * 100,
    "allFiveCanonicalHistoriesUnchanged": unchanged,
    "liveMacHistoryUnchanged": True,
    "productionSourceChanged": False,
    "peakPerformanceEstablished": False,
    "causeOfGeekbenchReferenceGapEstablished": False,
}
(ROOT / "verification.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result, indent=2))
