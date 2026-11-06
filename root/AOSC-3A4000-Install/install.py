#!/usr/bin/env python3
"""AOSC loongson3 offline Desktop installer wrapper; no disk is auto-selected.

Run in the official LiveKit. Default mode only checks prerequisites and lists
disks. --install additionally requires --disk and an exact interactive erase
confirmation. The real account hash and Wi-Fi secret never enter DeployKit.
"""
import argparse
import datetime
import fcntl
import json
import os
from pathlib import Path
import platform
import re
import secrets
import shutil
import subprocess
import sys
import tempfile

RECIPE = Path('/run/livekit/livemnt/manifest/recipe.json')
PAYLOAD = Path('/run/livekit/sysroots/desktop')
BUS = ['busctl', '--system', '--json=short', '--timeout=15', 'call',
       'io.aosc.Deploykit', '/io/aosc/Deploykit', 'io.aosc.Deploykit1']
VERSION_PREFIXES = {'dkcli': '0.4.9', 'deploykit-backend': '0.10.1'}
MIN_DISK = 32 * 1024 ** 3
UNSAFE_FS = {'LVM2_member', 'crypto_LUKS', 'linux_raid_member', 'zfs_member',
             'bcache', 'btrfs'}
REQUIRED = ['lsblk', 'sfdisk', 'udevadm', 'mkfs.vfat', 'mkfs.ext4', 'blkid',
            'busctl', 'dkcli', 'mount', 'umount', 'findmnt', 'chroot',
            'dpkg-query', 'systemctl', 'rsync', 'mkswap', 'swapon', 'swapoff', 'losetup']


class InstallError(RuntimeError):
    pass


def run(args, *, input_text=None, timeout=30, check=True):
    try:
        result = subprocess.run(args, input=input_text, capture_output=True,
                                text=True, timeout=timeout, check=False)
    except subprocess.TimeoutExpired:
        raise InstallError(f'{Path(args[0]).name}: operation timed out') from None
    if check and result.returncode:
        # Never include command arguments or stderr: a child may echo a secret.
        raise InstallError(f'{Path(args[0]).name}: operation failed ({result.returncode})')
    return result


def dbus(method, signature=None, *values):
    args = BUS + [method]
    if signature:
        args += [signature, *values]
    envelope = json.loads(run(args).stdout)
    if envelope.get('type') != 's' or len(envelope.get('data', [])) != 1:
        raise InstallError('Unexpected DeployKit D-Bus response format')
    value = json.loads(envelope['data'][0])
    if value.get('result') != 'Ok':
        raise InstallError(f'DeployKit {method} failed; inspect its local diagnostic log')
    return value.get('data')


def descendants(device):
    yield device
    for child in device.get('children', []):
        yield from descendants(child)


def disk_inventory():
    columns = 'NAME,TYPE,SIZE,MODEL,SERIAL,WWN,TRAN,RM,RO,FSTYPE,MOUNTPOINTS,MAJ:MIN'
    return json.loads(run(['lsblk', '--json', '--bytes', '--paths', '--output', columns]).stdout)['blockdevices']


def disk_rejections(device, sys_class=Path('/sys/class/block')):
    reasons = []
    if device.get('type') != 'disk':
        reasons.append('not a whole physical disk')
    if device.get('tran') == 'usb' or device.get('rm') or device.get('ro'):
        reasons.append('USB, removable, or read-only disk')
    if int(device.get('size') or 0) < MIN_DISK:
        reasons.append('less than 32 GiB')
    for item in descendants(device):
        if any(item.get('mountpoints') or []):
            reasons.append('disk or a descendant is mounted/in use (including LiveKit or swap)')
        if item.get('type') not in ('disk', 'part') or item.get('fstype') in UNSAFE_FS:
            reasons.append('complex/shared storage (LVM, RAID, encryption, Btrfs, etc.)')
        holders = sys_class / Path(item['name']).name / 'holders'
        if not holders.is_dir():
            reasons.append('cannot verify kernel block-device holders')
        elif any(holders.iterdir()):
            reasons.append('disk or a descendant has active block-device holders')
        sys_device = sys_class / Path(item['name']).name
        if 'usb' in str(sys_device.resolve()).lower():
            reasons.append('kernel device path belongs to a USB bridge')
    return sorted(set(reasons))


