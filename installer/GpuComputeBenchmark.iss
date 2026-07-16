; Mangekyo - Windows engineering installer
; Input is an already verified CMake stage. This script never discovers or
; copies dependencies from the build machine.

#define MyAppName "Mangekyo"
#define MyAppExeName "gpu_bench_gui.exe"
#define MyAppId "{{9DBD8675-1CE2-45DF-83BB-2E62EB71796B}"

#ifndef MyAppArch
  #define MyAppArch "x64"
#endif

#if MyAppArch == "arm64"
  #define MyAppArchAllowed "arm64"
#else
  #define MyAppArchAllowed "x64compatible"
#endif

#ifndef MyAppVersion
  #define MyAppVersion "0.1.1"
#endif
#ifndef StageDir
  #define StageDir AddBackslash(SourcePath) + "..\out\stage\windows-" + MyAppArch
#endif
#ifndef OutputDir
  #define OutputDir AddBackslash(SourcePath) + "..\out\installer"
#endif
#ifndef AllowCliOnly
  #define AllowCliOnly "0"
#endif
#ifndef EnableSigning
  #define EnableSigning "0"
#endif

#ifnexist StageDir + "\release-manifest.json"
  #error The staged release-manifest.json is missing. Run stage-windows-release.ps1 first.
#endif
#ifnexist StageDir + "\app\bin\gpu_benchmark.exe"
  #error The staged CLI is missing.
#endif
#if AllowCliOnly == "0"
  #ifnexist StageDir + "\app\bin\gpu_bench_gui.exe"
    #error The GUI-first installer requires app\bin\gpu_bench_gui.exe. Use AllowCliOnly=1 only for engineering smoke packages.
  #endif
#endif
#ifnexist StageDir + "\app\bin\glfw3.dll"
  #error The staged GLFW runtime is missing.
#endif
#ifnexist StageDir + "\app\bin\vcruntime140.dll"
  #error The staged MSVC runtime is missing.
#endif
#ifnexist StageDir + "\PACKAGE_LIMITATIONS.md"
  #error PACKAGE_LIMITATIONS.md must accompany this engineering installer.
#endif
#ifnexist StageDir + "\licenses\THIRD_PARTY_NOTICES.md"
  #error Third-party notices are missing from the stage.
#endif
#ifnexist StageDir + "\files.sha256"
  #error The staged per-file SHA-256 inventory is missing.
#endif

#define HasGui FileExists(StageDir + "\app\bin\gpu_bench_gui.exe")
#define HasRenderDocFiles FileExists(StageDir + "\tools\RenderDoc\renderdoccmd.exe") && FileExists(StageDir + "\tools\RenderDoc\qrenderdoc.exe") && FileExists(StageDir + "\tools\RenderDoc\renderdoc.dll") && FileExists(StageDir + "\tools\RenderDoc\renderdoc.json")
#define HasRenderDocLicense FileExists(StageDir + "\tools\RenderDoc\LICENSE.rtf") || FileExists(StageDir + "\tools\RenderDoc\LICENSE.md") || FileExists(StageDir + "\tools\RenderDoc\LICENSE.txt")
#define HasRenderDoc HasRenderDocFiles && HasRenderDocLicense
#define HasReportWorker FileExists(StageDir + "\tools\report_worker\report_worker.exe")

#if DirExists(StageDir + "\tools\RenderDoc") && !HasRenderDoc
  #error The staged RenderDoc directory is incomplete. Supply the complete portable distribution or remove the directory.
#endif

