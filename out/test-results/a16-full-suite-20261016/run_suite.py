"""Archive a real device's production complete-suite execution, without editing scores."""
import argparse
import json
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
DEVICE = "00008120-000E2D3E0CEBC01E"
BUNDLE = "com.fairchild.Mangekyo"


def copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    p = subprocess.run([
        "xcrun", "devicectl", "device", "copy", "from", "--device", DEVICE,
        "--domain-type", "appDataContainer", "--domain-identifier", BUNDLE,
        "--source", source, "--destination", str(target), "--timeout", "25",
    ], capture_output=True, text=True, timeout=35)
    if p.returncode:
        raise RuntimeError(p.stdout + p.stderr)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--prefix", default="whole-01")
    parser.add_argument("--resume", action="store_true")
    parser.add_argument("--jobs")
    parser.add_argument("--watch-only", action="store_true")
    args = parser.parse_args()
    folder = ROOT / args.prefix
    folder.mkdir(exist_ok=False)
    previous_id = None
    try:
        copy("Documents/suite-runs/latest.json", folder / "previous-manifest.json")
        previous_id = json.loads((folder / "previous-manifest.json").read_text())["id"]
    except RuntimeError as error:
        (folder / "previous-manifest-copy.log").write_text(str(error))
    if args.watch_only:
        previous_id = None

    command = [
        "xcrun", "devicectl", "device", "process", "launch", "--device", DEVICE,
        "--terminate-existing", "--console", "--json-output", str(folder / "launch.json"),
        BUNDLE, "--ios-allow-translated-api",
        "--ios-complete-suite-resume" if args.resume else "--ios-complete-suite",
    ]
    if args.jobs:
        command.extend(["--ios-suite-jobs", args.jobs])
    (folder / "command.json").write_text(json.dumps(command, indent=2))
    report = None
    last_state = None
    copy_failures = 0
    last_message = time.monotonic()
    with (folder / "console.log").open("w") as console:
        proc = None if args.watch_only else subprocess.Popen(command, stdout=console, stderr=subprocess.STDOUT)
        deadline = time.monotonic() + 7200
        while time.monotonic() < deadline:
            time.sleep(10)
            try:
                copy("Documents/suite-runs/latest.json", folder / "latest.json")
                candidate = json.loads((folder / "latest.json").read_text())
                copy_failures = 0
                if candidate["id"] != previous_id:
                    report = candidate
            except (RuntimeError, subprocess.TimeoutExpired, ValueError) as error:
                copy_failures += 1
                with (folder / "copy-errors.log").open("a") as log:
                    log.write(str(error) + "\n")
            if report is None:
                if proc is not None and proc.poll() is not None:
                    raise RuntimeError(f"Launch failed ({proc.returncode}); see {folder / 'console.log'}")
                if copy_failures >= (48 if args.watch_only else 6):
                    raise RuntimeError("No accessible fresh manifest; inspect connection and launch log")
                continue
            jobs = report["jobs"]
            finished = [j for j in jobs if j["status"] in ("passed", "review", "failed", "cancelled", "not_run")]
            running = [j["id"] for j in jobs if j["status"] == "running"]
            state = (report["status"], len(finished), tuple(running))
            if state != last_state or time.monotonic() - last_message >= 50:
                print(json.dumps({"status": report["status"], "finished": len(finished),
                    "total": len(jobs), "running": running,
                    "reviews": [j["id"] for j in jobs if j["status"] == "review"],
                    "failed": [j["id"] for j in jobs if j["status"] == "failed"]}), flush=True)
                last_message = time.monotonic()
                last_state = state
            (ROOT / "phase-status.json").write_text(json.dumps({
                "attempt": args.prefix, "manifestID": report["id"], "state": state,
                "completed": len(finished), "total": len(jobs),
            }, indent=2))
            if report["status"] != "running":
                copy("Documents", folder / "documents")
                (ROOT / "completed-manifest.json").write_text(json.dumps(report, indent=2))
                print(f"DONE {report['status']}; archived {folder / 'documents'}", flush=True)
                return
            if copy_failures >= 6:
                raise RuntimeError("Device manifest became inaccessible; preserve console and reconnect")
        raise RuntimeError("Complete suite exceeded the automation time limit")


if __name__ == "__main__":
    main()
