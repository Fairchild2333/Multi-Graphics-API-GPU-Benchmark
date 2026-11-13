#!/bin/bash
# Runs the standard series of Mangekyo tests on an Android device over adb
# and brings the results back. Bash port of scripts/adb-suite.ps1 (macOS /
# Linux); the test list, durations, judgments and output files are the same.
#
#   device.txt     what the phone says it is
#   NN-<test>.log  what the app and the engine logged for one test
#   results.json   the results file of the app, as it is after the series
#   summary.txt    one line per test
#
# Usage:
#   ./scripts/adb-suite.sh [-s serial] [-i] [-q] [-Only pattern]
#                          [-CoolDownSeconds n] [-StartBelowC degrees]
#                          [-Freeze]
#
#   -i    install android/app/build/outputs/apk/debug/app-debug.apk first
#   -q    3 s GPU tests and 1 s CPU steps, no pause between tests (not for
#         results)
#   -Only run only the tests whose name matches this pattern, e.g. "burn|fluid"
#   -Freeze suspend third-party background apps for the whole series (root
#         required); everything is unsuspended again at the end. A quieter
#         machine changes the conditions, so mark such runs in DEVICE-STATE.
#   -StartBelowC  before every test, wait until the battery is no warmer than
#         this and the system reports no throttling (max ten minutes)

set -u
# Test specs contain ? and &, which must not be globbed when the list is split.
set -f

Serial=""
Install=0
Quick=0
Only=""
CoolDownSeconds=30
StartBelowC=0
Freeze=0
while [ $# -gt 0 ]; do
    case "$1" in
        -s|-Serial) Serial="$2"; shift 2 ;;
        -Serial=*) Serial="${1#*=}"; shift ;;
        -i|-Install) Install=1; shift ;;
        -q|-Quick) Quick=1; shift ;;
        -Only) Only="$2"; shift 2 ;;
        -Only=*) Only="${1#*=}"; shift ;;
        -CoolDownSeconds) CoolDownSeconds="$2"; shift 2 ;;
        -CoolDownSeconds=*) CoolDownSeconds="${1#*=}"; shift ;;
        -StartBelowC) StartBelowC="$2"; shift 2 ;;
        -StartBelowC=*) StartBelowC="${1#*=}"; shift ;;
        -f|-Freeze) Freeze=1; shift ;;
        *) echo "Unknown argument: $1"; exit 2 ;;
    esac
done

repo=$(cd "$(dirname "$0")/.." && pwd)
package="com.mangekyo.benchmark"

adb_bin=$(command -v adb 2>/dev/null)
if [ -z "$adb_bin" ]; then
    adb_bin="$HOME/Library/Android/sdk/platform-tools/adb"
fi
if [ ! -x "$adb_bin" ]; then
    echo "adb was not found on PATH or in the Android SDK."
    exit 2
fi

# Invoke-Adb: prefix the serial when one is named.
if [ -n "$Serial" ]; then
    ADB=("$adb_bin" -s "$Serial")
else
    ADB=("$adb_bin")
fi

devices=$("${ADB[@]}" devices | awk '$2 == "device" { print $1 }')
device_count=$(echo "$devices" | grep -c .)
if [ "$device_count" -eq 0 ]; then
    echo "No device. Connect the phone, allow USB debugging on it, and check \`adb devices\`."
    exit 2
fi
if [ "$device_count" -gt 1 ] && [ -z "$Serial" ]; then
    echo "More than one device is connected; name one with -s:"
    echo "$devices" | sed 's/^/  /'
    exit 2
fi
if [ -n "$Serial" ] && ! echo "$devices" | grep -qx "$Serial"; then
    echo "Device '$Serial' is not connected. Connected:"
    echo "$devices" | sed 's/^/  /'
    exit 2
fi

get_prop() { echo "$("${ADB[@]}" shell getprop "$1" | tr -d '\r')" | sed 's/^ *//;s/ *$//'; }

# The serial of the one device in use.
if [ -n "$Serial" ]; then device="$Serial"; else device=$(echo "$devices" | head -1); fi

