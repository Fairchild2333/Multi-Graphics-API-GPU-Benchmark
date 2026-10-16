# 当前 Mac 与全部已找到历史成绩：2026-10-14

原始来源：当前Mac M4 Pro主库137条，iPhone A15原始历史67条，iPad A12X原始历史43条，仓库旧显卡220条（全部为粒子测试，很多没有保存分数/版本）。合计467条来源记录逐条导出，未将测试目录中的重复副本累加。另有HANDOFF中18台Android/鸿蒙设备的历史摘要；原始Windows adb/hdc目录未在当前Mac找到。摘要可供参考，不能冒充本轮重新核验的原始数据。

主表选用最新完整手机复测清单，Mac采用原有修正1280×720、关闭抓帧的32条选择，并更新至最新CPU全核/内存及Vulkan内存扫描；自动单线程采用最新15秒系统调度平均322.300。没有取历史最高分，也没有重新测试或修改任何原始结果。移动端为ios_preview，Mac为桌面合同；比值只能说明此内核/记录的观察，不是统一认证总分。

## 1. 当前三台设备：CPU、纯计算、内存与存储

| 项目 | 单位 | Mac M4 Pro | iPhone 13 Pro A15 | iPad Pro A12X |
|---|---|---:|---:|---:|
| CPU 单线程自动采样 | MWork/s | 322.300 | 147.268 | 109.415 |
| CPU 全核 | MWork/s | 3581.602 | 654.417 | 491.864 |
| CPU 全核内存读取 | GB/s | 240.615 | 34.518 | 51.204 |
| Metal 粒子65,536纯计算 | GB/s | 310.199 | 52.955 | 32.845 |
| Vulkan 粒子65,536纯计算 | GB/s | 239.127 | 38.168 | 23.373 |
| Metal 粒子1,048,576纯计算 | GB/s | 146.696 | 20.555 | 30.186 |
| Vulkan 粒子1,048,576纯计算 | GB/s | 219.681 | 21.188 | 22.618 |
| Metal 粒子4,194,304纯计算 | GB/s | 142.220 | 18.951 | 29.678 |
| Vulkan 粒子4,194,304纯计算 | GB/s | 145.311 | 19.149 | 27.317 |
| Metal 粒子16,777,216纯计算 | GB/s | 143.277 | 18.983 | 29.502 |
| Vulkan 粒子16,777,216纯计算 | GB/s | 133.438 | 19.027 | 28.935 |
| GPU Vulkan 顺序读取 | GB/s | 252.395 | 29.298 | 56.760 |
| GPU Metal 固定网格诊断 | GB/s | 244.776 | 30.917 | 59.904 |
| CPU 单线程内存读取 | GB/s | 80.400 | 35.100 | 42.100 |
| CPU 单线程L1读取 | GB/s | 132.700 | 96.100 | 78.900 |
| CPU 单线程L2读取 | GB/s | 112.100 | 79.200 | 52.300 |
| 页面首次映射 | GB/s | 22.160 | 21.120 | 13.740 |
| 存储顺序读 | MB/s | 3412.000 | 1382.000 | 1253.000 |
| 存储顺序写 | MB/s | 6914.000 | 1655.000 | 1541.000 |
| 存储随机4K读 | MB/s | 63.000 | 59.000 | 29.000 |
| 存储随机4K写 | MB/s | 37.000 | 30.000 | 21.000 |

![三台设备主要项目对比](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/out/test-results/all-device-comparison-20261014/apple-comparison.png>)

CPU全核分别14/6/8线程；单线程是系统调度采样。Mac另外已保存大核312.254、小核后台策略80.762 MWork/s，带≥99%归属校验；手机没有这类归属验证，不能把147.268/109.415分别称为大核或小核。Mac最新单线程缓存/内存来自验证过的大核组，手机对应系统调度单线程，条件不同。

Mac CPU全核相对iPhone约5.47倍、相对iPad约7.28倍；Metal16M纯计算分别约7.55/4.86倍。Vulkan独立顺序内存读取252.395，相对iPhone29.298约8.62倍、相对iPad56.760约4.45倍。粒子GB/s是按40字节估算的更新吞吐，不是DRAM带宽；Metal固定网格最佳值没有验证内存平台，也不能与Vulkan中位数顺序扫描相除推导API开销。页面首次映射不是swap换入/换出，三台都未单独测实际交换/压缩。

## 2. GPU绘制：保留原始分数，限制解释

