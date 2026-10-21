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

sudo systemctl set-default graphical.target
echo "Desktop installed; rebooting into an automatic login of $user."
sudo systemctl reboot
