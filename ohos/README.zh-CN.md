# Mangekyo — HarmonyOS（鸿蒙）版

[English](README.md)

鸿蒙 App：ArkTS 界面 + 与其他平台共用的 `gpu_engine`（Vulkan 画在 XComponent 的窗口里，CPU 测试直接调用引擎的命令行入口）。

> 状态（2026-10-06，事实分层）：
> - **代码已写**：四个页面（GPU、CPU、历史、图表）、NAPI 桥接、引擎的鸿蒙宿主（`src/ohos_engine_host.*`）。
> - **可编译**：在 Windows 上用 DevEco Studio 26.0 自带的工具链从命令行编出未签名的 HAP
>   （`arm64-v8a` 和 `x86_64`，约 6 MB）。ArkTS 编译器没有报错或警告。
> - **没有运行过**：没有连接鸿蒙设备，没有模拟器镜像，也没有签名配置。界面显示、Vulkan 是否可用、
>   CPU 绑核是否被系统允许、图表画得对不对，都没有实际验证。
> - **成绩身份**：GPU 测试写出 `stream_v1_ohos_preview`、
>   `gpu_burn_v3_fixed_steps_16_kaleidoscope_ohos_preview`；缓存 / 内存带宽测试写出
>   `cpu_membw_v1_ohos_preview`。默认 3 秒预览，不是桌面版的 15 秒，不能和桌面成绩混排。
> - **没有做**：流体测试、抓帧、正式的移动端时长合同、签名和发布包、CPU 测试的取消。

## 功能

| 页面 | 内容 |
|---|---|
| **GPU** | 预览画面；粒子测试和 GPU Burn（固定 16 步）各 3 秒；运行后显示成绩和每帧 GPU 耗时折线图 |
| **CPU** | 算力测试（单核、全核、单核 + 全核；1 秒或 15 秒）和缓存 / 内存带宽扫描；各级缓存表格、双对数曲线、存储读写条形图、逐处理器柱状图 |
| **历史** | 保存过的每一条成绩，可全部清除 |
| **图表** | 每项测试一张条形图：该测试最新版本下各设备的最好成绩 |

界面文字有中文和英文，跟随系统语言。

## 架构

```text
ArkTS（entry/src/main/ets）
  pages/        Index（四个标签页）、GpuPage、CpuPage、HistoryPage、ChartsPage
  components/   Card、Charts（Canvas 绘制的折线图、柱状图、条形图）
  common/       RunController（启动测试、收集输出）、EngineOutput（解析引擎输出）、
                ResultsStore（读 results.json）、I18n、Format
        │  import bridge from 'libmangekyo.so'
        ▼
NAPI（entry/src/main/cpp/napi_init.cpp → libmangekyo.so）
  ├── XComponent 回调：拿到 OHNativeWindow
  ├── startWorkload / stopWorkload：GPU 测试，画在预览画面里
  ├── startCli：CPU 测试，在原生线程上运行引擎的命令行入口
  └── pollOutput：引擎打印的每一行（同时写入 hilog，标签 MangekyoEngine）
        ▼
gpu_engine（仓库根目录的 CMake 工程，OHOS 分支）
  └── src/ohos_engine_host.cpp → VulkanBackend（vkCreateSurfaceOHOS）
```

SPIR-V 着色器打包在 `resources/rawfile/shaders/` 里，App 启动时复制到自己的文件目录，因为引擎按文件读取着色器。成绩写在 App 文件目录下的 `results/results.json`。

每条成绩记录这台设备的机型（品牌、市场名称和型号代码）、系统版本和内存，历史页每条成绩下面有一行显示。机型和系统版本来自系统的 deviceinfo 接口，内存来自 `/proc/meminfo`。系统没有提供芯片名的接口，所以芯片名仍然从 `/proc/cpuinfo` 读，读不到时记为 `Unknown CPU`，这时要靠型号代码去查。**这些字段只编译通过，没有在设备上核对过。**

## 窗口宽度、设备类型和新界面

