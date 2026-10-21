#!/bin/sh
# First-boot setup of a test machine installed from autoinstall.template.yaml,
# run over SSH from the PC:  ssh fairchild@mk-xxxxxx.local 'sh -s' < setup-desktop.sh
# Adds the GNOME desktop (the windowed tests need a running session), logs the
# test user in by itself, keeps the screen on and unlocked, and reboots into
# the desktop. Uses passwordless sudo.
set -eu
user=$(id -un)
export DEBIAN_FRONTEND=noninteractive
sudo -E apt-get update
sudo -E apt-get install -y ubuntu-desktop-minimal
sudo mkdir -p /etc/gdm3
printf '[daemon]\nAutomaticLoginEnable=true\nAutomaticLogin=%s\n' "$user" | sudo tee /etc/gdm3/custom.conf > /dev/null

# A dedicated test machine: no screen blanking, no lock screen, no suspend.
# GNOME blanks after 5 idle minutes and then locks; a series started on a
# locked screen would draw its windowed tests behind the lock. System-wide
# dconf defaults, so they hold before the first login too.
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

# The desktop brings NetworkManager, but the wired port is already set up by
# the server install's netplan (systemd-networkd). With both managing it, a
# NetworkManager IPv6 change crashed networkd and dropped the network for a
# minute; NetworkManager leaves wired ports alone.
sudo mkdir -p /etc/NetworkManager/conf.d
printf '[keyfile]\nunmanaged-devices=type:ethernet\n' | sudo tee /etc/NetworkManager/conf.d/90-mangekyo-unmanaged.conf > /dev/null

# No crash pop-ups: Apport's "System program problem detected" came back at
# every login for crashes that were long dealt with, and would collect a core
# dump in the middle of a series. A crash shows in the job's log anyway.
sudo sed -i 's/^enabled=1/enabled=0/' /etc/default/apport 2>/dev/null || true
sudo systemctl disable --now apport.service apport-forward.socket apport-autoreport.path apport-autoreport.timer 2>/dev/null || true

sudo systemctl set-default graphical.target
echo "Desktop installed; rebooting into an automatic login of $user."
sudo systemctl reboot
