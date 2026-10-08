<#
.SYNOPSIS
    Runs the standard series of Mangekyo tests on a HarmonyOS device over hdc
    and brings the results back.

.DESCRIPTION
    The same 29 tests, in the same order and with the same durations, as
    scripts\adb-suite.ps1 runs on an Android device; see there for what they
    are and why. The HarmonyOS app takes the same requests:

      hdc shell aa start -a EntryAbility -b com.mangekyo.benchmark --ps run <test> --pi seconds N

    What is different on HarmonyOS:

      - A real device installs a signed package only. Sign the project once in
        DevEco Studio (File > Project Structure > Signing Configs, with the
        device connected); scripts\build-ohos.ps1 then builds a signed HAP. An
        emulator takes the unsigned one.
      - The shell of a retail device may read very little: no battery
        temperature, no thermal state, no CPU clocks. The script records what
        it can and waits a fixed time between tests.
      - The app is stopped before every test, and the start waits two seconds
        after that: a start sent at once is accepted and then lost.

    Everything it collects goes to out\hdc\<model>-<time>\:

      device.txt     what the device says it is
      NN-<test>.log  what the app and the engine logged for one test
      results.json   the results file of the app, as it is after the series
      summary.txt    one line per test

.PARAMETER Target
    The device to use when more than one is connected (see `hdc list targets`).

.PARAMETER Install
    Installs the HAP that scripts\build-ohos.ps1 built first: the signed one
    where there is one, else the unsigned one.

.PARAMETER Quick
    3 s GPU tests and 1 s CPU steps, no pause between tests. For checking that
    every test starts, for example on an emulator. Not for results.

.PARAMETER Only
    Runs only the tests whose name matches this pattern, e.g. -Only "burn|fluid",
    -Only "^gles" for the OpenGL ES tests, -Only "^(?!gles)" for all the others.

.PARAMETER CoolDownSeconds
    Pause after every test. Default 30. A test the device refuses within a few
    seconds is not followed by a pause.

.EXAMPLE
    .\scripts\hdc-suite.ps1 -Install

.EXAMPLE
    .\scripts\hdc-suite.ps1 -Quick -Only "stream"
#>
param(
    [string]$Target = "",
    [switch]$Install,
    [switch]$Quick,
    [string]$Only = "",
    [int]$CoolDownSeconds = 30
)

$repo = Split-Path -Parent $PSScriptRoot
$package = "com.mangekyo.benchmark"
$hdc = (Get-Command hdc -ErrorAction SilentlyContinue).Source
if (-not $hdc) { $hdc = "C:\Program Files\Huawei\DevEco Studio\sdk\default\openharmony\toolchains\hdc.exe" }
if (-not (Test-Path $hdc)) {
    Write-Host "hdc was not found on PATH or in DevEco Studio."
    exit 2
}

function Get-Targets {
    return @(& $hdc list targets | ForEach-Object { "$_".Trim() } | Where-Object { $_ -and $_ -notmatch "^\[Empty\]" })
}

$targets = Get-Targets
if ($targets.Count -eq 0) {
    Write-Host "No device. Connect it, turn on USB debugging in its developer options, and check `hdc list targets`."
    exit 2
}
if ($targets.Count -gt 1 -and -not $Target) {
    Write-Host "More than one device is connected; name one with -Target:"
    $targets | ForEach-Object { Write-Host "  $_" }
    exit 2
}
if ($Target -and $targets -notcontains $Target) {
    Write-Host "Device '$Target' is not connected. Connected:"
    $targets | ForEach-Object { Write-Host "  $_" }
    exit 2
}
$device = if ($Target) { $Target } else { $targets[0] }

function Invoke-Hdc { & $hdc -t $device @args }

# True while hdc lists the device. Any other command may wait a long time for
# one that has dropped off.
function Test-Device { return (Get-Targets) -contains $device }

# A system parameter, or nothing where the shell may not read it.
function Get-Param([string]$name) {
    $value = ((Invoke-Hdc shell param get $name 2>&1) -join "").Trim()
    if ($value -match "fail!") { return "" }
    return $value
}

$model = ("{0} {1}" -f (Get-Param "const.product.brand"), (Get-Param "const.product.model")).Trim()
if (-not $model) { $model = "HarmonyOS-device" }
$folderName = (($model -replace "[^A-Za-z0-9]+", "-").Trim("-")) + "-" + (Get-Date -Format "yyyyMMdd-HHmmss")
$out = Join-Path $repo "out\hdc\$folderName"
New-Item -ItemType Directory -Force $out | Out-Null

