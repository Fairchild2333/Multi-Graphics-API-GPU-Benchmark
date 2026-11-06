#!/usr/bin/env python3
"""One first-boot attempt, or manual optional packages after offline install.

Metadata and downloads have a hard deadline. dpkg is never killed for a timeout:
installation begins only after the complete simulated plan is cached locally,
with APT explicitly forbidden from downloading. No upgrade/daemon is scheduled.
"""
from __future__ import annotations

import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import tempfile
import time

ALLOWED = {"gcc", "make", "cmake", "ninja", "git", "curl", "pciutils", "lm-sensors"}
CACHE = Path("/var/cache/aosc-autoinstall/archives")
STATE = Path("/var/lib/aosc-autoinstall")
CONFIG = Path("/etc/aosc-autoinstall-online.json")
STOP_REQUESTED = False


def _request_stop(_signal, _frame):
    global STOP_REQUESTED
    STOP_REQUESTED = True


def _run(args, timeout=15):
    """Bound read-only/download processes; timeout=None is mandatory for dpkg."""
    env = {**os.environ, "LC_ALL": "C", "LANG": "C", "DEBIAN_FRONTEND": "noninteractive"}
    try:
        process = subprocess.Popen(args, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True, env=env, start_new_session=True)
    except OSError:
        return 127, ""
    try:
        out, _ = process.communicate(timeout=timeout)
        return process.returncode, out
    except subprocess.TimeoutExpired:
        # Only metadata, download, simulation or inspection calls use a deadline.
        os.killpg(process.pid, signal.SIGTERM)
        try:
            process.communicate(timeout=2)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.communicate()
        return 124, ""


