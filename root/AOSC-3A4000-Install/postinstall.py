#!/usr/bin/env python3
"""Offline AOSC target configuration. Never run configure_target against '/'.

The caller mounts the completed target, including the usual chroot virtual
filesystems. Secrets are accepted in memory and are never printed. This module
does not partition disks, install packages, start target services, or reboot.
"""
from __future__ import annotations

import base64
import binascii
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import secrets
import shutil
import struct
import subprocess
import tempfile
import uuid


class ConfigurationError(RuntimeError):
    """An offline core requirement could not be verified."""


def _run(args, *, data=None, timeout=30, required=True):
    try:
        result = subprocess.run(
            [str(x) for x in args], input=data, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout,
            env={**os.environ, "LC_ALL": "C", "LANG": "C"},
        )
    except (OSError, subprocess.TimeoutExpired):
        if required:
            raise ConfigurationError("A required configuration command failed or timed out.") from None
        return None
    if result.returncode and required:
        # chpasswd or NM diagnostics can contain sensitive input. Do not relay it.
        raise ConfigurationError("A required configuration command returned an error.")
    return result


def _path(root: Path, name: str) -> Path:
    relative = PurePosixPath(name)
    if ".." in relative.parts:
        raise ConfigurationError("Invalid target path.")
    result = root / str(relative).lstrip("/")
    if not result.parent.resolve().is_relative_to(root):
        raise ConfigurationError("Target path escapes the installation root.")
    return result


def _rooted_resolve(root: Path, name: str) -> Path:
    """Resolve absolute target symlinks inside root, never on the live host."""
    parts = list(PurePosixPath(name).parts)
    current = root
    links = 0
    while parts:
        part = parts.pop(0)
        if part in ("/", "", "."):
            continue
        if part == "..":
            if current == root:
                raise ConfigurationError("Target symlink escapes installation root.")
            current = current.parent
            continue
        current = current / part
        if current.is_symlink():
            links += 1
            if links > 30:
                raise ConfigurationError("Too many target symlinks.")
            target = PurePosixPath(os.readlink(current))
            current = root if target.is_absolute() else current.parent
            parts = list(target.parts) + parts
    return current


def _write(root: Path, name: str, text: str, mode=0o644, uid=0, gid=0):
    dest = _path(root, name)
    dest.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=".aosc-install-", dir=dest.parent)
    try:
        os.fchmod(fd, mode)
        os.fchown(fd, uid, gid)
        with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, dest)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def _key(value: str) -> str:
    if not isinstance(value, str) or len(value) > 16384 or any(ord(x) < 32 or ord(x) == 127 for x in value):
        raise ConfigurationError("SSH public key must be one complete line without options.")
    fields = value.strip().split(None, 2)
    if len(fields) < 2 or fields[0] not in ("ssh-ed25519", "ssh-rsa"):
        raise ConfigurationError("Only complete ssh-ed25519 or ssh-rsa public keys are accepted.")
    try:
        blob = base64.b64decode(fields[1], validate=True)
        cursor = 0

        def field():
            nonlocal cursor
            if cursor + 4 > len(blob):
                raise ValueError()
            size = struct.unpack(">I", blob[cursor:cursor + 4])[0]
            cursor += 4
            if size > 32768 or cursor + size > len(blob):
                raise ValueError()
            data = blob[cursor:cursor + size]
            cursor += size
            return data

        if field() != fields[0].encode("ascii"):
            raise ValueError()
        if fields[0] == "ssh-ed25519":
            if len(field()) != 32:
                raise ValueError()
        else:
            exponent, modulus = field(), field()
            if not exponent or not modulus or exponent[0] & 128 or modulus[0] & 128:
                raise ValueError()
            if int.from_bytes(exponent, "big") < 3 or int.from_bytes(modulus, "big").bit_length() < 2048:
                raise ValueError()
        if cursor != len(blob):
            raise ValueError()
    except (ValueError, binascii.Error, struct.error):
        raise ConfigurationError("Malformed or unsupported SSH public key.") from None
    return " ".join(fields)


