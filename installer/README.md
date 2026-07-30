# Windows installers

See [`../docs/windows-installer-packaging.md`](../docs/windows-installer-packaging.md)
for the complete day-to-day build, dual-architecture release, signing and
clean-machine acceptance procedure.

The release installer is a native, dependency-free Win32 bootstrapper backed by
a WiX MSI. Two files are published: one native x64 EXE and one native ARM64 EXE.
Each is a single-file installer with an explicit English/Simplified Chinese
selector; there are no separate language downloads.

The native Setup draws its own theme-aware rounded progress indicator and keeps
the completion state visible instead of closing immediately. Before elevation,
users can choose a desktop shortcut (off by default) and a Start menu shortcut
(on by default). These choices are passed to conditional WiX MSI components so
repair and uninstall retain normal Windows Installer ownership and cleanup.
The same controls are enabled again on the completion page. Changed choices
switch the primary action to Apply and run MSI maintenance against transitive
shortcut components, so repeated application updates the same component and
never creates a second shortcut.

`scripts/build-native-bootstrapper.ps1` compiles the same source for both
architectures and embeds the matching MSI as an executable overlay. The Setup
extracts that MSI to a unique temporary path, invokes Windows Installer with
elevation, waits for completion, and removes the temporary file. MSI product and
upgrade identities remain architecture-specific, so x64 and ARM64 installations
can coexist on Windows on Arm.

`GpuComputeBenchmark.iss` and `scripts/build-inno-installer.ps1` remain a
**legacy** engineering path only.

For a local **developer** CLI+GUI tree (not an installer), use
`scripts/build-windows.ps1` instead — see `docs/building.md`.

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

## Build the release installers

Build the two WiX MSI intermediates first, then package them:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build-wix-installer.ps1 -Arch x64
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build-wix-installer.ps1 -Arch ARM64
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build-native-bootstrapper.ps1 -Arch Both
```

The resulting release files are:

```text
out/installer/Mangekyo-<version>-windows-x64-setup.exe
out/installer/Mangekyo-<version>-windows-arm64-setup.exe
```

Both installers support interactive language switching. For managed deployment,
use `--quiet` and optionally `--lang en` or `--lang zh-CN`. Public files should
be Authenticode-signed by passing a sign command containing `$f`; the build
script can enforce this with `-RequireSigned`.

## Build the legacy Inno installer

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

The x64 AppId remains `{9DBD8675-1CE2-45DF-83BB-2E62EB71796B}` so normal x64
upgrades keep the existing uninstall record. ARM64 uses the separate AppId
`{4B7DF8D6-8FE7-4A29-A04B-19B21957B58D}` and installs to `Mangekyo ARM64`;
therefore Windows on Arm can keep the native ARM64 and emulated x64 editions
installed side by side. Do not change either architecture identity for normal
upgrades. The x64 installer accepts x64-compatible Windows and
uses 64-bit install mode. A fresh install uses
`%LOCALAPPDATA%\Programs\Mangekyo`, creates Mangekyo Start Menu and optional
desktop shortcuts, and supplies automatic uninstall support. `UsePreviousAppDir`
is intentionally retained, so an in-place upgrade from an older branded build
can keep its existing program directory instead of breaking the uninstall
record.

## Legacy Inno languages

The Setup wizard ships English and Simplified Chinese
(`installer/languages/ChineseSimplified.isl`). It always shows the language
choice before installation, so Simplified Chinese remains discoverable even
when Windows or a previous installation selected English.
`UsePreviousLanguage=yes` preselects the previous choice on upgrade. The
installed GUI has its own in-app language switch and is independent of the
Setup wizard language.

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

The native release path still needs Authenticode signing and clean-machine
install/upgrade/uninstall validation before public use. The legacy Inno path is
not a release substitute. See `packaging/PACKAGE_LIMITATIONS.md`.

Inno references: [architecture checks](https://jrsoftware.org/ishelp/topic_setup_architecturesallowed.htm),
[stable AppId/upgrades](https://jrsoftware.org/ishelp/topic_setup_appid.htm), and
[automatic uninstall](https://jrsoftware.org/ishelp/topic_setup_uninstallable.htm).
