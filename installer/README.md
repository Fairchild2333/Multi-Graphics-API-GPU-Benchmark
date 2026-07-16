# Inno Setup installer

`GpuComputeBenchmark.iss` (retained as an internal legacy source filename)
turns an existing verified Mangekyo Windows x64 stage into a per-user Setup
executable. It does not build C++, compile shaders, inspect
`C:\Program Files\RenderDoc`, or copy anything from vcpkg/Visual Studio.

## Input contract

First create and verify the stage:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\stage-windows-release.ps1 `
  -BuildGui `
  -RenderDocDir out\dependencies\RenderDoc
```

The default input is `out/stage/windows-x64`. It must contain:

- `app/bin/gpu_bench_gui.exe`, `gpu_benchmark.exe`, GLFW, app-local MSVC CRT,
  the WinAppSDK self-contained payload, and runtime shaders;
- `assets`, `scripts`, `licenses`, `release-manifest.json`, and the package
  limitations/readme plus the complete `files.sha256` inventory;
- optionally, a complete portable RenderDoc tree at `tools/RenderDoc` and/or a
  frozen `tools/report_worker/report_worker.exe` tree.

An incomplete RenderDoc directory is a compile error. At minimum the staged
tree must contain `renderdoccmd.exe`, `qrenderdoc.exe`, `renderdoc.dll`,
`renderdoc.json`, and its license. RenderDoc is exposed as an optional installer
component only when that portable payload is present. The installer never
harvests an arbitrary installed copy.

## Build

Install Inno Setup 6.3 or later on the build machine, then run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\build-inno-installer.ps1 `
  -StageDir out\stage\windows-x64 `
  -Version 0.1.0
```

Use `-StaticOnly` on machines without Inno Setup to validate the input contract
and installer invariants. `-AllowCliOnly` exists solely for engineering smoke
artifacts; the normal installer fails if the GUI is missing. If `-Version` is
omitted, the build script uses `release-manifest.json`; an explicit mismatched
version is rejected so the Setup filename cannot mislabel its staged payload.

The stable AppId is `{9DBD8675-1CE2-45DF-83BB-2E62EB71796B}`. Never change it
for normal upgrades: Inno uses AppId to associate subsequent versions with the
same uninstall record. The installer accepts only x64-compatible Windows and
uses 64-bit install mode. A fresh install uses
`%LOCALAPPDATA%\Programs\Mangekyo`, creates Mangekyo Start Menu and optional
desktop shortcuts, and supplies automatic uninstall support. `UsePreviousAppDir`
is intentionally retained, so an in-place upgrade from an older branded build
can keep its existing program directory instead of breaking the uninstall
record.

For a signed public artifact, pass an Inno sign-tool command containing its
`$f` filename placeholder and make a valid signature mandatory:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\build-inno-installer.ps1 `
  -StageDir out\stage\windows-x64 `
  -SignToolCommand '<signtool command containing $f>' `
  -RequireSigned
```

This signs both Setup and its generated uninstaller. Certificate selection and
timestamp-server policy belong to release CI; do not put certificate passwords
in the repository or command history.

Uninstall deliberately preserves the existing data contract at
`%LOCALAPPDATA%\GpuComputeBenchmark\{results,captures,reports,logs}`. Mangekyo
continues using that legacy directory for compatibility; this branding change
does not migrate, duplicate, or delete historical results. Add a
separate, explicit user-data deletion UX if that policy changes; do not silently
add those directories to `[UninstallDelete]`.

## Release gates

This remains an engineering installer until the root project distribution
license is approved, third-party notices receive final review, binaries and
Setup are signed, and clean-machine install/upgrade/uninstall plus GPU/capture
tests pass. See `packaging/PACKAGE_LIMITATIONS.md`.

Inno references: [architecture checks](https://jrsoftware.org/ishelp/topic_setup_architecturesallowed.htm),
[stable AppId/upgrades](https://jrsoftware.org/ishelp/topic_setup_appid.htm), and
[automatic uninstall](https://jrsoftware.org/ishelp/topic_setup_uninstallable.htm).
