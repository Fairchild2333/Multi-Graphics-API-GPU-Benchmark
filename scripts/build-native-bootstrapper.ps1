[CmdletBinding()]
param(
    [ValidateSet('x64', 'ARM64', 'Both')]
    [string]$Arch = 'Both',
    [string]$Version,
    [string]$X64Msi,
    [string]$Arm64Msi,
    [string]$OutputDir,
    [string]$SignToolCommand,
    [switch]$RequireSigned
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0

$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $OutputDir) { $OutputDir = Join-Path $projectRoot 'out/installer' }
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$source = Join-Path $projectRoot 'installer/native-bootstrapper/main.cpp'
if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
    throw "Native bootstrapper source was not found: $source"
}

if (-not $Version) {
    $cmake = Get-Content -LiteralPath (Join-Path $projectRoot 'CMakeLists.txt') -Raw
    $match = [regex]::Match($cmake, 'project\([^\)]*VERSION\s+([0-9]+(?:\.[0-9]+){2,3})', 'IgnoreCase')
    if (-not $match.Success) { throw 'Could not determine the project version from CMakeLists.txt.' }
    $Version = $match.Groups[1].Value
}
if ($Version -notmatch '^\d+\.\d+\.\d+(?:\.\d+)?$') {
    throw "Installer version '$Version' must contain three or four numeric components."
}

