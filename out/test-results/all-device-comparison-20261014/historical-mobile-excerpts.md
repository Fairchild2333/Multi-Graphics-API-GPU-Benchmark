# 旧移动设备测试摘要原表

原始adb/hdc JSON未在当前Mac找到；本文件仅整理仓库已有记录，不执行复测。主报告采用后续更正值。

## 2026-10-08 第十八台设备：华为 Mate 60 Pro（麒麟 9000S，Maleoon 910，HarmonyOS 7）——第一台鸿蒙手机；全核 CPU 成绩不再受绑核影响；29 项里 27 项有成绩

来源：[HANDOFF.md:1265](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/HANDOFF.md:1265>)

| 测试 | 成绩 |
|---|---|
| Vulkan 粒子，窗口，65K / 1M / 4M / 16M | 1.47 / 5.79 / 7.24 / 7.53 GB/s（墙钟计时） |
| Vulkan 粒子，Headless，65K / 1M / 4M / 16M | 8.92 / 8.01 / 7.97 / 7.86 GB/s |
| Vulkan GPU Burn 16 / 64 / 256 步 | 0.39 / 0.52 / 0.55 Gpix-step/s |
| 流体 | 2.64 MParticle-step/s |
| 显存带宽 读 / 写 / 拷贝 | 29.3 / 35.5 / 28.9 GB/s（最大数据集 1024 MiB，数据校验通过；时间戳 146.1 ms 对墙钟 146.9 ms） |
| OpenGL ES 3.1 粒子，窗口，65K / 1M / 4M | 3.78 / 8.46 / 9.69 GB/s（墙钟计时） |
| OpenGL ES 3.1 粒子，Headless，65K / 1M / 4M | 3.44 / 9.20 / 10.74 GB/s |
| OpenGL ES 3.1 GPU Burn 16 / 64 / 256 步 | 0.40 / 0.57 / 0.63 |
| OpenGL ES 2.0 GPU Burn 16 / 64 / 256 步 | 7.36 / 11.07 / 15.28 |
| CPU 全核（12 线程，由系统调度） | 697.10 MWork/s |
| CPU 逐处理器 | 小核 40.2–41.6，中核 92.3–92.5，大核 111.7–112.0 |

## 2026-10-08 第十七台设备：华为 MateBook Pro（麒麟 X90，Maleoon 916，HarmonyOS 7）——第一台鸿蒙真机；有了 hdc 版系列脚本；修了物理核心数、绑核口径和 Vulkan 窗口粒子的计时；29 项里 27 项有成绩

来源：[HANDOFF.md:1315](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/HANDOFF.md:1315>)

| 测试 | 成绩 |
|---|---|
| Vulkan 粒子，窗口，65K / 1M / 4M / 16M | 4.04 / 11.58 / 16.78 / 17.57 GB/s（墙钟计时） |
| Vulkan 粒子，Headless，65K / 1M / 4M / 16M | 17.88 / 17.94 / 17.84 / 17.79 GB/s |
| Vulkan GPU Burn 16 / 64 / 256 步 | 0.72 / 0.97 / 1.04 Gpix-step/s |
| 流体 | 6.60 MParticle-step/s |
| 显存带宽 读 / 写 / 拷贝 | 60.2 / 65.3 / 61.2 GB/s（最大数据集 1024 MiB，数据校验通过；时间戳 72.4 ms 对墙钟 73.3 ms） |
| OpenGL ES 3.1 粒子，窗口，65K / 1M / 4M | 9.12 / 20.76 / 24.23 GB/s（墙钟计时） |
| OpenGL ES 3.1 粒子，Headless，65K / 1M / 4M | 8.82 / 21.70 / 24.30 GB/s |
| OpenGL ES 3.1 GPU Burn 16 / 64 / 256 步 | 0.78 / 1.09 / 1.21 |
| OpenGL ES 2.0 GPU Burn 16 / 64 / 256 步 | 18.25 / 27.24 / 30.74 |
| CPU 全核（20 线程，由系统调度） | 1901.70 MWork/s |
| CPU 逐处理器 | 小核 70.3 / 72.0 / 82.1 / 81.9，中核 106.4–107.4，大核 125.0–125.1 |