**新发现：Mac旧Metal绘制的1M/4M/16M粒子、三档GPU Burn也出现总命令跨度大于帧间隔；流体compute+render重叠。因此这些旧分数标为待复测，不参与得分倍数排行。手机的指定项已经使用新计时合同复测。此前Mac报告的“Vulkan得分高33%–57%”不能沿用为真实API优势。**

| 项目 | Mac Metal | Mac Vulkan | iPhone Metal | iPhone Vulkan | iPad Metal | iPad Vulkan | 单位 |
|---|---:|---:|---:|---:|---:|---:|---|
| 粒子 65,536绘制 | 151.353 | 50.542 | 26.313 | 16.767 | 23.338 | 2.287 | GB/s |
| 粒子 1,048,576绘制 | 52.401† | 85.926 | 12.886 | 19.614 | 28.269 | 22.833 | GB/s |
| 粒子 4,194,304绘制 | 54.918† | 132.732 | 16.318 | 19.085 | 30.004 | 27.392 | GB/s |
| 粒子 16,777,216绘制 | 57.878† | 127.469 | 15.884 | 18.733 | 27.198 | 27.497 | GB/s |
| GPU Burn 16步 | 4.421† | 5.879 | 1.203 | 0.936 | 1.306 | 0.668 | Gpix-step/s |
| GPU Burn 64步 | 4.517† | 6.460 | 1.555 | 1.074 | 1.485 | 0.847 | Gpix-step/s |
| GPU Burn 256步 | 4.386† | 6.888 | 1.586 | 0.967 | 1.527 | 0.894 | Gpix-step/s |

†=Mac计时待复测，只保留历史值。手机Vulkan16/64步沿用原有双帧成绩；256步为单帧复测。MacVulkan仍是双帧合同。不能把这些值汇成严格跨平台API总排名。

实际FPS更容易观察绘制表现，但Mac不跟显示刷新，iOS直接呈现且调度不同，仍是应用表现：

| 项目 | Mac Metal / Vulkan FPS | iPhone Metal / Vulkan FPS | iPad Metal / Vulkan FPS |
|---|---:|---:|---:|
| 1M粒子绘制 | 644.70 / 248.60 | 48.70 / 48.00 | 68.80 / 57.70 |
| 4M粒子绘制 | 155.30 / 135.10 | 17.30 / 19.30 | 10.80 / 9.60 |
| 16M粒子绘制 | 32.30 / 34.20 | 4.30 / 4.70 | 2.00 / 1.90 |
| GPU Burn256 | 18.20 / 14.30 | 3.30 / 1.70 | 3.20 / 1.80 |
| 流体 | 20.70 / 20.70 | 4.00 / 不支持 | 2.10 / 不支持 |

GPU缓存诊断Mac/iPhone/iPad版本分别v1/v2/v3，每链读取/预热条件不同，不能用340.717/311.192/356.150 ns/load排芯片快慢，也没有独立确认GPU L1/L2/L3/SoC-SLC归属。曲线原样保留在各自档案。

## 3. 以前Android / 鸿蒙设备：历史摘要参考

下面引用仓库HANDOFF记录，不是当前Mac已取得的原始JSON。GPU列固定为Vulkan16M纯计算、Vulkan顺序读取；GPU Burn列采用更完整的ES3.1 256步历史记录（不能与ES2.0混排）。不支持/无值以—表示，避免切换API凑一列。32位CPU版本、设备限制与温度不同，不作正式统一排行。