@(
    "model:        $model",
    "name:         $(Get-Param 'const.product.name')",
    "type:         $(Get-Param 'const.product.devicetype')",
    "system:       $(Get-Param 'const.product.software.version')",
    "openharmony:  $(Get-Param 'const.ohos.fullname') (API $(Get-Param 'const.ohos.apiversion'))",
    "abi:          $(Get-Param 'const.product.cpu.abilist')",
    "processors:   $(((Invoke-Hdc shell 'nproc 2>/dev/null || getconf _NPROCESSORS_ONLN' 2>&1) -join '').Trim()) (as the shell sees them)",
    "memory:       $(((Invoke-Hdc shell 'grep -E MemTotal /proc/meminfo' 2>&1) -join '').Trim())",
    "kernel:       $(((Invoke-Hdc shell uname -a 2>&1) -join '').Trim())",
    "series:       $(if ($Quick) { 'quick check, not for results' } else { 'formal durations' })",
    "started:      $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
) | Set-Content -Encoding utf8 (Join-Path $out "device.txt")
Get-Content (Join-Path $out "device.txt") | ForEach-Object { Write-Host $_ }

if ($Install) {
    $outputs = Join-Path $repo "ohos\entry\build\default\outputs\default"
    $hap = Join-Path $outputs "entry-default-signed.hap"
    if (-not (Test-Path $hap)) { $hap = Join-Path $outputs "entry-default-unsigned.hap" }
    if (-not (Test-Path $hap)) {
        Write-Host "No HAP in $outputs. Build it first: .\scripts\build-ohos.ps1"
        exit 2
    }
    Write-Host "Installing $hap"
    # hdc takes a path relative to where it runs; an absolute one is treated the same way.
    Push-Location (Split-Path -Parent $hap)
    $installed = (Invoke-Hdc install -r (Split-Path -Leaf $hap) 2>&1 | ForEach-Object { "$_" }) -join "`n"
    Pop-Location
    if ($installed -notmatch "install bundle successfully") {
        Write-Host $installed
        if ($installed -match "no signature file|9568320") {
            Write-Host "A real device installs a signed package only. Open ohos\ in DevEco Studio with the device"
            Write-Host "connected, sign in under File > Project Structure > Signing Configs and let it generate a"
            Write-Host "signature; then build again with .\scripts\build-ohos.ps1."
        }
        exit 2
    }
    Write-Host "  Success"
}
if (((Invoke-Hdc shell bm dump -n $package 2>&1) -join "") -notmatch [regex]::Escape($package)) {
    Write-Host "The app is not installed on the device. Run again with -Install."
    exit 2
}

$gpuSeconds = if ($Quick) { 3 } else { 15 }
$cpuSeconds = if ($Quick) { 1 } else { 15 }
if ($Quick) { $CoolDownSeconds = 0 }