## 2026-10-08 第十六台设备：TCL 电视（MT5889，Mali-G52，32 位系统）——Vulkan 驱动没有时间戳，给 Vulkan 后端补了墙钟计时；修了存储顺序读；29 项里 26 项有成绩

来源：[HANDOFF.md:1365](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/HANDOFF.md:1365>)

| 测试 | 时间戳 | 墙钟 |
|---|---|---|
| 粒子 1600 万，窗口 | 0.818 ms | 0.953 ms |
| 粒子 1600 万，Headless | 0.757 ms | 0.897 ms |
| GPU Burn 16 步 | 0.225 ms | 0.424 ms |
| 流体（计算 + 渲染） | 8.80 + 4.36 ms | 9.78 + 4.56 ms |
| 旧版流体 `fluid` | 0.050 ms | 0.232 ms |

| 测试 | 成绩 |
|---|---|
| Vulkan 粒子，窗口，65K / 1M / 4M | 1.91 / 2.20 / 2.20 GB/s |
| Vulkan 粒子，Headless，65K / 1M / 4M / 16M | 2.09 / 2.39 / 2.45 / 2.33 GB/s |
| Vulkan GPU Burn 16 / 64 / 256 步 | 0.021 / 0.032 / 0.039 Gpix-step/s（256 步一帧 12 秒，只量到 1 帧） |
| 流体 | 0.25 MParticle-step/s（一帧 12.9 秒，只量到 1 帧） |
| 显存带宽 读 / 写 / 拷贝 | 2.3 / 2.0 / 3.0 GB/s（最大数据集 256 MiB，数据校验通过） |
| OpenGL ES 3.1 粒子，窗口，65K / 1M / 4M | 1.96 / 2.34 / 1.68 GB/s（工作组 128） |
| OpenGL ES 3.1 粒子，Headless，65K / 1M / 4M | 2.07 / 2.44 / 2.40 GB/s |
| OpenGL ES 3.1 GPU Burn 16 / 64 / 256 步 | 0.022 / 0.032 / 0.039 |
| OpenGL ES 2.0 GPU Burn 16 / 64 / 256 步 | 0.30 / 0.32 / 0.31 |
| CPU 全核（4 线程，32 位） | 69.95 MWork/s |
| CPU 单核 | 18.0 / 18.6 / 18.8 / 18.6（1300 MHz，四个相加 74.0，全核是它的 95%） |

## 2026-10-08 第十五台设备：小米电视（MT9655，Mali-G57）——第一台走网络调试的设备，第一台有 Vulkan 的 Mali；29 项里 24 项有成绩

来源：[HANDOFF.md:1423](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/HANDOFF.md:1423>)

| 测试 | 成绩 |
|---|---|
| Vulkan 粒子，窗口，65K / 1M / 4M | 4.42 / 4.44 / 4.51 GB/s |
| Vulkan 粒子，Headless，65K / 1M / 4M | 4.72 / 4.49 / 4.56 GB/s |
| Vulkan GPU Burn 16 / 64 / 256 步 | 0.069 / 0.088 / 0.095 Gpix-step/s（256 步一帧 5 秒） |
| 流体 | 1.14 MParticle-step/s |
| 显存带宽 读 / 写 / 拷贝 | 5.2 / 7.7 / 7.7 GB/s |
| OpenGL ES 3.1 粒子，窗口，65K / 1M / 4M | 3.28 / 3.97 / 3.97 GB/s |
| OpenGL ES 3.1 粒子，Headless，65K / 1M / 4M | 3.93 / 3.96 / 4.00 GB/s |
| OpenGL ES 3.1 GPU Burn 16 / 64 / 256 步 | 0.068 / 0.087 / 0.094 |
| OpenGL ES 2.0 GPU Burn 16 / 64 / 256 步 | 0.93 / 0.77 / 0.79 |
| CPU 全核（4 线程） | 224.23 MWork/s |
| CPU 单核 | 62.3 / 67.1 / 66.7 / 66.9（1800 MHz，四个相加 263，全核是它的 85%） |

