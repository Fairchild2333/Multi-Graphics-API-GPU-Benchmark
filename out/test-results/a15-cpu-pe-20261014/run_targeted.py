"""Run production iOS jobs individually, preserving each suite's raw evidence."""
import argparse
import json
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
DEVICE = "00008110-00164D591485401E"
BUNDLE = "com.fairchild.Mangekyo"
JOBS = ["stream-1048576", "stream-4194304", "stream-16777216", "gpu_burn-16",
        "vulkan/stream-1048576", "vulkan/gpu_burn-16", "vulkan/gpu_burn-64", "vulkan/gpu_burn-256", "cpu_compute", "cpu_memory", "stream_headless-1048576", "vulkan/stream_headless-1048576", "stream-65536", "vulkan/stream-65536", "vulkan/stream-4194304", "vulkan/stream-16777216"]

def copy(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    p = subprocess.run(["xcrun", "devicectl", "device", "copy", "from", "--device", DEVICE,
        "--domain-type", "appDataContainer", "--domain-identifier", BUNDLE,
        "--source", source, "--destination", str(destination), "--timeout", "30"],
        capture_output=True, text=True, timeout=40)
    if p.returncode:
        raise RuntimeError(p.stdout + p.stderr)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--jobs", default=",".join(JOBS))
    parser.add_argument("--prefix", default="first")
    parser.add_argument("--rest", type=float, default=20)
    parser.add_argument("--legacy-coverage", action="store_true")
    parser.add_argument("--continue-on-failure", action="store_true")
    args = parser.parse_args()
    selected = args.jobs.split(",")
    assert selected and set(selected).issubset(JOBS)
    reports = []
    consoles = []
    for index, job in enumerate(selected):
        folder = ROOT / f"{args.prefix}-{index + 1:02d}-{job.replace('/', '-') }"
        folder.mkdir(exist_ok=False)
        old_manifest = ROOT / "before" / "suite-runs" / "latest.json"
        old_id = json.loads(old_manifest.read_text())["id"]
        if reports:
            old_id = reports[-1]["id"]
        print(f"START {index+1}/{len(selected)} {job}", flush=True)
        console = (folder / "console.log").open("w")
        proc = subprocess.Popen(["xcrun", "devicectl", "device", "process", "launch",
            "--device", DEVICE, "--terminate-existing", "--console", "--json-output",
            str(folder / "launch.json"), BUNDLE, "--ios-allow-translated-api",
            "--ios-complete-suite", "--ios-suite-jobs", job] +
            (["--ios-legacy-particle-coverage"] if args.legacy_coverage else []),
            stdout=console, stderr=subprocess.STDOUT)
        consoles.append((proc, console))
        deadline = time.monotonic() + 750
        manifest = folder / "latest.json"
        last_state = None
        while time.monotonic() < deadline:
            time.sleep(5)
            if proc.poll() is not None:
                raise RuntimeError(f"App launch/console ended ({proc.returncode}): {folder / 'console.log'}")
            copy("Documents/suite-runs/latest.json", manifest)
            report = json.loads(manifest.read_text())
            if report["id"] == old_id or [j["id"] for j in report["jobs"]] != [job]:
                continue
            state = (report["status"], report["jobs"][0]["status"])
            if state != last_state:
                print(f"STATE {job} {state}", flush=True)
                last_state = state
            if report["status"] != "running":
                break
        else:
            raise RuntimeError(f"Suite did not finish within 750 seconds: {job}")
        copy("Documents", folder / "documents")
        reports.append(report)
        (ROOT / f"{args.prefix}-manifests.json").write_text(json.dumps(reports, indent=2))
        saved = report["jobs"][0]
        print(f"END {job} {saved['status']} {saved['resultIDs']} thermal={saved['thermalStateStart']}->{saved['thermalStateEnd']} {saved['error']}", flush=True)
        if saved["status"] not in ("passed", "review") and not args.continue_on_failure:
            raise RuntimeError(f"Measurement failed: {job}")
        if index + 1 < len(selected):
            print(f"REST {args.rest:g}s", flush=True)
            time.sleep(args.rest)
    copy("Documents", ROOT / f"{args.prefix}-final")
    print(f"DONE {len(reports)} jobs; archived {ROOT / (args.prefix + '-final')}", flush=True)
    # The app remains open at its completed screen; no benchmark is running.
    for _, console in consoles:
        console.close()

if __name__ == "__main__":
    main()
