#!/bin/sh
# The desktop complete-suite jobs for every hardware GPU of this machine, run
# one by one with the suite's arguments, then the two CPU jobs. Linux has
# Vulkan and OpenGL; software rasterizers (llvmpipe, lavapipe) are left out.
# The windowed jobs draw on the logged-in desktop, so a monitor (or an HDMI
# dummy plug) and an automatic login are needed. RenderDoc is not used.
#
# Run on the test machine, normally through scripts/linux-suite.ps1:
#   sh ~/mangekyo/scripts/linux/series.sh <outdir> [only-regex]
out=$1
only=${2:-}
root=$(cd "$(dirname "$0")/../.." && pwd)
bin=$root/build/gpu_benchmark
cool=${COOL:-30}
mkdir -p "$out"

uid=$(id -u)
export XDG_RUNTIME_DIR=/run/user/$uid
export DBUS_SESSION_BUS_ADDRESS=unix:path=$XDG_RUNTIME_DIR/bus
[ -S "$XDG_RUNTIME_DIR/wayland-0" ] && export WAYLAND_DISPLAY=wayland-0
export DISPLAY=${DISPLAY:-:0}

# Keep the session awake for the whole series without changing any setting.
# Probe first: without a desktop session the inhibitor fails, and the series
# then runs without it.
if [ -z "${SERIES_INHIBITED:-}" ] && command -v gnome-session-inhibit >/dev/null 2>&1 &&
   gnome-session-inhibit --inhibit idle --reason "probe" true >/dev/null 2>&1; then
    SERIES_INHIBITED=1 exec gnome-session-inhibit --inhibit idle:suspend --reason "Mangekyo series" sh "$0" "$@"
fi

gpu_mhz() {
    if command -v nvidia-smi >/dev/null 2>&1; then
        nvidia-smi --query-gpu=clocks.gr,temperature.gpu,power.draw --format=csv,noheader,nounits 2>/dev/null | head -1 | tr -d ' ' | tr ',' ' '
        return
    fi
    for f in /sys/class/drm/card*/gt_act_freq_mhz /sys/class/drm/card*/device/tile0/gt0/freq0/act_freq; do
        [ -r "$f" ] && { echo "$(cat "$f") - -"; return; }
    done
    for f in /sys/class/drm/card*/device/pp_dpm_sclk; do
        [ -r "$f" ] && { echo "$(grep '\*' "$f" | awk '{print $2}' | tr -dc '0-9') - -"; return; }
    done
    echo "- - -"
}
cpu_temp() {
    t=$(cat /sys/class/thermal/thermal_zone*/temp 2>/dev/null | sort -n | tail -1)
    [ -n "$t" ] && echo $((t / 1000)) || echo -
}
telemetry() {
    echo "time,cpu_temp_c,cpu0_khz,gpu_mhz,gpu_temp_c,gpu_power_w" > "$out/telemetry.csv"
    while :; do
        set -- $(gpu_mhz)
        echo "$(date +%T),$(cpu_temp),$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq 2>/dev/null || echo -),$1,$2,$3" >> "$out/telemetry.csv"
        sleep 2
    done
}

{
    echo "host: $(hostname)"
    echo "source: $(cat "$root/SOURCE-COMMIT" 2>/dev/null)"
    echo "os: $(. /etc/os-release && echo "$PRETTY_NAME"), kernel $(uname -r)"
    echo "cpu: $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2-), $(nproc) threads, governor $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null)"
    echo "power profile: $(powerprofilesctl get 2>/dev/null || echo n/a)"
    echo "session: ${WAYLAND_DISPLAY:+wayland }${DISPLAY:+x11 $DISPLAY}"
    env | grep -E "^(MESA|EGL|LIBGL|GALLIUM)_" | sed "s/^/env: /"
    lspci | grep -i -E 'vga|3d|display'
    command -v nvidia-smi >/dev/null 2>&1 && nvidia-smi --query-gpu=name,driver_version,pcie.link.gen.current,pcie.link.width.current --format=csv,noheader
    # Xwayland of a GNOME session wants its key; the engine itself is left to
    # pick Wayland as it does without one.
    xauth=$(ls "$XDG_RUNTIME_DIR"/.mutter-Xwaylandauth.* 2>/dev/null | head -1)
    XAUTHORITY=$xauth vulkaninfo --summary 2>/dev/null | grep -E 'deviceName|driverName|driverInfo|apiVersion'
    XAUTHORITY=$xauth glxinfo -B 2>/dev/null | grep -E 'OpenGL renderer|OpenGL core profile version'
    "$bin" --list-gpus 2>/dev/null
} > "$out/state.txt" 2>&1

telemetry &
tpid=$!
trap 'kill $tpid 2>/dev/null' EXIT

