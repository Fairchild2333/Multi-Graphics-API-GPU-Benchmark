#!/bin/sh
# Installs the driver for the on-chip Ethernet of a Phytium D2000 board (two
# DesignWare GMACs, ACPI _HID PHYT0004) on a test machine with Ubuntu 26.04
# (Linux 7.0). Ubuntu's kernel has the stmmac core but not Phytium's glue, so
# the wired ports stay dark and the install runs over Wi-Fi
# (make-autoinstall.ps1 -Wifi).
#
# The glue is phytium/dwmac-phytium.c from Armbian
# (patch/kernel/archive/uefi-arm64-7.3/0001-net-stmmac-add-Phytium-DWMAC-glue-driver.patch),
# plus one change for 7.0, whose stmmac_plat_dat_alloc() does not yet
# allocate the DMA settings (the probe oopsed on them). DKMS builds it, and builds
# it again for each kernel update. It needs the stmmac driver's internal
# headers, which no headers package ships; they come from the linux-source
# package of the running kernel (about 190 MB, downloaded once).
#
# On the machine, after scripts/linux-suite.ps1 -BuildOnly has sent the source:
#   sh ~/mangekyo/scripts/linux/install-phytium-gmac.sh
# The installer's netplan configures any wired port named e* by DHCP, so the
# ports come up as soon as the module loads, and at every boot after.
set -eu
here=$(cd "$(dirname "$0")" && pwd)/phytium
kver=$(uname -r)
src=/usr/src/dwmac-phytium-1.0
export DEBIAN_FRONTEND=noninteractive

sudo -E apt-get install -y "linux-headers-$kver" dkms build-essential bzip2
sudo mkdir -p "$src/stmmac"
sudo cp "$here/dwmac-phytium.c" "$here/Makefile" "$here/dkms.conf" "$src/"
if [ ! -f "$src/stmmac/stmmac_platform.h" ]; then
    tmp=$(mktemp -d)
    pkgver=$(dpkg-query -W -f='${Version}' "linux-image-$kver")
    (cd "$tmp" && apt-get download "linux-source-${kver%%-*}=$pkgver")
    dpkg-deb --fsys-tarfile "$tmp"/linux-source-*.deb | tar -x -C "$tmp" --wildcards '*.tar.bz2'
    tar -xjf "$tmp"/usr/src/linux-source-*/linux-source-*.tar.bz2 -C "$tmp" \
        --wildcards '*/drivers/net/ethernet/stmicro/stmmac/*.h'
    sudo cp "$tmp"/linux-source-*/drivers/net/ethernet/stmicro/stmmac/*.h "$src/stmmac/"
    rm -rf "$tmp"
fi

# A rerun replaces an earlier build.
sudo dkms remove dwmac-phytium/1.0 --all 2>/dev/null || true
sudo dkms add dwmac-phytium/1.0
for k in $(ls /lib/modules); do
    [ -d "/lib/modules/$k/build" ] && sudo dkms install dwmac-phytium/1.0 -k "$k"
done
dkms status dwmac-phytium
sudo modprobe dwmac-phytium
sleep 5
ip -br addr
