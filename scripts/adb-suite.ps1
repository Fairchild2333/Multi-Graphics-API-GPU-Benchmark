<#
.SYNOPSIS
    Runs the standard series of Mangekyo tests on an Android device over adb
    and brings the results back.

.DESCRIPTION
    The series is what was run on every GPU with the Windows app (the particle
    test at its four sizes, shown and headless, GPU Burn and the fluid), with
    the two cache tests added. On Android there is one API, Vulkan.

      particle test, shown     65,536 / 1,048,576 / 4,194,304 / 16,777,216
      particle test, headless  the same four sizes
      GPU Burn                 64 steps
      fluid
      VRAM / GPU cache bandwidth
      CPU compute              every core alone, then all cores
      CPU cache, memory and storage

    A GPU test runs for 15 s (2 s warm-up, 13 s measured), as a formal run on
    the desktop does, and the CPU compute test takes 15 s per step. The engine
    still saves every result as an Android preview, in its own group.

    Before it starts, the phone must be unlocked and "USB debugging" allowed.
    The script installs nothing unless -Install is given, changes no setting of
    the phone, and waits between tests so that one test does not heat up the
    next. Everything it collects goes to out\adb\<model>-<time>\:

      device.txt     what the phone says it is
      NN-<test>.log  what the app and the engine logged for one test
      results.json   the results file of the app, as it is after the series
      summary.txt    one line per test

.PARAMETER Serial
    The device to use when more than one is connected (see `adb devices`).

.PARAMETER Install
    Installs android\app\build\outputs\apk\debug\app-debug.apk first.

.PARAMETER Quick
    3 s GPU tests and 1 s CPU steps, no pause between tests. For checking that
    every test starts, for example on an emulator. Not for results.

.PARAMETER Only
    Runs only the tests whose name matches this pattern, e.g. -Only "burn|fluid".

.PARAMETER CoolDownSeconds
    Pause after every test. Default 30.

.EXAMPLE
    .\scripts\adb-suite.ps1 -Install

.EXAMPLE
    .\scripts\adb-suite.ps1 -Quick -Only "stream"
#>
param(
    [string]$Serial = "",
    [switch]$Install,
    [switch]$Quick,
    [string]$Only = "",
    [int]$CoolDownSeconds = 30
)

$repo = Split-Path -Parent $PSScriptRoot
$package = "com.mangekyo.benchmark"
$adb = (Get-Command adb -ErrorAction SilentlyContinue).Source
if (-not $adb) { $adb = Join-Path $env:LOCALAPPDATA "Android\Sdk\platform-tools\adb.exe" }
if (-not (Test-Path $adb)) {
    Write-Host "adb was not found on PATH or in the Android SDK."
    exit 2
}

function Invoke-Adb {
    if ($Serial) { & $adb -s $Serial @args } else { & $adb @args }
}

$devices = @(& $adb devices | Select-Object -Skip 1 | Where-Object { $_ -match "\tdevice$" })
if ($devices.Count -eq 0) {
    Write-Host "No device. Connect the phone, allow USB debugging on it, and check `adb devices`."
    exit 2
}
if ($devices.Count -gt 1 -and -not $Serial) {
    Write-Host "More than one device is connected; name one with -Serial:"
    $devices | ForEach-Object { Write-Host "  $_" }
    exit 2
}
if ($Serial -and -not ($devices | Where-Object { ($_ -split "\t")[0] -eq $Serial })) {
    Write-Host "Device '$Serial' is not connected. Connected:"
    $devices | ForEach-Object { Write-Host "  $_" }
    exit 2
}

function Get-Prop([string]$name) { ((Invoke-Adb shell getprop $name) -join "").Trim() }

# The serial of the one device in use.
$device = if ($Serial) { $Serial } else { ($devices[0] -split "\t")[0] }

# True while adb can talk to the phone. `adb devices` asks the adb server and
# returns at once, whereas any other command waits without end for a phone that
# has dropped off (unplugged, frozen, restarted).
function Test-Device {
    return [bool](@(& $adb devices) | Where-Object { $_ -match ("^" + [regex]::Escape($device) + "\tdevice$") })
}

