<#
.SYNOPSIS
    Runs the desktop series of Mangekyo tests on a Linux machine over SSH and
    brings the results back.

.DESCRIPTION
    For the test machines installed from scripts\linux\autoinstall.template.yaml
    (key login, passwordless sudo, GNOME logged in by itself), but any Linux
    machine with the build packages and a logged-in desktop will do.

      1. Sends the source as it is in this checkout, uncommitted and new files
         included, to ~/mangekyo on the machine. Only files whose content
         changed are replaced, so the next build is incremental.
      2. Builds it there (scripts/linux/build.sh) and runs the result-file test.
      3. Starts scripts/linux/series.sh detached from the SSH session: for every
         hardware GPU the particle test at its four sizes shown and headless,
         GPU Burn at 16 / 64 / 256 steps, the fluid and the VRAM bandwidth on
         Vulkan; the particle test and GPU Burn on OpenGL; then the two CPU
         jobs. A pause follows every job.
      4. Shows each finished job as it comes and, when the series is done,
         copies the run folder to out\linux\<name>-series-<time>\:

      state.txt      the machine, its drivers and GPUs at the start
      build.log      the build and the test
      NN-<job>.log   what the engine printed for one job
      telemetry.csv  CPU temperature and clock, GPU clock every 2 s
      results.json   the results file of the machine after the series
      summary.txt    two lines per job

    The series keeps running if this script or the network stops; run the
    script again with -Attach <run> to follow it and fetch the results.

.PARAMETER Target
    user@host of the machine, e.g. fairchild@mk-ea0ffb.local.

.PARAMETER Name
    Start of the run's folder name. Default: the machine's host name.

.PARAMETER Only
    Runs only the jobs whose name matches this extended regular expression,
    e.g. -Only "vulkan-gpu-burn|cpu". Job names are <gpu>-<api>-<test>.

.PARAMETER CoolDownSeconds
    Pause after every job. Default 30.

.PARAMETER SkipBuild
    Uses the build already on the machine; sends nothing.

.PARAMETER BuildOnly
    Sends the source and builds it, then stops; runs no job.

.PARAMETER Env
    Environment variables for every job, e.g. -Env "MESA_GL_VERSION_OVERRIDE=4.3"
    (space-separated NAME=value pairs, no quotes inside). They are listed in
    state.txt.

.PARAMETER Attach
    Follows a run that is already going (its folder name, as printed when it
    started) and fetches it when it is done. Sends, builds and starts nothing.

.EXAMPLE
    .\scripts\linux-suite.ps1 fairchild@mk-ea0ffb.local

.EXAMPLE
    .\scripts\linux-suite.ps1 fairchild@mk-ea0ffb.local -Attach mk-ea0ffb-series-20261021-101500
#>
param(
    [Parameter(Mandatory = $true, Position = 0)][string]$Target,
    [string]$Name = "",
    [string]$Only = "",
    [int]$CoolDownSeconds = 30,
    [switch]$SkipBuild,
    [switch]$BuildOnly,
    [string]$Env = "",
    [string]$Attach = ""
)

$repo = Split-Path -Parent $PSScriptRoot
$sshOptions = @("-o", "BatchMode=yes", "-o", "ConnectTimeout=15", "-o", "ServerAliveInterval=20", "-o", "LogLevel=ERROR")
if ($Only -match "'") { Write-Host "-Only cannot contain a single quote."; exit 2 }
if ($Env -and $Env -notmatch '^([A-Za-z_][A-Za-z0-9_]*=[^\s''"]*\s*)+$') { Write-Host "-Env takes NAME=value pairs without quotes."; exit 2 }

# Runs one command on the machine. Remote commands are written with single
# quotes only: Windows PowerShell drops double quotes inside native arguments.
function Invoke-Remote([string]$command) { & ssh @sshOptions $Target $command }

$hostName = (Invoke-Remote "hostname") -join ""
if ($LASTEXITCODE -ne 0 -or -not $hostName) {
    Write-Host "Cannot log in to $Target with the SSH key. Check the name (ping it) and"
    Write-Host "that the key is in ~/.ssh/authorized_keys on the machine."
    exit 2
}
Write-Host "Connected to $hostName."