| 设备 | CPU全核 MWork/s | Vulkan16M纯计算 GB/s | Vulkan内存读 GB/s | ES3.1 Burn256 Gpix-step/s | 备注 |
|---|---:|---:|---:|---:|---|
| MIX Fold3 / SD8 Gen2 | 936.750 | 26.140 | 53.000 | 3.080 | CPU8线程；内存扫描上限128MiB，未到平台；源2286/2084 |
| Quest3 / XR2 Gen2 | 608.500 | 14.790 | 32.600 | 2.640 | CPU6线程；头显频率限制；ES新方法；源2253/1758 |
| 黑鲨5Pro / SD8 Gen1 | 844.990 | 19.240 | 44.170 | 1.900 | CPU8线程；cpu6受到额外干扰；源2012/1749 |
| MIX4 / SD888+ | 758.800 | 20.850 | 32.700 | 1.090 | 内存校验后改为32.7；16M512MiB粒子缓冲未做同类校验；源1972/1665 |
| 黑鲨3Pro / SD865 | 773.170 | 13.460 | 23.200 | 0.810 | CPU8线程；256MiB内存校验；16M粒子缓冲未核验；源1941/1665 |
| 小米平板5 / SD860 | 607.790 | 10.170 | 24.600 | 0.660 | Vulkan Burn256不稳，使用ES列；源1913/1665 |
| MIX2S / SD845 | 548.440 | 9.070 | 15.400 | 0.480 | Vulkan Burn256不稳；源1874/1665 |
| 小米5C / 澎湃S1 | 416.180 | — | — | 0.079 | CPU8线程强制全速状态；多数GPU项不支持；源1828 |
| Switch / TegraX1 | 273.970 | 5.130 | 11.200 | 0.740 | 内存时钟800MHz；CPU4线程；源1714 |
| 小米平板3 / MT8176 | 352.110 | — | — | 0.076 | CPU6线程；无Vulkan；重档只3帧；源1625 |
| 初代小米平板 / TegraK1 | 129.770 | — | — | 0.570 | 32位CPU4线程；另有performance策略130.92，单独保留；无Vulkan；源1583/1510 |
| 小米平板2 / Atomx5 | 105.580 | — | — | 0.173 | 32位CPU4线程；无Vulkan；重档5帧；源1549 |
| 小米3 / Tegra4 | 94.790 | — | — | — | CPU4线程强制在线后；仅CLI日志；ES2独立256步0.45；源1510 |
| Quest2 / XR2 | 359.230 | 7.720 | 14.200 | 0.660 | CPU4线程；GPU限制490–525MHz；源1455 |
| 小米电视 / MT9655 | 224.230 | — | 5.200 | 0.094 | CPU4线程；16M不稳定；重档2–3帧；源1423 |
| TCL电视 / MT5889 | 69.950 | 2.330 | 2.300 | 0.039 | 32位CPU4线程；16M超驱动声明范围未验数据；Burn仅1帧；源1365 |
| MateBookPro / KirinX90 | 1901.700 | 17.790 | 60.200 | 1.210 | CPU20线程；CPU内存扫描10线程；源1315 |
| Mate60Pro / Kirin9000S | 697.100 | 7.860 | 29.300 | 0.630 | CPU12线程；CPU内存扫描8线程；源1265 |

历史上RTX5090独立内存校验记录1544.6 GB/s（HANDOFF1665节），当前M4Pro252.395约为其16.3%；这是不同显存/统一内存平台的同内核观察。其余旧显卡当前220条原始文件只包含粒子项，没有GPU Burn/CPU/内存分数，不能从粒子时间补造这些项目。

## 4. 仓库旧显卡：相同1M纯计算的原始耗时

220条旧记录多数没有workloadVersion与score。下表直接使用保存的计算毫秒，越低越快；不把由舍入时间反算的GB/s冒充原有分数。DX11的0ms按计时不可用处理。先按同API/同1M/headless分组，保留原驱动与版本不足的限制。

| 设备 | Vulkan计算 ms | DX12计算 ms | DX11计算 ms | OpenGL计算 ms |
|---|---:|---:|---:|---:|
| AMD Radeon RX 6900 XT | 0.055 | 0.048 | 0.047 | 2.750 |
| AMD Radeon RX 9070 XT | 0.034 | 0.034 | 0.034 | 0.034 |
| Apple M4 Pro | — | — | — | — |
| Moore Threads MTT S80 | 0.093 | 0.027 | 无有效计时 | 54.701 |
| NVIDIA GeForce GT 730 | 6.145 | 6.107 | 无有效计时 | 6.123 |
| NVIDIA GeForce GTX 570 | — | 1.124 | 无有效计时 | 0.848 |
| NVIDIA GeForce GTX 680 | 0.573 | 0.556 | 无有效计时 | 0.508 |
| NVIDIA GeForce RTX 2080 | 0.173 | 0.174 | 无有效计时 | 0.174 |
| NVIDIA GeForce RTX 5090 | 0.023 | 0.014 | 无有效计时 | 0.018 |

当前M4Pro同档Metal0.286ms、Vulkan0.191ms；iPhoneMetal2.041ms、Vulkan1.980ms；iPadMetal1.389ms、Vulkan同步1.854ms。iPadVk包含提交等待，版本不同。小耗时舍入误差、驱动历史与呈现条件不同，旧显卡表不形成综合GPU等级结论。

无Headless配对的其他显卡与软件/虚拟设备仍全部进入all-raw-history.csv/json。220条的完整设备/API/数据量/模式统计如下：

