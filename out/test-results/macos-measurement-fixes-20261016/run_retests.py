"""Run only the authorized Mac follow-ups; preserve every attempt and raw log."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

root = Path(__file__).resolve().parent
repo = root.parents[2]
worker = repo / "out/macos/arm64/Release/Mangekyo.app/Contents/Helpers/gpu_benchmark"
env = dict(os.environ, GPU_BENCH_DATA_DIR=str(root / "data"))
plans = [
    ("vulkan-membw-v3", [str(worker), "--gui-worker", "--gpu", "0", "--backend", "vulkan", "--allow-translated-api", "--workload", "membw", "--no-renderdoc"], 900),
    ("metal-fixed-render-1m", [str(worker), "--gui-worker", "--gpu", "0", "--backend", "metal", "--workload", "stream", "--particles", "1048576", "--time", "15", "--no-renderdoc"], 120),
    ("vulkan-fixed-render-1m-a", [str(worker), "--gui-worker", "--gpu", "0", "--backend", "vulkan", "--allow-translated-api", "--workload", "stream", "--particles", "1048576", "--time", "15", "--no-renderdoc"], 120),
    ("metal-headless-1m", [str(worker), "--gui-worker", "--gpu", "0", "--backend", "metal", "--workload", "stream", "--particles", "1048576", "--headless", "--time", "15", "--no-renderdoc"], 120),
    ("vulkan-headless-1m", [str(worker), "--gui-worker", "--gpu", "0", "--backend", "vulkan", "--allow-translated-api", "--workload", "stream", "--particles", "1048576", "--headless", "--time", "15", "--no-renderdoc"], 120),
    ("vulkan-fixed-render-1m-b", [str(worker), "--gui-worker", "--gpu", "0", "--backend", "vulkan", "--allow-translated-api", "--workload", "stream", "--particles", "1048576", "--time", "15", "--no-renderdoc"], 120),
]
status = []

def thermal():
    return json.loads(subprocess.check_output([str(root / "thermal-probe")], text=True))

def rows():
    return json.loads((root / "data/results/results.json").read_text())

def save():
    (root / "run-status.json").write_text(json.dumps(status, indent=2) + "\n")

for index, (name, command, timeout) in enumerate(plans):
    if index:
        print("Cooling down for 30 seconds", flush=True)
        time.sleep(30)
    deadline = time.monotonic() + 600
    before = thermal()
    while before["thermalState"] != 0 and time.monotonic() < deadline:
        print("Waiting for nominal thermal state:", before, flush=True)
        time.sleep(10)
        before = thermal()
    if before["thermalState"] != 0:
        raise RuntimeError("Nominal thermal state unavailable; no next measurement")
    old = {row["id"] for row in rows()}
    entry = {"job": name, "command": command, "thermalBefore": before, "state": "running", "workerSha256": hashlib.sha256(worker.read_bytes()).hexdigest()}
    status.append(entry)
    save()
    print("Running", name, flush=True)
    with (root / (name + ".log")).open("w") as log:
        try:
            result = subprocess.run(command, cwd=repo, env=env, stdout=log,
                                    stderr=subprocess.STDOUT, timeout=timeout)
            entry["returnCode"] = result.returncode
        except subprocess.TimeoutExpired:
            entry["returnCode"] = 124
            entry["error"] = "timeout"
    entry["thermalAfter"] = thermal()
    fresh = [row for row in rows() if row["id"] not in old]
    entry["results"] = [{k: row.get(k) for k in ["id", "workload", "workloadVersion", "score", "scoreUnit", "avgComputeMs", "avgTotalGpuMs", "avgFrameTimeMs", "stableScore", "throttlePct"]} for row in fresh]
    entry["state"] = "completed" if entry["returnCode"] == 0 else "failed"
    save()
    print("Finished", name, "exit", entry["returnCode"], "new rows", len(fresh), flush=True)
print("Authorized Mac follow-ups ended", flush=True)
