# Building Mangekyo from Source

Mangekyo's internal CMake target and command remain `gpu_benchmark`; the display
brand does not require downstream build scripts to rename the executable.

**Windows day-to-day default:** `scripts/build-windows.ps1` builds the CLI and
WinUI GUI together and stages shaders beside the GUI worker. Release ZIP/native
multilingual Setup packaging remains a separate flow (`scripts/build-windows-github-release.ps1` /
`packaging/README.md`).

## Prerequisites

| Dependency | Install |
|---|---|
| **CMake 3.25+** | https://cmake.org/download/ or system package manager |
| **C++17 compiler** | MSVC (Visual Studio 2019+), GCC 8+, Clang, or Apple Clang |
| **GLFW** | `vcpkg install glfw3` / `brew install glfw` / `sudo apt install libglfw3-dev` |
| **Vulkan SDK** (optional) | [LunarG](https://vulkan.lunarg.com/sdk/home) or `sudo apt install libvulkan-dev` |
| **Windows SDK** (for DX) | Included with Visual Studio |
| **Xcode CLT** (for Metal) | `xcode-select --install` (macOS) |

---

## Linux

> **Tested on Ubuntu.** Fedora and Arch commands are provided for
> convenience but have not been verified by the author.

**Ubuntu / Debian** (`apt`):

```bash
sudo apt install build-essential cmake libglfw3-dev libgl-dev
sudo apt install libvulkan-dev vulkan-tools glslc   # optional, for Vulkan backend
```

**Fedora / RHEL** (`dnf`):

```bash
sudo dnf install gcc-c++ cmake glfw-devel mesa-libGL-devel
sudo dnf install vulkan-loader-devel vulkan-tools glslc   # optional, for Vulkan backend
```

**Arch / Manjaro** (`pacman`):

```bash
sudo pacman -S base-devel cmake glfw-x11 mesa
sudo pacman -S vulkan-icd-loader vulkan-tools shaderc   # optional, for Vulkan backend
```

At least one of the OpenGL (`libgl-dev` / `mesa-libGL-devel` / `mesa`) or
Vulkan development packages must be installed — otherwise no backend will be
available. DirectX and Metal backends are automatically disabled on Linux.

| Backend | Available on Linux | Driver Requirement |
|---------|-------------------|--------------------|
| Vulkan  | Yes (with `libvulkan-dev`) | Mesa or NVIDIA proprietary driver |
| OpenGL 4.3 | Yes (with `libgl-dev`) | Mesa or NVIDIA proprietary driver |
| DirectX 11/12 | No | Windows only |
| Metal | No | macOS only |

**GPU selection for OpenGL:** On Linux, the application uses the `DRI_PRIME`
environment variable to route OpenGL to the user's chosen GPU. This is set
automatically when a GPU is selected via the interactive menu or `--gpu`.
You can also set it manually:

```bash
DRI_PRIME=1 ./build/gpu_benchmark --backend opengl   # use secondary GPU
```

For NVIDIA proprietary drivers, use:

```bash
__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia ./build/gpu_benchmark --backend opengl
```

### NVIDIA Jetson Xavier NX (JetPack 5.1.x, aarch64)

> **Verification status: built on real hardware** (Xavier NX Developer Kit,
> JetPack 5.1.7 / L4T R35.6.5, 2026-10-17): configures, builds with no
> warnings and passes `ctest`; Vulkan and OpenGL are both detected. The
> Vulkan 1.2.131 headers of Ubuntu 20.04 lack `VK_KHR_portability_enumeration`;
> the engine compiles that code only where the headers have it.

Xavier NX supports JetPack up to 5.1.x (Ubuntu 20.04); JetPack 6 does not
support Xavier. JetPack 4.x ships GCC 7, whose C++17 `<filesystem>` support is
incomplete, so it is not a supported target. Only the CLI runs on Jetson (the
WinUI GUI is Windows-only). Available backends are Vulkan and OpenGL 4.3;
Cinematic Liquid is Vulkan-only.

**1. Dependencies.** Ubuntu 20.04 ships CMake 3.16, but the project requires
3.22 or newer, so install CMake from pip. `glslc` is not packaged for Ubuntu
20.04, so do not include it in the `apt` line.

```bash
sudo apt install build-essential libglfw3-dev libgl-dev libvulkan-dev vulkan-tools python3-pip
pip3 install --user "cmake<4"
```

**2. Build.** Configuration prints `glslc not found – Vulkan shader compilation
disabled.`; this is expected because SPIR-V comes from step 3.

```bash
~/.local/bin/cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGPU_BENCH_ENABLE_PACKAGING=OFF
~/.local/bin/cmake --build build -j6
```

**3. SPIR-V shaders.** SPIR-V is architecture-independent. Copy the `*.spv`
files from a Windows build (`out/build/windows-x64-release/Release/*.spv`, 51
files at 0.2.6) into `build/` beside `gpu_benchmark`, and copy them again
whenever the GLSL sources under `shaders/` change. The OpenGL shader sources
are copied into `build/` automatically.

```bash
scp out/build/windows-x64-release/Release/*.spv <user>@<jetson-ip>:<repo>/build/
```

**4. Before benchmarking.**

- Log in to the Jetson desktop. Every GPU run, including `--headless`, calls
  `glfwInit()`, and GLFW 3.3 requires X11. Over SSH, prefix GPU commands with
  `DISPLAY=:0`.
- Use a power mode that keeps all six CPU cores online, then lock the clocks.
  Low-power `nvpmodel` modes take CPU cores offline, and the CPU benchmark
  measures only the cores the OS allows. Check the current mode with
  `sudo nvpmodel -q`, pick a six-core mode from `/etc/nvpmodel.conf` with
  `sudo nvpmodel -m <id>`, then run `sudo jetson_clocks`.
- `vulkaninfo --summary` should list the Tegra GPU.
- RenderDoc has no official Linux ARM64 build, so pass `--no-renderdoc`. A
  missing `librenderdoc.so` is not fatal, but the flag keeps runs explicit.

**5. Run the four product workloads.** These commands mirror a WinUI Custom
run (15 s, 1M particles, 64 GPU Burn steps) without capture. For Particle and
GPU Burn, replace `--backend vulkan` with `--backend opengl` to measure the
OpenGL backend. The CPU benchmark needs no display.

```bash
DISPLAY=:0 ./build/gpu_benchmark --time 15 --backend vulkan --gpu 0 --particles 1048576 --no-renderdoc
DISPLAY=:0 ./build/gpu_benchmark --time 15 --backend vulkan --gpu 0 --workload gpu_burn --iter 64 --no-renderdoc
DISPLAY=:0 ./build/gpu_benchmark --time 15 --backend vulkan --gpu 0 --workload cinematic_liquid --no-renderdoc
./build/gpu_benchmark --cpu-benchmark all --cpu-time 15 --cpu-warmup 0.2
```

Results are appended to
`~/.local/share/GpuComputeBenchmark/results/results.json` (or
`$XDG_DATA_HOME/GpuComputeBenchmark/results/results.json`).

**Plausibility checks.**

- The Xavier NX CPU and GPU share 128-bit LPDDR4x. Its ceiling depends on the
  power mode: the 20 W modes allow EMC 1866 MHz (59.7 GB/s), the 10 W and
  15 W modes 1600 MHz (51.2 GB/s); `/etc/nvpmodel.conf` lists `EMC MAX_FREQ`
  per mode. Its caches cannot hold the 32 MiB working set of 1M particles, so
  a Particle result above the ceiling of the mode in use indicates a timing or
  accounting error. These results are not directly comparable with discrete
  GPUs whose L2 cache holds the whole working set (for example, the RTX
  5090's 96 MB L2).
- Measured on 2026-10-17 (JetPack 5.1.7, MODE_20W_6CORE, no `jetson_clocks`):
  particles about 24 GB/s Vulkan and 26 GB/s OpenGL headless, GPU Burn
  1.73 / 2.18 / 2.33 Gpix-step/s at 16 / 64 / 256 steps, Cinematic Liquid
  6.36 MParticle-step/s (about 2 FPS), GPU memory read 56.3 GB/s (94 % of
  59.7), CPU 54.7 per core and 335.2 all cores MWork/s. With `jetson_clocks`
  the GPU and memory figures were the same; the CPU read 7 % higher per core
  (58.3) and 1.7 % higher on all cores, because the default governor does not
  always hold a single-thread load at 1420.8 MHz.

**CPU software rendering (Mesa).** Linux uses Mesa's CPU rasterisers:
lavapipe for Vulkan and llvmpipe for OpenGL. `--warp` applies only to
DirectX. Make only lavapipe visible to Vulkan, so `--gpu 0` selects it, and
force the Mesa GLX vendor for OpenGL, because Jetson otherwise uses NVIDIA's
GL driver:

```bash
sudo apt install mesa-vulkan-drivers libgl1-mesa-dri libglx-mesa0
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.aarch64.json DISPLAY=:0 ./build/gpu_benchmark --time 15 --backend vulkan --gpu 0 --workload gpu_burn --iter 64 --no-renderdoc
__GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1 DISPLAY=:0 ./build/gpu_benchmark --time 15 --backend opengl --gpu 0 --particles 1048576 --no-renderdoc
```

- Both renderers report the device name `llvmpipe (...)`, and results store
  that device with `isSoftware=true`, keeping them separate from hardware GPU
  results.
- **Do not use OpenGL/llvmpipe GPU Burn scores.** In an Ubuntu 20.04 +
  Mesa 21.2.6 container, the GL timer reported 5.9 ms of render time per frame
  while frames actually took about 330 ms (3 FPS), inflating the score about
  40× (19.89 versus 0.48 Gpix-step/s on Vulkan/lavapipe). Vulkan/lavapipe
  timers matched wall-clock frame time.
- Cinematic Liquid on lavapipe took about 2.2 s per frame on a 16-thread
  Ryzen 7 9800X3D. Jetson's six Carmel cores are expected to be much slower, so
  use `--time 120` or longer; otherwise the run may measure no frames after the
  2 s warm-up.

### Raspberry Pi 4 (Raspberry Pi OS 64-bit, aarch64)

> **Verification status: run on real hardware** (Pi 4 Model B Rev 1.5, 4 GB,
> Raspberry Pi OS based on Debian 13 trixie, Mesa 26.2.2, 2026-10-17; windowed
> runs with a monitor 2026-10-20). The first ARM Linux board the CLI has run on.

Use the 64-bit image: a 32-bit build costs the CPU compute test a factor of
about 2.6 on the same core type (see the TCL TV). The GPU (V3D 4.2) has
Vulkan 1.3 through Mesa's V3DV driver with 64-bit timestamps, so the Vulkan
workloads run. OpenGL is not available (the backend needs 4.3; the Pi offers
3.1 / ES 3.1), and Cinematic Liquid is refused (no linear-filterable
R32_SFLOAT 3D images, as on Adreno).

**1. Dependencies.** Everything is packaged, `glslc` included, so the shaders
are built on the Pi. Every GPU run calls `glfwInit()`, headless ones too, so a
display server must be running: the desktop session, or `xvfb-run` on an image
without one.

```bash
sudo apt-get install -y cmake libglfw3-dev libvulkan-dev glslc xvfb
```

**2. Build** (about 15 minutes with `-j4`):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGPU_BENCH_ENABLE_PACKAGING=OFF
cmake --build build -j4
ctest --test-dir build
```

**3. Before benchmarking.**

- Cooling: the Pi 4 throttles at 80 C and on undervoltage. Check
  `vcgencmd get_throttled` (`0x0` means neither has happened since boot) and
  `vcgencmd measure_temp`. A Pimoroni Fan SHIM is switched by GPIO18; the
  kernel can drive it with `dtoverlay=gpio-fan,gpiopin=18,temp=50000` in
  `/boot/firmware/config.txt`.
- `--gpu 0` is V3D; llvmpipe is listed as GPU 1. `--complete-suite` would run
  every GPU job a second time on llvmpipe, so run the V3D jobs on their own
  with the suite's arguments.
- Without a monitor, headless runs measure the GPU normally, but windowed
  runs have no valid way to present. The desktop (labwc, Wayland) keeps
  running with no output, and GLFW 3.4 connects to it (`wayland-0`) even over
  SSH; frames then go to a desktop that has no screen. Forced onto Xvfb
  (`WAYLAND_DISPLAY` pointing nowhere, under `xvfb-run`), V3D cannot present at
  all and the run stops with `Requested GPU index unsuitable`. Connect a
  monitor, or an HDMI dummy plug, for windowed runs.

**Known limits on V3D.**

- In a windowed particle frame V3D's compute timestamps do not bracket the
  work, with or without a monitor: the same step read 0.01 - 93 ms on the
  screenless desktop (2026-10-17) and 0.014 - 29.6 ms with a monitor at
  1920x1080 120 Hz (2026-10-20), so 65K/1M/4M read up to 70 GB/s against
  about 2.0 headless. The startup check of the embedded hosts
  (`CheckTimestampsAgainstWallClock`) passed there (324 ms both ways). The
  Vulkan backend therefore times a shown particle test on Broadcom V3D by
  the wall clock (compute part submitted alone and waited for): 65K/1M/4M
  1.32 / 1.52 / 1.68 GB/s, the submit-and-wait included.
- 16M windowed cannot run: drawing 16,777,216 points in one frame hangs V3D
  and the kernel resets the GPU about once a second (`v3d_reset ... Resetting
  GPU for hang` in dmesg); the run then stops with an out-of-date swapchain.
  Headless 16M runs.
- GPU Burn gives the same figures with a monitor as on the screenless desktop
  (0.027 / 0.036 / 0.039 Gpix-step/s at 16 / 64 / 256 steps).
- V3D allows 65,535 work groups per dispatch, and 16,777,216 particles need
  65,536. Before 2026-10-17 that dispatch did no work (15,086 GB/s); the
  Vulkan backend now splits such a step into dispatches over separate parts
  of the buffer, and 16M reads 1.98 GB/s like the other sizes.

### x86-64 test machines (Ubuntu 26.04 LTS, installed unattended)

> **Verification status: run on real hardware** (Lenovo ThinkCentre M700 Tiny,
> i5-6400T with HD 530, 2026-10-21).

Spare PCs are wiped and installed the same way, then driven from the Windows
PC over SSH. The machine needs a monitor (or an HDMI dummy plug) and a wired
network during the install.

**1. Install.** Write the Ubuntu 26.04 live-server ISO to a USB stick (Rufus,
ISO mode). Run `scripts\linux\make-autoinstall.ps1`, put the hash of the
test user's password (`openssl passwd -6`) in place of `__PASSWORD_HASH__`,
and copy `out\linux-autoinstall\autoinstall.yaml` to the root of the stick,
next to `casper`. Boot from the stick and pick the first GRUB entry. The
install asks nothing: it **wipes the largest SSD** (the largest disk without
one), installs the build packages, gives the user passwordless sudo and this
PC's SSH key, masks sleep, names the machine `mk-<last 6 hex digits of the
wired MAC>` and reboots. Packages come from the Australian mirror
(`au.archive.ubuntu.com`). Without a network the install still finishes,
from the ISO; the build packages are then installed later over SSH. A
machine whose wired port has no driver in Ubuntu's kernel installs over
Wi-Fi: run `make-autoinstall.ps1 -Wifi` (add `-WifiInterface <name>` if the
installer's `ip -br link` names it other than `wlp5s0`) and fill in the
network's name and password in the written file.

**2. Desktop.** The windowed tests need a running session, so GNOME is added
after the first boot and logs the user in by itself. The script also turns
off screen blanking, the lock screen and idle suspend (system dconf
defaults): a series started on a locked screen would draw its windowed tests
behind the lock.

```bash
ssh fairchild@mk-xxxxxx.local 'sh -s' < scripts/linux/setup-desktop.sh
```

**3. Run the series.** From the repository on the Windows PC:

```powershell
.\scripts\linux-suite.ps1 fairchild@mk-xxxxxx.local
```

It sends this checkout (uncommitted changes included), builds it on the
machine, runs `scripts/linux/series.sh` there detached from SSH (the
desktop-suite jobs of every hardware GPU; the OpenGL ES 2.0 GPU Burn at 16 /
64 / 256 steps on the GPU behind EGL, unless that is a software renderer;
then the two CPU jobs) and fetches
the run to `out\linux\<host>-series-<time>\`. `-Only <regex>` runs some jobs,
`-SkipBuild` keeps the build already there, `-BuildOnly` stops after the
build, `-Env "NAME=value ..."` sets environment variables for the series
(recorded in its `state.txt`), and `-Attach <run>` follows a run that is
already going.

**Known limits on Intel GPUs (Mesa, i915).**

- i915 resets a GPU context that cannot be pre-empted within 640 ms
  (`/sys/class/drm/card*/engine/rcs0/preempt_timeout_ms`; Windows allows 2 s).
  A draw is not pre-empted part way, and GPU Burn at 256 steps draws about
  435 ms twice a frame on an HD 530 at 900 MHz, well over 1 s while the
  clock is still at 350 MHz. Vulkan then reports a lost device; OpenGL
  carries on with empty frames, which the engine now refuses to save (no
  timing data). For that job the user raised the limit to 2000 ms and set it
  back afterwards (`echo 2000 | sudo tee .../rcs0/preempt_timeout_ms`); it
  also returns to 640 at a reboot.
- Mesa's Intel OpenGL drivers run compute dispatches in a batch of their own
  and put GL timestamp queries in the render batch, so the queries never
  cover compute. The OpenGL backend times compute workloads on "Mesa Intel"
  by the wall clock (compute submitted alone and waited for). The wait costs
  about 0.5 - 0.9 ms a step, so OpenGL particle results at 65K and 1M read
  lower than Vulkan's (4.6 against 26.9 GB/s headless at 65K); from 4M on
  they agree within a few per cent. GPU Burn draws only and keeps its
  timestamps.

**Zhaoxin KX-6000 / KX-6900 (C-960 GPU).** Mainline Linux has no driver for
the C-960 (PCI `1d17:3a04`); only the firmware framebuffer (simpledrm) runs.
Zhaoxin's own driver (the UOS build, `KX-6000_6900-UOS_x64-21.00.93.zip` from
zhaoxin.com) offers OpenGL 3.2 and OpenGL ES 2.0, no Vulkan, and supports
Linux 4.19 / 5.10 / 6.6. Its kernel module is ported to 7.0 by
`scripts/linux/zhaoxin/zx-21.00.93-linux-7.0.patch`; copy the `.deb` to the
machine, send the source with `-BuildOnly`, then run
`sh ~/mangekyo/scripts/linux/install-zhaoxin.sh ~/<the .deb>` and reboot.

- The engine's GPU jobs need Vulkan or OpenGL 4.3, so the only GPU test is
  the stand-alone OpenGL ES 2.0 GPU Burn (`mangekyo_es2burn`, built by
  `scripts/linux/build.sh`). Its figures stay in the logs.
- The driver hangs the GPU when it draws into a pbuffer, and its EGL fences
  and `glFinish` can return before a frame is drawn. On renderer "ZX ..."
  the tool draws into a texture of the same size and reads one pixel back
  after every frame.
- GTK 4 needs OpenGL 3.3 / ES 3.0 and draws blank windows on this driver;
  the install script sets `GSK_RENDERER=cairo` for the user.
- On 7.0 the driver has no framebuffer console of its own (the fbdev helpers
  it used are gone); the desktop drives the screen through KMS.

### Phytium D2000 (arm64) with a Radeon HD 6450

> **Verification status: run on real hardware** (Phytium D2000/8 board with
> an AMD Caicos card, Ubuntu 26.04.1 and Kylin V10 SP1, 2026-10-22).

The same unattended install works on arm64, over Wi-Fi: the D2000's two
on-chip Gigabit ports (DesignWare GMAC, ACPI `PHYT0004`) have no driver in
Ubuntu's kernel, which has the stmmac core but not Phytium's glue. Once the
machine is reachable over Wi-Fi, send the source with `-BuildOnly` and run

```bash
sh ~/mangekyo/scripts/linux/install-phytium-gmac.sh
```

It builds Armbian's `dwmac-phytium` glue (`scripts/linux/phytium/`) with
DKMS for every installed kernel, taking the stmmac driver's internal headers
from the running kernel's `linux-source` package (about 190 MB, once), and
loads it. The installer's netplan gives any wired port named `e*` an address
by DHCP, so the ports (`enaphyt4i0`, `enaphyt4i1`) come up at once and at
every boot after. DKMS builds it only for 7.0 kernels, whose stmmac headers
it was taken from.

If the install ran without a network, the installed system lacks
`wpasupplicant`. Install it from the stick, mounted at `/mnt`: `apt-cdrom`
rejects the stick's signature (the key is not on the installed system), so
add it as a trusted local source instead:

```bash
echo "deb [trusted=yes] file:/mnt resolute main restricted" | sudo tee /etc/apt/sources.list.d/usb.list
sudo apt update; sudo apt install -y wpasupplicant && sudo netplan apply
```

Remove `/etc/apt/sources.list.d/usb.list` once the machine is online.

- Mesa 26 gives the Caicos OpenGL 4.6; there is no Vulkan driver for it (only
  llvmpipe, which the series skips). Kylin V10's Mesa 20 offers 3.3 (no
  fp64); there the series ran with
  `-Env "MESA_GL_VERSION_OVERRIDE=4.3 MESA_GLSL_VERSION_OVERRIDE=430"`.
- The firmware sometimes failed to bring up the card's PCIe link at power-on
  (no picture, not even the firmware's), after cold and warm starts alike,
  while the same monitor was also connected to another running PC. With that
  PC's cable unplugged the card came up at every one of several power
  cycles: probably the card is fed through the display cable and not reset
  cleanly (not verified). Do not share the monitor with another PC at power-on,
  or use an HDMI dummy plug.
- The radeon kernel driver declares the GPU hung when a ring makes no
  progress for 10 s. OpenGL GPU Burn at 256 steps takes 2.4 s a frame, and
  the frames still queued at its end trip that check; the driver resets the
  GPU (the screen goes black for a moment) after the measurement has ended.
  Run that job last (`-Only 'opengl-gpu-burn-256$'`).
- On the radeon kernel driver (Mesa renderer "... (DRM 2.x ...)") a polled EGL
  fence only reads done at the driver's half-second check, so the OpenGL ES
  2.0 GPU Burn waits for each frame by reading one pixel back, as on
  Zhaoxin's driver.

### Loongson 3A4000 (MIPS64) with AOSC OS

> **Verification status: run on real hardware** (Loongson
> LS3A4000-7A1000-NUC-SE, AOSC OS 14.0.0 loongson3, Linux 7.1.13, Mesa
> 26.1.5, 2026-10-25): the CPU jobs of the series, and the OpenGL ES 2.0 GPU
> Burn up to 8 steps only (see below). The board came with UnionTech OS 20,
> removed since (its CPU run is at the end of this section).

AOSC OS's loongson3 port is a current distribution for this MIPS64 board
(kernel 7.1, GCC 16, Mesa 26). Its installer showed nothing on the board's
HDMI port (see below), so it was installed from the system the board came
with: the base tarball (`aosc-os_base_<date>_loongson3.tar.xz`) unpacked
into a spare partition, set up in a chroot (user, SSH key, passwordless sudo,
fstab) and booted through the old system's GRUB first. Then the old system was
deleted, the AOSC partition grown into its space, and AOSC's own GRUB
installed:

```bash
sudo grub-install --target=mips64el-efi --efi-directory=/efi --removable
```

The firmware has no writable EFI variables and starts `\EFI\BOOT\BOOTMIPS.EFI`,
the removable path. With the source sent (`linux-suite.ps1 -BuildOnly`) and
Xorg, LightDM and Xfce installed, run

```bash
sh ~/mangekyo/scripts/linux/install-ls7a.sh
```

and reboot. It installs the files in `scripts/linux/loongson/` and builds
the GPU stack below (about 15 minutes, most of it Mesa).

- **Boot without initramfs.** NVMe and ext4 are built into AOSC's kernel, and
  the kernel finds its root by PARTUUID itself; the dracut initramfs did not
  boot from the old system's GRUB. `/etc/grub.d/09_ls7a` writes such an entry
  for the newest kernel, first in the menu (`GRUB_DEFAULT=0`), with the
  display options below. AOSC rebuilds the initramfs and `grub.cfg` whenever a
  package asks for it; its own entries (initramfs, root by UUID) come after.
- **HDMI.** The port hangs off DVO output DPI-2 of the LS7A1000's display
  controller through an MS7210 DVO-to-HDMI bridge. Mainline `drm/loongson`
  treats the bridge as transparent, which leaves it able to carry pixel
  clocks up to about 98 MHz only: 1080p60 (148.5 MHz) gave no signal.
  `ms7210-init` replays the 91 register writes UnionTech OS's driver makes at
  boot (I2C address 0x59 on the controller's second bus, 16-bit register
  numbers low byte first; recorded with an I2C trace of that boot,
  `ms7210-boot-trace.txt`), after the driver has set the mode
  (`ms7210-init.service`, and LightDM's `display-setup-script`). A mode set
  later (a GPU driver loading, a monitor plugged in, Xorg re-probing) leaves
  the bridge black again, so a udev rule runs it again after every DRM card
  event (`ms7210-reinit.service`). The bridge passes no EDID: the kernel
  command line forces DPI-2 on and gives it a 1080p60 EDID
  (`video=DPI-2:e drm.edid_firmware=DPI-2:edid/ls7a-dvo.bin`, written by
  `make-edid.py`). Until the bridge is set up, about 2 s after the driver
  takes the screen, there is no picture.
- **The GPU.** The bridge's Vivante GC1000 (rev 5037, PCI `0014:7a15`,
  registers in the first 64 KiB of BAR 0, 64 MiB VRAM behind BAR 2) is a PCI
  function, and mainline etnaviv has no PCI support (Loongson's patch series
  never landed). `etnaviv-ls7a` creates the platform devices etnaviv looks
  for, with a fixed 396 MHz "core" clock, and loads with the GPU's PCI ID;
  `/etc/modprobe.d/etnaviv-ls7a.conf` loads the display driver first, so the
  display stays `card0`. `ls7a-gpu-linux-7.1.patch` changes etnaviv and
  loongson; the built modules go to `/lib/modules/<kernel>/updates`:
  - The bridge's DMA is cache coherent, so GPU writes can stay in the CPU's
    caches; etnaviv's write-combined CPU mappings then read stale memory
    (readbacks a frame behind, or never changing). Buffers are mapped cached
    on a coherent device.
  - The display controller scans out of VRAM only, and loongson moved every
    buffer it shared to system memory. Now a VRAM buffer is shared as it is,
    as one DMA range, and etnaviv imports memory without struct pages: the
    GC1000 draws straight into the buffer on screen.
  - Unloading etnaviv at run time oopsed in its teardown and left the next
    shutdown hanging (power-cycled); reboot instead.
- **Mesa for the X server.** Mesa's render-only driver (kmsro) pairs a
  display device with a GPU only when the display device is on the platform
  bus; the LS7A1000's is a PCI device, so GBM on `card0` fell back to
  llvmpipe. `mesa-26.1-kmsro-ls7a.patch` makes the exception; that Mesa goes
  to `/opt/mesa-ls7a`, and LightDM starts the X server with
  `LD_LIBRARY_PATH=/opt/mesa-ls7a/lib` (`lightdm-90-mangekyo.conf`) and
  `AccelMethod glamor` (`xorg-20-mangekyo.conf`). Applications keep the
  system Mesa and llvmpipe (OpenGL 4.6); with
  `LD_LIBRARY_PATH=/opt/mesa-ls7a/lib` one runs on the GC1000 instead
  (OpenGL 2.0 / ES 2.0).
- **What the GC1000 does.** It is slower than the CPU at most things, and
  everything it does runs at about 1.3 GB/s, probably the link between the
  bridge and the CPU. Measured with `scripts/linux/loongson/tests/`
  (`EGL_PLATFORM=surfaceless`):

  | 1920x1080, one full-screen layer | GC1000 | llvmpipe (4 cores) |
  |---|---|---|
  | Texture copy | 12.4 ms (167 Mpixel/s) | 5.4 ms (387 Mpixel/s) |
  | Texture alpha blend | 12.7 ms | 34.2 ms |
  | ES 2.0 GPU Burn, 8 steps, 1280x720 frame | 452 ms | 152 ms |

  A frame drawn into the VRAM buffer on screen takes 20.5 ms, one into the
  GPU's own memory 7.6 ms: the GC1000 renders tiled, and Mesa copies each
  frame into the linear buffer the controller scans out. With glamor the
  desktop no longer stutters, but a dragged window trails the pointer when
  xfwm4 composites without waiting for vblank (`vblank_mode off`), and keeps
  up in steps with `xpresent`; window shadows make no difference. The
  framebuffer console is copied to VRAM in batches (every 50 ms), so text
  scrolls in blocks.
- **ES 2.0 GPU Burn.** The GC1000 has no instruction cache and takes 256
  instructions a shader. Mesa unrolls the burn loop, and from 16 steps the
  shader no longer fits: etnaviv drops every draw with no GL error, and at 64
  steps the GPU hung (etnaviv recovered it). `mangekyo_es2burn` now reports a
  black frame as "did not draw" and gives no rate. Up to 8 steps it draws;
  8 steps took 451.89 ms a frame (0.03 Gpix-step/s), a reference only.
- **Desktop off by default.** The machine boots to the console
  (`systemctl set-default multi-user.target`); `sudo systemctl start lightdm`
  starts Xfce, logged in as the user. The ES 2.0 job draws off-screen and
  needs no desktop.

#### Before: UnionTech OS 20

> **Verification status: CPU tests run on real hardware** (UOS 20 Pro, kernel
> 4.19, 2026-10-21). No GPU test.

UOS 20 on this board has no C++ compiler, and without activation its main
package source refuses access. The CPU tool is cross-built on the PC with
Docker instead, statically linked, from the four sources the Android build
of `mangekyo_cpumem` uses (run from the repository root in PowerShell):

```powershell
docker run --rm -v "${PWD}\src:/src:ro" -v "${PWD}\out\mips64:/out" debian:bookworm bash -c "apt-get update -qq && apt-get install -y -qq g++-mips64el-linux-gnuabi64 && mips64el-linux-gnuabi64-g++ -std=c++17 -O2 -Wall -Wextra -static -pthread -I /src /src/cpu_membw.cpp /src/cpu_benchmark.cpp /src/system_info.cpp /src/cpumem_cli.cpp -o /out/mangekyo_cpumem"
```

Copy it to the machine and run `./mangekyo_cpumem --compute`, then
`./mangekyo_cpumem`; the results stay in the output. On MIPS the cache
figures are those of the generic 8-byte path (no Loongson vector
instructions).

The GPU in the 7A1000 bridge (Vivante GC1000, PCI `0014:7a15`) is held by
the built-in Galcore driver, but UOS installs none of Vivante's user-space
libraries, so OpenGL is llvmpipe. etnaviv knows the PCI ID, yet LS7A1000
support never fully reached upstream etnaviv / Mesa, and unbinding Galcore
at run time froze the machine.

### Synology DSM NAS (x86-64, CPU only)

> **Verification status: CPU tests run on real hardware** (Synology DS923+,
> AMD Ryzen Embedded R1600, DSM 7.4.1, 2026-10-24).

DSM has no compiler, so the CPU tool is cross-built on the PC with Docker
like the Loongson one, statically, with the flags of the Linux CMake Release
build (run from the repository root in PowerShell):

```powershell
docker run --rm -v "${PWD}\src:/src:ro" -v "${PWD}\out\x64-static:/out" debian:bookworm bash -c "apt-get update -qq && apt-get install -y -qq g++ && g++ -std=c++17 -O3 -DNDEBUG -Wall -Wextra -static -pthread -I /src /src/cpu_membw.cpp /src/cpu_benchmark.cpp /src/system_info.cpp /src/cpumem_cli.cpp -o /out/mangekyo_cpumem"
```

Turn on SSH (Control Panel > Terminal & SNMP) and the user home service
(Control Panel > User & Group > Advanced); DSM leaves home folders open to
everyone, and SSH ignores a key in them until the folder is `chmod 755`. Copy
the tool with `ssh nas 'cat > ~/mangekyo-cpumem/mangekyo_cpumem' < ...` (a
recent scp goes through SFTP, which DSM turns on separately), and run it from a folder on `/volume1`, so that the
storage test writes there and not into the small system partition. DSM swaps
to zram, so the storage figures are those of the data volume.

---

## Windows (x64 / ARM64)

### 1. Install Visual Studio C++ Build Tools

Install [**Visual Studio 2026**](https://visualstudio.microsoft.com/)
(Community edition is free) with the following workloads selected in the
Visual Studio Installer:

- **Desktop development with C++** — provides MSVC compiler (`cl`), Windows
  SDK, CMake, and the linker.
- **C++ CMake tools for Windows** — bundled CMake integration.

If you only need command-line builds (no IDE), install
[Build Tools for Visual Studio](https://visualstudio.microsoft.com/visual-cpp-build-tools/)
instead — select the same workloads above.

> **Note:** Visual Studio does **not** add `cmake` or `cl` to the system
> PATH by default. They are only available inside the **Developer PowerShell
> for VS** (or **Native Tools Command Prompt**). If you run `cmake` or `cl`
> in a regular PowerShell window, you will get:
>
> ```
> cmake : The term 'cmake' is not recognized as the name of a cmdlet, function, script file, or operable program. Check
> the spelling of the name, or if a path was included, verify that the path is correct and try again.
> At line:1 char:1
> + cmake --version
> + ~~~~~
>     + CategoryInfo          : ObjectNotFound: (cmake:String) [], CommandNotFoundException
>     + FullyQualifiedErrorId : CommandNotFoundException
> ```
>
> To make them available globally, add their directories to your User PATH
> (adjust the VS year/edition and MSVC version to match your installation):

```powershell
# cmake
$cmakeDir = "C:\Program Files\Microsoft Visual Studio\<year>\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"

# cl (x64 — for ARM64, replace Hostx64\x64 with Hostarm64\arm64)
$clDir = "C:\Program Files\Microsoft Visual Studio\<year>\Community\VC\Tools\MSVC\<version>\bin\Hostx64\x64"

$currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
foreach ($dir in @($cmakeDir, $clDir)) {
    if ($currentPath -notlike "*$dir*") {
        $currentPath = "$currentPath;$dir"
    }
}
[Environment]::SetEnvironmentVariable("Path", $currentPath, "User")
```

To find the exact MSVC version installed on your system:

```powershell
ls "C:\Program Files\Microsoft Visual Studio\<year>\Community\VC\Tools\MSVC"
# Example output: 14.50.35717
```

Reopen your terminal, then verify:

```powershell
cmake --version   # Should be 3.25+
cl                # Should print MSVC version information
```

### 2. Install vcpkg (manifest mode)

The repository now contains `vcpkg.json` with a pinned builtin baseline and a
declared GLFW dependency. Do not manually mutate a shared classic-mode package
set for release builds. Clone and bootstrap vcpkg, then expose its root to
CMake:

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
$env:VCPKG_ROOT = 'C:\vcpkg'
& "$env:VCPKG_ROOT\vcpkg.exe" version
```

The target computer never needs vcpkg; this is only a build-machine input.

### 3. Install Vulkan SDK (optional — required for Vulkan API)

Download and install the [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
for Windows. The installer will set the `VULKAN_SDK` environment variable and
add the SDK `Bin` directory (containing `glslc`, `vulkaninfo`, etc.) to PATH.

After installation, verify that your GPU supports Vulkan:

```powershell
vulkaninfo --summary
```

Expected output (example):

```
==========
VULKANINFO
==========
Vulkan Instance Version: 1.x.xxx

Devices:
========
GPU0:
    apiVersion         = 1.3.xxx
    driverVersion      = xxx.xx
    vendorID           = 0x10de
    deviceID           = 0x2684
    deviceType         = PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
    deviceName         = NVIDIA GeForce RTX 5090
    driverName         = NVIDIA
    driverInfo         = xxx.xx
```

If `vulkaninfo` reports no physical devices, your GPU driver may not support
Vulkan — the application will still work with DirectX or OpenGL backends.

> **Cross-compiling?** The Vulkan SDK installer is architecture-specific. The
> `VULKAN_SDK` environment variable must point to the SDK matching your
> **target** architecture, not your host. For example, on an ARM64 machine
> cross-compiling for x64, you need to install both the ARM64 and x64
> versions of the Vulkan SDK in separate directories, and set `VULKAN_SDK`
> accordingly before running CMake:
>
> ```powershell
> # Targeting ARM64 (native on ARM64 host)
> $env:VULKAN_SDK = "C:\VulkanSDK\1.4.x.x"
>
> # Targeting x64 (cross-compiling from ARM64 host)
> $env:VULKAN_SDK = "C:\VulkanSDK-x64\1.4.x.x"
> ```
>
> If the `VULKAN_SDK` architecture does not match the build target, the
> linker will fail with `LNK4272: library machine type conflicts with target
> machine type`.

### 4. Python 3 (optional report tooling only)

OpenGL no longer downloads or generates GLAD during configure: a reproducible
GLAD 2.0.8 OpenGL 4.3 loader is checked into `third_party/glad`. Python is only
needed on the development machine for the legacy chart/report scripts under
`scripts/`; it is not needed to compile or run the CLI/GUI.

### 5. Restore GLFW via vcpkg

No separate `vcpkg install glfw3` command is required. The CMake toolchain
restores the architecture-specific dependency from `vcpkg.json`. Select it with
`VCPKG_TARGET_TRIPLET=x64-windows` or `arm64-windows` while configuring.

The DX12 and DX11 backends only need the Windows SDK (bundled with Visual
Studio). No additional driver installation is needed — D3D12/D3D11 work
through the built-in Windows graphics stack.

---

## macOS (Apple Silicon / Intel)

```bash
brew install glfw cmake
```

The Metal backend uses the system Metal framework — no additional SDK or
driver installation is needed.

---

## Verify Environment

### Linux

```bash
cmake --version    # Should be 3.20+
g++ --version      # GCC 8+ or clang++ 7+
pkg-config --modversion glfw3   # Should print 3.x
glslc --version    # Optional — only required for the Vulkan backend
```

If `glslc` is not found and you need the Vulkan backend, install the LunarG
Vulkan SDK or `sudo apt install glslc`.

### Windows

Before building on Windows, ensure that `cmake`, `cl` (MSVC compiler), and
`glslc` (Vulkan shader compiler — optional) are available in your PATH.
If they are not found, see [Step 1](#1-install-visual-studio-c-build-tools)
for how to add them. Verify in PowerShell:

```powershell
cmake --version   # Should be 3.20+
cl                # Should print MSVC version information
glslc --version   # Optional — only required for the Vulkan backend
```

Typical default paths (Visual Studio 2026 Community on ARM64 as an example):

| Tool | Default Path |
|------|-------------|
| cmake | `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin` |
| cl | `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\<version>\bin\Hostarm64\arm64` |
| glslc | `C:\VulkanSDK\<version>\Bin` |

Add the relevant directories to **User environment variables → Path**, then
reopen your terminal for the changes to take effect.

### macOS

```bash
cmake --version   # Should be 3.20+
clang --version   # Apple Clang (comes with Xcode Command Line Tools)
```

If `cmake` is not found, install it via Homebrew: `brew install cmake`.

---

## Build Steps

### Linux

```bash
# Configure (backends auto-detected based on installed packages)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run
./build/gpu_benchmark
```

CMake will print which backends are enabled during configuration:

```
-- Vulkan backend: ENABLED
-- DX12 backend:   DISABLED (not Windows)
-- DX11 backend:   DISABLED (not Windows)
-- Metal backend:  DISABLED (not macOS)
-- OpenGL backend: ENABLED
```

### Windows

**Default developer build (CLI + WinUI GUI):**

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build-windows.ps1
# GUI: gui\x64\Release\gpu_bench_gui.exe
# CLI: out\build\windows-x64-release\Release\gpu_benchmark.exe
```

The script builds the CMake CLI/engine, then MSBuilds the WinUI GUI and stages
`gpu_benchmark.exe` plus HLSL/SPIR-V/OpenGL shader assets beside the GUI so the
adjacent worker can find them. Pass `-SkipGui` for a CLI-only tree, or
`-Arch ARM64` for a native ARM64 developer build.

**CLI-only CMake path** (same as before; does not build the GUI):

```powershell
# Configure (vcpkg toolchain, all backends auto-detected)
# By default, CMake targets the host architecture (x64 on x64 machines, ARM64 on ARM64 machines).
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake

# Build
cmake --build build --config Release
```

To explicitly target a specific architecture (e.g., cross-compiling), use `-A`
and `-DVCPKG_TARGET_TRIPLET`:

```powershell
# Target x64 (needed when cross-compiling from ARM64)
$env:VULKAN_SDK = "C:\VulkanSDK-x64\1.4.x.x"   # must point to x64 SDK
cmake -S . -B build -A x64 -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows

# Target ARM64 (needed when cross-compiling from x64)
$env:VULKAN_SDK = "C:\VulkanSDK\1.4.x.x"        # must point to ARM64 SDK
cmake -S . -B build -A ARM64 -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=arm64-windows
```

> **Cross-compiling checklist:**
>
> 1. **Vulkan SDK** — Install the SDK for **both** host and target
>    architectures. Set `$env:VULKAN_SDK` to the target architecture's SDK
>    path before running CMake. If the architectures don't match, the linker
>    will fail with `LNK4272: library machine type conflicts with target
>    machine type`.
>
> 2. **vcpkg packages** — Install GLFW (and any other vcpkg dependencies) for
>    the **target** triplet:
>    ```powershell
>    vcpkg install glfw3:x64-windows       # when targeting x64
>    vcpkg install glfw3:arm64-windows     # when targeting ARM64
>    ```
>
> 3. **Clean build directory** — When switching target architectures, always
>    delete the old `build` directory first (`rm -r build`) to avoid stale
>    CMake cache entries.

### macOS

```bash
# Configure (Metal backend auto-detected on macOS)
cmake -S . -B build

# Build
cmake --build build --config Release
```

### Apple Watch (CPU only)

The independent watchOS 10+ project reuses the native CPU engine without GPU
dependencies. A checked-in Xcode project and shared scheme are provided:

```bash
bash scripts/build-watchos.sh # unsigned Release: device and simulator
```

Open `watch-gui/MangekyoWatch.xcodeproj` in Xcode to select a development Team
and a physical Watch for signing/installing. Simulator flow tests, memory
budgets, local `results.json` and measurement limits are documented in
[`watch-gui/README.md`](../watch-gui/README.md). Simulator CPU scores belong to
the host Mac and cannot be used as Watch performance results.

### Backend Toggles

```bash
cmake -S . -B build -DENABLE_VULKAN=OFF -DENABLE_DX12=ON -DENABLE_DX11=ON -DENABLE_METAL=OFF ...
```

## Native CPU benchmark build and smoke

The supplementary CPU benchmark is compiled into the same `gpu_benchmark`
target; it has no additional runtime dependency, graphics window, shader or
RenderDoc requirement. On Windows the WinUI project also contains the dedicated
CPU page and starts the adjacent CLI as a no-window child process.

After a Release build (`scripts\build-windows.ps1` or CMake), run a
non-persistent smoke first:

```powershell
.\out\build\windows-x64-release\Release\gpu_benchmark.exe `
  --cpu-benchmark all `
  --cpu-time 0.09 `
  --cpu-warmup 0 `
  --cpu-no-save
```

Expected Windows acceptance signals are exit code 0, `CPU_META` reporting
`strict_group_affinity`, one valid `CPU_RESULT kind=core` for every available
logical processor, and a valid multi result with `pinned_threads=thread_count`.
Exit code 3 means the strict affinity/completion contract failed for a
per-processor result; those results are not persisted and must not be packaged
as validation data. Since 2026-10-08 the all-core result does not depend on the
pinning (twenty busy threads fill twenty processors wherever the system puts
them): it is valid whenever it was measured, `pinned_threads` says how many
threads were held, and it is saved even when a per-processor result is not.

The formal score command is intentionally long, especially on high-thread-count
systems: 15 seconds is applied separately to every logical processor and then to
the multi-core stage, with a 0.2-second warm-up per test and three median rounds.

```powershell
.\out\build\windows-x64-release\Release\gpu_benchmark.exe `
  --cpu-benchmark all `
  --cpu-time 15 `
  --cpu-warmup 0.2
```

Only Windows x64 is currently validated. Linux/Android enumerate the process
allowed CPU set and require pthread affinity to read back as the requested sole
CPU; successful results use `strict_sched_affinity`, while failure is invalid and
returns 3. Native Linux/container/cpuset and Android device tests are still
required. macOS is scheduler-managed and its logical-to-physical mapping is
estimated. iOS and Web/WASM CPU modes are not release targets yet.

## Windows x64 release staging

Release staging is intentionally separate from a developer build. It installs
only runtime files into a deterministic tree, verifies the tree, creates a ZIP,
and emits a SHA-256 file. User results, captures, reports, logs, PDBs, SDKs,
vcpkg, Python, and shader compilers are not copied.

For a complete GitHub Release candidate, use the umbrella command. It rebuilds
the CMake CLI/engine and self-contained WinUI GUI, validates and bundles the
official RenderDoc portable tree, creates the ZIP and native multilingual Setup
installer (using an embedded WiX MSI as its transaction layer), then
writes a unified asset manifest and `SHA256SUMS.txt`:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\build-windows-github-release.ps1 `
  -RenderDocArchive C:\release-inputs\RenderDoc_<version>_64.zip `
  -RenderDocSha256 <sha256>
```

Use `-RenderDocDownloadUrl <pinned-https-zip> -RenderDocSha256 <sha256>`
instead for an online build. A mutable unverified "latest" download is never
accepted. Add `-SignToolCommand '<signtool command containing $f>' -RequireSigned`
in signed release CI. Final assets are placed in `out/release/windows-x64`
(or `windows-arm64`). The wrapper requires a clean Git worktree; use
`-AllowDirtySource` only for a non-publishable local engineering candidate.

`-BuildGui` creates a fresh self-contained WinUI payload and then reconfigures
the install rules around it. `-GuiPayloadDir` selects an externally prepared
self-contained output when a separate CI job owns the GUI build.

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\stage-windows-release.ps1 `
  -BuildGui `
  -RenderDocDir out\dependencies\RenderDoc
```

Outputs are written below `out/`:

```text
out/stage/windows-x64/                 verified install tree
out/packages/Mangekyo-*.zip
out/packages/Mangekyo-*.zip.sha256
```

For an explicit CLI-only smoke artifact, pass `-SkipGui`. This is not the
intended final GUI product. To add prebuilt portable tools:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\stage-windows-release.ps1 `
  -RenderDocDir C:\release-inputs\RenderDoc `
  -ReportWorkerDir C:\release-inputs\report_worker
```

The RenderDoc input must be the complete portable distribution with its
license, not a copied `renderdoc.dll`. The report-worker input must contain a
frozen `report_worker.exe`; packaged `.py` files alone are not a portable
report feature.

The equivalent CMake commands inspect the raw install/CPack rules:

```powershell
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release
cmake --install out/build/windows-x64-release --config Release `
  --prefix out/stage/windows-x64
cpack --preset windows-x64-zip
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\verify-windows-stage.ps1 `
  -StageDir out\stage\windows-x64 -SmokeTest
```

Raw CMake/CPack output does not contain the post-install `files.sha256`
inventory or GitHub `release-assets.json`; use the PowerShell staging/release
flow for anything intended for distribution.

The preset does not guess a GUI output; pass
`-DGPU_BENCH_GUI_PAYLOAD_DIR=<absolute-directory>` during configuration when
using the lower-level commands.

### Current release gates

Normal verification checks that the core staged artifact is internally
complete and prints unresolved portability warnings. `-RequirePortable` turns
those warnings into CI failures. It currently fails by design until all of the
following are true:

- delay-loaded Vulkan fallback starts successfully on a clean DirectX-only
  machine with no `vulkan-1.dll`;
- a frozen report worker is present and integrated into the GUI, and a complete
  portable RenderDoc bundle is present;
- the staged GUI and fifth-second capture flow pass a clean-machine test;
- the project MIT license is staged (`licenses/LICENSE`; dependency notices are
  also under `licenses/`);
- VC runtime redistribution and installer signing have been reviewed.

The target computer does not need Visual Studio, vcpkg, Python, a Vulkan SDK,
or a shader compiler. Windows builds delay-load and probe `vulkan-1.dll`, so its
absence is designed to disable Vulkan while leaving DirectX/WARP available;
that exact no-loader path remains a clean-machine release test.

The CPU engine adds no new redistributable, so the normal install rules include
it automatically through `gpu_benchmark.exe`. However, any ZIP/Setup built
before the CPU page/engine changes predates this feature. Rebuild the CLI and
self-contained GUI, regenerate the stage and installer, then add an installed-
location CPU smoke to release acceptance: the GUI must find
`app/bin/gpu_benchmark.exe`, complete Quick, cancel a longer run cleanly, and
append its isolated preview/formal summary under the user-data results path.

CPack defaults to `ZIP;WIX`. See `packaging/README.md` and the generated
`PACKAGE_LIMITATIONS.md` for the complete gates.

For the exact two-architecture installer workflow, including rebuilding from an
existing stage, installation-directory selection, bilingual command-line
switches, Authenticode signing and the clean-machine checklist, follow
[`windows-installer-packaging.md`](windows-installer-packaging.md). Raw MSI files
are intermediate inputs; public Windows installer assets are the native x64 and
ARM64 `*-setup.exe` files.