def fingerprint(device):
    return {key: device.get(key) for key in ('name', 'size', 'model', 'serial', 'wwn', 'maj:min')}


def find_disk(devices, path):
    selected = str(Path(path).resolve(strict=True))
    matches = [d for d in devices if d['name'] == selected and d['type'] == 'disk']
    if len(matches) != 1:
        raise InstallError('Selected path is not exactly one listed whole disk')
    reasons = disk_rejections(matches[0])
    if reasons:
        raise InstallError('Selected disk refused: ' + '; '.join(reasons))
    return matches[0]


def load_settings(path):
    from postinstall import validate_settings
    value = json.loads(path.read_text(encoding='utf-8-sig'))
    validate_settings(value)
    if value.get('schema_version') != 1 or value.get('desktop') is not True:
        raise InstallError('Expected a schema-1 Desktop installation profile')
    if not re.fullmatch(r'[a-z_][a-z0-9_-]{0,30}', value.get('username', '')):
        raise InstallError('Invalid username')
    if not re.fullmatch(r'\$(?:6|y)\$[^\s:]+', value.get('password_hash', '')):
        raise InstallError('Expected an existing SHA-512/yescrypt password hash')
    if not value.get('public_keys') or not isinstance(value['public_keys'], list):
        raise InstallError('SSH public keys are required')
    for key in value['public_keys']:
        if not re.fullmatch(r'(ssh-ed25519|ssh-rsa) [A-Za-z0-9+/]+={0,3}(?: [^\r\n]+)?', key):
            raise InstallError('Invalid SSH public key')
    for field in ('locale', 'timezone', 'fullname'):
        if not isinstance(value.get(field), str) or not value[field] or any(c in value[field] for c in '\r\n\0'):
            raise InstallError(f'Invalid {field}')
    # This bundle intentionally targets the user's verified zone. dkcli's
    # whitelist is narrower than the set of existing zoneinfo files.
    if value['timezone'] != 'Australia/Sydney':
        raise InstallError('This reviewed profile requires Australia/Sydney')
    zone_table = Path('/usr/share/zoneinfo/zone1970.tab').read_text()
    if not any(len(line.split('\t')) >= 3 and line.split('\t')[2] == value['timezone']
               for line in zone_table.splitlines() if not line.startswith('#')):
        raise InstallError('Timezone is unavailable in the installer whitelist')
    if value.get('keyboard') != 'us' or value.get('rtc_as_localtime') is not False:
        raise InstallError('This reviewed bundle expects US keyboard and UTC hardware clock')
    wifi = value.get('wifi')
    if wifi:
        if not isinstance(wifi, dict) or not isinstance(wifi.get('ssid'), str) or not 1 <= len(wifi['ssid'].encode()) <= 32:
            raise InstallError('Invalid Wi-Fi SSID')
        if not isinstance(wifi.get('password'), str) or not 8 <= len(wifi['password'].encode()) <= 63:
            raise InstallError('Invalid Wi-Fi password length')
        if any(c in wifi['ssid'] + wifi['password'] for c in '\r\n\0'):
            raise InstallError('Wi-Fi profile contains unsupported control characters')
    return value


