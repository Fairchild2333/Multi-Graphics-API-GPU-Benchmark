# Windows 安装包打包规范

本文是 Mangekyo Windows 安装包的当前唯一打包规范。更早的 Inno Setup
与“直接发布 MSI”记录仅用于历史追踪，不再作为正式发布流程。

## 正式产物

每个版本发布两个安装包：

```text
Mangekyo-<version>-windows-x64-setup.exe
Mangekyo-<version>-windows-arm64-setup.exe
```

- x64 安装包的启动程序、MSI 和应用本体都是 x64。
- ARM64 安装包的启动程序、MSI 和应用本体都是原生 ARM64。
- 两个 EXE 都是单文件安装包，安装界面可手动切换 English / 简体中文。
- 不再按语言拆包，也不发布一个同时包含两种 CPU 架构的通用安装包。
- Windows on Arm 可以并存安装原生 ARM64 版和模拟运行的 x64 版；两者使用独立的
  MSI UpgradeCode、安装目录和开始菜单目录。
- `*.msi` 是构建 Setup EXE 所需的中间文件，不是正式 GitHub Release 资产。

安装包由两层组成：

1. CPack + WiX 生成对应架构的 MSI，负责文件复制、升级、修复和卸载注册。
2. `installer/native-bootstrapper/main.cpp` 编译为对应架构的原生 Win32 启动程序，
   提供中英文界面、安装路径选择，并内嵌 MSI。

启动程序使用自绘圆角语言选择、许可证勾选框和操作按钮；界面读取 Windows
`AppsUseLightTheme` 设置并随系统自动切换浅色/深色主题，标题栏、卡片、输入框与进度条
保持一致。语言展开菜单同样由安装器按当前主题绘制，路径输入框使用无系统凹边框的
圆角容器。窗口尺寸按客户区计算并收紧底部布局，底部按钮不得因标题栏或 DPI 边框
而被裁切，也不应为空的进度状态长期保留大块空白。

许可证不得调用不跟随主题的系统 `MessageBox`：当前 Setup 使用自己的主题窗口、Fluent
风格信息图标和圆角确认按钮。安装进度由 Setup 自绘为与当前深浅色主题一致的圆角细轨道，
安装期间显示移动的强调色段与状态文字，Windows 任务栏同步显示不确定进度；成功后主窗口
保留明确的完成状态、完整强调色进度和“关闭”按钮，任务栏同步显示完成进度。路径校验和
安装失败也显示在主题化的内嵌状态区，不得回退到旧式系统消息框。

安装前提供“创建桌面快捷方式”和“创建开始菜单快捷方式”两个选择；桌面默认关闭，开始菜单
默认开启。Setup 将选择分别传入 MSI 公共属性 `CREATE_DESKTOP_SHORTCUT` 与
`CREATE_START_MENU_SHORTCUT`。快捷方式必须由 MSI 条件组件创建，不得由启动程序在安装完成后
临时复制 `.lnk`，这样升级、修复与卸载才能保持一致并自动清理。x64 与 ARM64 使用不同的
开始菜单目录，以支持 Windows on Arm 并存安装。

安装完成后两个选项重新启用。用户改变选择时主按钮显示“应用”，Setup 使用缓存的同一 MSI
执行维护更新；快捷方式组件必须设置为 Transitive，使 Windows Installer 重新评估条件并创建
或移除对应组件。重复应用相同选择不会创建第二份 `.lnk`，取消选择也由 MSI 删除原组件。

内嵌 MSI 的解包缓冲区必须分配在堆上。安装工作线程使用 Windows 默认线程栈；不得在
该线程调用链中声明 1 MiB 级局部数组，否则会在进入 UAC 前以 `0xc00000fd` 栈溢出退出。

Setup EXE 必须嵌入 `asInvoker`、Common Controls 6 和 Per-Monitor V2 DPI 清单。这样
Windows 不会仅因文件名含 `setup` 就在启动阶段套用旧式安装器检测；只有用户点击安装、
启动 `msiexec` 的 `runas` 路径时才请求 UAC，同时系统控件使用 Windows 11 样式。

Inno Setup 脚本仍留在仓库中供旧版本复现，但后续版本不得用它生成正式安装包。

安装器与应用共用 `gui/app.ico`。需要更换品牌图标时只替换这个文件并重新编译 GUI
和 Setup；`build-native-bootstrapper.ps1` 会把它写入两个架构 EXE 的标题栏、任务栏
与文件资源，不要再维护另一份安装器专用图标。

## 构建机要求

Windows 构建机需要：

- Visual Studio Build Tools，包含 MSVC x64/x86、MSVC ARM64 和 Windows SDK；
- CMake 与 PowerShell 5.1 或更高版本；
- vcpkg 的 `x64-windows` 与 `arm64-windows` 依赖；
- WiX Toolset 5，推荐固定为项目当前使用的 5.0.2：

```powershell
dotnet tool install --global wix --version 5.0.2
```

确认工具可用：

```powershell
wix --version
cmake --version
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
  -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.ARM64 `
  -property installationPath