$model = ("{0} {1}" -f (Get-Prop "ro.product.manufacturer"), (Get-Prop "ro.product.model")).Trim()
$folderName = (($model -replace "[^A-Za-z0-9]+", "-").Trim("-")) + "-" + (Get-Date -Format "yyyyMMdd-HHmmss")
$out = Join-Path $repo "out\adb\$folderName"
New-Item -ItemType Directory -Force $out | Out-Null

@(
    "model:        $model",
    "device:       $(Get-Prop 'ro.product.device')",
    "chip:         $(Get-Prop 'ro.soc.manufacturer') $(Get-Prop 'ro.soc.model')",
    "hardware:     $(Get-Prop 'ro.hardware')",
    "platform:     $(Get-Prop 'ro.board.platform')",
    "android:      $(Get-Prop 'ro.build.version.release') (API $(Get-Prop 'ro.build.version.sdk'))",
    "build:        $(Get-Prop 'ro.build.display.id')",
    "abi:          $(Get-Prop 'ro.product.cpu.abilist')",
    "display:      $(((Invoke-Adb shell wm size) -join ' ').Trim())",
    "series:       $(if ($Quick) { 'quick check, not for results' } else { 'formal durations' })",
    "started:      $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
) | Set-Content -Encoding utf8 (Join-Path $out "device.txt")
Get-Content (Join-Path $out "device.txt") | ForEach-Object { Write-Host $_ }

# The app can only come to the front of an unlocked phone. Unlocking is the
# owner's to do; wait for it.
function Test-Locked {
    $state = (Invoke-Adb shell dumpsys window) | Where-Object { $_ -match "isKeyguardShowing=|mDreamingLockscreen=" }
    return [bool]($state -match "isKeyguardShowing=true|mDreamingLockscreen=true")
}
if (Test-Locked) {
    Invoke-Adb shell input keyevent KEYCODE_WAKEUP | Out-Null
    Write-Host "The phone is locked. Unlock it; waiting up to 10 minutes ..."
    $waited = 0
    while ((Test-Locked) -and $waited -lt 600) { Start-Sleep -Seconds 3; $waited += 3 }
    if (Test-Locked) {
        Write-Host "Still locked. Nothing was run."
        exit 2
    }
    Start-Sleep -Seconds 2
}

if ($Install) {
    $apk = Join-Path $repo "android\app\build\outputs\apk\debug\app-debug.apk"
    if (-not (Test-Path $apk)) {
        Write-Host "No APK at $apk. Build it first: cd android; .\gradlew.bat :app:assembleDebug"
        exit 2
    }
    Write-Host "Installing $apk"
    Write-Host "  (a phone that asks whether to install over USB waits about 10 s for the answer)"
    $installed = (Invoke-Adb install -r $apk 2>&1 | ForEach-Object { "$_" }) -join "`n"
    if ($installed -notmatch "Success") {
        Write-Host $installed
        if ($installed -match "INSTALL_FAILED_USER_RESTRICTED") {
            Write-Host "The phone refused the installation. On a Xiaomi phone turn on 'Install via USB'"
            Write-Host "(USB 安装) under Developer options, and confirm the question the phone asks."
        } elseif ($installed -match "INSTALL_FAILED_UPDATE_INCOMPATIBLE") {
            Write-Host "Another build of the app, signed differently, is on the phone. Removing it deletes"
            Write-Host "its saved results, so that is left to you: adb uninstall $package"
        }
        exit 2
    }
    Write-Host "  Success"
}
if (-not ((Invoke-Adb shell pm list packages $package) -join "").Contains($package)) {
    Write-Host "The app is not installed on the phone. Run again with -Install."
    exit 2
}

$gpuSeconds = if ($Quick) { 3 } else { 15 }
$cpuSeconds = if ($Quick) { 1 } else { 15 }
if ($Quick) { $CoolDownSeconds = 0 }