## 2026-10-08 第十四台设备：Meta Quest 2（骁龙 XR2，Adreno 650）——29 项里 27 项有成绩；改正了"等待核对"的误判，修了大粒子数下 GPU 设备丢失

来源：[HANDOFF.md:1455](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/HANDOFF.md:1455>)

| 步数 | 命令行（离屏） | App | 差 |
|---|---|---|---|
| 16 | 13.47 | 11.86 | +14% |
| 64 | 12.79 | 11.82 | +8% |
| 256 | 15.25 | 15.83 | −4% |

| 测试 | 成绩 |
|---|---|
| Vulkan 粒子，窗口，65K / 1M / 4M / 16M | 11.99 / 8.15 / 7.57 / 8.01 GB/s |
| Vulkan 粒子，Headless，65K / 1M / 4M / 16M | 8.16 / 7.61 / 7.75 / 7.72 GB/s |
| Vulkan GPU Burn 16 / 64 / 256 步 | 0.42 / 0.57 / 0.63 Gpix-step/s |
| 流体 | 1.69 MParticle-step/s |
| 显存带宽 读 / 写 / 拷贝 | 14.2 / 21.5 / 16.8 GB/s（最大数据集 128 MiB，数据校验通过） |
| OpenGL ES 3.1 粒子，窗口，65K / 1M / 4M | 5.24 / 6.32 / 7.23 GB/s |
| OpenGL ES 3.1 粒子，Headless，65K / 1M / 4M | 5.24 / 6.48 / 6.71 GB/s |
| OpenGL ES 3.1 GPU Burn 16 / 64 / 256 步 | 0.46 / 0.66 / 0.66 |
| OpenGL ES 2.0 GPU Burn 16 / 64 / 256 步 | 11.86 / 11.82 / 15.83 |
| CPU 全核（4 线程） | 359.23 MWork/s |
| CPU 单核 | A77 89.3–89.8（1478 MHz），大核 136.2（2458 MHz） |

## 2026-10-08 第十三台设备：小米 3（Tegra 4，Android 4.4.4）——App 装不了，用命令行工具跑了 CPU 两项和 OpenGL ES 2.0 GPU Burn

来源：[HANDOFF.md:1510](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/HANDOFF.md:1510>)

| 步数 | 成绩 | 每帧 | 帧数 |
|---|---|---|---|
| 16 | 0.43 Gpix-step/s | 68.9 ms | 188 |
| 64 | 0.46 | 255.9 ms | 50 |
| 256 | 0.45 | 1039.5 ms | 12 |

## 2026-10-08 第十二台设备：小米平板 2（Atom x5，Android 6.0.1，x86）——App 在 Android 6.0 上原来完全不能用（已修）；29 项里 14 项有成绩

来源：[HANDOFF.md:1549](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/HANDOFF.md:1549>)

| 测试 | 成绩 |
|---|---|
| OpenGL ES 3.1 粒子，窗口，65K / 1M / 4M | 4.13 / 5.37 / 5.43 GB/s |
| OpenGL ES 3.1 粒子，Headless，65K / 1M / 4M | 5.15 / 5.32 / 5.31 GB/s |
| OpenGL ES 3.1 GPU Burn 16 / 64 / 256 步 | 0.125 / 0.161 / 0.173 Gpix-step/s（256 步一帧 2.7 秒，量到 5 帧） |
| OpenGL ES 2.0 GPU Burn 16 / 64 / 256 步 | 2.40 / 2.74 / 2.85 |
| CPU 全核（4 线程，名字带 `_32bit`） | 105.58 MWork/s |
| CPU 单核 | 29.40 / 30.31 / 30.16 / 30.14（四个相加 120.0，全核是它的 88%） |

