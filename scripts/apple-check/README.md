# Checking the iOS and macOS code without a Mac

The iOS and macOS apps can only be built with Xcode. This folder holds what
can be checked on any machine with Docker, so that Swift written on Windows or
Linux is at least free of language and concurrency errors before it reaches a
Mac.

```powershell
.\scripts\check-apple.ps1
```

The first run downloads the `swift:6.0-jammy` image (about 4.3 GB on disk).
Remove it afterwards with `docker rmi swift:6.0-jammy` if the space matters;
the next run downloads it again.

## What is checked

| Check | How | What it proves |
|---|---|---|
| Parser of engine output, chart geometry, translation of engine phrases | Compiled with Swift 6.0 and run on saved engine output (`tests.swift`, `fixtures/`) | These files compile and give the expected values |
| `BenchEngine`, `CpuView`, `ChartsView`, `RunView`, the models and the chart views of both apps | Swift 6 type-check against the stand-in modules in `stubs/`, the real Foundation of Linux and the real C bridge header | No Swift language or concurrency error in these files, as far as the stand-ins match the SDK |
| The files both apps share | Text comparison | The iOS and macOS copies are the same |
| `cpu_membw.cpp`, `cpu_benchmark.cpp`, `cpumem_cli.cpp`, `system_info.cpp`, `benchmark_results.cpp`, `path_service.cpp` | Compiled to arm64 and x64 macOS objects with the clang and macOS headers of zig (`zig_check.sh`) | The macOS code paths of the CPU tests, of the machine details and of the results file compile |

## What is not checked

- **Nothing is built with the Apple SDK.** The stand-ins in `stubs/` are
  written by hand from memory of the SwiftUI, AppKit and UIKit interfaces. A
  call that the stand-in accepts and the real SDK rejects is not found here.
- **Not checked at all:** `ContentView`, `HistoryView`, `SettingsView`,
  `AboutView`, `GlassCard`, `MetalBenchView`, the app entry points, the
  Objective-C++ bridge (`GpuBenchBridge.mm`) and the Xcode project files.
- **iOS-only engine code** (`TARGET_OS_IPHONE`): zig ships no iOS headers.
- **Availability:** the stand-ins do not know which API needs which OS
  version. `View.fontWeight` is left out on purpose, because the macOS app
  supports macOS 12 and that modifier needs macOS 13.
- **Nothing runs.** Layout, text overlap, dark mode and behaviour on a device
  are only seen on a Mac or a phone.

## Keeping it useful

- When a checked file uses a type or modifier the stand-in lacks, add it to
  `stubs/SwiftUI.swift` with the signature of the real SDK. Do not loosen a
  declaration to make an error go away.
- To check another file, add it to the list in `check.sh` and add what it
  needs to the stand-ins.
- `fixtures/` holds real engine output (a Ryzen 7 9800X3D on Windows) with
  local paths removed. After a change of the engine's output format, capture
  new output and update the expected values in `tests.swift`.

## History

Written on 2026-10-06. Its first run found that `BenchEngine.swift` did not
compile (an `await` on the right side of `&&`), that both apps ran the
blocking engine calls on the main actor, and a broken escape sequence in a
bridge header. All three are fixed.
