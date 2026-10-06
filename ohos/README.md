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
> - **Not done**: the fluid test, frame capture, a formal mobile duration
>   contract, signing and release packages, cancelling a CPU test.

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
  other from 840 vp. History and Charts are grids. The preview is always 16:9.
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

An unsigned HAP cannot be installed on a real device. Sign in to DevEco Studio
with a Huawei account, then **File > Project Structure > Signing Configs >
Automatically generate signature**, and run. An emulator needs a system image
from **Device Manager** first. Both steps are yours to do; neither has been
done on this machine.

## Testing over hdc

A debug build starts a test without touching the device (**compiled only,
never tried on a device**):

```bash
hdc shell aa start -a EntryAbility -b com.mangekyo.benchmark --ps run cpu_membw
hdc hilog | grep Mangekyo
```

- **`--ps run <test>`**: `cpu_membw`, `cpu_all`, `cpu_per_core`, `cpu_multi`,
  `stream`, `gpu_burn`.
- **`--pi seconds <N>`** (optional): seconds per CPU compute test, or the
  length of a GPU run.
- **Log**: the tag `MangekyoEngine` carries every line the engine prints; the
  tag `Mangekyo` carries `AUTORUN accepted / started / refused / failed` and,
  at the end, `RUN finished kind=cpu exit=0`.
- The launch parameter works in debug builds only. A GPU test needs the screen
  on and unlocked.

## Known risks (all to be checked on a device)

- **Binding threads to cores**: the engine uses `sched_setaffinity`. If the app
  sandbox forbids it, the compute test is rejected as invalid and the sweep is
  marked as not bound.
- **Cache sizes**: read from `/sys/devices/system/cpu`. If the sandbox hides
  them, the cache levels are read off the curve instead.
- **Vulkan**: the engine asks for Vulkan 1.0 entry points only; whether devices
  and the emulator all offer `VK_OHOS_surface` has not been checked.
- **Storage test**: writes a temporary file of up to 1 GiB in the app's files
  directory.