def validate_settings(settings: dict):
    if settings.get("schema_version") != 1:
        raise ConfigurationError("Unsupported settings schema.")
    user = settings.get("username", "")
    if not isinstance(user, str) or not re.fullmatch(r"[a-z_][a-z0-9_-]{0,30}", user) or user == "root":
        raise ConfigurationError("Invalid non-root username.")
    hashed = settings.get("password_hash", "")
    if not isinstance(hashed, str) or not re.fullmatch(r"\$(?:6|5|y|2[aby])\$[A-Za-z0-9./$=,-]{12,500}", hashed):
        raise ConfigurationError("A valid existing crypt password hash is required.")
    keys = settings.get("public_keys", [])
    if not isinstance(keys, list) or not keys:
        raise ConfigurationError("At least one SSH public key is required.")
    for value in keys:
        _key(value)
    for item in ("fullname", "locale", "timezone", "keyboard", "hostname_prefix"):
        value = settings.get(item, "")
        if not isinstance(value, str) or not value or any(ord(x) < 32 or ord(x) == 127 for x in value):
            raise ConfigurationError("Invalid identity or regional setting.")
    if ":" in settings["fullname"]:
        raise ConfigurationError("Full name must not contain a colon.")
    if not re.fullmatch(r"[A-Za-z0-9_.@-]+", settings["locale"]):
        raise ConfigurationError("Invalid locale setting.")
    if not re.fullmatch(r"[A-Za-z0-9_+/-]+", settings["timezone"]) or ".." in settings["timezone"].split("/"):
        raise ConfigurationError("Invalid timezone setting.")
    if not re.fullmatch(r"[A-Za-z0-9_-]+", settings["keyboard"]):
        raise ConfigurationError("Invalid keyboard layout.")
    if not re.fullmatch(r"[a-z][a-z0-9-]{0,20}", settings["hostname_prefix"]):
        raise ConfigurationError("Invalid hostname prefix.")
    if "hostname" in settings and (not isinstance(settings["hostname"], str) or not re.fullmatch(r"[a-z0-9][a-z0-9-]{0,61}[a-z0-9]", settings["hostname"])):
        raise ConfigurationError("Invalid hostname.")
    if settings.get("rtc_as_localtime") is not False:
        raise ConfigurationError("This installation profile requires an RTC in UTC.")
    for item in ("desktop", "passwordless_sudo", "disable_sleep"):
        if not isinstance(settings.get(item), bool):
            raise ConfigurationError("Invalid boolean setting.")
    _wifi(settings)


def _wifi(settings: dict):
    wifi = settings.get("wifi")
    if not wifi:
        return None
    if not isinstance(wifi, dict):
        raise ConfigurationError("Invalid Wi-Fi settings.")
    ssid, password = wifi.get("ssid"), wifi.get("password")
    if not isinstance(ssid, str) or not (1 <= len(ssid.encode("utf-8")) <= 32) or any(ord(c) < 32 for c in ssid):
        raise ConfigurationError("Invalid Wi-Fi SSID.")
    if not isinstance(password, str) or not (8 <= len(password.encode("utf-8")) <= 63) or any(ord(c) < 32 for c in password):
        raise ConfigurationError("Wi-Fi passphrase must be 8–63 bytes without control characters.")
    return ssid, password


def _wifi_keyfile(settings: dict) -> str:
    ssid, password = _wifi(settings)
    # NM's integer-array SSID encoding avoids escaping arbitrary Unicode/spaces.
    encoded_ssid = ";".join(str(x) for x in ssid.encode("utf-8")) + ";"
    psk = hashlib.pbkdf2_hmac("sha1", password.encode("utf-8"), ssid.encode("utf-8"), 4096, 32).hex()
    return (
        "[connection]\nid=aosc-wifi-backup\nuuid=" + str(uuid.uuid4()) +
        "\ntype=wifi\nautoconnect=true\nautoconnect-retries=0\n\n"
        "[wifi]\nmode=infrastructure\nssid=" + encoded_ssid + "\n\n"
        "[wifi-security]\nkey-mgmt=wpa-psk\npsk=" + psk + "\n\n"
        "[ipv4]\nmethod=auto\nroute-metric=2048\n\n"
        "[ipv6]\nmethod=auto\nroute-metric=2048\n"
    )