# name, what adb asks the app for, seconds given to the app, and how long to wait for the end.
$tests = @(
    @{ name = "particle-65k";          run = "stream?particles=65536";             seconds = $gpuSeconds; wait = 120 },
    @{ name = "particle-1m";           run = "stream?particles=1048576";           seconds = $gpuSeconds; wait = 120 },
    @{ name = "particle-4m";           run = "stream?particles=4194304";           seconds = $gpuSeconds; wait = 120 },
    @{ name = "particle-16m";          run = "stream?particles=16777216";          seconds = $gpuSeconds; wait = 180 },
    @{ name = "particle-65k-headless"; run = "stream_headless?particles=65536";    seconds = $gpuSeconds; wait = 120 },
    @{ name = "particle-1m-headless";  run = "stream_headless?particles=1048576";  seconds = $gpuSeconds; wait = 120 },
    @{ name = "particle-4m-headless";  run = "stream_headless?particles=4194304";  seconds = $gpuSeconds; wait = 120 },
    @{ name = "particle-16m-headless"; run = "stream_headless?particles=16777216"; seconds = $gpuSeconds; wait = 180 },
    @{ name = "gpu-burn-64";           run = "gpu_burn?steps=64";                  seconds = $gpuSeconds; wait = 180 },
    @{ name = "fluid";                 run = "cinematic_liquid";                   seconds = $gpuSeconds; wait = 240 },
    @{ name = "vram-bandwidth";        run = "membw";                              seconds = 0;           wait = 1800 },
    @{ name = "cpu-compute";           run = "cpu_all";                            seconds = $cpuSeconds; wait = 1800 },
    @{ name = "cpu-cache-memory";      run = "cpu_membw";                          seconds = 0;           wait = 1800 }
)

function Get-Temperature {
    # Tenths of a degree Celsius, as the battery reports it.
    $line = (Invoke-Adb shell dumpsys battery) | Where-Object { $_ -match "temperature:\s*(\d+)" } | Select-Object -First 1
    if ($line -and $line -match "temperature:\s*(\d+)") { return [double]$Matches[1] / 10.0 }
    return $null
}