[Setup]
AppId={#MyAppId}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher=Mangekyo contributors
VersionInfoVersion={#MyAppVersion}
VersionInfoDescription={#MyAppName} Setup
VersionInfoProductName={#MyAppName}
; Install into Program Files for all users.  Use the explicit {pf}/{pf32}
; constants (resolved by ArchitecturesInstallIn64BitMode) instead of {autopf}:
; {autopf} defers to the privileges dialog and can silently fall back to a
; per-user %LOCALAPPDATA%\Programs path, which is not what we want.  A hard
; admin install keeps {app} in Program Files; app data still lives in
; %LOCALAPPDATA%, so the install directory stays read-only at run time.
DefaultDirName={pf}\Mangekyo
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed={#MyAppArchAllowed}
ArchitecturesInstallIn64BitMode={#MyAppArchAllowed}
MinVersion=10.0.17763
OutputDir={#OutputDir}
OutputBaseFilename=Mangekyo-{#MyAppVersion}-windows-{#MyAppArch}-setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes
SetupIconFile={#SourcePath}\..\gui\app.ico
Uninstallable=yes
UninstallDisplayName={#MyAppName}
#if HasGui
UninstallDisplayIcon={app}\app\bin\{#MyAppExeName}
#else
UninstallDisplayIcon={app}\app\bin\gpu_benchmark.exe
#endif
UsePreviousAppDir=yes
UsePreviousGroup=no
UsePreviousTasks=yes
CloseApplications=yes
RestartApplications=no
InfoBeforeFile={#StageDir}\PACKAGE_LIMITATIONS.md
ChangesAssociations=no
ChangesEnvironment=no
#if EnableSigning == "1"
SignTool=release
SignedUninstaller=yes
#endif

[Types]
Name: "full"; Description: "Full installation"
Name: "compact"; Description: "Core benchmark only"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "core"; Description: "Mangekyo"; Types: full compact custom; Flags: fixed
#if HasRenderDoc
Name: "renderdoc"; Description: "RenderDoc capture tools"; Types: full custom
#endif
#if HasReportWorker
Name: "reports"; Description: "Report worker payload (not yet GUI-integrated)"; Types: full custom
#endif

[Tasks]
#if HasGui
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked
#endif

[Files]
Source: "{#StageDir}\app\*"; DestDir: "{app}\app"; Components: core; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\assets\*"; DestDir: "{app}\assets"; Components: core; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\scripts\*"; DestDir: "{app}\scripts"; Components: core; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\licenses\*"; DestDir: "{app}\licenses"; Components: core; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\release-manifest.json"; DestDir: "{app}"; Components: core; Flags: ignoreversion
Source: "{#StageDir}\files.sha256"; DestDir: "{app}"; Components: core; Flags: ignoreversion
Source: "{#StageDir}\PACKAGE_LIMITATIONS.md"; DestDir: "{app}"; Components: core; Flags: ignoreversion
Source: "{#StageDir}\README.md"; DestDir: "{app}"; Components: core; Flags: ignoreversion
#if HasRenderDoc
Source: "{#StageDir}\tools\RenderDoc\*"; DestDir: "{app}\tools\RenderDoc"; Components: renderdoc; Flags: ignoreversion recursesubdirs createallsubdirs
#endif
#if HasReportWorker
Source: "{#StageDir}\tools\report_worker\*"; DestDir: "{app}\tools\report_worker"; Components: reports; Flags: ignoreversion recursesubdirs createallsubdirs
#endif

[InstallDelete]
; The stable AppId upgrades legacy installations in place. Remove only the old
; product shortcuts; the historical application-data directory is preserved.
Type: files; Name: "{autoprograms}\GPU Compute Benchmark.lnk"
Type: files; Name: "{autoprograms}\GPU Compute Benchmark CLI.lnk"
Type: files; Name: "{autoprograms}\GPU Compute Benchmark\RenderDoc.lnk"
Type: dirifempty; Name: "{autoprograms}\GPU Compute Benchmark"
Type: files; Name: "{autodesktop}\GPU Compute Benchmark.lnk"

[Icons]
#if HasGui
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\app\bin\{#MyAppExeName}"; WorkingDir: "{app}\app\bin"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\app\bin\{#MyAppExeName}"; WorkingDir: "{app}\app\bin"; Tasks: desktopicon
#else
Name: "{autoprograms}\{#MyAppName} CLI"; Filename: "{app}\app\bin\gpu_benchmark.exe"; Parameters: "--help"; WorkingDir: "{app}\app\bin"
#endif
#if HasRenderDoc
Name: "{autoprograms}\{#MyAppName}\RenderDoc"; Filename: "{app}\tools\RenderDoc\qrenderdoc.exe"; WorkingDir: "{app}\tools\RenderDoc"; Components: renderdoc
#endif

[Run]
#if HasGui
Filename: "{app}\app\bin\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; WorkingDir: "{app}\app\bin"; Flags: nowait postinstall skipifsilent
#endif

; No [UninstallDelete] entry is intentional. Results, captures, reports and
; logs continue to live under the legacy %LOCALAPPDATA%\GpuComputeBenchmark
; data contract and survive uninstall and product-brand upgrades.