def try_live_wifi(settings: dict) -> list[str]:
    """Best effort, at most ~30 s; no Ethernet changes or service starts.

    A generated PSK exists only in a 0600 file under /run, then is unlinked.
    The active NM connection stays in memory until live media shutdown.
    """
    try:
        if not _wifi(settings):
            return []
        nmcli = shutil.which("nmcli")
        if not nmcli:
            return ["Live Wi-Fi skipped: NetworkManager is unavailable."]
        devices = _run([nmcli, "-t", "-f", "TYPE,STATE", "device", "status"], timeout=3, required=False)
        if devices is None or devices.returncode or not any(line.startswith("wifi:") for line in devices.stdout.splitlines()):
            return ["Live Wi-Fi skipped: no wireless device is available."]
        if any(line in ("wifi:connected", "wifi:connected (externally)") for line in devices.stdout.splitlines()):
            return []
        folder = Path("/run/NetworkManager/system-connections")
        folder.mkdir(parents=True, exist_ok=True, mode=0o700)
        fd, name = tempfile.mkstemp(prefix="aosc-install-wifi-", suffix=".nmconnection", dir=folder)
        profile = Path(name)
        try:
            os.fchmod(fd, 0o600)
            with os.fdopen(fd, "w") as handle:
                handle.write(_wifi_keyfile(settings))
            loaded = _run([nmcli, "connection", "load", str(profile)], timeout=4, required=False)
            if loaded is None or loaded.returncode:
                return ["Live Wi-Fi skipped: the temporary profile could not be loaded."]
            # UUID, never password or PSK, may appear in process arguments.
            profile_uuid = next(line[5:] for line in profile.read_text().splitlines() if line.startswith("uuid="))
            connected = _run([nmcli, "--wait", "12", "connection", "up", "uuid", profile_uuid], timeout=15, required=False)
            if connected is None or connected.returncode:
                return ["Live Wi-Fi did not connect in time; installation continues offline."]
            return []
        finally:
            profile.unlink(missing_ok=True)
    except (ConfigurationError, OSError, ValueError):
        return ["Live Wi-Fi was skipped; installation continues offline."]


def _command(root: Path, name: str) -> str:
    for folder in ("/usr/sbin", "/usr/bin", "/sbin", "/bin"):
        candidate = folder + "/" + name
        if _rooted_resolve(root, candidate).is_file():
            return candidate
    raise ConfigurationError("Required target command is missing: " + name)


def _chroot(root, name, *args, **kwargs):
    return _run(["chroot", root, _command(root, name), *args], **kwargs)


def _unit(root: Path, name: str):
    for folder in ("/etc/systemd/system", "/usr/lib/systemd/system", "/lib/systemd/system"):
        candidate = _rooted_resolve(root, folder + "/" + name)
        if candidate.is_file() and candidate != root / "dev/null":
            return candidate
    return None


def _shadow(root: Path, username: str) -> str:
    for line in _path(root, "/etc/shadow").read_text().splitlines():
        fields = line.split(":")
        if len(fields) >= 2 and fields[0] == username:
            return fields[1]
    raise ConfigurationError("Required account is absent from target shadow file.")