## 2026-10-08 第十一台设备：初代小米平板（Tegra K1）——第一台 32 位设备；32 位构建没用上 NEON（已修），29 项里 14 项有成绩

来源：[HANDOFF.md:1583](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/HANDOFF.md:1583>)

| 测试 | GPU 频率 | 内存时钟 |
|---|---|---|
| 粒子 400 万，Headless | 924 MHz | 924 MHz（最高） |
| 粒子 6.5 万，Headless | 180–396 MHz | 300 MHz |
| GPU Burn（ES 3.1 和 ES 2.0） | 648–756 MHz | 528 MHz |

| 测试 | 成绩 |
|---|---|
| OpenGL ES 3.1 粒子，窗口，65K / 1M / 4M | 0.90 / 5.62 / 6.14 GB/s |
| OpenGL ES 3.1 粒子，Headless，65K / 1M / 4M | 1.07 / 6.08 / 6.92 GB/s |
| OpenGL ES 3.1 GPU Burn 16 / 64 / 256 步 | 0.39 / 0.53 / 0.57 Gpix-step/s |
| OpenGL ES 2.0 GPU Burn 16 / 64 / 256 步 | 3.50 / 4.47 / 4.97 |
| CPU 全核（4 线程，名字带 `_32bit`） | 129.77 MWork/s（没带标识的那次 125.79，已从副本剔除） |
| CPU 单核 | 30.57 / 30.84 / 31.05 / 31.51（四个相加 124.0） |

## 2026-10-08 第十台设备：小米平板 3（MT8176，PowerVR GX6250）——第一台 PowerVR、第一台联发科；修了两处引擎问题，29 项里 14 项有成绩

来源：[HANDOFF.md:1625](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/HANDOFF.md:1625>)

| 测试 | 成绩 |
|---|---|
| OpenGL ES 3.1 粒子，窗口，65K / 1M / 4M | 2.10 / 3.45 / 3.92 GB/s |
| OpenGL ES 3.1 粒子，Headless，65K / 1M / 4M | 2.49 / 3.82 / 4.00 GB/s |
| OpenGL ES 3.1 GPU Burn 16 / 64 / 256 步 | 0.054 / 0.070 / 0.076 Gpix-step/s（256 步一帧 6.2 秒，只量到 3 帧） |
| OpenGL ES 2.0 GPU Burn 16 / 64 / 256 步 | 0.88 / 0.96 / 1.02 |
| CPU 全核（6 线程） | 352.11 MWork/s（单独跑的那次；强开核心后那次 350.86；系列里那次是 348.22，没存） |
| CPU 单核（强开核心后，6 个都有效） | A53 49.9–51.8，A72 76.3 |

## 2026-10-08 显存带宽加了数据校验（已改引擎）；MIX 4 重跑，读 32.7 GB/s

来源：[HANDOFF.md:1665](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/HANDOFF.md:1665>)


## 2026-10-08 第九台设备：任天堂 Switch（Tegra X1，Android 11）——29 项全过，但内存时钟卡在 800 MHz，成绩要带着这个标注看

来源：[HANDOFF.md:1714](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/HANDOFF.md:1714>)

| 测试 | Vulkan | OpenGL ES 3.1 |
|---|---|---|
| 粒子 65,536，有画面 / 无画面 | 4.74 / 5.12 GB/s | 5.25 / 5.27 |
| 粒子 1,048,576，有画面 / 无画面 | 4.64 / 4.82 | 4.77 / 5.16 |
| 粒子 4,194,304，有画面 / 无画面 | 4.83 / 4.93 | 4.96 / 5.19 |
| 粒子 16,777,216，有画面 / 无画面 | 4.85 / 5.13 | 5.17 / 5.29 |
| GPU Burn 16 / 64 / 256 步 | 0.54 / 0.69 / 0.74 Gpix-step/s | 0.53 / 0.69 / 0.74 |
| 流体 | 2.65 MParticle-step/s（每帧计算 683 ms + 渲染 527 ms） | — |
| 显存带宽 读 / 写 / 拷贝 | 11.2 / 10.6 / 10.0 GB/s | — |