# name, what hdc asks the app for, seconds given to the app, and how long to wait for the end.
# The list of scripts\adb-suite.ps1: keep the two the same.
$tests = @(
    @{ name = "particle-65k";          run = "stream?particles=65536";             seconds = $gpuSeconds; wait = 120 },
    @{ name = "particle-1m";           run = "stream?particles=1048576";           seconds = $gpuSeconds; wait = 120 },
    @{ name = "particle-4m";           run = "stream?particles=4194304";           seconds = $gpuSeconds; wait = 120 },
    @{ name = "particle-16m";          run = "stream?particles=16777216";          seconds = $gpuSeconds; wait = 180 },
    @{ name = "particle-65k-headless"; run = "stream_headless?particles=65536";    seconds = $gpuSeconds; wait = 120 },
    @{ name = "particle-1m-headless";  run = "stream_headless?particles=1048576";  seconds = $gpuSeconds; wait = 120 },
    @{ name = "particle-4m-headless";  run = "stream_headless?particles=4194304";  seconds = $gpuSeconds; wait = 120 },
    @{ name = "particle-16m-headless"; run = "stream_headless?particles=16777216"; seconds = $gpuSeconds; wait = 180 },
    @{ name = "gpu-burn-16";           run = "gpu_burn?steps=16";                  seconds = $gpuSeconds; wait = 180 },
    @{ name = "gpu-burn-64";           run = "gpu_burn?steps=64";                  seconds = $gpuSeconds; wait = 180 },
    @{ name = "gpu-burn-256";          run = "gpu_burn?steps=256";                 seconds = $gpuSeconds; wait = 240 },
    @{ name = "fluid";                 run = "cinematic_liquid";                   seconds = $gpuSeconds; wait = 240 },
    @{ name = "vram-bandwidth";        run = "membw";                              seconds = 0;           wait = 1800 },
    @{ name = "gles-particle-65k";          run = "stream?particles=65536&api=gles";             seconds = $gpuSeconds; wait = 120 },
    @{ name = "gles-particle-1m";           run = "stream?particles=1048576&api=gles";           seconds = $gpuSeconds; wait = 120 },
    @{ name = "gles-particle-4m";           run = "stream?particles=4194304&api=gles";           seconds = $gpuSeconds; wait = 120 },
    @{ name = "gles-particle-16m";          run = "stream?particles=16777216&api=gles";          seconds = $gpuSeconds; wait = 180 },
    @{ name = "gles-particle-65k-headless"; run = "stream_headless?particles=65536&api=gles";    seconds = $gpuSeconds; wait = 120 },
    @{ name = "gles-particle-1m-headless";  run = "stream_headless?particles=1048576&api=gles";  seconds = $gpuSeconds; wait = 120 },
    @{ name = "gles-particle-4m-headless";  run = "stream_headless?particles=4194304&api=gles";  seconds = $gpuSeconds; wait = 120 },
    @{ name = "gles-particle-16m-headless"; run = "stream_headless?particles=16777216&api=gles"; seconds = $gpuSeconds; wait = 180 },
    @{ name = "gles-gpu-burn-16";           run = "gpu_burn?steps=16&api=gles";                  seconds = $gpuSeconds; wait = 180 },
    @{ name = "gles-gpu-burn-64";           run = "gpu_burn?steps=64&api=gles";                  seconds = $gpuSeconds; wait = 180 },
    @{ name = "gles-gpu-burn-256";          run = "gpu_burn?steps=256&api=gles";                 seconds = $gpuSeconds; wait = 240 },
    @{ name = "gles2-gpu-burn-16";          run = "gpu_burn?steps=16&api=gles2";                 seconds = $gpuSeconds; wait = 180 },
    @{ name = "gles2-gpu-burn-64";          run = "gpu_burn?steps=64&api=gles2";                 seconds = $gpuSeconds; wait = 180 },
    @{ name = "gles2-gpu-burn-256";         run = "gpu_burn?steps=256&api=gles2";                seconds = $gpuSeconds; wait = 240 },
    @{ name = "cpu-compute";           run = "cpu_all";                            seconds = $cpuSeconds; wait = 1800 },
    @{ name = "cpu-cache-memory";      run = "cpu_membw";                          seconds = 0;           wait = 1800 }
)

# The app's own lines of the log, as the device has them now.
function Get-AppLog {
    return @(Invoke-Hdc shell hilog -x -T "Mangekyo,MangekyoEngine" 2>&1 | ForEach-Object { "$_" })
}