| 原始设备名 | 记录数 | API |
|---|---:|---|
| AMD FirePro D700 (FireGL V) #1 | 4 | DX11, DX12, OpenGL, Vulkan |
| AMD FirePro D700 (FireGL V) #2 | 3 | DX11, DX12, Vulkan |
| AMD Radeon HD 5700 Series | 2 | DX11, OpenGL |
| AMD Radeon RX 6600 XT | 3 | DX11, OpenGL, Vulkan |
| AMD Radeon RX 6600 XT (FL 12_1) | 1 | DX12 |
| AMD Radeon RX 6900 XT | 15 | DX11, DX12, OpenGL, Vulkan |
| AMD Radeon RX 6900 XT (FL 12_1) | 1 | DX12 |
| AMD Radeon RX 9070 XT | 16 | DX11, DX12, OpenGL, Vulkan |
| AMD Radeon(TM) Graphics | 5 | DX11, OpenGL, Vulkan |
| AMD Radeon(TM) Graphics (FL 12_1) | 2 | DX12 |
| Apple M4 Pro | 8 | Metal |
| Intel(R) UHD Graphics 630 | 4 | DX11, DX12, OpenGL, Vulkan |
| Mi Pad 5 Adreno 640 GPU | 5 | DX11, DX12, Vulkan |
| Mi Pad 5 Adreno 640 GPU (FL 12_1) | 1 | DX12 |
| Microsoft Basic Render Driver | 18 | DX11, DX12, Vulkan |
| Microsoft WARP (CPU Software Renderer) | 6 | DX11, DX12 |
| Moore Threads MTT S80 | 20 | DX11, DX12, OpenGL, Vulkan |
| NVIDIA GeForce GT 730 | 20 | DX11, DX12, OpenGL, Vulkan |
| NVIDIA GeForce GTX 570 | 15 | DX11, DX12, OpenGL |
| NVIDIA GeForce GTX 680 | 20 | DX11, DX12, OpenGL, Vulkan |
| NVIDIA GeForce GTX 970 | 2 | DX11, Vulkan |
| NVIDIA GeForce GTX 970 (FL 12_1) | 1 | DX12 |
| NVIDIA GeForce GTX 970/PCIe/SSE2 | 1 | OpenGL |
| NVIDIA GeForce RTX 2080 | 16 | DX11, DX12, OpenGL, Vulkan |
| NVIDIA GeForce RTX 4070 | 4 | DX11, DX12, OpenGL, Vulkan |
| NVIDIA GeForce RTX 4070 SUPER | 4 | DX11, DX12, OpenGL, Vulkan |
| NVIDIA GeForce RTX 5090 | 7 | DX11, DX12, OpenGL, Vulkan |
| NVIDIA GeForce RTX 5090 #1 | 4 | DX11, DX12, OpenGL, Vulkan |
| NVIDIA GeForce RTX 5090 (FL 12_1) | 1 | DX12 |
| NVIDIA GeForce RTX 5090/PCIe/SSE2 | 1 | OpenGL |
| Radeon RX 580 Series | 3 | DX11, OpenGL, Vulkan |
| Radeon RX 580 Series (FL 12_0) | 1 | DX12 |
| Radeon Vega Frontier Edition | 3 | DX11, OpenGL, Vulkan |
| Radeon Vega Frontier Edition (FL 12_1) | 1 | DX12 |
| SVGAMPWDDM Device | 2 | DX11 |

## 5. 同一台Mac的旧记录波动与限制

Mac137条中有早期几秒预览、抓帧、不同渲染尺寸、完整15秒、CPU不同归属策略。历史最高值不能混成一套“最佳成绩”。例如默认15秒CPU全核3010.644→3726.693→最新3581.602，变化既有系统状态也有序列条件，不能据此称硬件升级或回退；短时1秒3734.093不进入正式15秒比较。Metal16M纯计算原有完整本轮143.277GB/s，与早期113–149GB/s有波动。

尤其Mac最新Metal Burn256旧分4.386 Gpix-step/s，总命令跨度107.592ms、帧间隔55.089ms（约195%）；流体compute38.811+render47.953ms、total48.011ms。这些具有与手机修复前相同的口径风险，应该单帧核验后再用分数与手机、Vulkan比较。本次用户要求对比，没有重新启动Mac测试。

## 6. 输出文件

- `apple-metrics.csv/json`：三台最新选择的CPU、纯计算、内存与存储。
- `selected-apple-results.json`：各32条完整选择，保留原始ID与合同。
- `all-raw-history.csv/json`：467条原始来源记录及异常/旧版标记，不改原始文件。
- `historical-mobile-excerpts.md`：18台旧设备的原摘要表、后续更正及来源行。
- `source-hashes.json`：本次读取来源的SHA256。