```

## 打包前准备

1. 在 `CMakeLists.txt` 中更新项目版本，并保证 WinUI 文件版本、引擎版本和安装包版本一致。
2. 提交本次版本的源代码。正式发布脚本默认拒绝 dirty worktree。
3. 准备固定版本的 RenderDoc 官方 portable ZIP 及其 SHA-256。RenderDoc 只打入 x64 包；
   ARM64 当前不支持 RenderDoc，因此 ARM64 构建使用 `-SkipRenderDoc`。
4. 如需可移植报告，准备已冻结的 `report_worker.exe` 目录。
5. 正式公开发布必须准备 Authenticode 证书和带时间戳的签名命令。

## 推荐：完整发布构建

完整发布一次构建一个架构。先构建 x64：

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'

powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\build-windows-github-release.ps1 `
  -Arch x64 `
  -RenderDocArchive 'C:\release-inputs\RenderDoc_1.45_64.zip' `
  -RenderDocSha256 '<官方或独立复核的 SHA-256>' `
  -ReportWorkerDir 'C:\release-inputs\report_worker'
```

再构建原生 ARM64：

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'

powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\build-windows-github-release.ps1 `
  -Arch ARM64 `
  -SkipRenderDoc `
  -ReportWorkerDir 'C:\release-inputs\report_worker'
```

本地工程候选允许未提交改动时，可额外传入 `-AllowDirtySource`。这种产物会在
`release-assets.json` 中记录 `sourceTreeDirty=true`，不得作为正式公开版本上传。

完整脚本会依次完成：

1. 编译 CLI、引擎和 self-contained WinUI GUI；
2. 创建并验证 stage，生成 portable ZIP；
3. 生成对应架构的 WiX MSI 中间文件；
4. 编译同架构原生双语启动程序并内嵌 MSI；
5. 将 ZIP、Setup EXE、`release-assets.json` 和 `SHA256SUMS.txt` 写入：

```text
out/release/windows-x64/
out/release/windows-arm64/
```

## 已有 stage 时只重打安装包

如果应用本体和 stage 已验证，只需重新生成 MSI 与 Setup：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\build-wix-installer.ps1 `
  -StageDir out\stage\windows-x64 `
  -BuildDir out\build\windows-x64-release `
  -Arch x64

powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\build-wix-installer.ps1 `
  -StageDir out\stage\windows-arm64 `
  -BuildDir out\build\windows-arm64-release `
  -Arch ARM64

powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\build-native-bootstrapper.ps1 `
  -Arch Both
```

输出位于：

```text
out/installer/Mangekyo-<version>-windows-x64-setup.exe
out/installer/Mangekyo-<version>-windows-arm64-setup.exe
```

不要混用不同版本的 stage、MSI 和 `-Version`。脚本会检查 stage manifest 与请求版本，
但发布人仍需确认两个架构来自同一提交。

## 只重新封装现有 MSI

如果两个 MSI 已经生成且内容不需要改变，可直接运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\build-native-bootstrapper.ps1 `
  -Arch Both `
  -Version 0.2.6
```

也可以把 `Both` 改为 `x64` 或 `ARM64`，只生成一个架构。

## 签名

正式发布脚本会把同一个签名模板传给 MSI 中间文件和最终 Setup EXE。模板中的
`$f` 由脚本替换为当前文件路径；在 PowerShell 中必须用单引号，避免 `$f` 被提前展开：

```powershell
-SignToolCommand 'signtool sign /fd SHA256 /tr https://timestamp.example /td SHA256 /a "$f"' `
-RequireSigned
```

不要在仓库、脚本或 shell history 中写证书密码。证书选择与时间戳服务器应由
发布 CI 或受控的本地签名环境配置。

## 安装器使用

交互运行时可以切换语言，并可直接编辑安装目录或点击“浏览”。默认目录是：

```text
x64:   %ProgramFiles%\Mangekyo
ARM64: %ProgramFiles%\Mangekyo ARM64
```

所选目录通过 MSI 的 `INSTALL_ROOT` 属性执行，不只是界面上的假选项。受控部署支持：

```powershell
.\Mangekyo-<version>-windows-x64-setup.exe --lang en
.\Mangekyo-<version>-windows-x64-setup.exe --lang zh-CN
.\Mangekyo-<version>-windows-x64-setup.exe --quiet --lang zh-CN
```

`--quiet` 使用默认安装路径并仍可能显示 Windows UAC 提示；它只隐藏 Mangekyo 与
MSI 界面，不会绕过管理员授权。静默安装默认不创建桌面快捷方式、创建开始菜单快捷方式；
对应默认值由 MSI Property 表保存。

## 发布前验证

脚本会自动检查 Setup EXE 的 PE 架构、内嵌 MSI 大小、MSI/OLE 文件头、版本资源、
SHA-256 和签名状态。正式上传前还必须在干净系统完成实际验收：

- x64 Windows 10/11：英文安装、中文安装、自定义路径、旧版本升级、修复和卸载；
- ARM64 Windows 11：确认安装器、GUI、CLI 和 DLL 都是 ARM64，并完成同样流程；
- Windows on Arm：确认 x64 与 ARM64 可并存，快捷方式和卸载项能明确区分；
- 分别验证桌面与开始菜单选项在安装前及完成页的开启/关闭组合；重复应用不产生第二份 `.lnk`；
  所创建的快捷方式可以启动 GUI，GUI 能找到相邻的 `gpu_benchmark.exe`，卸载后快捷方式与
  空的开始菜单目录均被移除；
- 卸载不删除 `%LOCALAPPDATA%\GpuComputeBenchmark` 下的历史成绩和抓帧；
- x64 包验证 bundled RenderDoc；ARM64 明确显示 RenderDoc 不可用；
- `Get-AuthenticodeSignature` 返回 `Valid`，`SHA256SUMS.txt` 与实际文件一致；
- 在无 Visual Studio、vcpkg、Python、Vulkan SDK 和独立 VC Redist 的干净机器验证启动。

发布资产中不要加入 raw MSI、Inno Setup EXE、构建目录或未签名的临时安装包。
