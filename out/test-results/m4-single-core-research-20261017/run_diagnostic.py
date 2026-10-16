"""Run a fixed diagnostic matrix after Geekbench, never alongside it."""
from pathlib import Path
import datetime
import json
import statistics
import subprocess
import time

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
assert json.loads((ROOT / "geekbench-exit.json").read_text())["returnCode"] == 0
assert "geekbench_aarch64" not in subprocess.check_output(["ps", "-Ac", "-o", "comm="], text=True)
probe = REPO / "out/test-results/macos-measurement-fixes-20261016/final/thermal-probe"
thermal_before = json.loads(subprocess.check_output([str(probe)], text=True))
assert thermal_before["thermalState"] == 0 and not thermal_before["lowPowerMode"]
(ROOT / "diagnostic-thermal-before.json").write_text(json.dumps(thermal_before, indent=2) + "\n")
commands = json.loads((ROOT / "diagnostic-build-commands.json").read_text())
for build in commands:
    print("BUILD", build["target"], flush=True)
    p = subprocess.run(build["command"], capture_output=True, text=True)
    (ROOT / (build["binary"] + "-build.log")).write_text(p.stdout + p.stderr)
    p.check_returncode()
    p = subprocess.run(["xcrun", "llvm-objdump", "--disassemble", "--demangle", str(ROOT / build["binary"])],
                       capture_output=True, text=True)
    (ROOT / (build["binary"] + "-assembly.txt")).write_text(p.stdout + p.stderr)
    p.check_returncode()
matrix = [("default", "random"), ("apple-m4", "random"),
          ("apple-m4", "alternating"), ("default", "alternating")]
measurements = []
for target, mode in matrix:
    time.sleep(5)
    print("MEASURE", target, mode, flush=True)
    started = datetime.datetime.now(datetime.timezone.utc).isoformat()
    p = subprocess.run([str(ROOT / ("diagnostic-" + target)), mode], capture_output=True, text=True)
    (ROOT / (target + "-" + mode + ".log")).write_text(p.stdout + p.stderr)
    print(p.stdout[-1200:], flush=True)
    p.check_returncode()
    parsed = []
    for line in p.stdout.splitlines():
        if line.startswith("ROUND "):
            c = dict(v.split("=", 1) for v in line.split()[1:])
            parsed.append({k: (v if k in ["mode", "checksum"] else float(v)) for k, v in c.items()})
    assert len(parsed) == 3 and all(r["valid"] == 1 for r in parsed)
    selected = sorted(parsed, key=lambda r: r["score"])[1]
    fixed = next(line for line in p.stdout.splitlines() if line.startswith("FIXED "))
    measurements.append({"target": target, "mode": mode, "startedAt": started,
                         "medianScore": selected["score"], "medianRound": selected,
                         "fixedChecksum": fixed.split("checksum=")[1], "rounds": parsed})
for mode in ["random", "alternating"]:
    selected = [r for r in measurements if r["mode"] == mode]
    assert selected[0]["fixedChecksum"] == selected[1]["fixedChecksum"]
thermal_after = json.loads(subprocess.check_output([str(probe)], text=True))
(ROOT / "diagnostic-thermal-after.json").write_text(json.dumps(thermal_after, indent=2) + "\n")
result = {"scope": "isolated single-thread diagnostic; not an App score", "measurements": measurements,
          "fixedChecksumsMatchAcrossTargets": True, "productionSourceChanged": False,
          "thermalBefore": thermal_before, "thermalAfter": thermal_after}
(ROOT / "diagnostic-results.json").write_text(json.dumps(result, indent=2) + "\n")
print("DONE fixed matrix; no extra attempts or result persistence", flush=True)