def inspect_payload():
    from postinstall import _rooted_resolve, _command, _unit
    recipe = json.loads(RECIPE.read_text())
    entries = [s for v in recipe['variants'] if v.get('name') == 'Desktop' and v.get('dir-name') == 'desktop'
               for s in v.get('squashfs', []) if s.get('arch') == 'loongson3']
    if len(entries) != 1 or entries[0].get('path') != str(PAYLOAD):
        raise InstallError('Expected the local loongson3 Desktop payload')
    if entries[0].get('date') != '20261017':
        raise InstallError('This bundle was reviewed for the 2026-10-17 ISO; recheck a different image')
    os_release = _rooted_resolve(PAYLOAD, '/usr/lib/os-release')
    if not os_release.is_file() or not re.search(r'^ID=["\']?aosc["\']?$', os_release.read_text(), re.M):
        raise InstallError('Mounted offline Desktop root is unavailable or not AOSC OS')
    needed = [('usr/bin/bash',), ('usr/bin/systemctl',), ('usr/bin/ssh-keygen',),
              ('usr/sbin/sshd', 'usr/bin/sshd'), ('usr/sbin/chpasswd', 'usr/bin/chpasswd'),
              ('usr/sbin/visudo', 'usr/bin/visudo'), ('usr/bin/sudo',),
              ('usr/lib/systemd/system/sshd.service', 'usr/lib/systemd/system/ssh.service'),
              ('usr/lib/systemd/system/sddm.service', 'usr/lib/systemd/system/gdm.service',
               'usr/lib/systemd/system/lightdm.service')]
    for candidates in needed:
        if not any(_rooted_resolve(PAYLOAD, '/' + rel).is_file() for rel in candidates):
            raise InstallError('Required offline system component missing: ' + candidates[0])
    for command in ('update-initramfs', 'grub-install', 'grub-mkconfig', 'useradd', 'usermod',
                    'chpasswd', 'passwd', 'bash', 'ssh-keygen', 'sshd', 'visudo', 'sudo',
                    'locale', 'hwclock', 'python3'):
        _command(PAYLOAD, command)
    if not _unit(PAYLOAD, 'display-manager.service') or not _unit(PAYLOAD, 'graphical.target'):
        raise InstallError('Offline Desktop has no configured display manager or graphical target')
    if not any(folder.is_dir() and list(folder.glob('*.desktop')) for folder in
               (PAYLOAD / 'usr/share/xsessions', PAYLOAD / 'usr/share/wayland-sessions')):
        raise InstallError('Offline Desktop has no graphical login session')
    for required_file in ('etc/fstab', 'etc/passwd', 'etc/shadow', 'etc/sudoers',
                          'usr/share/zoneinfo/Australia/Sydney'):
        if not _rooted_resolve(PAYLOAD, '/' + required_file).is_file():
            raise InstallError('Offline target is missing: ' + required_file)
    boot = _rooted_resolve(PAYLOAD, '/boot')
    if not any(p.is_file() and ('vmlinuz' in p.name or 'vmlinux' in p.name) for p in boot.iterdir()):
        raise InstallError('Offline Desktop does not contain a kernel')
    modules = _rooted_resolve(PAYLOAD, '/usr/lib/modules')
    grub_modules = _rooted_resolve(PAYLOAD, '/usr/lib/grub/mips64el-efi')
    if not modules.is_dir() or not any(modules.iterdir()):
        raise InstallError('Offline kernel module tree is absent')
    if not grub_modules.is_dir() or not any(grub_modules.glob('*.mod')):
        raise InstallError('Offline MIPS EFI GRUB modules are absent')
    for sibling in ('postinstall.py', 'finish-online.py'):
        if not Path(__file__).with_name(sibling).is_file():
            raise InstallError('Installation bundle is incomplete: ' + sibling)
    return {'date': entries[0]['date'], 'installed_bytes': entries[0]['instSize'],
            'path': str(PAYLOAD), 'variant': 'Desktop', 'architecture': 'loongson3'}


def preflight():
    if platform.system() != 'Linux' or platform.machine() not in ('mips64', 'mips64el') or sys.byteorder != 'little':
        raise InstallError('Run only in the MIPS64 little-endian AOSC LiveKit on the Loongson machine')
    if os.geteuid() != 0:
        raise InstallError('Run with sudo in AOSC LiveKit')
    missing = [cmd for cmd in REQUIRED if not shutil.which(cmd)]
    if missing:
        raise InstallError('LiveKit is missing tools: ' + ', '.join(missing))
    payload = inspect_payload()
    versions = {}
    for package, version in VERSION_PREFIXES.items():
        actual = run(['dpkg-query', '-W', '-f=${Version}', package]).stdout.strip()
        if not re.fullmatch(re.escape(version) + r'(?:-[0-9]+)?', actual):
            raise InstallError(f'{package} version differs from the reviewed {version}; stop before changing disks')
        versions[package] = actual
    if dbus('IsEFI') is not True:
        raise InstallError('DeployKit does not recognize EFI/Loongson LEFI; this wrapper will not guess a boot method')
    progress = dbus('GetProgress')
    if not isinstance(progress, dict) or progress.get('status') == 'Working':
        raise InstallError('Another DeployKit installation is running or status is unknown')
    return {'payload': payload, 'installer_versions': versions, 'firmware': 'DeployKit IsEFI=true (includes MIPS LEFI)'}