$summary = @()
$index = 0
foreach ($test in $tests) {
    $index++
    if ($Only -and $test.name -notmatch $Only) { continue }
    $log = Join-Path $out ("{0:D2}-{1}.log" -f $index, $test.name)
    Write-Host ("[{0:D2}/{1}] {2,-26} ..." -f $index, $tests.Count, $test.name) -NoNewline

    Invoke-Hdc shell power-shell wakeup 2>&1 | Out-Null
    Invoke-Hdc shell aa force-stop $package 2>&1 | Out-Null
    Invoke-Hdc shell hilog -r 2>&1 | Out-Null
    # A start sent straight after the stop is accepted and then lost.
    Start-Sleep -Seconds 2
    # The app's lines go to the file as they come: the log buffer of a device is
    # small (512 KiB for all apps on a MateBook Pro) and a long test overruns it.
    $capture = Start-Process $hdc -ArgumentList @("-t", $device, "shell", "hilog", "-T", "Mangekyo,MangekyoEngine") `
        -RedirectStandardOutput $log -WindowStyle Hidden -PassThru
    $command = "aa start -a EntryAbility -b $package --ps run '$($test.run)'"
    if ($test.seconds -gt 0) { $command += " --pi seconds $($test.seconds)" }
    Invoke-Hdc shell $command 2>&1 | Out-Null

    $watch = [Diagnostics.Stopwatch]::StartNew()
    $end = ""
    $accepted = $false
    $lost = $false
    while ($watch.Elapsed.TotalSeconds -lt $test.wait) {
        Start-Sleep -Seconds 2
        if (-not (Test-Device)) {
            $gone = [Diagnostics.Stopwatch]::StartNew()
            while (-not (Test-Device) -and $gone.Elapsed.TotalSeconds -lt 75) { Start-Sleep -Seconds 3 }
            if (-not (Test-Device)) { $lost = $true; break }
        }
        $lines = @(Get-Content $log -Encoding UTF8 -ErrorAction SilentlyContinue)
        # The capture ends when hdc drops out; what the buffer still holds then
        # is the only way to see the end of the test.
        if ($capture.HasExited) { $lines += Get-AppLog }
        if (-not $accepted -and ($lines -match "AUTORUN accepted")) { $accepted = $true }
        $end = $lines | Where-Object { $_ -match "RUN finished|AUTORUN refused|AUTORUN failed" } | Select-Object -Last 1
        if ($end) { break }
        if (-not $accepted -and $watch.Elapsed.TotalSeconds -gt 25) { break }
    }
    $captureLost = $capture.HasExited
    if (-not $captureLost) { Stop-Process -Id $capture.Id -Force -ErrorAction SilentlyContinue }
    $capture.WaitForExit(5000) | Out-Null
    if ($lost) {
        Write-Host (" DEVICE LOST after {0:N0} s" -f $watch.Elapsed.TotalSeconds)
        $summary += [pscustomobject]@{
            Test = $test.name; Status = "DEVICE LOST"; Seconds = [int]$watch.Elapsed.TotalSeconds
            Result = "hdc lost the device during this test"; Note = ""
        }
        break
    }
    if ($captureLost) { Get-AppLog | Add-Content -Encoding utf8 $log }
    $engine = @(Get-Content $log -Encoding UTF8 -ErrorAction SilentlyContinue)

    $status = "ok"
    $detail = ""
    if (-not $accepted) {
        $status = "NOT STARTED"
        $detail = "the app did not take the request; is the device unlocked and its screen on?"
    } elseif (-not $end) {
        $status = "TIMEOUT"
        $detail = "no end after $($test.wait) s"
        Invoke-Hdc shell aa force-stop $package 2>&1 | Out-Null
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
        # The device moved a thread off the core it was to measure, or took a
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
    $present = $engine | Where-Object { $_ -match "\[(?:Vulkan|OpenGL ES)\] Present mode: (\S+)" } | Select-Object -Last 1
    if ($present -match "Present mode: (\S+)") { $note = (($note, "present $($Matches[1])") | Where-Object { $_ }) -join "; " }
    # Without a usable GPU clock a frame is timed from the CPU side.
    if ($engine | Where-Object { $_ -match "using synchronized wall-clock timing|timed by wall clock" }) {
        $note = (($note, "timed by wall clock") | Where-Object { $_ }) -join "; "
    }
    # The system log drops lines of an app that writes many at once.
    if ($engine | Where-Object { $_ -match "lines dropped|log is dropped|flow control" }) {
        $note = (($note, "the system log dropped lines of this test") | Where-Object { $_ }) -join "; "
    }

    Write-Host (" {0} in {1:N0} s  {2}" -f $status, $watch.Elapsed.TotalSeconds, $detail)
    $summary += [pscustomobject]@{
        Test = $test.name; Status = $status; Seconds = [int]$watch.Elapsed.TotalSeconds
        Result = $detail; Note = $note
    }
    if ($status -eq "NOT STARTED") { break }
    # A test the device refused at once did no work: there is nothing to cool.
    $refusedAtOnce = $status -eq "FAILED" -and $watch.Elapsed.TotalSeconds -lt 6
    if ($CoolDownSeconds -gt 0 -and -not $refusedAtOnce) { Start-Sleep -Seconds $CoolDownSeconds }
}

$results = Join-Path $out "results.json"
if (Test-Device) {
    # The files of a debug build are open to hdc by the app's name; an emulator
    # also gives the whole path.
    Push-Location $out
    Invoke-Hdc file recv -b $package "./data/storage/el2/base/haps/entry/files/results/results.json" "results.json" 2>&1 | Out-Null
    if (-not (Test-Path $results)) {
        Invoke-Hdc file recv "/data/app/el2/100/base/$package/haps/entry/files/results/results.json" "results.json" 2>&1 | Out-Null
    }
    Pop-Location
} else {
    Write-Host "hdc has lost the device (unplugged, frozen or restarted). Reconnect it and look at it;"
    Write-Host "the tests that finished are saved on it, and -Only runs the rest."
}
$table = ($summary | Format-Table -AutoSize -Wrap | Out-String -Width 200).TrimEnd()
$table | Set-Content -Encoding utf8 (Join-Path $out "summary.txt")
Write-Host ""
Write-Host $table
Write-Host ""
Write-Host "Collected in $out"
if (-not (Test-Path $results)) {
    Write-Host "The results file could not be pulled from the device."
}
if ($summary | Where-Object { $_.Status -ne "ok" }) { exit 1 }