# True while adb can talk to the phone. `adb devices` asks the adb server and
# returns at once, whereas any other command waits without end for a phone that
# has dropped off (unplugged, frozen, restarted).
test_device() { "$adb_bin" devices | grep -q "^$(echo "$device" | sed 's/[^A-Za-z0-9_.-]/\\&/g')[[:space:]]*device$"; }

model="$(get_prop ro.product.manufacturer) $(get_prop ro.product.model)"
# Two units of the same model must stay distinguishable, so the serial goes
# into the folder name and device.txt.
folder_name=$(echo "$model" | sed 's/[^A-Za-z0-9]\+/-/g;s/^-*//;s/-*$//')-"$device"-$(date +%Y%m%d-%H%M%S)
out="$repo/out/adb/$folder_name"
mkdir -p "$out"

if [ "$Quick" -eq 1 ]; then series_note="quick check, not for results"; else series_note="formal durations"; fi
{
    echo "model:        $model"
    echo "serial:       $device"
    echo "device:       $(get_prop ro.product.device)"
    echo "chip:         $(get_prop ro.soc.manufacturer) $(get_prop ro.soc.model)"
    echo "hardware:     $(get_prop ro.hardware)"
    echo "platform:     $(get_prop ro.board.platform)"
    echo "android:      $(get_prop ro.build.version.release) (API $(get_prop ro.build.version.sdk))"
    echo "build:        $(get_prop ro.build.display.id)"
    echo "abi:          $(get_prop ro.product.cpu.abilist)"
    echo "display:      $(echo "$("${ADB[@]}" shell wm size | tr -d '\r')" | tr '\n' ' ' | sed 's/^ *//;s/ *$//')"
    echo "series:       $series_note"
    echo "started:      $(date '+%Y-%m-%d %H:%M:%S')"
} > "$out/device.txt"
cat "$out/device.txt"

# The app can only come to the front of an unlocked phone. Unlocking is the
# owner's to do; wait for it.
test_locked() { "${ADB[@]}" shell dumpsys window 2>/dev/null | grep -q "isKeyguardShowing=true\|mDreamingLockscreen=true"; }
if test_locked; then
    "${ADB[@]}" shell input keyevent KEYCODE_WAKEUP > /dev/null
    echo "The phone is locked. Unlock it; waiting up to 10 minutes ..."
    waited=0
    while test_locked && [ $waited -lt 600 ]; do sleep 3; waited=$((waited + 3)); done
    if test_locked; then
        echo "Still locked. Nothing was run."
        exit 2
    fi
    sleep 2
fi

if [ $Install -eq 1 ]; then
    apk="$repo/android/app/build/outputs/apk/debug/app-debug.apk"
    if [ ! -f "$apk" ]; then
        echo "No APK at $apk. Build it first: cd android; ./gradlew :app:assembleDebug"
        exit 2
    fi
    echo "Installing $apk"
    echo "  (a phone that asks whether to install over USB waits about 10 s for the answer)"
    installed=$("${ADB[@]}" install -r "$apk" 2>&1)
    echo "$installed"
    if ! echo "$installed" | grep -q "Success"; then
        if echo "$installed" | grep -q "INSTALL_FAILED_USER_RESTRICTED"; then
            echo "The phone refused the installation. On a Xiaomi phone turn on 'Install via USB'"
            echo "(USB 安装) under Developer options, and confirm the question the phone asks."
        elif echo "$installed" | grep -q "INSTALL_FAILED_UPDATE_INCOMPATIBLE"; then
            echo "Another build of the app, signed differently, is on the phone. Removing it deletes"
            echo "its saved results, so that is left to you: adb uninstall $package"
        fi
        exit 2
    fi
fi
if ! "${ADB[@]}" shell pm list packages "$package" | tr -d '\r' | grep -q "$package"; then
    echo "The app is not installed on the phone. Run again with -Install."
    exit 2
fi

