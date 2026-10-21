#!/bin/sh
# Installs Zhaoxin's graphics driver for a KX-6000 / KX-6900 (C-960 GPU) on a
# test machine with Ubuntu 26.04 (Linux 7.0), after setup-desktop.sh.
#
# The driver is the UOS build from zhaoxin.com (KX-6000_6900-UOS_x64-21.00.93.zip,
# zhaoxin-linux-graphics-driver-dri_21.00.93_amd64.deb). Zhaoxin supports Linux
# 4.19, 5.10 and 6.6 with it, and its kernel module does not build on 7.0;
# zhaoxin/zx-21.00.93-linux-7.0.patch ports it (every change is behind a
# version check, older kernels build as before). On 7.0 the driver has no
# framebuffer console of its own; the desktop drives the screen through KMS.
#
# On the machine, after scripts/linux-suite.ps1 -BuildOnly has sent the source
# and the .deb has been copied to the home folder:
#   sh ~/mangekyo/scripts/linux/install-zhaoxin.sh ~/zhaoxin-linux-graphics-driver-dri_21.00.93_amd64.deb
# Then reboot: zx takes the GPU over from simpledrm at boot.
set -eu
deb=$(readlink -f "$1")
patch=$(cd "$(dirname "$0")" && pwd)/zhaoxin/zx-21.00.93-linux-7.0.patch
src=/usr/src/zx-21.00.93
export DEBIAN_FRONTEND=noninteractive

sudo -E apt-get install -y "linux-headers-$(uname -r)" dkms patch
# The package runs its own DKMS build, which fails on 7.0 without the patch;
# the package itself installs regardless.
sudo -E apt-get install -y "$deb" || true
if grep -q "Kbuild in Linux 7.0" "$src/Makefile"; then
    echo "$src is already patched."
else
    sudo patch -p1 -d "$src" < "$patch"
fi
for k in $(ls /lib/modules); do
    [ -d "/lib/modules/$k/build" ] && sudo dkms install zx/21.00.93 -k "$k"
done
dkms status zx

# GTK 4 draws with OpenGL 3.3 / ES 3.0 at least; the driver offers 3.2 / ES 2.0
# and GTK 4 windows (Settings) came up blank. GTK 4 draws in software instead.
mkdir -p "$HOME/.config/environment.d"
printf 'GSK_RENDERER=cairo\n' > "$HOME/.config/environment.d/90-gsk-cairo.conf"
echo "Done; reboot to load the zx driver."
