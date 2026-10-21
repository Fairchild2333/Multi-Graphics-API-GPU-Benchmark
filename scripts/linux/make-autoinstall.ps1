<#
.SYNOPSIS
    Writes out\linux-autoinstall\autoinstall.yaml from the template with this
    PC's SSH public key, ready for the password hash and the USB stick.

.DESCRIPTION
    The output folder is ignored by Git. Add the password hash by hand:
    in Git Bash, `openssl passwd -6`, then replace __PASSWORD_HASH__ with the
    output. Copy the file to the root of the USB stick written from the
    Ubuntu 26.04 live-server ISO (next to the casper directory).
#>
param(
    [string]$PublicKey = (Join-Path $env:USERPROFILE ".ssh\id_ed25519.pub")
)
$ErrorActionPreference = "Stop"
$repo = Split-Path (Split-Path $PSScriptRoot)
$key = (Get-Content $PublicKey -Raw).Trim()
if ($key -notmatch '^ssh-(ed25519|rsa) ') { throw "$PublicKey does not look like an SSH public key" }
$template = Get-Content (Join-Path $PSScriptRoot "autoinstall.template.yaml") -Raw
$outDir = Join-Path $repo "out\linux-autoinstall"
New-Item -ItemType Directory -Force $outDir | Out-Null
$out = Join-Path $outDir "autoinstall.yaml"
[IO.File]::WriteAllText($out, $template.Replace("__SSH_PUBLIC_KEY__", $key), (New-Object Text.UTF8Encoding $false))
Write-Host "Wrote $out"
Write-Host "Next: replace __PASSWORD_HASH__ with the output of 'openssl passwd -6' (Git Bash)."