# Suspend every third-party app except the benchmark, the launcher and the
# input method, so the CPU tests see a quieter machine. Everything is
# unsuspended at the end and on any exit. A suspended app keeps its data;
# this changes nothing on disk.
frozen_list=""
cleanup_freeze() {
    if [ -n "$frozen_list" ]; then
        for p in $frozen_list; do
            "${ADB[@]}" shell su -c "pm unsuspend $p" > /dev/null 2>&1
        done
        echo "Unsuspended $(echo $frozen_list | wc -w | tr -d ' ') apps."
        frozen_list=""
    fi
}
trap cleanup_freeze EXIT
if [ $Freeze -eq 1 ]; then
    uid=$("${ADB[@]}" shell su -c id -u 2>/dev/null | tr -d '\r' | tr -d ' ')
    if [ "$uid" != "0" ]; then
        echo "-Freeze needs root (su); the device did not give it (uid: '$uid')."
        exit 2
    fi
    # Samsung answers get-default-launcher with a ComponentInfo{...}; older
    # devices may only answer resolve-activity. Keep the package name.
    launcher=$("${ADB[@]}" shell cmd shortcut get-default-launcher 2>/dev/null | tr -d '\r' | sed -n 's/.*ComponentInfo{\([^/}]*\).*/\1/p')
    if [ -z "$launcher" ]; then
        launcher=$("${ADB[@]}" shell cmd package resolve-activity --brief -c android.intent.category.HOME 2>/dev/null | tr -d '\r' | grep -v '^$' | tail -1 | cut -d/ -f1)
    fi
    case "$launcher" in *No\ activity*|"") echo "Could not identify the launcher; not freezing (a frozen launcher can leave the phone without a home screen)."; exit 2 ;; esac
    ime=$("${ADB[@]}" shell settings get secure default_input_method 2>/dev/null | tr -d '\r' | cut -d/ -f1)
    keep="com.mangekyo.benchmark $launcher $ime"
    for p in $("${ADB[@]}" shell pm list packages -3 | tr -d '\r' | sed 's/package://'); do
        case " $keep " in *" $p "*) continue ;; esac
        if "${ADB[@]}" shell su -c "pm suspend $p" > /dev/null 2>&1; then
            frozen_list="$frozen_list $p"
        fi
    done
    echo "$frozen_list" > "$out/frozen-packages.txt"
    echo "Frozen $(echo $frozen_list | wc -w | tr -d ' ') third-party apps (launcher: $launcher, ime: $ime)."
fi

if [ $Quick -eq 1 ]; then gpu_seconds=3; cpu_seconds=1; CoolDownSeconds=0; else gpu_seconds=15; cpu_seconds=15; fi

# name|what adb asks the app for|seconds given to the app|how long to wait for the end
all_tests="
particle-65k|stream?particles=65536|$gpu_seconds|120
particle-1m|stream?particles=1048576|$gpu_seconds|120
particle-4m|stream?particles=4194304|$gpu_seconds|120
particle-16m|stream?particles=16777216|$gpu_seconds|180
particle-65k-headless|stream_headless?particles=65536|$gpu_seconds|120
particle-1m-headless|stream_headless?particles=1048576|$gpu_seconds|120
particle-4m-headless|stream_headless?particles=4194304|$gpu_seconds|120
particle-16m-headless|stream_headless?particles=16777216|$gpu_seconds|180
gpu-burn-16|gpu_burn?steps=16|$gpu_seconds|180
gpu-burn-64|gpu_burn?steps=64|$gpu_seconds|180
gpu-burn-256|gpu_burn?steps=256|$gpu_seconds|240
fluid|cinematic_liquid|$gpu_seconds|240
vram-bandwidth|membw|0|1800
gles-particle-65k|stream?particles=65536&api=gles|$gpu_seconds|120
gles-particle-1m|stream?particles=1048576&api=gles|$gpu_seconds|120
gles-particle-4m|stream?particles=4194304&api=gles|$gpu_seconds|120
gles-particle-16m|stream?particles=16777216&api=gles|$gpu_seconds|180
gles-particle-65k-headless|stream_headless?particles=65536&api=gles|$gpu_seconds|120
gles-particle-1m-headless|stream_headless?particles=1048576&api=gles|$gpu_seconds|120
gles-particle-4m-headless|stream_headless?particles=4194304&api=gles|$gpu_seconds|120
gles-particle-16m-headless|stream_headless?particles=16777216&api=gles|$gpu_seconds|180
gles-gpu-burn-16|gpu_burn?steps=16&api=gles|$gpu_seconds|180
gles-gpu-burn-64|gpu_burn?steps=64&api=gles|$gpu_seconds|180
gles-gpu-burn-256|gpu_burn?steps=256&api=gles|$gpu_seconds|240
gles2-gpu-burn-16|gpu_burn?steps=16&api=gles2|$gpu_seconds|180
gles2-gpu-burn-64|gpu_burn?steps=64&api=gles2|$gpu_seconds|180
gles2-gpu-burn-256|gpu_burn?steps=256&api=gles2|$gpu_seconds|240
cpu-compute|cpu_all|$cpu_seconds|1800
cpu-cache-memory|cpu_membw|0|1800
"
tests=$(echo "$all_tests" | grep -v '^$')
total=$(echo "$tests" | wc -l | tr -d ' ')