def configure_target(root: Path, settings: dict) -> list[str]:
    """Configure a mounted, offline target; raise on core verification failure."""
    validate_settings(settings)
    root = Path(root).resolve(strict=True)
    if root == Path("/") or not root.is_dir() or not (root / "etc/passwd").is_file():
        raise ConfigurationError("An installed target root, never the live root, is required.")
    if os.geteuid() != 0:
        raise ConfigurationError("Target configuration requires root.")
    user = settings["username"]
    account = next((line.split(":") for line in (root / "etc/passwd").read_text().splitlines() if line.startswith(user + ":")), None)
    if not account or len(account) != 7 or int(account[2]) == 0 or account[5] != "/home/" + user:
        raise ConfigurationError("The installer-created target user could not be verified.")
    uid, gid = int(account[2]), int(account[3])
    # Preflight all core requirements before replacing the random account password.
    for command in ("chpasswd", "passwd", "usermod", "sshd", "ssh-keygen", "visudo"):
        _command(root, command)
    ssh_unit = next((unit for unit in ("sshd.service", "ssh.service") if _unit(root, unit)), None)
    if not ssh_unit:
        raise ConfigurationError("The offline image does not include an SSH service.")
    if settings["desktop"] and not _unit(root, "display-manager.service"):
        raise ConfigurationError("The desktop image has no configured display-manager.service.")
    zone = _rooted_resolve(root, "/usr/share/zoneinfo/" + settings["timezone"])
    if not zone.is_file():
        raise ConfigurationError("The offline image does not contain the selected timezone.")
    warnings = []
    _chroot(root, "chpasswd", "-e", data=user + ":" + settings["password_hash"] + "\n")
    if not secrets.compare_digest(_shadow(root, user), settings["password_hash"]):
        raise ConfigurationError("Target account password verification failed.")
    _chroot(root, "passwd", "-l", "root")
    if not _shadow(root, "root").startswith(("!", "*")):
        raise ConfigurationError("Root password lock could not be verified.")
    _chroot(root, "usermod", "-c", settings["fullname"], user)
    ssh_dir = _path(root, "/home/" + user + "/.ssh")
    if ssh_dir.is_symlink():
        raise ConfigurationError("Target SSH directory must not be a symlink.")
    ssh_dir.mkdir(parents=True, exist_ok=True, mode=0o700)
    os.chmod(ssh_dir, 0o700)
    os.chown(ssh_dir, uid, gid)
    _write(root, "/home/" + user + "/.ssh/authorized_keys", "\n".join(dict.fromkeys(_key(k) for k in settings["public_keys"])) + "\n", 0o600, uid, gid)
    ssh_policy = "/etc/ssh/sshd_config.d/00-aosc-autoinstall.conf"
    _write(root, ssh_policy, "PubkeyAuthentication yes\nPasswordAuthentication no\nKbdInteractiveAuthentication no\nPermitRootLogin no\nAuthenticationMethods publickey\n")
    main_config = _path(root, "/etc/ssh/sshd_config")
    original = main_config.read_text() if main_config.exists() else ""
    include = "Include " + ssh_policy
    if not original.startswith(include + "\n"):
        _write(root, "/etc/ssh/sshd_config", include + "\n" + original)
    _path(root, "/run/sshd").mkdir(parents=True, exist_ok=True, mode=0o755)
    _chroot(root, "ssh-keygen", "-A")
    host_key = _path(root, "/etc/ssh/ssh_host_ed25519_key")
    if not host_key.is_file() or host_key.is_symlink() or host_key.stat().st_uid != 0 or host_key.stat().st_mode & 0o077:
        raise ConfigurationError("Target SSH host key generation could not be verified.")
    _chroot(root, "sshd", "-t")
    effective = _chroot(root, "sshd", "-T").stdout.splitlines()
    for value in ("pubkeyauthentication yes", "passwordauthentication no", "kbdinteractiveauthentication no", "permitrootlogin no", "authenticationmethods publickey"):
        if value not in effective:
            raise ConfigurationError("The effective SSH authentication policy could not be verified.")
    _run(["systemctl", "--root", root, "enable", ssh_unit])
    enabled = _run(["systemctl", "--root", root, "is-enabled", ssh_unit])
    if enabled.stdout.strip() not in ("enabled", "alias"):
        raise ConfigurationError("Target SSH service enablement could not be verified.")
    if settings["passwordless_sudo"]:
        _write(root, "/etc/sudoers.d/90-aosc-autoinstall", user + " ALL=(ALL:ALL) NOPASSWD: ALL\n", 0o440)
        sudoers = _path(root, "/etc/sudoers")
        existing = sudoers.read_text()
        if not any(re.match(r"\s*(?:@includedir|#includedir)\s+/etc/sudoers.d\s*$", line) for line in existing.splitlines()):
            _write(root, "/etc/sudoers", existing.rstrip() + "\n@includedir /etc/sudoers.d\n", 0o440)
        _chroot(root, "visudo", "-c")
    if settings["disable_sleep"]:
        _run(["systemctl", "--root", root, "mask", "sleep.target", "suspend.target", "hibernate.target", "hybrid-sleep.target", "suspend-then-hibernate.target"])
        _write(root, "/etc/systemd/logind.conf.d/90-aosc-autoinstall.conf", "[Login]\nHandleLidSwitch=ignore\nHandleLidSwitchExternalPower=ignore\nIdleAction=ignore\n")
    target = "graphical.target" if settings["desktop"] else "multi-user.target"
    _run(["systemctl", "--root", root, "set-default", target])
    default = _path(root, "/etc/systemd/system/default.target")
    if not default.is_symlink() or PurePosixPath(os.readlink(default)).name != target:
        raise ConfigurationError("Default systemd target verification failed.")
    locale = settings["locale"]
    normalized = lambda s: s.lower().replace("-", "")
    try:
        available = _chroot(root, "locale", "-a", required=False)
    except ConfigurationError:
        available = None
    found = available and available.returncode == 0 and normalized(locale) in {normalized(x) for x in available.stdout.splitlines()}
    if not found:
        source = locale.split(".")[0]
        if _rooted_resolve(root, "/usr/share/i18n/locales/" + source).is_file():
            try:
                generated = _chroot(root, "localedef", "-i", source, "-f", "UTF-8", locale, timeout=90, required=False)
                found = generated is not None and generated.returncode == 0
            except ConfigurationError:
                found = False
        if not found:
            warnings.append("Requested locale could not be generated offline; the image's existing locale was retained.")
    if found:
        _write(root, "/etc/locale.conf", "LANG=" + locale + "\n")
    localtime = _path(root, "/etc/localtime")
    localtime.unlink(missing_ok=True)
    localtime.symlink_to("/usr/share/zoneinfo/" + settings["timezone"])
    _write(root, "/etc/timezone", settings["timezone"] + "\n")
    _write(root, "/etc/adjtime", "0.0 0 0.0\n0\nUTC\n")
    _write(root, "/etc/vconsole.conf", "KEYMAP=" + settings["keyboard"] + "\n")
    _write(root, "/etc/X11/xorg.conf.d/00-keyboard.conf", 'Section "InputClass"\n    Identifier "aosc-install-keyboard"\n    MatchIsKeyboard "on"\n    Option "XkbLayout" "' + settings["keyboard"] + '"\nEndSection\n')
    hostname = settings.get("hostname") or settings["hostname_prefix"] + "-" + secrets.token_hex(3)
    _write(root, "/etc/hostname", hostname + "\n")
    hosts_file = _path(root, "/etc/hosts")
    hosts = hosts_file.read_text() if hosts_file.exists() else "127.0.0.1 localhost\n::1 localhost\n"
    hosts = "\n".join(line for line in hosts.splitlines() if not re.match(r"\s*127\.0\.1\.1\s", line))
    _write(root, "/etc/hosts", hosts + "\n127.0.1.1 " + hostname + "\n")
    try:
        if _unit(root, "NetworkManager.service"):
            other_manager = False
            for manager in ("systemd-networkd.service", "connman.service", "dhcpcd.service"):
                enabled = _run(["systemctl", "--root", root, "is-enabled", manager], required=False)
                if enabled and enabled.returncode == 0 and enabled.stdout.strip() in ("enabled", "enabled-runtime"):
                    other_manager = True
            if other_manager:
                warnings.append("Another network manager is enabled; Wi-Fi provisioning was skipped to avoid competing managers.")
            else:
                _write(root, "/etc/NetworkManager/system-connections/aosc-wired.nmconnection", "[connection]\nid=aosc-wired\nuuid=" + str(uuid.uuid4()) + "\ntype=ethernet\nautoconnect=true\nautoconnect-priority=100\n\n[ethernet]\n\n[ipv4]\nmethod=auto\nroute-metric=1024\n\n[ipv6]\nmethod=auto\nroute-metric=1024\n", 0o600)
                if _wifi(settings):
                    _write(root, "/etc/NetworkManager/system-connections/aosc-wifi-backup.nmconnection", _wifi_keyfile(settings), 0o600)
                _write(root, "/etc/NetworkManager/conf.d/90-aosc-autoinstall.conf", "[main]\nno-auto-default=*\n")
                _run(["systemctl", "--root", root, "enable", "NetworkManager.service"])
        else:
            warnings.append("NetworkManager is absent; Wi-Fi provisioning was skipped and no additional network manager was enabled.")
    except (ConfigurationError, OSError):
        warnings.append("NetworkManager provisioning was incomplete; the installed system remains bootable, and Wi-Fi can be set up from its desktop.")
    # This file contains only a fixed optional package allowlist, never settings.
    _write(root, "/etc/aosc-autoinstall-online.json", json.dumps({"schema_version": 1, "optional_packages": ["gcc", "make", "cmake", "ninja", "git", "curl", "pciutils", "lm-sensors"]}, indent=2) + "\n", 0o600)
    online_script = Path(__file__).with_name("finish-online.py")
    if not online_script.is_file():
        raise ConfigurationError("The offline bundle is missing finish-online.py.")
    _write(root, "/usr/local/sbin/aosc-finish-online", online_script.read_text(encoding="utf-8"), 0o755)
    _write(root, "/etc/systemd/system/aosc-finish-online.service", """[Unit]
Description=One optional package download attempt after offline AOSC installation
ConditionPathExists=!/var/lib/aosc-online/first-attempt
After=local-fs.target network.target NetworkManager.service

[Service]
Type=exec
ExecStart=/usr/local/sbin/aosc-finish-online --first-boot
Restart=no
# The script defers TERM/INT until the current package operation completes.
# Never escalate a stop request into SIGKILL while dpkg is configuring packages.
KillMode=process
TimeoutStopSec=infinity
SendSIGKILL=no
Nice=10

[Install]
WantedBy=multi-user.target
""")
    _run(["systemctl", "--root", root, "enable", "aosc-finish-online.service"])
    return warnings