## 2026-10-08 Quest 3：OpenGL ES 用现在的计时方法重跑，补了缓存测试

来源：[HANDOFF.md:1758](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/HANDOFF.md:1758>)

| 测试 | 新方法 | 10-07 旧方法 |
|---|---|---|
| 粒子 65,536，有画面 / 无画面 | 8.07 / 9.18 GB/s | 5.33 / 6.43 |
| 粒子 1,048,576，有画面 / 无画面 | 12.78 / 12.63 | 12.23 / 12.70 |
| 粒子 4,194,304，有画面 / 无画面 | 13.41 / 13.08 | 13.37 / 13.22 |
| 粒子 16,777,216 | 不支持 | 不支持 |
| OpenGL ES 3.1 GPU Burn 16 / 64 / 256 步 | 1.66 / 2.37 / 2.64 Gpix-step/s | 1.64 / 2.35 / 2.63 |
| OpenGL ES 2.0 GPU Burn 16 / 64 / 256 步 | 24.42 / 31.09 / 35.59 | 21.46 / 30.61 / 35.28 |

## 2026-10-08 第八台设备：小米 5C（澎湃 S1，Mali-T860）——第一台非高通设备，29 项里只有 8 项能跑

来源：[HANDOFF.md:1828](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/HANDOFF.md:1828>)

| 测试 | 成绩 | 每帧 |
|---|---|---|
| OpenGL ES 3.1 GPU Burn 16 步 | 0.029 Gpix-step/s | 1013 ms（14 帧） |
| OpenGL ES 3.1 GPU Burn 64 步 | 0.055 | 2133 ms（8 帧） |
| OpenGL ES 3.1 GPU Burn 256 步 | 0.079 | 5985 ms（只有 3 帧） |
| OpenGL ES 2.0 GPU Burn 16 / 64 / 256 步 | 0.54 / 0.60 / 0.49 | 55 / 198 / 959 ms |

| | 6 核、限频（平时的状态） | 8 核，root 之后 |
|---|---|---|
| 全核 | 251.43（6 线程） | 378.82（8 线程） |
| 小核 × 4 | 36.2–37.3（1248 MHz） | 41.3–42.0（1404 MHz，高 12.5%，正好是频率比） |
| 大核 | 54.9、55.2（两个） | 55.0–55.1（四个，和限频时一样） |

## 2026-10-08 第七台设备：小米 MIX 2S（骁龙 845），29 项跑齐；两台旧 GPU 上 Vulkan 的 GPU Burn 256 步都比 OpenGL ES 慢

来源：[HANDOFF.md:1874](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/HANDOFF.md:1874>)

| 测试 | Vulkan | OpenGL ES 3.1（墙钟计时） |
|---|---|---|
| 粒子 65,536，有画面 | 10.96 GB/s | 15.27 GB/s |
| 粒子 1,048,576，有画面 | 9.20 | 8.17 |
| 粒子 4,194,304，有画面 | 9.09 | 8.33 |
| 粒子 16,777,216，有画面 | 9.11 | 不支持 |
| 粒子 65,536，无画面 | 9.13 | 7.63 |
| 粒子 1,048,576，无画面 | 9.12 | 8.04 |
| 粒子 4,194,304，无画面 | 9.10 | 8.38 |
| 粒子 16,777,216，无画面 | 9.07 | 不支持 |
| GPU Burn 16 / 64 / 256 步 | 0.16 / 0.30 / **0.37（不稳）** Gpix-step/s | 0.12 / 0.28 / 0.48 |
| 流体 | 不支持（缺可线性过滤的 `R32_SFLOAT` 三维纹理） | — |
| 显存带宽 读 / 写 / 拷贝 | 15.3 / 25.6 / 19.2 GB/s | — |