def live_media_devices():
    """Resolve physical/mapper/loop ancestors of the mounted installer media."""
    source = run(['findmnt', '--noheadings', '--raw', '--output', 'SOURCE', '--target',
                  '/run/livekit/livemnt']).stdout.strip().split('[', 1)[0]
    protected = set()
    for _ in range(6):
        if not source.startswith('/dev/'):
            raise InstallError('Cannot identify the block device backing LiveKit; no disk writes permitted')
        source = str(Path(source).resolve(strict=True))
        tree = json.loads(run(['lsblk', '--json', '--inverse', '--paths', '--output', 'NAME,TYPE', source]).stdout)
        entries = [item for root in tree['blockdevices'] for item in descendants(root)]
        protected.update(item['name'] for item in entries)
        if any(item['type'] in ('disk', 'rom') for item in entries):
            return protected
        if any(item['type'] == 'loop' for item in entries):
            backing = run(['losetup', '--noheadings', '--raw', '--output', 'BACK-FILE', source]).stdout.strip()
            if not backing.startswith('/') or '\n' in backing:
                break
            source = run(['findmnt', '--noheadings', '--raw', '--output', 'SOURCE', '--target', backing]).stdout.strip().split('[', 1)[0]
        else:
            break
    raise InstallError('LiveKit storage ancestry is ambiguous; no disk writes permitted')


def hostname(settings):
    prefix = settings.get('hostname_prefix', 'mk')
    if not re.fullmatch(r'[a-z][a-z0-9-]{0,20}', prefix):
        raise InstallError('Invalid hostname prefix')
    interfaces = sorted(Path('/sys/class/net').iterdir())
    for interface in interfaces:
        if (interface / 'device').exists():
            mac = (interface / 'address').read_text().strip().replace(':', '')
            if re.fullmatch('[0-9a-f]{12}', mac) and mac != '000000000000':
                return prefix + '-' + mac[-6:]
    return prefix + '-3a4000'


def partition_disk(device):
    # This function is reached ONLY after final interactive confirmation and a
    # fresh identity/mounted/holders check. Never use DeployKit AutoPartition:
    # its legacy LVM cleanup can remove unrelated device-mapper mappings.
    layout = ('label: gpt\n'
              'size=1GiB, type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B, name="AOSC EFI"\n'
              'type=0FC63DAF-8483-4772-8E79-3D69D8477DE4, name="AOSC root"\n')
    run(['sfdisk', '--lock=yes', '--wipe', 'always', '--wipe-partitions', 'always', device], input_text=layout)
    run(['udevadm', 'settle', '--timeout=15'])
    parts = json.loads(run(['sfdisk', '--json', device]).stdout)['partitiontable']['partitions']
    if len(parts) != 2:
        raise InstallError('Partition result is unexpected; do not proceed')
    efi, root = [p['node'] for p in parts]
    for path in (efi, root):
        parent = run(['lsblk', '--noheadings', '--paths', '--output', 'PKNAME', path]).stdout.strip()
        if str(Path(parent).resolve()) != device:
            raise InstallError('Partition belongs to a different disk; do not proceed')
    run(['mkfs.vfat', '-F', '32', '-n', 'AOSCEFI', efi], timeout=120)
    run(['mkfs.ext4', '-F', '-L', 'aosc-root', root], timeout=300)
    run(['udevadm', 'settle', '--timeout=15'])
    return root, efi


def write_installer_config(path, settings, root, efi, machine_name):
    # The backend currently logs its config, so this password is deliberately
    # disposable. The real hash is set only after the official install finishes.
    config = {'offline_install': True, 'rtc_as_localtime': False, 'variant': 'Desktop',
              'fullname': settings['fullname'], 'user': settings['username'],
              'password': secrets.token_urlsafe(48), 'hostname': machine_name,
              'timezone': settings['timezone'], 'target_part': root, 'efi_disk': efi,
              'locale': 'en_US.UTF-8', 'swapfile_size': 4}
    lines = [f'{key} = {json.dumps(value, ensure_ascii=False)}' for key, value in config.items()]
    path.write_text('\n'.join(lines) + '\n', encoding='utf-8')
    path.chmod(0o600)