n=0
run() {
    name=$1; shift
    if [ -n "$only" ] && ! echo "$name" | grep -E -q "$only"; then return; fi
    n=$((n + 1))
    log=$(printf "%s/%02d-%s.log" "$out" "$n" "$name")
    start=$(date +%s)
    echo "== $n $name: $(date +%T) cpu $(cpu_temp)C" | tee -a "$out/summary.txt"
    "$bin" "$@" > "$log" 2>&1 < /dev/null
    code=$?
    end=$(date +%s)
    result=$(grep -a -E "VRAM rate:|Burn rate:|Fluid rate:|Memory rate:|All-core score|read +[0-9.]+ GB/s|unreliable|cannot be used|Fatal|CRASH|Saved as" "$log" | tail -4 | tr '\n' ' ')
    echo "   exit=$code $((end - start)) s cpu $(cpu_temp)C  $result" | tee -a "$out/summary.txt"
    sleep "$cool"
}

# GPU <i> <name> <vk> <dx12> <dx11> <gl> <fl> <dx11cs> <nodes> <sw> <metal> <glcs> <vram>
"$bin" --list-gpus 2>/dev/null | grep '^GPU' > "$out/gpus.tsv"
while IFS="$(printf '\t')" read -r _ idx gname vk _ _ gl _ _ _ sw _ glcs _; do
    [ "$sw" = 1 ] && continue
    tag=$(echo "$gname" | tr -c 'A-Za-z0-9\n' '-' | sed 's/--*/-/g; s/-$//')
    apis=""
    [ "$vk" = 1 ] && apis="vulkan"
    [ "$gl" = 1 ] && apis="$apis opengl"
    for api in $apis; do
        compute=1
        [ "$api" = opengl ] && [ "$glcs" != 1 ] && compute=0
        if [ $compute = 1 ]; then
            for p in 65536 1048576 4194304 16777216; do
                run "$tag-$api-stream-$p-shown" --backend $api --gpu "$idx" --workload stream --time 15 --particles $p --no-renderdoc
            done
            for p in 65536 1048576 4194304 16777216; do
                run "$tag-$api-stream-$p-headless" --backend $api --gpu "$idx" --workload stream --time 15 --particles $p --headless --no-renderdoc
            done
        fi
        for s in 16 64 256; do
            run "$tag-$api-gpu-burn-$s" --backend $api --gpu "$idx" --workload gpu_burn --time 15 --iter $s --no-renderdoc
        done
        if [ "$api" = vulkan ]; then
            run "$tag-vulkan-fluid" --backend vulkan --gpu "$idx" --workload cinematic_liquid --time 15 --no-renderdoc
            run "$tag-vulkan-vram-bandwidth" --backend vulkan --gpu "$idx" --workload membw --headless --no-renderdoc
        fi
    done
done < "$out/gpus.tsv"

# OpenGL ES 2.0 GPU Burn (its own score group, as on Android), drawn off-screen
# by build/mangekyo_es2burn on the GPU behind EGL. It is the only GPU test a
# driver with OpenGL ES 2.0 alone can run (Zhaoxin C-960). Not saved in
# results.json; the figures are in the logs. Skipped on a software renderer.
es2=$root/build/mangekyo_es2burn
# eglinfo lists every EGL platform whatever EGL_PLATFORM says; take the
# surfaceless one the test runs on (GBM gave llvmpipe on a Radeon HD 6450).
es2gpu=$(eglinfo -B 2>/dev/null | sed -n '/^Surfaceless platform:/,/ platform:$/ s/^OpenGL ES profile renderer: //p' | head -1)
# Older distributions (Kylin V10, Ubuntu 20.04) have no eglinfo; the GLX
# renderer is the same GPU.
[ -z "$es2gpu" ] && es2gpu=$(glxinfo -B 2>/dev/null | sed -n 's/^OpenGL renderer string: //p' | head -1)
if [ -x "$es2" ] && [ -n "$es2gpu" ] && ! echo "$es2gpu" | grep -q -i -E "llvmpipe|softpipe|lavapipe"; then
    tag=$(echo "$es2gpu" | tr -c 'A-Za-z0-9\n' '-' | sed 's/--*/-/g; s/-$//')
    for s in 16 64 256; do
        if [ -n "$only" ] && ! echo "$tag-gles2-gpu-burn-$s" | grep -E -q "$only"; then continue; fi
        n=$((n + 1))
        log=$(printf "%s/%02d-%s-gles2-gpu-burn-%s.log" "$out" "$n" "$tag" "$s")
        start=$(date +%s)
        echo "== $n $tag-gles2-gpu-burn-$s: $(date +%T) cpu $(cpu_temp)C" | tee -a "$out/summary.txt"
        EGL_PLATFORM=surfaceless "$es2" --steps "$s" --seconds 15 --shader-dir "$root/shaders" > "$log" 2>&1 < /dev/null
        code=$?
        end=$(date +%s)
        result=$(grep -a -E "Burn rate:|Last frame:|did not|failed|offers no" "$log" | tr '\n' ' ')
        echo "   exit=$code $((end - start)) s cpu $(cpu_temp)C  $result" | tee -a "$out/summary.txt"
        sleep "$cool"
    done
fi

run "cpu-compute" --cpu-benchmark all --cpu-time 15 --cpu-warmup 0.2
run "cpu-cache-memory" --cpu-membw

cp "$HOME/.local/share/GpuComputeBenchmark/results/results.json" "$out/results.json" 2>/dev/null
echo "SERIES DONE $(date +%T)" | tee -a "$out/summary.txt"
