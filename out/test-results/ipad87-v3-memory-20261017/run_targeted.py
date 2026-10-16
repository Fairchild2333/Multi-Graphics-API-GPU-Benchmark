"""Run the one pending production Vulkan memory job; keep every record and raw console log."""
import json
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
DEVICE = "00008027-000C39923E85002E"
BUNDLE = "com.fairchild.Mangekyo"
JOBS = ["vulkan/memory_sweep"]


def copy(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    command = ["xcrun", "devicectl", "device", "copy", "from", "--device", DEVICE,
               "--domain-type", "appDataContainer", "--domain-identifier", BUNDLE,
               "--source", source, "--destination", str(destination), "--timeout", "30"]
    p = subprocess.run(command, capture_output=True, text=True, timeout=40)
    if p.returncode:
        raise RuntimeError(p.stdout + p.stderr)


def main():
    previous = json.loads((ROOT / "phone-before/suite-runs/latest.json").read_text())["id"]
    reports = []
    for index, job in enumerate(JOBS, 1):
        folder = ROOT / f"{index:02d}-{job.replace('/', '-')}"
        folder.mkdir(exist_ok=False)
        command = ["xcrun", "devicectl", "device", "process", "launch", "--device", DEVICE,
                   "--terminate-existing", "--console", "--json-output", str(folder / "launch.json"),
                   BUNDLE, "--ios-allow-translated-api", "--ios-complete-suite", "--ios-suite-jobs", job]
        (folder / "command.json").write_text(json.dumps(command, indent=2) + "\n")
        print(f"START {index}/{len(JOBS)} {job}", flush=True)
        with (folder / "console.log").open("w") as console:
            proc = subprocess.Popen(command, stdout=console, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 1000
            last_state = None
            while time.monotonic() < deadline:
                time.sleep(5)
                if proc.poll() is not None:
                    raise RuntimeError(f"Launch/console stopped ({proc.returncode}): {folder}")
                copy("Documents/suite-runs/latest.json", folder / "latest.json")
                report = json.loads((folder / "latest.json").read_text())
                if report["id"] == previous or [j["id"] for j in report["jobs"]] != [job]:
                    continue
                saved = report["jobs"][0]
                state = (report["status"], saved["status"])
                if state != last_state:
                    print(f"STATE {job} {state}", flush=True)
                    last_state = state
                if report["status"] != "running":
                    break
            else:
                raise RuntimeError(f"Timed out: {job}")
            copy("Documents", folder / "documents")
            reports.append(report)
            (ROOT / "manifests.json").write_text(json.dumps(reports, indent=2) + "\n")
            previous = report["id"]
            print(f"END {job} {saved['status']} ids={saved['resultIDs']} thermal={saved['thermalStateStart']}->{saved['thermalStateEnd']} error={saved['error']}", flush=True)
            if saved["status"] not in ("passed", "review"):
                raise RuntimeError(f"Job did not complete: {job}")
        if index < len(JOBS):
            print("REST 25s", flush=True)
            time.sleep(25)
    copy("Documents", ROOT / "phone-final")
    print("DONE; all raw Documents copied to phone-final", flush=True)


if __name__ == "__main__":
    main()