# Tenths of a degree Celsius, as the battery reports it.
get_temperature() {
    line=$("${ADB[@]}" shell dumpsys battery | tr -d '\r' | grep -m1 -o 'temperature: [0-9]*')
    if [ -n "$line" ]; then echo "$line" | awk '{print $2 / 10.0}'; fi
}
# 0 while the system holds nothing back for heat; empty when it does not say.
get_thermal_status() {
    "${ADB[@]}" shell dumpsys thermalservice 2>/dev/null | tr -d '\r' | grep -m1 '^Thermal Status:' | awk '{print $3}'
}

summary=""
index=0
old_ifs=$IFS
IFS='
'
for entry in $tests; do
    IFS=$old_ifs
    index=$((index + 1))
    name=$(echo "$entry" | cut -d'|' -f1)
    run=$(echo "$entry" | cut -d'|' -f2)
    seconds=$(echo "$entry" | cut -d'|' -f3)
    wait_for=$(echo "$entry" | cut -d'|' -f4)
    if [ -n "$Only" ] && ! echo "$name" | grep -Eq "$Only"; then continue; fi
    log="$out/$(printf '%02d' $index)-$name.log"

    if [ "$(echo "$StartBelowC > 0" | bc)" = "1" ]; then
        cooling=0
        while [ $cooling -lt 600 ] && test_device; do
            now=$(get_temperature)
            thermal_now=$(get_thermal_status)
            if [ -n "$now" ] && [ "$(echo "$now <= $StartBelowC" | bc)" = "1" ] && [ -z "$thermal_now" -o "$thermal_now" = "0" ]; then
                break
            fi
            sleep 10
            cooling=$((cooling + 10))
        done
    fi

    temperature=$(get_temperature)
    thermal=$(get_thermal_status)
    printf '[%02d/%d] %-26s battery %s C ...' "$index" "$total" "$name" "$temperature"

    "${ADB[@]}" shell input keyevent KEYCODE_WAKEUP > /dev/null
    "$adb_bin" logcat -c > /dev/null 2>&1
    # The app's lines go to the file as they come. Reading them back at the end
    # loses the start of a long test on a device that logs a lot: a Quest 3
    # fills its log buffer in a minute or two.
    if [ -n "$Serial" ]; then
        "$adb_bin" -s "$Serial" logcat -s Mangekyo:V MangekyoEngine:V > "$log" 2>/dev/null &
    else
        "$adb_bin" logcat -s Mangekyo:V MangekyoEngine:V > "$log" 2>/dev/null &
    fi
    capture_pid=$!

    # Sample GPU and per-cluster CPU clocks every two seconds while the test
    # runs: read-only evidence of what the device actually did, appended to
    # one file for the whole series.
    echo "=== $(printf '%02d' $index) $name (battery $temperature C) ===" >> "$out/clocks-every-2s.txt"
    "$adb_bin" ${Serial:+-s "$Serial"} shell 'while :; do printf "%s gpu=%s" "$(date +%H:%M:%S)" "$(cat /sys/class/kgsl/kgsl-3d0/devfreq/cur_freq 2>/dev/null)"; for p in /sys/devices/system/cpu/cpufreq/policy*; do printf " %s=%s" "$(basename "$p")" "$(cat "$p/scaling_cur_freq" 2>/dev/null)"; done; echo; sleep 2; done' >> "$out/clocks-every-2s.txt" 2>/dev/null &
    sampler_pid=$!

    command="am start -S -n $package/.MainActivity --es run '$run'"
    if [ "$seconds" -gt 0 ]; then command="$command --ei seconds $seconds"; fi
    "${ADB[@]}" shell "$command" > /dev/null

    elapsed=0
    end=""
    accepted=0
    lost=0
    dumped=0
    while [ $elapsed -lt $wait_for ]; do
        sleep 2
        elapsed=$((elapsed + 2))
        if ! test_device; then
            # A device that adb has marked "offline" stays so until adb is told
            # to reconnect; it then takes the phone up to a minute, during
            # which it shows as "unauthorized". The test goes on meanwhile.
            "$adb_bin" reconnect offline > /dev/null 2>&1
            gone=0
            while ! test_device && [ $gone -lt 75 ]; do sleep 3; gone=$((gone + 3)); done
            if ! test_device; then lost=1; break; fi
        fi
        # The capture ends when adb drops out; what the buffer still holds then
        # is the only way to see the end of the test.
        if ! kill -0 $capture_pid 2>/dev/null && [ $dumped -eq 0 ]; then
            "${ADB[@]}" logcat -d -s Mangekyo:V >> "$log" 2>/dev/null
            dumped=1
        fi
        if [ $accepted -eq 0 ] && grep -q "AUTORUN accepted" "$log" 2>/dev/null; then
            accepted=1
        fi
        end=$(grep -E "RUN finished|AUTORUN refused|AUTORUN failed" "$log" 2>/dev/null | tail -1)
        if [ -n "$end" ]; then break; fi
        if [ $accepted -eq 0 ] && [ $elapsed -gt 25 ]; then break; fi
    done
    capture_lost=0
    if kill -0 $capture_pid 2>/dev/null; then
        kill $capture_pid 2>/dev/null
        wait $capture_pid 2>/dev/null
    else
        capture_lost=1
    fi
    kill $sampler_pid 2>/dev/null
    wait $sampler_pid 2>/dev/null

    if [ $lost -eq 1 ]; then
        kill $sampler_pid 2>/dev/null; wait $sampler_pid 2>/dev/null
        kill $capture_pid 2>/dev/null; wait $capture_pid 2>/dev/null
        echo " DEVICE LOST after $elapsed s"
        summary="$summary