def finish_target(runtime, root_part, efi_part, settings):
    from postinstall import configure_target
    target = runtime / 'target'
    target.mkdir(mode=0o700)
    run(['mount', root_part, str(target)])
    efi_mounted = False
    virtual_mounts = []
    try:
        run(['mount', '--make-private', str(target)])
        (target / 'efi').mkdir(exist_ok=True)
        run(['mount', efi_part, str(target / 'efi')])
        efi_mounted = True
        # These are mandatory success conditions, not optional download steps.
        if not (target / 'etc/fstab').is_file() or not (target / 'boot/grub/grub.cfg').is_file():
            raise InstallError('Installed fstab or GRUB configuration is missing')
        boot_files = list((target / 'boot').rglob('*'))
        if not any(p.is_file() and ('vmlinuz' in p.name or 'vmlinux' in p.name) for p in boot_files):
            raise InstallError('Installed kernel was not found')
        if not any(p.is_file() and ('initramfs' in p.name or 'initrd' in p.name) for p in boot_files):
            raise InstallError('Installed initramfs was not found')
        if not any(p.is_file() and p.name.lower() == 'bootmips.efi' for p in (target / 'efi').rglob('*')):
            raise InstallError('MIPS removable EFI loader was not found')
        for name in ('dev', 'proc', 'sys', 'run'):
            mountpoint = target / name
            if mountpoint.is_symlink():
                raise InstallError('Target virtual filesystem path unexpectedly is a symlink')
            mountpoint.mkdir(mode=0o755, exist_ok=True)
            if name in ('dev', 'sys'):
                run(['mount', '--rbind', '/' + name, str(mountpoint)])
                virtual_mounts.append(mountpoint)
                run(['mount', '--make-rslave', str(mountpoint)])
            else:
                options = ['-t', 'proc', 'proc'] if name == 'proc' else ['-t', 'tmpfs', '-o', 'mode=755,nosuid,nodev', 'tmpfs']
                run(['mount', *options, str(mountpoint)])
                virtual_mounts.append(mountpoint)
        warnings = configure_target(target, settings)
        # Clear the installer's disposable credential from its in-memory config.
        dbus('ResetConfig')
        return warnings
    finally:
        cleanup_errors = []
        for mountpoint in reversed(virtual_mounts):
            if run(['umount', '--recursive', str(mountpoint)], timeout=60, check=False).returncode:
                cleanup_errors.append(str(mountpoint))
        if efi_mounted:
            if run(['umount', str(target / 'efi')], timeout=60, check=False).returncode:
                cleanup_errors.append(str(target / 'efi'))
        if run(['umount', str(target)], timeout=60, check=False).returncode:
            cleanup_errors.append(str(target))
        if cleanup_errors:
            raise InstallError('Target unmount did not finish; do not remove power; inspect ' + str(runtime))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--settings', type=Path, default=Path(__file__).resolve().with_name('settings.json'))
    parser.add_argument('--disk', help='Explicit whole disk path or /dev/disk/by-id/...; never inferred')
    parser.add_argument('--install', action='store_true', help='Enable installation; still asks final erase confirmation')
    args = parser.parse_args(argv)
    os.umask(0o077)
    print('AOSC 3A4000: offline Desktop first; optional networking cannot block the base installation.')
    checked = preflight()
    settings = load_settings(args.settings)
    source_users = {line.split(':', 1)[0] for line in (PAYLOAD / 'etc/passwd').read_text().splitlines()}
    if settings['username'] in source_users:
        raise InstallError('Username already exists in the offline image; review before installing')
    machine_name = hostname(settings)
    settings['hostname'] = machine_name
    devices = disk_inventory()
    protected = live_media_devices()
    print(json.dumps(checked, ensure_ascii=False, indent=2))
    print(f'Account: {settings["username"]}; hostname: {machine_name}; default: graphical desktop')
    print('Disks (nothing has been erased):')
    for device in devices:
        if device['type'] == 'disk':
            description = fingerprint(device)
            description['refused_because'] = disk_rejections(device)
            if any(d['name'] in protected for d in descendants(device)):
                description['refused_because'].append('LiveKit installer media')
            print(json.dumps(description, ensure_ascii=False))
    if not args.install:
        print('Preflight finished. To install, add --disk /dev/disk/by-id/EXACT-DISK --install.')
        return 0
    if not args.disk or not sys.stdin.isatty():
        raise InstallError('--disk and a real terminal are required for final erase confirmation')
    selected = find_disk(devices, args.disk)
    if any(d['name'] in protected for d in descendants(selected)):
        raise InstallError('Selected disk backs the running installer')
    original_identity = fingerprint(selected)
    print('\nONLY THIS WHOLE DISK WILL BE ERASED:')
    print(json.dumps(original_identity, ensure_ascii=False, indent=2))
    confirm = 'ERASE ' + selected['name']
    if input(f'Type exactly {confirm} to erase it; anything else cancels: ').strip() != confirm:
        raise InstallError('Cancelled; no disk changes made')
    with open('/run/aosc-unattended.lock', 'w') as lock:
        fcntl.flock(lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        current = find_disk(disk_inventory(), args.disk)
        if fingerprint(current) != original_identity:
            raise InstallError('Disk identity changed after confirmation')
        if dbus('GetProgress').get('status') == 'Working':
            raise InstallError('Another installer started; no disk changes made')
        from postinstall import try_live_wifi
        warnings = try_live_wifi(settings)
        # Wi-Fi work must not weaken the final storage identity/mount checks.
        current = find_disk(disk_inventory(), args.disk)
        if fingerprint(current) != original_identity:
            raise InstallError('Disk identity changed before writing')
        if any(d['name'] in live_media_devices() for d in descendants(current)):
            raise InstallError('Selected disk now backs the running installer')
        final_progress = dbus('GetProgress')
        if not isinstance(final_progress, dict) or final_progress.get('status') not in ('Pending', 'Finish', 'Error'):
            raise InstallError('Another installer started or its status is unknown; no disk changes made')
        runtime = Path(tempfile.mkdtemp(prefix='aosc-unattended-', dir='/run'))
        report = {'started_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  'disk': original_identity, 'preflight': checked, 'warnings': warnings,
                  'complete': False, 'rebooted': False, 'stage': 'before-partitioning'}
        config_path = runtime / 'installer.toml'
        try:
            dbus('ResetConfig')
            dbus('ResetProgressStatus')
            root_part, efi_part = partition_disk(selected['name'])
            report['stage'] = 'partitioned'
            write_installer_config(config_path, settings, root_part, efi_part, machine_name)
            print('Installing the local Desktop image. This stage performs no system-image download.')
            # Do not kill long MIPS copy/initramfs/bootloader operations with a
            # network-style timeout. The official CLI displays live progress.
            completed = subprocess.run(['dkcli', '--config', str(config_path)], check=False)
            if completed.returncode != 0 or dbus('GetProgress').get('status') != 'Finish':
                raise InstallError('DeployKit did not finish successfully; do not reboot into the target yet')
            report['stage'] = 'core-copied-initialization-pending'
            print('Core system copied; verifying boot files and initializing the real account and SSH.')
            warnings += finish_target(runtime, root_part, efi_part, settings)
            os.sync()
            report.update(complete=True, stage='complete', root_partition=root_part, efi_partition=efi_part,
                          finished_utc=datetime.datetime.now(datetime.timezone.utc).isoformat())
        finally:
            config_path.unlink(missing_ok=True)
            # This report contains only status and disk identity, never settings.
            (runtime / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
        print('\nOFFLINE DESKTOP INSTALLATION COMPLETE. No automatic reboot was requested.')
        print('Your original account password and SSH public keys were preserved; Wi-Fi is saved if supported.')
        for warning in warnings:
            print('Optional step: ' + warning)
        print('Reboot when ready. First boot will try optional downloads in the background once; offline is OK.')
        print('Retry optional downloads later: sudo /usr/local/sbin/aosc-finish-online')
        print('Diagnostic summary: ' + str(runtime / 'result.json'))
        return 0


if __name__ == '__main__':
    from postinstall import ConfigurationError
    try:
        raise SystemExit(main())
    except (InstallError, ConfigurationError, OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        # Only our curated errors are printable; external exceptions may carry
        # settings fragments. Do not silently skip a failed core installation.
        message = str(error) if isinstance(error, (InstallError, ConfigurationError)) else type(error).__name__
        print('STOPPED: ' + message, file=sys.stderr)
        raise SystemExit(1)
