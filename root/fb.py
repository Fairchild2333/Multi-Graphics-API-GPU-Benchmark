"""Minimal fastboot client over pyusb/libusb.

Usage: python fb.py <command> [command...]
Examples:
  python fb.py getvar:current-slot
  python fb.py oem unlock
  python fb.py continue
Fastboot wire protocol: ASCII commands out on bulk-OUT; responses on bulk-IN
as 4-char tags OKAY/FAIL/DATA/INFO + payload. DATA<size> precedes a download
data phase. Written for the Black Shark 3 Pro unlock session (18D1:D00D).
"""
import os
import sys
import time

# Make libusb-1.0.dll (shipped inside the edl repo) loadable.
_DLL_DIR = os.path.join(os.environ.get("TEMP", ""), "edl-master", "edlclient", "Windows")
if os.path.isdir(_DLL_DIR):
    os.add_dll_directory(_DLL_DIR)
    os.environ["PATH"] = _DLL_DIR + ";" + os.environ["PATH"]

import usb.core
import usb.util

VID, PID = 0x18D1, 0xD00D
TIMEOUT_MS = 30000


def open_device():
    dev = usb.core.find(idVendor=VID, idProduct=PID)
    if dev is None:
        raise SystemExit("no fastboot device (18D1:D00D) visible to libusb")
    try:
        dev.set_configuration()
    except usb.core.USBError as e:
        if "Resource busy" not in str(e) and e.errno != 13:
            raise
    cfg = dev.get_active_configuration()
    intf = cfg[(0, 0)]
    bulk_out = None
    bulk_in = None
    for ep in intf:
        if ep.bmAttributes == 2:  # bulk
            if ep.bEndpointAddress & 0x80:
                bulk_in = ep
            else:
                bulk_out = ep
    if bulk_in is None or bulk_out is None:
        raise SystemExit(f"no bulk endpoints on interface 0 (in={bulk_in} out={bulk_out})")
    return dev, bulk_out, bulk_in


def read_response(bulk_in):
    """Read fastboot response packets until a terminal OKAY/FAIL."""
    while True:
        data = bytes(bulk_in.read(bulk_in.wMaxPacketSize or 512, TIMEOUT_MS))
        if len(data) < 4:
            continue
        tag, payload = data[:4].decode("ascii", "replace"), data[4:]
        if tag == "INFO" or tag == "TEXT":
            print("INFO " + payload.decode("utf-8", "replace"))
            continue
        if tag == "OKAY":
            return True, payload.decode("utf-8", "replace")
        if tag == "FAIL":
            return False, payload.decode("utf-8", "replace")
        if tag == "DATA":
            return True, "DATA " + payload.decode("ascii", "replace")
        print("?? " + data.hex())


def send_command(bulk_out, bulk_in, cmd):
    bulk_out.write(cmd.encode("ascii"), TIMEOUT_MS)
    return read_response(bulk_in)


def send_data(dev, bulk_out, bulk_in, blob):
    ok, resp = send_command(bulk_out, bulk_in, "download:%08x" % len(blob))
    if not ok or not resp.startswith("DATA"):
        raise SystemExit("download rejected: " + resp)
    size = int(resp[4:12], 16)
    if size != len(blob):
        raise SystemExit(f"size mismatch device={size} host={len(blob)}")
    for off in range(0, len(blob), 1024 * 1024):
        bulk_out.write(blob[off:off + 1024 * 1024], TIMEOUT_MS)
    ok, resp = read_response(bulk_in)
    if not ok:
        raise SystemExit("data phase failed: " + resp)


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    dev, out_ep, in_ep = open_device()
    print(f"device open: {VID:#06x}:{PID:#06x} in={in_ep.bEndpointAddress:#04x} out={out_ep.bEndpointAddress:#04x}")
    # Args starting with @: download that file first. Remaining args join
    # into ONE command ("oem unlock" is one command, not two).
    for arg in sys.argv[1:]:
        if arg.startswith("@"):
            with open(arg[1:], "rb") as f:
                print(f"> download {arg[1:]}")
                send_data(dev, out_ep, in_ep, f.read())
    cmd = " ".join(a for a in sys.argv[1:] if not a.startswith("@"))
    if cmd:
        print(f"> {cmd}")
        ok, resp = send_command(out_ep, in_ep, cmd)
        print(("OKAY " if ok else "FAIL ") + resp)


if __name__ == "__main__":
    main()