$name|DEVICE LOST|$elapsed|$temperature|adb lost the phone during this test|"
        break
    fi
    if [ $capture_lost -eq 1 ] && [ $dumped -eq 0 ]; then
        # Whatever came after the capture ended, as far as the buffer has it.
        "${ADB[@]}" logcat -d -s Mangekyo:V MangekyoEngine:V >> "$log" 2>/dev/null
    fi

    status="ok"
    detail=""
    if [ $accepted -eq 0 ]; then
        status="NOT STARTED"
        detail="the app did not take the request; is the phone unlocked?"
    elif [ -z "$end" ]; then
        status="TIMEOUT"
        detail="no end after $wait_for s"
        "${ADB[@]}" shell am force-stop "$package" > /dev/null 2>&1
    elif echo "$end" | grep -qE "AUTORUN (refused|failed)"; then
        status="FAILED"
        detail=$(echo "$end" | sed -n 's/.*reason=\(.*\)$/\1/p')
    elif echo "$end" | grep -q "error="; then
        status="FAILED"
        detail=$(echo "$end" | sed -n 's/.*error=\(.*\)$/\1/p')
    elif echo "$end" | grep -qE "kind=cpu exit=[0-9]+"; then
        exit_code=$(echo "$end" | sed -n 's/.*kind=cpu exit=\([0-9]*\).*$/\1/p')
        if [ "$exit_code" != "0" ]; then status="FAILED"; detail="exit code $exit_code"; fi
        line=$(grep -E "All-core score|CPUMEM_LEVEL +mode=single +level=memory" "$log" | tail -1)
        allcore=$(echo "$line" | sed -n 's/All-core score[^:]*: *\(.*\)$/\1/p')
        if [ -n "$allcore" ]; then detail="all cores $allcore"; fi
        # The phone moved a thread off the core it was to measure, or took a
        # core away: the engine marks that core invalid and ends with an error.
        pinning=$(grep "CPU_ERROR" "$log" | grep "required_affinity_failed" | tail -1)
        invalid=$(echo "$pinning" | sed -n 's/.*invalid_count=\([0-9]*\).*$/\1/p')
        if [ -n "$invalid" ]; then
            status="INVALID"
            detail="$invalid core(s) could not be held to their thread; $detail"
        elif [ -n "$line" ]; then
            read_gbps=$(echo "$line" | sed -n 's/.*read_gbps=\([0-9.]*\).*$/\1/p')
            if [ -n "$read_gbps" ]; then detail="memory read, one thread $read_gbps GB/s"; fi
        fi
    elif echo "$end" | grep -q "score="; then
        detail=$(echo "$end" | sed -n 's/.*score=\(.*\)$/\1/p')
        if [ "$detail" = "none" ]; then status="NO SCORE"; fi
    fi
    # What the engine says about a run that the display or the CPU held back.
    note=""
    if grep -q ">> Display-bound" "$log" 2>/dev/null; then
        note="held at the refresh rate: the score may be too low"
    fi
    present=$(grep -E "\[(Vulkan|OpenGL ES)\] Present mode: " "$log" 2>/dev/null | tail -1 | sed -n 's/.*Present mode: \(\S*\).*$/\1/p')
    if [ -n "$present" ]; then
        if [ -n "$note" ]; then note="$note; "; fi
        note="${note}present $present"
    fi
    # OpenGL ES without a usable GPU clock times a frame from the CPU side.
    if grep -qE "using synchronized wall-clock timing|timed by wall clock" "$log" 2>/dev/null; then
        if [ -n "$note" ]; then note="$note; "; fi
        note="$note timed by wall clock"
    fi
    if [ -n "$thermal" ] && [ "$thermal" != "0" ]; then
        if [ -n "$note" ]; then note="$note; "; fi
        note="$note thermal status $thermal at the start"
    fi

    echo " $status in $elapsed s  $detail"
    summary="$summary