$summary = @()
$index = 0
foreach ($test in $tests) {
    $index++
    if ($Only -and $test.name -notmatch $Only) { continue }
    $log = Join-Path $out ("{0:D2}-{1}.log" -f $index, $test.name)
    $temperature = Get-Temperature
    Write-Host ("[{0:D2}/{1}] {2,-22} battery {3} C ..." -f $index, $tests.Count, $test.name, $temperature) -NoNewline

    Invoke-Adb shell input keyevent KEYCODE_WAKEUP | Out-Null
    Invoke-Adb logcat -c | Out-Null
    $command = "am start -S -n $package/.MainActivity --es run '$($test.run)'"
    if ($test.seconds -gt 0) { $command += " --ei seconds $($test.seconds)" }
    Invoke-Adb shell $command | Out-Null

    $watch = [Diagnostics.Stopwatch]::StartNew()
    $end = ""
    $accepted = $false
    $lost = $false
    while ($watch.Elapsed.TotalSeconds -lt $test.wait) {
        Start-Sleep -Seconds 2
        if (-not (Test-Device)) {
            # Give a short drop-out half a minute to mend itself.
            $gone = [Diagnostics.Stopwatch]::StartNew()
            while (-not (Test-Device) -and $gone.Elapsed.TotalSeconds -lt 30) { Start-Sleep -Seconds 3 }
            if (-not (Test-Device)) { $lost = $true; break }
        }
        $lines = @(Invoke-Adb logcat -d -s Mangekyo:V)
        if (-not $accepted -and ($lines -match "AUTORUN accepted")) { $accepted = $true }
        $end = $lines | Where-Object { $_ -match "RUN finished|AUTORUN refused|AUTORUN failed" } | Select-Object -Last 1
        if ($end) { break }
        if (-not $accepted -and $watch.Elapsed.TotalSeconds -gt 25) { break }
    }
    if ($lost) {
        Write-Host (" DEVICE LOST after {0:N0} s" -f $watch.Elapsed.TotalSeconds)
        $summary += [pscustomobject]@{
            Test = $test.name; Status = "DEVICE LOST"; Seconds = [int]$watch.Elapsed.TotalSeconds
            BatteryC = $temperature; Result = "adb lost the phone during this test"; Note = ""
        }
        break
    }
    Invoke-Adb logcat -d -s Mangekyo:V MangekyoEngine:V | Set-Content -Encoding utf8 $log
    $engine = Get-Content $log

    $status = "ok"
    $detail = ""
    if (-not $accepted) {
        $status = "NOT STARTED"
        $detail = "the app did not take the request; is the phone unlocked?"
    } elseif (-not $end) {
        $status = "TIMEOUT"
        $detail = "no end after $($test.wait) s"
        Invoke-Adb shell am force-stop $package | Out-Null
    } elseif ($end -match "AUTORUN (refused|failed).*reason=(.*)$") {
        $status = "FAILED"
        $detail = $Matches[2]
    } elseif ($end -match "error=(.*)$") {
        $status = "FAILED"
        $detail = $Matches[1]
    } elseif ($end -match "kind=cpu exit=(\d+)") {
        if ($Matches[1] -ne "0") { $status = "FAILED"; $detail = "exit code $($Matches[1])" }
        $line = $engine | Where-Object { $_ -match "All-core score|CPUMEM_LEVEL\s+mode=single\s+level=memory" } | Select-Object -Last 1
        if ($line -match "All-core score[^:]*:\s*(.*)$") { $detail = "all cores " + $Matches[1].Trim() }
        # The phone moved a thread off the core it was to measure, or took a
        # core away: the engine marks that core invalid and ends with an error.
        $pinning = $engine | Where-Object { $_ -match "CPU_ERROR\s+message=required_affinity_failed\s+invalid_count=(\d+)" } | Select-Object -Last 1
        if ($pinning -match "invalid_count=(\d+)") {
            $status = "INVALID"
            $detail = "$($Matches[1]) core(s) could not be held to their thread; $detail"
        }
        elseif ($line -match "read_gbps=([0-9.]+)") { $detail = "memory read, one thread " + $Matches[1] + " GB/s" }
    } elseif ($end -match "score=(.*)$") {
        $detail = $Matches[1].Trim()
        if ($detail -eq "none") { $status = "NO SCORE" }
    }
    # What the engine says about a run that the display or the CPU held back.
    $verdict = $engine | Where-Object { $_ -match ">> (Display-bound|CPU-bound)" } | Select-Object -Last 1
    $note = ""
    if ($verdict -match ">> Display-bound") { $note = "held at the refresh rate: the score may be too low" }
    $present = $engine | Where-Object { $_ -match "\[Vulkan\] Present mode: (\S+)" } | Select-Object -Last 1
    if ($present -match "Present mode: (\S+)") { $note = (($note, "present $($Matches[1])") | Where-Object { $_ }) -join "; " }

    Write-Host (" {0} in {1:N0} s  {2}" -f $status, $watch.Elapsed.TotalSeconds, $detail)
    $summary += [pscustomobject]@{
        Test = $test.name; Status = $status; Seconds = [int]$watch.Elapsed.TotalSeconds
        BatteryC = $temperature; Result = $detail; Note = $note
    }
    if ($status -eq "NOT STARTED") { break }
    if ($CoolDownSeconds -gt 0) { Start-Sleep -Seconds $CoolDownSeconds }
}

if (Test-Device) {
    Invoke-Adb pull "/sdcard/Android/data/$package/files/results/results.json" (Join-Path $out "results.json") 2>&1 | Out-Null
} else {
    Write-Host "adb has lost the phone (unplugged, frozen or restarted). Reconnect it and look at the phone;"
    Write-Host "the tests that finished are saved on it, and -Only runs the rest."
}
$table = ($summary | Format-Table -AutoSize -Wrap | Out-String -Width 200).TrimEnd()
$table | Set-Content -Encoding utf8 (Join-Path $out "summary.txt")
Write-Host ""
Write-Host $table
Write-Host ""
Write-Host "Collected in $out"
if (-not (Test-Path (Join-Path $out "results.json"))) {
    Write-Host "The results file could not be pulled from the phone."
}
if ($summary | Where-Object { $_.Status -ne "ok" }) { exit 1 }