| 每帧 ms | 16 步 | 64 步 | 256 步 |
|---|---|---|---|
| Adreno 630，Vulkan | 189 | 393 | 1263（多数帧 1195，两帧 1538） |
| Adreno 630，OpenGL ES | 251 | 426 | 990（波动 0.11%） |
| Adreno 640，Vulkan | 52 | 208 | 870（807–920 之间跳） |
| Adreno 640，OpenGL ES | 55 | 206 | 715（波动 0.26%） |
| Adreno 650，Vulkan / ES | 71 / 67 | 185 / 178 | 612 / 581 |
| Adreno 740，Vulkan / ES | 13.6 / 13.2 | 42.9 / 41.2 | 159.7 / 153.3 |

## 2026-10-08 第六台设备：小米平板 5（骁龙 860），29 项跑齐；Vulkan 的 GPU Burn 256 步不稳

来源：[HANDOFF.md:1913](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/HANDOFF.md:1913>)

| 测试 | Vulkan | OpenGL ES 3.1（墙钟计时） |
|---|---|---|
| 粒子 65,536，有画面 | 20.02 GB/s | 19.72 GB/s |
| 粒子 1,048,576，有画面 | 7.70 | 9.35 |
| 粒子 4,194,304，有画面 | 9.77 | 8.85 |
| 粒子 16,777,216，有画面 | 10.23 | 不支持 |
| 粒子 65,536，无画面 | 10.67 | 9.47 |
| 粒子 1,048,576，无画面 | 10.27 | 10.01 |
| 粒子 4,194,304，无画面 | 10.21 | 10.08 |
| 粒子 16,777,216，无画面 | 10.17 | 不支持 |
| GPU Burn 16 / 64 / 256 步 | 0.56 / 0.57 / **0.54（不稳）** Gpix-step/s | 0.53 / 0.57 / 0.66 |
| 流体 | 不支持（缺可线性过滤的 `R32_SFLOAT` 三维纹理） | — |
| 显存带宽 读 / 写 / 拷贝 | 24.5 / 26.1 / 26.2 GB/s | — |

## 2026-10-07 第五台设备：黑鲨 3 Pro（骁龙 865），29 项跑齐

来源：[HANDOFF.md:1941](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/HANDOFF.md:1941>)

| 测试 | Vulkan | OpenGL ES 3.1 |
|---|---|---|
| 粒子 65,536，有画面 | 16.43 GB/s | 20.19 GB/s |
| 粒子 1,048,576，有画面 | 12.48 | 13.29 |
| 粒子 4,194,304，有画面 | 12.40 | 13.26 |
| 粒子 16,777,216，有画面 | 12.68 | 不支持 |
| 粒子 65,536，无画面 | 14.78 | 12.96 |
| 粒子 1,048,576，无画面 | 13.82 | 13.22 |
| 粒子 4,194,304，无画面 | 13.46 | 13.07 |
| 粒子 16,777,216，无画面 | 13.46 | 不支持 |
| GPU Burn 16 / 64 / 256 步 | 0.42 / 0.64 / 0.77 Gpix-step/s | 0.44 / 0.66 / 0.81 |
| 流体 | 不支持（缺可线性过滤的 `R32_SFLOAT` 三维纹理） | — |
| 显存带宽 读 / 写 / 拷贝 | 23.5 / 32.8 / 26.5 GB/s | — |

## 2026-10-07 第四台设备：小米 MIX 4（骁龙 888+），29 项跑齐；显存带宽的"读"在它上面不可信；交换空间报错了（已修）

来源：[HANDOFF.md:1972](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/HANDOFF.md:1972>)

