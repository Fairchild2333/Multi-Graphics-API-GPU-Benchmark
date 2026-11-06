# TWRP for Black Shark 5 Pro (katyusha)

Unofficial TWRP 3.7.1 (twrp-12.1) for the Xiaomi Black Shark 5 Pro (SM8450).
Tested on JOYUI V11.0.4.0 (Android 12) and an Android 16 GSI.

## Status
**Working:** display, touch, adb, MTP, sideload, USB OTG, zip and image flashing, backup/restore, mounting all partitions, battery, vibration, all reboot options.

**Not working:** `/data` decryption. Use USB OTG or `adb sideload` instead of internal storage.

## Install
Requires an unlocked bootloader and `vbmeta` with verification disabled on the active slot.
```
fastboot flash recovery_a twrp-katyusha-v1.0.1.img
```
Replace `_a` with your active slot. Keep a backup of your stock recovery.

## Build
See [docs/BUILD.md](docs/BUILD.md). The device tree is in `device/blackshark/katyusha`.
More details in [docs/](docs/).

## Disclaimer
This project was built with the help of AI (Claude Code). The released build was tested on real hardware. Use at your own risk.

I'm not responsible for your emmc chip corrupting itself because you did a userdata backup 5000 times, your phone exploding, melting or in any other form deconstructing itself during a Magisk flash or your bootloader locking itself because it felt like it.
