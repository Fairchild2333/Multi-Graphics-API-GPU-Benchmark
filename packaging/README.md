# Windows x64 release staging

This directory defines the Windows x64 release contract. The release script
builds the CLI and self-contained WinUI GUI, installs the app-local runtime and
all shaders, adds a pinned RenderDoc portable tree, verifies PE architecture and
Vulkan delay imports, and inventories every staged file with SHA-256:

```text
GpuComputeBenchmark-<version>-windows-x64/
  app/bin/             CLI, optional GUI, DLLs, and shader compatibility copies
  assets/shaders/      canonical read-only shader mirror
  scripts/             legacy Python report sources (not a bundled Python runtime)
  tools/RenderDoc/     optional complete portable RenderDoc distribution
  tools/report_worker/ optional frozen report worker
  release-manifest.json
  files.sha256
  PACKAGE_LIMITATIONS.md
```

Writable results, captures, reports, and logs are deliberately excluded. The
application stores them below the platform user-data directory through
`PathService`.

## One-command GitHub Release build

Download the official RenderDoc **64-bit portable ZIP** on a connected machine.
The offline path is the recommended reproducible release input:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\build-windows-github-release.ps1 `
  -RenderDocArchive C:\release-inputs\RenderDoc_<version>_64.zip `
  -RenderDocSha256 <published-or-independently-verified-sha256> `
  -ProjectLicenseFile C:\release-inputs\LICENSE.txt
```

An online build is also supported, but it deliberately requires both an HTTPS
portable-ZIP URL and its expected SHA-256; there is no mutable "latest" download:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\build-windows-github-release.ps1 `
  -RenderDocDownloadUrl https://renderdoc.org/<pinned-path>/RenderDoc_<version>_64.zip `
  -RenderDocSha256 <sha256> `
  -ProjectLicenseFile C:\release-inputs\LICENSE.txt
```

The default flow rebuilds both CMake and WinUI outputs instead of harvesting a
developer output directory. It produces the portable ZIP, the Inno Setup EXE,
`SHA256SUMS.txt`, and `release-assets.json` under
`out/release/windows-x64`. The target computer does not need Visual Studio,
vcpkg, Python, a shader compiler, the Vulkan SDK, or a separate Windows App SDK
installation. A compatible graphics driver is still required.
Before release assets are emitted, every file streamed back from the portable
ZIP is checked against the verified stage inventory; a missing, extra or changed
entry fails the build.

The GitHub Release wrapper refuses a dirty Git worktree so the recorded commit
actually identifies its binaries. `-AllowDirtySource` exists only for local
engineering candidates and is recorded as `sourceTreeDirty: true`.

Use `-SignToolCommand '<Inno sign command containing $f>' -RequireSigned` for a
public signed build. Without it the artifacts are intentionally reported as
unsigned and remain susceptible to SmartScreen warnings. The repository still
needs a project distribution license, legal review, and clean-machine
install/capture validation before a public GitHub Release is qualified.

## Staging and portable ZIP only

The lower-level staging command remains useful in CI and engineering builds:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\stage-windows-release.ps1 `
  -BuildGui `
  -RenderDocDir out\dependencies\RenderDoc
```

`prepare-renderdoc-portable.ps1` converts an official ZIP into the validated
directory and writes `BUNDLE_SOURCE.json` with its source URL/name, version and
archive digest. `-SkipGui` and `-SkipRenderDoc` are only explicit engineering
escape hatches; they are not the normal GUI-first release.

Direct CMake commands can inspect the raw install rules:

```powershell
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release
cmake --install out/build/windows-x64-release --config Release `
  --prefix out/stage/windows-x64
cpack --preset windows-x64-zip
```

They do not generate the post-install `files.sha256` inventory or final GitHub
asset manifest; use the PowerShell release script for distributable artifacts.

Set `GPU_BENCH_GUI_PAYLOAD_DIR`, `GPU_BENCH_RENDERDOC_DIR`, and
`GPU_BENCH_REPORT_WORKER_DIR` at configure time to add prebuilt payloads. A
RenderDoc directory is accepted only when it contains `renderdoccmd.exe`,
`renderdoc.dll`, and a license file. A report-worker directory must contain
`report_worker.exe`.

## MSI / WiX alternative

The same install rules can feed CPack's `WIX` generator, but MSI generation is
intentionally blocked until `GPU_BENCH_PACKAGE_LICENSE_FILE` points to an
approved project distribution license. A public installer also needs audited
`THIRD_PARTY_NOTICES`, signing, upgrade/uninstall tests, and a VC Redist
redistribution review. ZIP remains the honest engineering artifact until those
release gates are satisfied.

To request WIX after those gates are met:

```powershell
cmake -S . -B out/build/windows-x64-release `
  -DGPU_BENCH_CPACK_GENERATORS='ZIP;WIX' `
  -DGPU_BENCH_PACKAGE_LICENSE_FILE=C:\approved\LICENSE.txt
cpack --config out/build/windows-x64-release/CPackConfig.cmake -C Release -G WIX
```

Always run `scripts/verify-windows-stage.ps1`. Its normal mode validates the
core artifact and reports unresolved portability gates. `-RequirePortable`
turns those gates into failures for a future release CI job.

## Inno Setup installer

The same verified stage can be wrapped in a maintainable per-user x64 Setup:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts/build-inno-installer.ps1 `
  -StageDir out/stage/windows-x64 `
  -Version 0.1.0
```

The source is `installer/GpuComputeBenchmark.iss`. It uses a stable AppId,
provides Start Menu and optional desktop shortcuts, and supports normal Inno
upgrade/uninstall behavior. A complete staged `tools/RenderDoc` payload becomes
an optional installer component; the script never copies from a developer's
installed RenderDoc directory. Use `-StaticOnly` when ISCC is unavailable to
check the stage and installer invariants without producing a Setup executable.

The installer includes the verified stage inventory and keeps RenderDoc as a
default-selected full-install component. Detailed input, signing and release
requirements are in `installer/README.md`.
