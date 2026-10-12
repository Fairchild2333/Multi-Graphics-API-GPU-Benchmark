# Mangekyo — HarmonyOS app

[简体中文](README.zh-CN.md)

The HarmonyOS app: ArkTS pages on top of the `gpu_engine` shared with every
other platform. Vulkan draws into the window of an XComponent; the CPU tests
call the engine's command-line entry point.

> Status (2026-10-06, fact layers):
> - **Code written**: four pages (GPU, CPU, History, Charts), the NAPI bridge
>   and the engine's HarmonyOS host (`src/ohos_engine_host.*`).
> - **Builds**: an unsigned HAP (`arm64-v8a` and `x86_64`, about 6 MB) from the
>   command line on Windows, with the tools of DevEco Studio 26.0. The ArkTS
>   compiler reports no errors or warnings.
> - **Never run**: no HarmonyOS device is connected, no emulator image is
>   installed and no signing configuration exists. How it looks, whether
>   Vulkan works, whether the system lets the app bind threads to cores and
>   whether the charts are right has not been checked.
> - **Score identity**: the GPU tests save `stream_v1_ohos_preview` and
>   `gpu_burn_v3_fixed_steps_16_kaleidoscope_ohos_preview`; the cache and
>   memory sweep saves `cpu_membw_v1_ohos_preview`. Runs take 3 s, not the 15 s
>   of the desktop version; do not mix them with desktop scores.
> - **Not done**: frame capture, a formal mobile duration contract, signing
>   and release packages, cancelling a CPU test.
>
> Added 2026-10-07 (**builds, still never run**):
> - **Fluid** (`cinematic_liquid`): runs full screen; a 3 s run is saved as
>   `cinematic_liquid_v2_physical_scene_v8_preview_ohos_preview`.
> - **VRAM / GPU cache bandwidth** (`membw`): saved as `membw_v1_ohos_preview`.
>   It only computes and shows no picture, so it runs through the engine's
>   command-line entry, like the CPU tests.
> - **Heavier forms of the particle and GPU Burn tests**: `stream_heavy`
>   (1M particles), `gpu_burn_medium` (64 steps), `gpu_burn_heavy` (256 steps).
> - **GPU tests run full screen in landscape** and always render 1280 × 720.
> - The same code has run on the Android emulator.
>
> Added 2026-10-07 (**run on the emulators of DevEco Studio**; the unsigned HAP
> installs there as it is):
> - **Works**: HarmonyOS 5.1.1 phone and tri-fold (pages, CPU tests), 6.1.1 TV
>   (pages, GPU Burn), 7.0 two-in-one (pages, CPU tests, particle, GPU Burn, the
>   floating tab bar). GPU Burn gives the same number as on Windows and on the
>   Android emulator with the same graphics card.
> - **Limits of the emulators**: the 5.1.1 images have no Vulkan driver, so no GPU
>   test runs there; the virtual GPU of 6.1.1 and 7.0 lacks the texture format
>   the fluid needs, and its timing of the particle test and of the memory sweep
>   cannot be trusted. The car emulator (7.0 Beta1) has to be started from
>   DevEco Studio with an account signed in.
> - **Not checked**: a car, the full-screen picture on a device of the phone
>   type. See section 10 of `HANDOFF.md` in the repository root.
>
> Added 2026-10-08 (**run on a real device**: a HUAWEI MateBook Pro, HarmonyOS 7.0,
> Kirin X90, Maleoon 916):
> - **Works**: installing the signed HAP, every CPU test, and under all three
>   APIs every GPU test except the two OpenGL ES particle tests at 16,777,216
>   particles (27 of the 29 tests of `scripts\hdc-suite.ps1`).
> - **Three things the device showed, all fixed**: ten cores of two threads were
>   taken for twenty cores; the system does not hold a thread to one big-core
>   processor, so CPU results there are `scheduler_managed` as on macOS; and the
>   Vulkan timestamps of a shown particle frame are wrong when one frame follows
>   another, which a check before the run now finds (such runs are timed by wall
>   clock). Details in section 10 of `HANDOFF.md`.

## Features

| Page | Content |
|---|---|
| **GPU** | Preview surface; Particle and GPU Burn (16 fixed steps), 3 s each; the score and a chart of the GPU time per frame after a run |
| **CPU** | Compute test (single-core, all-core, both; 1 s or 15 s) and the cache and memory sweep; level tables, log-log curves, storage bars, one bar per logical processor |
| **History** | Every saved result; clear all |
| **Charts** | One bar chart per test: the best score of every device under the newest version of the test |

The interface is in English and Chinese and follows the system language.

## Architecture