2026-10-06 加入，**只编译通过，没有在任何设备或模拟器上看过**。

- **布局按窗口宽度切换：** 600 vp 以下页签在底部、单列；600 vp 起页签竖排在侧边；840 vp 起每页分成左右两栏。历史页和图表页是网格。预览画面固定 16:9。
- **设备类型：** phone、tablet、2in1、tv、car。
- **两套外壳：** `pages/Index.ets` 适用于所有系统；HarmonyOS 6.1（API 23）以上的手机、平板和二合一改为加载 `pages/IndexHds.ets`，它用 UI Design Kit 的悬浮页签和系统材质。选择在 `EntryAbility.ets` 里。编译 `IndexHds.ets` 时的"接口版本高于兼容版本"警告是预期的。
- **系统材质：** 新外壳的悬浮页签用了它；`module.json5` 里打开了应用级开关（HarmonyOS 7 起对系统控件生效）。按华为文档，普通组件上的材质只在标题栏和底部页签栏里生效，所以内容卡片没有用。

## 构建

需要 DevEco Studio（已在 26.0 上验证，自带 SDK、hvigor、ohpm、node）和 Vulkan SDK 的 `glslc`。

```powershell
.\scripts\build-ohos.ps1
```

脚本先把着色器编译进 rawfile，再执行 `ohpm install` 和 `hvigorw assembleHap`。产物是
`ohos\entry\build\default\outputs\default\entry-default-unsigned.hap`。

也可以用 DevEco Studio 打开 `ohos/` 目录直接构建。**第一次从 IDE 构建前请先运行一次上面的脚本**（或至少让它编译出着色器）：
打包资源发生在原生库编译之前，全新检出的工程里还没有 `.spv` 文件，第一次打出的包会缺着色器，GPU 测试会失败。

**仓库路径不能太长。** 原生库的中间文件路径很深，仓库放在一百多个字符的路径下时，CMake 会警告路径超过 250 个字符，随后构建失败（`ninja: error: loading 'build.ninja'`）。用 `subst` 映射盘符没有用，构建工具会还原成真实路径。

## 安装到设备

未签名的 HAP 装不到真机上。在 DevEco Studio 里登录华为账号后，
**File → Project Structure → Signing Configs → Automatically generate signature**，再点运行即可。
模拟器需要先在 **Device Manager** 里下载系统镜像。这两步都需要你本人操作，没有在这台机器上做过。

## 用 hdc 远程测试

debug 包可以不碰设备就开始一项测试（**只编译通过，没有在设备上试过**）：

```bash
hdc shell aa start -a EntryAbility -b com.mangekyo.benchmark --ps run cpu_membw
hdc hilog | grep Mangekyo
```

- **`--ps run <测试>`**：`cpu_membw`、`cpu_all`、`cpu_per_core`、`cpu_multi`、`stream`、`gpu_burn`。
- **`--pi seconds <N>`**（可选）：CPU 算力测试每项的秒数，或 GPU 测试的时长。
- **日志**：标签 `MangekyoEngine` 是引擎打印的每一行；标签 `Mangekyo` 下有
  `AUTORUN accepted / started / refused / failed` 和结束时的 `RUN finished kind=cpu exit=0`。
- 启动参数只在 debug 包里生效；GPU 测试需要屏幕亮着并解锁。

## 已知风险（都要在设备上确认）

- **CPU 绑核**：引擎用 `sched_setaffinity` 把线程绑到指定核心。如果鸿蒙的应用沙箱不允许，
  算力测试会按"绑核失败"判为无效，缓存 / 内存扫描的结果会标记为未绑核。
- **缓存容量**：从 `/sys/devices/system/cpu` 读取。如果沙箱里读不到，会改为从曲线上识别各级缓存。
- **Vulkan**：引擎只请求 Vulkan 1.0 的入口；设备和模拟器是否都提供 `VK_OHOS_surface` 没有验证。
- **存储测试**：会在 App 文件目录里写一个最大 1 GiB 的临时文件。