| 测试 | Vulkan | OpenGL ES 3.1 |
|---|---|---|
| 粒子 65,536，有画面 | 22.21 GB/s | 19.79 GB/s |
| 粒子 1,048,576，有画面 | 19.32 | 19.97 |
| 粒子 4,194,304，有画面 | 19.96 | 20.60 |
| 粒子 16,777,216，有画面 | 12.41（每步 54.1 ms） | 不支持 |
| 粒子 65,536，无画面 | 22.46 | 20.83 |
| 粒子 1,048,576，无画面 | 21.24 | 20.89 |
| 粒子 4,194,304，无画面 | 20.22 | 20.86 |
| 粒子 16,777,216，无画面 | 20.85（每步 32.2 ms） | 不支持 |
| GPU Burn 16 / 64 / 256 步 | 0.68 / 0.98 / 1.10 Gpix-step/s | 0.67 / 0.97 / 1.09 |
| 流体 | 不支持（缺可线性过滤的 `R32_SFLOAT` 三维纹理） | — |

## 2026-10-07 第三台设备：黑鲨 5 Pro（骁龙 8 Gen 1），29 项跑齐

来源：[HANDOFF.md:2012](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/HANDOFF.md:2012>)

| 测试 | Vulkan | OpenGL ES 3.1（墙钟计时） |
|---|---|---|
| 粒子 65,536，有画面 | 37.39 GB/s（0.070 ms） | 25.39 GB/s（0.103 ms） |
| 粒子 1,048,576，有画面 | 19.76 GB/s | 18.33 GB/s |
| 粒子 4,194,304，有画面 | 19.32 GB/s | 18.94 GB/s |
| 粒子 16,777,216，有画面 | 19.15 GB/s | 不支持 |
| 粒子 65,536，无画面 | 20.65 GB/s（0.127 ms，CPU 和 GPU 各占一半） | 30.80 GB/s（0.085 ms） |
| 粒子 1,048,576，无画面 | 19.69 GB/s | 18.44 GB/s |
| 粒子 4,194,304，无画面 | 19.31 GB/s | 19.01 GB/s |
| 粒子 16,777,216，无画面 | 19.24 GB/s | 不支持 |
| GPU Burn 16 / 64 / 256 步 | 1.42 / 1.82 / 1.95 Gpix-step/s | 1.36 / 1.76 / 1.90 |
| 流体 | 不支持（缺可线性过滤的 `R32_SFLOAT` 三维纹理） | — |
| 显存带宽 | 44.17 GB/s | — |

| | 第一遍 | 第二遍 | 只有大核的那个状态 |
|---|---|---|---|
| 全核 | 835.74（8 线程） | 844.99（8 线程） | 648.67（4 线程） |
| A510 × 4 | 50.6–52.4 | 51.4–52.2 | 没测到 |
| A710（cpu4、cpu5） | 162.8、162.4 | 162.9、158.6 | 163.1、163.2 |
| A710（cpu6） | **137.1** | **134.6** | 163.3 |
| X2（cpu7） | 185.9 | 184.1 | 192.0 |

| | 第一级 | 第二级 | 第三级 | 内存 |
|---|---|---|---|---|
| X2 | 89.0 / 90.3（到 512 KiB） | 59.5 / 75.8（到 4 MiB） | — | 33.2 / 46.5 |
| A710 | 61.4 / 74.3（到 32 KiB） | 47.7 / 73.9（到 512 KiB） | 33.8 / 35.1（到 6 MiB） | 24.5 / 41.1 |
| A510 | 17.7 / 19.5（到 128 KiB） | 13.0 / 14.3（到 6 MiB） | — | 10.2 / 13.4 |

## 2026-10-07 OpenGL ES 墙钟计时在手机上的两处错误（已修）；MIX Fold 3 补跑

来源：[HANDOFF.md:2066](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/HANDOFF.md:2066>)