```text
ArkTS (entry/src/main/ets)
  pages/        Index (four tabs), GpuPage, CpuPage, HistoryPage, ChartsPage
  components/   Card, Charts (line, column and bar charts on a Canvas)
  common/       RunController (starts tests, collects output), EngineOutput
                (parses engine output), ResultsStore (reads results.json),
                I18n, Format
        |  import bridge from 'libmangekyo.so'
        v
NAPI (entry/src/main/cpp/napi_init.cpp -> libmangekyo.so)
  - XComponent callbacks: receive the OHNativeWindow
  - startWorkload / stopWorkload: GPU tests, drawn into the preview surface
  - startCli: CPU tests, the engine's command line on a native thread
  - pollOutput: every line the engine prints (also sent to hilog, tag
    MangekyoEngine)
        v
gpu_engine (the root CMake project, OHOS branch)
  - src/ohos_engine_host.cpp -> VulkanBackend (vkCreateSurfaceOHOS)
```

The SPIR-V shaders are packaged in `resources/rawfile/shaders/` and copied to
the app's files directory at start-up, because the engine reads shaders as
files. Results go to `results/results.json` in the app's files directory.

Every result records the device model (brand, market name and model code), the
system release and the memory, and the History page shows them in one line
under each result. Model and release come from the system's deviceinfo
interface, the memory from `/proc/meminfo`. The system has no interface for the
name of the chip, so that is still read from `/proc/cpuinfo` and saved as
`Unknown CPU` when it is not there; the model code then identifies the chip.
**These fields compile and have not been checked on a device.**

## Window width, device types and the newer look

Added on 2026-10-06. **Compiled only; never seen on a device or an emulator.**

- **The layout follows the width of the window:** tabs at the bottom and one
  column below 600 vp; tabs at the side from 600 vp; two panes beside each
  other from 840 vp. History and Charts are grids.
- **GPU tests run full screen (2026-10-07):** a test turns the screen to
  landscape and shows the picture alone. It always renders 1280 x 720 and the
  system scales it. The page no longer has a small preview. Compiled only.
- **Device types:** phone, tablet, 2in1, tv, car.
- **Two shells:** `pages/Index.ets` runs everywhere. Phones, tablets and 2-in-1s
  on HarmonyOS 6.1 (API 23) or later load `pages/IndexHds.ets` instead, which
  uses the floating tab bar and system material of the UI Design Kit. The
  choice is made in `EntryAbility.ets`. The compiler warnings about interfaces
  newer than the compatible version in `IndexHds.ets` are expected.
- **System material:** the floating tab bar of the newer shell uses it, and
  `module.json5` switches it on for the app (system controls take it from
  HarmonyOS 7). Huawei's documentation says that on ordinary components the
  material only takes effect in a title bar or the bottom tab bar, so the
  content cards do not use it.

## Build

Needs DevEco Studio (checked with 26.0, which ships the SDK, hvigor, ohpm and
node) and `glslc` from the Vulkan SDK.

```powershell
.\scripts\build-ohos.ps1
```

The script compiles the shaders into the rawfile resources, then runs
`ohpm install` and `hvigorw assembleHap`. The result is
`ohos\entry\build\default\outputs\default\entry-default-unsigned.hap`.

You can also open the `ohos/` folder in DevEco Studio and build there. **Run
the script once before the first build in the IDE** (or at least let it compile
the shaders): the resources are packaged before the native library is built,
and a fresh checkout has no `.spv` files yet, so the first package would lack
the shaders and the GPU tests would fail.

**Keep the repository path short.** The native build nests its files deeply. With the
repository under a path of well over a hundred characters, CMake warns that object paths
exceed 250 characters and the build then fails (`ninja: error: loading 'build.ninja'`).
A drive letter mapped with `subst` does not help: the build tools resolve the real path.

## Installing on a device

An unsigned HAP cannot be installed on a real device (`hdc install` answers
`code:9568320 error: no signature file`). Sign the project once:

1. Connect the device, with USB debugging on.
2. Open this folder in DevEco Studio, signed in with a Huawei account.
3. **File > Project Structure > Project > Signing Configs**, tick
   **Automatically generate signature**, Apply.

`scripts\build-ohos.ps1` then also writes `entry-default-signed.hap`, and
`hdc install -r` takes it. An emulator needs a system image from **Device
Manager** and takes the unsigned HAP.

