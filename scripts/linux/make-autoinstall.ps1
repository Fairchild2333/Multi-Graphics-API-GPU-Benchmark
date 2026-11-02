<#
.SYNOPSIS
    Writes out\linux-autoinstall\autoinstall.yaml from the template with this
    PC's SSH public key, ready for the password hash and the USB stick.

.DESCRIPTION
    The output folder is ignored by Git. Add the password hash by hand:
    in Git Bash, `openssl passwd -6`, then replace __PASSWORD_HASH__ with the
    output. A hash already filled into an earlier autoinstall.yaml is kept.
    Copy the file to the root of the USB stick written from the Ubuntu 26.04
    live-server ISO (next to the casper directory).

.PARAMETER Wifi
    Installs over Wi-Fi as well, for a machine whose wired port has no driver
    in Ubuntu's kernel (the on-chip Ethernet of Phytium D2000 boards). Replace
    __WIFI_SSID__ and __WIFI_PASSWORD__ in the written file by hand; they are
    never committed.

.PARAMETER WifiInterface
    The Wi-Fi interface, as the installer names it (`ip -br link` in its
    shell). systemd-networkd, which the installer uses, accepts Wi-Fi only by
    interface name, not by a match. Default wlp5s0 (the Phytium D2000 board).

.PARAMETER Focal
    Writes out\linux-autoinstall\focal\user-data and meta-data for an Ubuntu 20.04
    test machine instead (autoinstall-2004.template.yaml). Both go to the root of
    the USB stick written from the 20.04 live-server ISO. Takes the password hash
    from an earlier autoinstall.yaml if there is one. Not with -Wifi.
#>
param(
    [string]$PublicKey = (Join-Path $env:USERPROFILE ".ssh\id_ed25519.pub"),
    [switch]$Wifi,
    [string]$WifiInterface = "wlp5s0",
    [switch]$Focal
)
$ErrorActionPreference = "Stop"
if ($Focal -and $Wifi) { throw "-Focal does not support -Wifi" }
$repo = Split-Path (Split-Path $PSScriptRoot)
$key = (Get-Content $PublicKey -Raw).Trim()
if ($key -notmatch '^ssh-(ed25519|rsa) ') { throw "$PublicKey does not look like an SSH public key" }
$templateName = if ($Focal) { "autoinstall-2004.template.yaml" } else { "autoinstall.template.yaml" }
$template = Get-Content (Join-Path $PSScriptRoot $templateName) -Raw
$outDir = Join-Path $repo "out\linux-autoinstall"
New-Item -ItemType Directory -Force $outDir | Out-Null
$out = Join-Path $outDir "autoinstall.yaml"

$text = $template.Replace("__SSH_PUBLIC_KEY__", $key)

# Keep a password hash filled into an earlier file (it is not shown).
$hashKept = $false
if (Test-Path $out) {
    $line = Select-String -Path $out -Pattern '^\s+password: "(\$[^"]+)"' | Select-Object -First 1
    if ($line) {
        $text = $text.Replace('"__PASSWORD_HASH__"', '"' + $line.Matches[0].Groups[1].Value + '"')
        $hashKept = $true
    }
}

if ($Wifi) {
    $network = @'
  # The live installer adds wpa_supplicant from the ISO only once it sees a
  # Wi-Fi configuration, and applied the network before that had finished
  # (Wi-Fi never started). Wait for it, up to 3 minutes.
  early-commands:
    - "for i in $(seq 1 90); do [ -x /usr/sbin/wpa_supplicant ] && [ ! -L /usr/sbin/wpa_supplicant ] && exit 0; sleep 2; done; exit 0"
  # Wi-Fi as well as any wired port (the mirror and the build packages need
  # the network; without it the install still finishes from the ISO).
  network:
    version: 2
    ethernets:
      wired:
        match:
          name: "e*"
        dhcp4: true
        optional: true
    wifis:
      __WIFI_INTERFACE__:
        dhcp4: true
        access-points:
          "__WIFI_SSID__":
            password: "__WIFI_PASSWORD__"
'@
    $network = $network.Replace("__WIFI_INTERFACE__", $WifiInterface)
    $text = $text.Replace("  locale: en_AU.UTF-8", $network.TrimEnd() + "`n  locale: en_AU.UTF-8")
    $text = $text.Replace(" avahi-daemon ", " avahi-daemon wpasupplicant ")
}

$utf8 = New-Object Text.UTF8Encoding $false
if ($Focal) {
    # This ISO's installer takes a NoCloud seed from the stick (see the template).
    $focalDir = Join-Path $outDir "focal"
    New-Item -ItemType Directory -Force $focalDir | Out-Null
    $out = Join-Path $focalDir "user-data"
    [IO.File]::WriteAllText($out, "#cloud-config`n" + $text.Replace("`r`n", "`n"), $utf8)
    [IO.File]::WriteAllText((Join-Path $focalDir "meta-data"), "", $utf8)
} else {
    [IO.File]::WriteAllText($out, $text.Replace("`r`n", "`n"), $utf8)
}
Write-Host "Wrote $out"
if ($hashKept) { Write-Host "The password hash of the earlier file was kept." }
else { Write-Host "Next: replace __PASSWORD_HASH__ with the output of 'openssl passwd -6' (Git Bash)." }
if ($Wifi) { Write-Host "Next: replace __WIFI_SSID__ and __WIFI_PASSWORD__ with your network's name and password." }
