#!/bin/sh
# Sets up a Loongson LS3A4000-7A1000-NUC-SE (3A4000 CPU, LS7A1000 bridge) with
# AOSC OS (loongson3 port, Linux 7.1) as a test machine: 1080p60 on its HDMI
# port, and the bridge's Vivante GC1000 GPU for OpenGL ES 2.0 and for the X
# server's glamor. docs/building.md (Loongson 3A4000) says why each part.
#
# - HDMI: DVO output DPI-2 goes through an MS7210 DVO-to-HDMI bridge that
#   mainline drm/loongson leaves alone. ms7210-init replays the writes
#   UnionTech OS's driver makes at boot (loongson/ms7210-boot-trace.txt);
#   without them nothing above about 100 MHz shows. The bridge passes no EDID:
#   the GRUB entry 09_ls7a forces DPI-2 on and gives it a 1080p60 EDID.
# - GPU: etnaviv-ls7a hands the GC1000, a PCI function, to etnaviv, which has
#   no PCI support. ls7a-gpu-linux-7.1.patch makes etnaviv map buffers cached
#   on this cache-coherent bridge and lets the GPU draw into the VRAM the
#   display controller scans out from. The patched etnaviv and loongson
#   modules go to /lib/modules/<kernel>/updates.
# - Mesa: mesa-26.1-kmsro-ls7a.patch pairs the PCI display controller with
#   etnaviv. That Mesa goes to /opt/mesa-ls7a and only the X server loads it
#   (lightdm-90-mangekyo.conf); applications keep the system Mesa.
#
# Xorg, LightDM and Xfce are installed first. On the machine, after
# scripts/linux-suite.ps1 -BuildOnly has sent the source:
#   sh ~/mangekyo/scripts/linux/install-ls7a.sh
# then reboot. It downloads the kernel and Mesa sources of the running
# versions into ~/ls7a-build (about 230 MB) and builds Mesa (about 10
# minutes); a rerun reuses them.
set -eu
here=$(cd "$(dirname "$0")" && pwd)/loongson
kver=$(uname -r)
base=${kver%%-*}
mver=$(dpkg-query -W -f='${Version}' mesa)
mver=${mver#*:}
mver=${mver%-*}
work=$HOME/ls7a-build
kbuild=/usr/lib/modules/$kver/build
mkdir -p "$work"

sudo oma install -y gcc make patch meson ninja bison flex i2c-tools

# HDMI: the EDID, the bridge's set-up at boot, and again after a display change
python3 "$here/make-edid.py" "$work/ls7a-dvo.bin"
sudo install -D -m644 "$work/ls7a-dvo.bin" /usr/lib/firmware/edid/ls7a-dvo.bin
sudo install -D -m755 "$here/ms7210-init" /usr/local/sbin/ms7210-init
sudo install -m644 "$here/ms7210-init.service" "$here/ms7210-reinit.service" /etc/systemd/system/
sudo install -m644 "$here/90-ms7210.rules" /etc/udev/rules.d/
echo i2c-dev | sudo tee /etc/modules-load.d/i2c-dev.conf >/dev/null
sudo systemctl daemon-reload
sudo systemctl enable ms7210-init.service
sudo udevadm control --reload

# GRUB: 09_ls7a writes the first entry, which is the default
sudo install -m755 "$here/09_ls7a" /etc/grub.d/09_ls7a
sudo sed -i -e 's/^GRUB_DEFAULT=.*/GRUB_DEFAULT=0/' -e 's/^GRUB_SAVEDEFAULT=.*/GRUB_SAVEDEFAULT=false/' /etc/default/grub
grep -q '^GRUB_DISABLE_OS_PROBER=' /etc/default/grub ||
    echo 'GRUB_DISABLE_OS_PROBER=true' | sudo tee -a /etc/default/grub >/dev/null
sudo grub-mkconfig -o /boot/grub/grub.cfg

# Kernel modules: etnaviv and loongson from the running version's source
ksrc=$work/linux-$base
if [ ! -d "$ksrc" ]; then
    rm -rf "$ksrc.tmp"
    mkdir "$ksrc.tmp"
    curl -fL "https://cdn.kernel.org/pub/linux/kernel/v${base%%.*}.x/linux-$base.tar.xz" | xz -dc |
        tar -x -C "$ksrc.tmp" "linux-$base/drivers/gpu/drm/etnaviv" "linux-$base/drivers/gpu/drm/loongson"
    patch -p1 -d "$ksrc.tmp/linux-$base" < "$here/ls7a-gpu-linux-7.1.patch"
    mv "$ksrc.tmp/linux-$base" "$ksrc"
    rmdir "$ksrc.tmp"
fi
make -C "$kbuild" M="$ksrc/drivers/gpu/drm/etnaviv" CONFIG_DRM_ETNAVIV=m modules
make -C "$kbuild" M="$ksrc/drivers/gpu/drm/loongson" CONFIG_DRM_LOONGSON=m modules
rm -rf "$work/etnaviv-ls7a"
cp -r "$here/etnaviv-ls7a" "$work/"
make -C "$kbuild" M="$work/etnaviv-ls7a" modules
sudo install -D -m644 "$ksrc/drivers/gpu/drm/etnaviv/etnaviv.ko" "/usr/lib/modules/$kver/updates/etnaviv.ko"
sudo install -m644 "$ksrc/drivers/gpu/drm/loongson/loongson.ko" "$work/etnaviv-ls7a/etnaviv_ls7a.ko" \
    "/usr/lib/modules/$kver/updates/"
sudo install -m644 "$here/etnaviv-ls7a.conf" /etc/modprobe.d/etnaviv-ls7a.conf
sudo depmod -a "$kver"

# Mesa for the X server: the system's version, with the kmsro pairing
msrc=$work/mesa-$mver
if [ ! -d "$msrc" ]; then
    rm -rf "$msrc.tmp"
    mkdir "$msrc.tmp"
    curl -fL "https://archive.mesa3d.org/mesa-$mver.tar.xz" | xz -dc | tar -x -C "$msrc.tmp"
    patch -p1 -d "$msrc.tmp/mesa-$mver" < "$here/mesa-26.1-kmsro-ls7a.patch"
    mv "$msrc.tmp/mesa-$mver" "$msrc"
    rmdir "$msrc.tmp"
fi
if [ ! -d "$msrc/build" ]; then
    meson setup "$msrc/build" "$msrc" --prefix=/opt/mesa-ls7a --buildtype=release \
        -Db_ndebug=true \
        -Dgallium-drivers=etnaviv -Dvulkan-drivers= -Dvulkan-layers= \
        -Dplatforms=x11 -Dglx=dri -Degl=enabled -Dgbm=enabled -Dlibgbm-external=false \
        -Dglvnd=enabled -Dopengl=true -Dgles1=disabled -Dgles2=enabled \
        -Dllvm=disabled -Dgallium-rusticl=false -Dgallium-va=disabled -Dvideo-codecs= \
        -Dvalgrind=disabled -Dlibunwind=disabled -Dlmsensors=disabled \
        -Dshader-cache=enabled -Dzstd=enabled -Dxlib-lease=disabled \
        -Dtools= -Dbuild-tests=false -Dhtml-docs=disabled -Dperfetto=false -Dteflon=false
fi
ninja -C "$msrc/build"
sudo ninja -C "$msrc/build" install

# X: glamor on the GC1000; LightDM logs this user in to Xfce
sudo install -D -m644 "$here/xorg-20-mangekyo.conf" /etc/X11/xorg.conf.d/20-mangekyo.conf
sudo mkdir -p /etc/lightdm/lightdm.conf.d
sed "s/^autologin-user=.*/autologin-user=$(id -un)/" "$here/lightdm-90-mangekyo.conf" |
    sudo tee /etc/lightdm/lightdm.conf.d/90-mangekyo.conf >/dev/null

echo "Done; reboot to load the patched modules."