- **DevEco Studio does not open a project whose path has a space in it** ("Invalid
  path"). Give it a path without one through a directory junction to the
  repository, for example
  `New-Item -ItemType Junction -Path C:\Users\<you>\mangekyo -Target "<repository>"`,
  and open `C:\Users\<you>\mangekyo\ohos`. The command-line build is not
  affected and runs from the real path.
- **Do not commit the signing configuration.** Signing writes the paths of your
  certificate files and an encrypted password into `build-profile.json5`; they
  belong to one machine and one account.

## Testing over hdc

`scripts\hdc-suite.ps1` runs the standard series of 29 tests on a connected
device and brings the results back to `out\hdc\<model>-<time>\` (`-Install`
installs the HAP first, `-Quick` only checks that every test starts, `-Only`
picks tests by name). It is `scripts\adb-suite.ps1` for hdc: same tests, same
durations.

A debug build starts a single test without touching the device:

```bash
hdc shell aa start -a EntryAbility -b com.mangekyo.benchmark --ps run cpu_membw
hdc hilog | grep Mangekyo
```

- **`--ps run <test>`**: `cpu_membw`, `cpu_all`, `cpu_per_core`, `cpu_multi`,
  `stream` (particle, 1M by default), `gpu_burn` (16 steps by default),
  `cinematic_liquid` (fluid), `membw` (VRAM / GPU cache bandwidth).
- **Headless particle test**: `--ps run "stream_headless?particles=1048576"` computes
  only and shows no picture (builds; not run yet).
- **OpenGL ES 3.1** (added 2026-10-07): choose it under "API" on the GPU page, or add
  `api=gles` to a request, for example `--ps run "'stream?particles=1048576&api=gles'"`
  (quote the `&`). It has the particle test (also headless) and GPU Burn; the fluid and
  the memory sweep need Vulkan. The shaders are the desktop OpenGL ones with another
  version line. It runs on the 7.0 and the 5.1.1 emulators; the 5.1.1 images have no
  Vulkan driver, so these two tests are what a GPU can run there. The emulators offer
  no GPU timestamps, so their numbers are not results; no real device was tried.
- **OpenGL ES 2.0** (added 2026-10-07): GPU Burn only, with a shader of its own, saved in
  its own group (`gpu_burn_es2_legacy_v1_...`) and timed by wall clock. Add `api=gles2`
  to a request, for example `--ps run "'gpu_burn?steps=64&api=gles2'"`. It runs on the
  5.1.1 emulator; no real device was tried.
- **A GPU test may carry its size**: `--ps run "stream?particles=4194304"`,
  `--ps run "gpu_burn?steps=128"` (particles are rounded up to 256, steps are
  16 to 2048). The older names `stream_heavy`, `gpu_burn_medium` and
  `gpu_burn_heavy` still work. The GPU page offers the same settings, with the
  presets and defaults of the Windows app.
- **`--pi seconds <N>`** (optional): seconds per CPU compute test, or the
  length of a GPU run.
- **Log**: the tag `MangekyoEngine` carries every line the engine prints; the
  tag `Mangekyo` carries `AUTORUN accepted / started / refused / failed` and,
  at the end, `RUN finished kind=cpu exit=0`.
- The launch parameter works in debug builds only. A GPU test needs the screen
  on and unlocked.

## What a real device does differently

Seen on the MateBook Pro (2026-10-08); a phone may differ.

- **Binding threads to cores**: the system's own kernel pins a thread to a
  small-core or middle-core processor, but answers a mask that names one
  big-core processor with all of `4-19`, and its `sched_getcpu()` counts cores,
  not processors. The engine finds this out before a CPU test and then saves
  the results as `scheduler_managed`. An emulator runs a Linux kernel, pins
  everything and stays strict.
- **Cores and threads**: `topology/core_id` is numbered per processor there; the
  engine reads which processors share a core from `thread_siblings_list`.
- **Cache sizes**: the app may read `/sys/devices/system/cpu`, but the cache
  sizes could not be had from it, so the cache levels are read off the curve.
- **The debugging shell reads almost nothing** (not `/proc/cpuinfo`, no battery,
  no thermal state, no CPU clock), so the series cannot record the power source,
  the performance mode or a temperature. Note them by hand.
- **The app runs in a window** on a PC; a GPU test still draws 1280 x 720.
- **Dark mode**: an app with no `resources/dark` directory that does not call
  `setColorMode` keeps its system colours light in dark mode, while the floating
  tab bar's material goes dark. `EntryAbility` now calls
  `setColorMode(COLOR_MODE_NOT_SET)` so the app follows the system (seen in dark
  mode on a Mate 60 Pro; light mode was not looked at again).

## Known risks (still to be checked)

- **Vulkan**: the engine asks for Vulkan 1.0 entry points only; whether devices
  and the emulator all offer `VK_OHOS_surface` has not been checked.
- **Storage test**: writes a temporary file of up to 1 GiB in the app's files
  directory.