if ($Attach) {
    $run = $Attach
} else {
    $busy = (Invoke-Remote "pgrep -f mangekyo/scripts/linux/series.sh") -join " "
    if ($busy) { Write-Host "A series is already running on $hostName (pid $busy); follow it with -Attach."; exit 2 }

    $session = (Invoke-Remote 'ls /run/user/$(id -u)/wayland-0 /tmp/.X11-unix/X0 2>/dev/null') -join " "
    if (-not $session) {
        Write-Host "Warning: no desktop session is running on $hostName; the windowed jobs will fail."
        Write-Host "Log in on its screen (or set up the automatic login) first."
    }

    if (-not $Name) { $Name = $hostName }
    $run = (($Name -replace "[^A-Za-z0-9]+", "-").Trim("-")) + "-series-" + (Get-Date -Format "yyyyMMdd-HHmmss")
    Invoke-Remote "mkdir -p mangekyo-runs/$run" | Out-Null

    if (-not $SkipBuild) {
        # A tree of this checkout as it is: tracked files with their edits plus
        # new files that are not ignored, in the folders the build needs.
        $paths = @("CMakeLists.txt", "cmake", "src", "shaders", "tests", "third_party", "assets", "packaging", "scripts/linux")
        $index = Join-Path $env:TEMP "mangekyo-linux-index"
        $tar = Join-Path $env:TEMP "mangekyo-linux-src.tar"
        Remove-Item $index, $tar -ErrorAction SilentlyContinue
        Push-Location $repo
        try {
            $env:GIT_INDEX_FILE = $index
            git read-tree HEAD
            git -c core.safecrlf=false add -A -- @paths
            $tree = (git write-tree) -join ""
        } finally {
            Remove-Item Env:\GIT_INDEX_FILE
        }
        # LF line endings whatever core.autocrlf says: the shell scripts run on Linux.
        git -c core.autocrlf=false -c core.eol=lf archive --format=tar -o $tar $tree @paths
        $commit = (git rev-parse --short HEAD) -join ""
        $source = if ($tree -eq ((git rev-parse "HEAD^{tree}") -join "")) { $commit } else { "$commit with uncommitted changes (tree $($tree.Substring(0, 10)))" }
        Pop-Location
        Remove-Item $index -ErrorAction SilentlyContinue

        Write-Host "Sending the source ($source)..."
        & scp -q @sshOptions $tar "${Target}:mangekyo-src.tar"
        if ($LASTEXITCODE -ne 0) { Write-Host "Copying the source failed."; exit 1 }
        Remove-Item $tar
        # rsync -c replaces only files whose content changed, so make rebuilds
        # only what changed; build/ stays.
        Invoke-Remote ("rm -rf mangekyo-src && mkdir -p mangekyo-src mangekyo && tar -xf mangekyo-src.tar -C mangekyo-src && " +
            "rsync -rlc --delete --exclude=/build --exclude=/SOURCE-COMMIT mangekyo-src/ mangekyo/ && " +
            "rm -rf mangekyo-src mangekyo-src.tar && echo '$source' > mangekyo/SOURCE-COMMIT")
        if ($LASTEXITCODE -ne 0) { Write-Host "Unpacking the source failed."; exit 1 }

        Write-Host "Building on $hostName..."
        Invoke-Remote "sh mangekyo/scripts/linux/build.sh > mangekyo-runs/$run/build.log 2>&1"
        if ($LASTEXITCODE -ne 0) {
            Write-Host "The build or its test failed. The end of build.log:"
            Invoke-Remote "tail -n 30 mangekyo-runs/$run/build.log"
            exit 1
        }
        Write-Host "Built; the result-file test passed."
        if ($BuildOnly) {
            Invoke-Remote "rm -rf mangekyo-runs/$run" | Out-Null
            exit 0
        }
    }

    $dir = "mangekyo-runs/$run"
    Invoke-Remote ("$Env COOL=$CoolDownSeconds setsid nohup sh mangekyo/scripts/linux/series.sh `$HOME/$dir '$Only' " +
        "> $dir/series.out 2>&1 < /dev/null & echo `$! > $dir/series.pid")
    if ($LASTEXITCODE -ne 0) { Write-Host "Starting the series failed."; exit 1 }
    Write-Host "Series started: $run"
    Write-Host "If this window closes, follow it again with -Attach $run"
}

# Follow summary.txt until the series says it is done or its process is gone.
$dir = "mangekyo-runs/$run"
$shown = 0
$unreachableSince = $null
while ($true) {
    $lines = @(Invoke-Remote "cat $dir/summary.txt 2>/dev/null; kill -0 `$(cat $dir/series.pid) 2>/dev/null && echo '#RUNNING'")
    if ($LASTEXITCODE -eq 255) {
        if (-not $unreachableSince) { $unreachableSince = Get-Date; Write-Host "$hostName does not answer; still trying..." }
        elseif (((Get-Date) - $unreachableSince).TotalMinutes -gt 30) { Write-Host "$hostName has not answered for 30 minutes; stopping. Use -Attach $run later."; exit 1 }
        Start-Sleep -Seconds 30
        continue
    }
    $unreachableSince = $null
    $summary = @($lines | Where-Object { $_ -ne "#RUNNING" })
    for ($i = $shown; $i -lt $summary.Count; $i++) { Write-Host $summary[$i] }
    $shown = $summary.Count
    if ($summary -match "^SERIES DONE") { break }
    if (-not ($lines -contains "#RUNNING")) {
        Write-Host "The series stopped before it was done. The end of series.out:"
        Invoke-Remote "tail -n 20 $dir/series.out"
        break
    }
    Start-Sleep -Seconds 30
}

$localParent = Join-Path $repo "out\linux"
New-Item -ItemType Directory -Force $localParent | Out-Null
& scp -q -r @sshOptions "${Target}:$dir" $localParent
if ($LASTEXITCODE -ne 0) { Write-Host "Fetching the results failed; they are in ~/$dir on $hostName."; exit 1 }
Write-Host "Results: $(Join-Path $localParent $run)"
