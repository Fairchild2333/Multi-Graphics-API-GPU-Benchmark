#!/bin/sh
# First-boot setup of an Ubuntu 20.04 test machine installed from
# autoinstall-2004.template.yaml, run over SSH from the PC:
#   ssh fairchild@mk-xxxxxx.local 'sh -s' < setup-focal.sh
# Build packages, GNOME on X.org with an automatic login, and a kernel that stays
# at 5.4: NVIDIA 340.108 supports kernels up to 5.4 and 390.157 is reported broken
# from 6.5. Then the same test-machine settings as setup-desktop.sh. Uses
# passwordless sudo. The NVIDIA driver is installed afterwards, for the card in the
# machine:  sudo apt-get install -y nvidia-340 (or nvidia-driver-390, -470)
set -eu
user=$(id -un)
export DEBIAN_FRONTEND=noninteractive
sudo -E apt-get update

# glslc is not packaged here: Vulkan shaders come as SPIR-V from a Windows build
# (docs/building.md). CMake 3.16 is too old: 3.22 or newer from pip, which
# scripts/linux/build.sh finds in ~/.local/bin.
sudo -E apt-get install -y build-essential git pkg-config libglfw3-dev libgl-dev \
    libvulkan-dev vulkan-tools libegl-dev libgles-dev mesa-utils lm-sensors pciutils \
    avahi-daemon curl rsync python3-pip dkms
python3 -m pip install --user "cmake<4"

sudo -E apt-get install -y ubuntu-desktop-minimal

# X.org: the 340 and 390 drivers cannot run GNOME on Wayland, and this keeps one
# session type for every card on this system.
sudo mkdir -p /etc/gdm3
printf '[daemon]\nAutomaticLoginEnable=true\nAutomaticLogin=%s\nWaylandEnable=false\n' "$user" | sudo tee /etc/gdm3/custom.conf > /dev/null

# Stay on the 5.4 kernel: no automatic upgrades, and no move to the HWE series.
sudo -E apt-get remove -y unattended-upgrades 2>/dev/null || true
sudo systemctl disable --now apt-daily.timer apt-daily-upgrade.timer 2>/dev/null || true
for p in linux-generic linux-image-generic linux-headers-generic; do
    sudo apt-mark hold "$p" 2>/dev/null || true
done
sudo -E apt-get install -y "linux-headers-$(uname -r)"
uname -r

# A dedicated test machine: no screen blanking, no lock screen, no suspend.
sudo mkdir -p /etc/dconf/profile /etc/dconf/db/local.d
printf 'user-db:user\nsystem-db:local\n' | sudo tee /etc/dconf/profile/user > /dev/null
sudo tee /etc/dconf/db/local.d/90-mangekyo > /dev/null <<'EOF'
[org/gnome/desktop/session]
idle-delay=uint32 0

[org/gnome/desktop/screensaver]
lock-enabled=false
idle-activation-enabled=false

[org/gnome/settings-daemon/plugins/power]
sleep-inactive-ac-type='nothing'
sleep-inactive-battery-type='nothing'
EOF
sudo dconf update

# The wired port stays with the server install's netplan (see setup-desktop.sh).
sudo mkdir -p /etc/NetworkManager/conf.d
printf '[keyfile]\nunmanaged-devices=type:ethernet;type:wifi\n' | sudo tee /etc/NetworkManager/conf.d/90-mangekyo-unmanaged.conf > /dev/null
sudo mkdir -p /etc/systemd/system/systemd-networkd-wait-online.service.d
printf '[Service]\nExecStart=\nExecStart=-/lib/systemd/systemd-networkd-wait-online --any --timeout=5\n' | sudo tee /etc/systemd/system/systemd-networkd-wait-online.service.d/90-mangekyo.conf > /dev/null

# No crash pop-ups in the middle of a series.
sudo sed -i 's/^enabled=1/enabled=0/' /etc/default/apport 2>/dev/null || true
sudo systemctl disable --now apport.service 2>/dev/null || true

sudo systemctl set-default graphical.target
echo "Desktop installed; rebooting into an automatic login of $user on X.org."
sudo systemctl reboot