| 测试 | OpenGL ES 3.1 | Vulkan |
|---|---|---|
| 粒子 65,536，有画面 | 12.53 GB/s（0.209 ms） | 11.09 |
| 粒子 1,048,576，有画面 | 21.41 GB/s（1.959 ms） | 20.97 |
| 粒子 4,194,304，有画面 | 24.16 GB/s（6.944 ms） | 25.99 |
| 粒子 16,777,216 | 不支持（驱动一次调度最多 65,535 个工作组） | 25.60 |
| 粒子 65,536，无画面 | 10.45 GB/s（0.251 ms） | 15.62 |
| 粒子 1,048,576，无画面 | 23.86 GB/s（1.758 ms） | 27.19 |
| 粒子 4,194,304，无画面 | 24.66 GB/s（6.803 ms） | 26.40 |
| GPU Burn 16 步 | 2.24 Gpix-step/s（13.174 ms） | **2.16**（13.624 ms，本次） |
| GPU Burn 64 步 | 2.86 Gpix-step/s（41.181 ms） | 2.75（42.9 ms，15:37 那次） |
| GPU Burn 256 步 | 3.08 Gpix-step/s（153.290 ms） | **2.95**（159.736 ms，本次） |

| OpenGL ES 2.0 的 GPU Burn（自己一组） | 成绩 | 每帧 | 是否稳定 |
|---|---|---|---|
| 16 步 | 40.17 Gpix-step/s | 0.734 ms | 稳定（CV 0.53%） |
| 64 步 | 40.07 Gpix-step/s | 2.944 ms | **不稳定**，每帧在 2.6–3.3 ms 之间来回 |
| 256 步 | 47.98 Gpix-step/s | 9.835 ms | 稳定（CV 0.19%） |

## 2026-10-07 缓存 / 内存成绩和公开资料的对照

来源：[HANDOFF.md:2116](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/HANDOFF.md:2116>)


## 2026-10-07 更正：GPU Burn 要跑 16 / 64 / 256 三档；SoC 缓存在手机上的表现

来源：[HANDOFF.md:2133](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/HANDOFF.md:2133>)

| API | 16 步 | 64 步 | 256 步 |
|---|---|---|---|
| Vulkan（GPU 时间戳） | 1.82（16.2） | 2.49（47.3） | 2.75（171.5） |
| OpenGL ES 3.1（墙钟） | 1.64（17.9） | 2.35（50.2） | 2.63（179.5） |
| OpenGL ES 2.0（墙钟，自己一组） | 21.46（1.37） | 30.61（3.85） | 35.28（13.37） |

## 2026-10-07 第二台设备：Meta Quest 3（骁龙 XR2 Gen 2）

来源：[HANDOFF.md:2253](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/HANDOFF.md:2253>)

| 测试 | Quest 3 | MIX Fold 3（同是 Adreno 740） |
|---|---|---|
| 粒子 65,536，窗口 / Headless | 27.38 / 25.51 GB/s | 11.09 / 15.62（受 CPU 限制并降频） |
| 粒子 1,048,576，窗口 / Headless | 14.32 / 14.41 GB/s | 20.97（被降频）/ 27.19 |
| 粒子 4,194,304，窗口 / Headless | 14.48 / 14.34 GB/s | 25.99 / 26.40 |
| 粒子 16,777,216，窗口 / Headless | 14.54 / 14.79 GB/s | 25.60 / 26.14 |
| GPU Burn 64 步 | 2.49 Gpix-step/s | 2.75 |
| 流体 | **5.10 MParticle-step/s**（每帧计算 432 ms + 渲染 196 ms） | 跑不了 |
| 显存带宽 读 / 写 / 复制 | 32.6 / 18.3 / 26.4 GB/s，L1 满载 197 | 53.0 / 50.3 / 47.0，L1 235 |
| CPU 算力，全核 | 608.5 MWork/s（6 线程） | 936.75（8 线程） |
| CPU 算力，逐核 | cpu0–1：96.9 / 101.4；cpu2–5：105.7 / 138.0 / 130.3 / 136.2 | A510 58–60，A715 171，A710 184，X3 200 |
| 内存读，单线程 / 全核 | 16.3 / 21.3 GB/s | 35.1 / 44.2 |
| 存储 顺序读 / 写 | 1563 / 1513 MB/s | 1787 / 1666 |
| 存储 随机读 / 写 | 33.1 / 97.3 MB/s | 19.6 / 81.0 |