if (-not $X64Msi) { $X64Msi = Join-Path $OutputDir "Mangekyo-$Version-windows-x64.msi" }
if (-not $Arm64Msi) { $Arm64Msi = Join-Path $OutputDir "Mangekyo-$Version-windows-arm64.msi" }

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
    throw 'vswhere.exe was not found; install Visual Studio Build Tools with the MSVC x64 and ARM64 tools.'
}
$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.ARM64 -property installationPath
if (-not $visualStudio) {
    throw 'Visual Studio with the MSVC ARM64 tools was not found.'
}
$vcvars = Join-Path $visualStudio 'VC/Auxiliary/Build/vcvarsall.bat'
if (-not (Test-Path -LiteralPath $vcvars -PathType Leaf)) { throw "vcvarsall.bat was not found: $vcvars" }

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$buildDir = Join-Path $projectRoot 'out/build/native-bootstrapper'
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
$versionParts = @($Version.Split('.') | ForEach-Object { [int]$_ })
while ($versionParts.Count -lt 4) { $versionParts += 0 }
$versionCommas = ($versionParts[0..3] -join ',')
$resourceSource = Join-Path $buildDir 'bootstrapper-version.rc'
$manifestSource = Join-Path $buildDir 'bootstrapper.manifest'
$iconPath = [IO.Path]::GetFullPath((Join-Path $projectRoot 'gui/app.ico'))
if (-not (Test-Path -LiteralPath $iconPath -PathType Leaf)) { throw "Application icon was not found: $iconPath" }
$resourceIconPath = $iconPath.Replace('\', '\\')
$manifestText = @"
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity version="$($versionParts[0]).$($versionParts[1]).$($versionParts[2]).$($versionParts[3])"
                    processorArchitecture="*" name="Mangekyo.Setup" type="win32" />
  <description>Mangekyo multilingual setup</description>
  <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">
    <security>
      <requestedPrivileges>
        <requestedExecutionLevel level="asInvoker" uiAccess="false" />
      </requestedPrivileges>
    </security>
  </trustInfo>
  <dependency>
    <dependentAssembly>
      <assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls" version="6.0.0.0"
                        processorArchitecture="*" publicKeyToken="6595b64144ccf1df" language="*" />
    </dependentAssembly>
  </dependency>
  <application xmlns="urn:schemas-microsoft-com:asm.v3">
    <windowsSettings>
      <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/pm</dpiAware>
      <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2,PerMonitor</dpiAwareness>
      <longPathAware xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">true</longPathAware>
    </windowsSettings>
  </application>
</assembly>
"@
Set-Content -LiteralPath $manifestSource -Value $manifestText -Encoding utf8
$resourceManifestPath = $manifestSource.Replace('\', '\\')
$resourceText = @"
#include <windows.h>
101 ICON "$resourceIconPath"
1 24 "$resourceManifestPath"
VS_VERSION_INFO VERSIONINFO
 FILEVERSION $versionCommas
 PRODUCTVERSION $versionCommas
 FILEFLAGSMASK 0x3fL
 FILEFLAGS 0x0L
 FILEOS VOS_NT_WINDOWS32
 FILETYPE VFT_APP
 FILESUBTYPE VFT2_UNKNOWN
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904B0"
    BEGIN
      VALUE "CompanyName", "Mangekyo contributors"
      VALUE "FileDescription", "Mangekyo multilingual setup"
      VALUE "FileVersion", "$Version"
      VALUE "InternalName", "Mangekyo Setup"
      VALUE "OriginalFilename", "Mangekyo Setup.exe"
      VALUE "ProductName", "Mangekyo"
      VALUE "ProductVersion", "$Version"
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x0409, 1200
  END
END
"@
Set-Content -LiteralPath $resourceSource -Value $resourceText -Encoding utf8

function Get-PeMachine([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $reader = [IO.BinaryReader]::new($stream)
        $stream.Position = 0x3c
        $peOffset = $reader.ReadInt32()
        $stream.Position = $peOffset + 4
        return $reader.ReadUInt16()
    } finally {
        $stream.Dispose()
    }
}

function Add-MsiOverlay([string]$Stub, [string]$Msi, [string]$Output) {
    $magic = [Text.Encoding]::ASCII.GetBytes("MANGEKYO_MSI_V1`0")
    if ($magic.Length -ne 16) { throw 'Internal payload footer magic must be 16 bytes.' }
    $input = [IO.File]::OpenRead($Msi)
    $stubInput = [IO.File]::OpenRead($Stub)
    $outputStream = [IO.File]::Create($Output)
    try {
        $stubInput.CopyTo($outputStream)
        $input.CopyTo($outputStream)
        $outputStream.Write($magic, 0, $magic.Length)
        $sizeBytes = [BitConverter]::GetBytes([UInt64]$input.Length)
        $outputStream.Write($sizeBytes, 0, $sizeBytes.Length)
    } finally {
        $outputStream.Dispose()
        $stubInput.Dispose()
        $input.Dispose()
    }
}

function Test-MsiOverlay([string]$Installer, [string]$Msi) {
    $stream = [IO.File]::OpenRead($Installer)
    try {
        $reader = [IO.BinaryReader]::new($stream)
        $stream.Position = $stream.Length - 24
        $magic = [Text.Encoding]::ASCII.GetString($reader.ReadBytes(16))
        $payloadSize = $reader.ReadUInt64()
        if ($magic -ne "MANGEKYO_MSI_V1`0" -or $payloadSize -ne (Get-Item -LiteralPath $Msi).Length) {
            throw "Embedded MSI footer validation failed: $Installer"
        }
        $stream.Position = $stream.Length - 24 - $payloadSize
        $oleHeader = $reader.ReadBytes(8)
        $expected = [byte[]](0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1)
        if ([BitConverter]::ToString($oleHeader) -ne [BitConverter]::ToString($expected)) {
            throw "Embedded payload is not a Windows Installer database: $Installer"
        }
    } finally {
        $stream.Dispose()
    }
}

function Build-One([string]$Name, [string]$VcArch, [UInt16]$ExpectedMachine, [string]$Msi) {
    $Msi = [IO.Path]::GetFullPath($Msi)
    if (-not (Test-Path -LiteralPath $Msi -PathType Leaf)) { throw "MSI input was not found: $Msi" }
    $stub = Join-Path $buildDir "Mangekyo-setup-$Name-stub.exe"
    $object = Join-Path $buildDir "Mangekyo-setup-$Name.obj"
    $resource = Join-Path $buildDir "Mangekyo-setup-$Name.res"
    $output = Join-Path $OutputDir "Mangekyo-$Version-windows-$Name-setup.exe"
    $compile = 'call "{0}" {1} >nul && cl.exe /nologo /utf-8 /std:c++20 /O2 /EHsc /DUNICODE /D_UNICODE /c "{2}" /Fo"{3}" && rc.exe /nologo /fo"{4}" "{5}" && link.exe /nologo /subsystem:windows /entry:wWinMainCRTStartup /out:"{6}" "{3}" "{4}" user32.lib gdi32.lib shell32.lib shlwapi.lib ole32.lib comctl32.lib advapi32.lib dwmapi.lib uxtheme.lib' -f $vcvars, $VcArch, $source, $object, $resource, $resourceSource, $stub
    Write-Host "> build native $Name bootstrapper" -ForegroundColor DarkGray
    cmd.exe /d /s /c $compile
    if ($LASTEXITCODE -ne 0) { throw "Native $Name bootstrapper compilation failed with exit code $LASTEXITCODE" }
    $machine = Get-PeMachine $stub
    if ($machine -ne $ExpectedMachine) {
        throw ('Native {0} bootstrapper has PE machine 0x{1:x4}, expected 0x{2:x4}.' -f $Name, $machine, $ExpectedMachine)
    }
    Add-MsiOverlay $stub $Msi $output
    Test-MsiOverlay $output $Msi
    if ((Get-PeMachine $output) -ne $ExpectedMachine) { throw "Appending the MSI changed the $Name PE architecture." }

    if ($SignToolCommand) {
        if ($SignToolCommand -notmatch '\$f') { throw 'The sign-tool command must contain the $f file-name placeholder.' }
        $sign = $SignToolCommand.Replace('$f', $output)
        Write-Host "> $sign" -ForegroundColor DarkGray
        cmd.exe /c $sign
        if ($LASTEXITCODE -ne 0) { throw "SignTool failed with exit code $LASTEXITCODE" }
    }
    $signature = (Get-AuthenticodeSignature -LiteralPath $output).Status.ToString()
    if ($RequireSigned -and $signature -ne 'Valid') { throw "RequireSigned was set but $output is $signature" }
    $hash = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash.ToLowerInvariant()
    Set-Content -LiteralPath ($output + '.sha256') -Value "$hash  $([IO.Path]::GetFileName($output))" -Encoding ascii
    Write-Host "Native $Name setup: $output" -ForegroundColor Green
    Write-Host "SHA256: $hash"
    Write-Host "Signature: $signature"
}

if ($Arch -in 'x64', 'Both') { Build-One 'x64' 'amd64' 0x8664 $X64Msi }
if ($Arch -in 'ARM64', 'Both') { Build-One 'arm64' 'amd64_arm64' 0xAA64 $Arm64Msi }