def _save(status):
    STATE.mkdir(parents=True, exist_ok=True, mode=0o700)
    status["updated_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    fd, name = tempfile.mkstemp(prefix=".online-", dir=STATE)
    try:
        os.fchmod(fd, 0o600)
        with os.fdopen(fd, "w", encoding="utf-8") as handle:
            json.dump(status, handle, indent=2)
            handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(name, STATE / "online-status.json")
    finally:
        if os.path.exists(name):
            os.unlink(name)


def _installed(package):
    code, text = _run(["dpkg-query", "-W", "-f=${Status}", package], timeout=5)
    return code == 0 and text.strip() == "install ok installed"


def _plan(text):
    """Reject removals, upgrades, downgrades, or an unparseable install plan."""
    result = {}
    for line in text.splitlines():
        if line.startswith("Remv "):
            return None
        if line.startswith("Inst "):
            match = re.match(r"^Inst ([A-Za-z0-9.+:-]+) \(([^\s()]+)\s", line)
            if not match:  # Includes 'Inst name [old-version] (new-version ...)'.
                return None
            result[match[1].split(":", 1)[0]] = match[2]
    return result


def _cached(plan):
    expected = {}
    for name, version in plan.items():
        code, text = _run(["apt-cache", "show", name + "=" + version], timeout=5)
        hashes = re.findall(r"^SHA256: ([0-9a-fA-F]{64})$", text, re.MULTILINE)
        if code or not hashes:
            return False
        expected[(name, version)] = {value.lower() for value in hashes}
    available = set()
    for deb in CACHE.glob("*.deb"):
        code, text = _run(["dpkg-deb", "-f", str(deb), "Package", "Version"], timeout=10)
        if code:
            continue
        metadata = dict(line.split(": ", 1) for line in text.splitlines() if ": " in line)
        if "Package" in metadata and "Version" in metadata:
            key = (metadata["Package"], metadata["Version"])
            if key in expected:
                checksum = hashlib.sha256()
                try:
                    with deb.open("rb") as handle:
                        while chunk := handle.read(1024 * 1024):
                            checksum.update(chunk)
                except OSError:
                    continue
                if checksum.hexdigest() in expected[key]:
                    available.add(key)
    return all((name, version) in available for name, version in plan.items())


def finish(first_boot=False):
    if os.geteuid() != 0:
        print("Run with sudo; the installed desktop remains usable without this optional step.", file=sys.stderr)
        return 1
    try:
        config = json.loads(CONFIG.read_text())
        packages = config["optional_packages"]
        if config.get("schema_version") != 1 or not isinstance(packages, list) or any(not isinstance(p, str) or p not in ALLOWED for p in packages):
            raise ValueError()
        packages = list(dict.fromkeys(packages))
    except (OSError, ValueError, KeyError):
        print("Optional package settings are missing or invalid; nothing was changed.")
        return 1
    STATE.mkdir(parents=True, exist_ok=True, mode=0o700)
    lock = (STATE / "online.lock").open("a")
    os.chmod(STATE / "online.lock", 0o600)
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        print("Optional package setup is already running; skipping this invocation.")
        lock.close()
        return 0
    if first_boot:
        marker = Path("/var/lib/aosc-online/first-attempt")
        marker.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
        try:
            with os.fdopen(os.open(marker, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600), "w") as handle:
                handle.write(datetime.datetime.now(datetime.timezone.utc).isoformat() + "\n")
        except FileExistsError:
            lock.close()
            return 0
    signal.signal(signal.SIGTERM, _request_stop)
    signal.signal(signal.SIGINT, _request_stop)
    status = {"schema_version": 1, "packages": {p: {"state": "pending"} for p in packages}, "system_upgrade_requested": False}
    try:
        code, audit = _run(["dpkg", "--audit"], timeout=15)
        if code or audit.strip():
            status["pending_reason"] = "Existing package database needs attention; no optional installation attempted."
            _save(status)
            print(status["pending_reason"])
            return 0
        CACHE.mkdir(parents=True, exist_ok=True, mode=0o755)
        (CACHE / "partial").mkdir(exist_ok=True, mode=0o700)
        apt = ["apt-get", "-o", "Acquire::Retries=0", "-o", "Acquire::http::Timeout=8",
               "-o", "Acquire::https::Timeout=8", "-o", "DPkg::Lock::Timeout=5",
               "-o", "Dir::Cache::archives=" + str(CACHE), "-o", "APT::Get::List-Cleanup=0"]
        deadline = time.monotonic() + 180
        # update never invokes dpkg; lack of network is a pending optional step.
        update_code, _ = _run(apt + ["update"], timeout=35)
        status["index_refresh"] = "complete" if update_code == 0 else "unavailable_or_timed_out"
        for package in packages:
            record = status["packages"][package]
            if STOP_REQUESTED:
                record["reason"] = "Stop requested; safe to retry manually."
                _save(status)
                continue
            if _installed(package):
                record["state"] = "already_installed"
                _save(status)
                continue
            if time.monotonic() >= deadline:
                record["reason"] = "Download time budget exhausted."
                _save(status)
                continue
            code, policy = _run(["apt-cache", "policy", package], timeout=5)
            candidate = re.search(r"^\s*Candidate:\s*(\S+)", policy, re.MULTILINE)
            if code or not candidate or candidate[1] == "(none)":
                record["reason"] = "No repository candidate for this architecture."
                _save(status)
                continue
            version = candidate[1]
            if not re.fullmatch(r"[A-Za-z0-9.+:~_-]+", version):
                record["reason"] = "Unrecognized repository version."
                _save(status)
                continue
            request = ["--no-remove", "--no-upgrade", "install", package + "=" + version]
            code, simulation = _run(apt + ["--simulate"] + request, timeout=10)
            plan = _plan(simulation) if code == 0 else None
            if not plan or package not in plan or len(plan) > 250:
                record["reason"] = "Would change existing packages or dependency plan is unavailable."
                _save(status)
                continue
            timeout = min(60, deadline - time.monotonic())
            if timeout <= 0:
                record["reason"] = "Download time budget exhausted."
                _save(status)
                continue
            code, _ = _run(apt + ["--assume-yes", "--download-only"] + request, timeout=timeout)
            if code or not _cached(plan):
                record["reason"] = "Complete, verified local package cache is unavailable; retry when online."
                _save(status)
                continue
            code, current = _run(apt + ["--simulate"] + request, timeout=10)
            if code or _plan(current) != plan:
                record["reason"] = "Dependency plan changed after download; retry later."
                _save(status)
                continue
            record["state"] = "installing_from_local_cache"
            _save(status)
            print("Installing cached optional package: " + package, flush=True)
            # No network and NO HARD TIMEOUT: never interrupt dpkg/maintainer scripts.
            code, _ = _run(apt + ["--assume-yes", "--no-download",
                                 "-o", "Dpkg::Options::=--force-confold"] + request, timeout=None)
            if code or not _installed(package):
                record["state"] = "pending"
                record["reason"] = "Package configuration needs manual attention; stopped remaining optional installs."
                _save(status)
                break
            record["state"] = "installed"
            _save(status)
        completed = sum(p["state"] in ("installed", "already_installed") for p in status["packages"].values())
        print("Optional setup: %s/%s packages ready. Details: %s" % (completed, len(packages), STATE / "online-status.json"))
        return 0
    finally:
        lock.close()


if __name__ == "__main__":
    if sys.argv[1:] not in ([], ["--first-boot"]):
        print("Usage: aosc-finish-online [--first-boot]", file=sys.stderr)
        raise SystemExit(2)
    raise SystemExit(finish(first_boot="--first-boot" in sys.argv[1:]))