$name|$status|$elapsed|$temperature|$detail|$note"

    if [ "$status" = "NOT STARTED" ]; then break; fi
    # A test the device refused at once did no work: there is nothing to cool.
    refused_at_once=0
    if [ "$status" = "FAILED" ] && [ $elapsed -lt 6 ]; then refused_at_once=1; fi
    if [ $CoolDownSeconds -gt 0 ] && [ $refused_at_once -eq 0 ]; then
        # Keep waking the screen through the pause: a phone whose display
        # timeout is shorter than the pause would lock and refuse the next
        # test. No setting is changed.
        cooled=0
        while [ $cooled -lt $CoolDownSeconds ]; do
            poke=$((CoolDownSeconds - cooled)); [ $poke -gt 20 ] && poke=20
            sleep $poke
            cooled=$((cooled + poke))
            "${ADB[@]}" shell input keyevent KEYCODE_WAKEUP > /dev/null 2>&1
        done
    fi
done
IFS=$old_ifs

if test_device; then
    "${ADB[@]}" pull "/sdcard/Android/data/$package/files/results/results.json" "$out/results.json" > /dev/null 2>&1
else
    echo "adb has lost the phone (unplugged, frozen or restarted). Reconnect it and look at the phone;"
    echo "the tests that finished are saved on it, and -Only runs the rest."
fi

{
    printf '%-26s %-12s %7s %9s %-44s %s\n' "Test" "Status" "Seconds" "BatteryC" "Result" "Note"
    echo "$summary" | grep -v '^$' | while IFS='|' read -r t s sec temp res note; do
        printf '%-26s %-12s %7s %9s %-44s %s\n' "$t" "$s" "$sec" "$temp" "$res" "$note"
    done
} > "$out/summary.txt"
echo ""
cat "$out/summary.txt"
echo ""
echo "Collected in $out"
if [ ! -f "$out/results.json" ]; then
    echo "The results file could not be pulled from the phone."
fi
if echo "$summary" | grep -v '^$' | cut -d'|' -f2 | grep -qv '^ok$'; then exit 1; fi
