# ⚠️ 苹果设备：下一步要测的和有问题的成绩（2026-10-16，先读这一份）

在 Mac 上接手的人（ChatGPT / Codex）请从这里开始。每项任务都写了怎么跑、怎么判断结果对不对。原因和背景在文末链接的文档里。

## 规则

- **只测下面列出的项目。** 已有的稳定成绩不要重跑；要重跑别的，先问用户。
- **测试内容不改**（内核、着色器、计分方式、默认设置）。唯一的例外：`scripts/metal-math-probe` 如果编译不过，可以修这个小测试本身。
- 原始数据照旧放在 `out/test-results/<名字>-<日期>/`，原样保存，已经纳入 Git。
- 测完在本文件里勾选对应的项、写下结果，并在 `HANDOFF.md` 里记一笔。
- iPhone 和 iPad 的任务要等用户接上设备再做。

## Mac（M4 Pro）上的任务

### [ ] M1. CPU 算力：确认内核分支保留了，然后重跑大核、小核、全核

**背景：** macOS 版的编译器把 CPU 内核里那个随机分支改写成了条件选择指令（`csel`/`fcsel`），M4 Pro 因此不用付猜错的代价。它每个工作单元只要 13 个周期，其他平台要 16–25 个。现在的代码里用 `GPU_BENCH_KEEP_BRANCH` 保留了这个分支，只对 arm64 macOS 生效。

1. 拉取最新代码，用 `./scripts/build-macos.sh` 重新编译 Release 版。
2. 用 Xcode 的编译器确认分支保留了：
   ```bash
   xcrun clang++ -std=c++17 -O3 -DNDEBUG -arch arm64 -mmacosx-version-min=12.0 -I src -S src/cpu_benchmark.cpp -o /tmp/cpu_benchmark.s
   awk '/ror #47/{n=40} n>0{print; n--}' /tmp/cpu_benchmark.s | grep -cE '^\s+fcsel'
   awk '/ror #47/{n=40} n>0{print; n--}' /tmp/cpu_benchmark.s | grep -cE '^\s+tbn?z\s+w[0-9]+, #0'
   ```
   **对的结果：** 第一条输出 0（内核循环里没有 `fcsel`），第二条大于 0（有按第 0 位跳转的分支）。
3. 在 App 的 CPU 页跑一次正式的算力测试（大核、小核、全核，每项 15 秒）。只跑这一项，不跑缓存/内存测试。

**对的结果：**
- 新成绩的版本名里有 `_kept_branch`。
- 大核单线程明显低于原来的 312 MWork/s。
- 用日志里 `CPU_RESIDENCY` 的 `effective_ghz` 算每单元周期数（GHz × 1000 ÷ 分数），应该在 16–25 之间，而不是 13。

旧的 Mac CPU 成绩留在原来的组里，不删。

### [ ] M2. Metal 数学小测试

**背景：** GPU Burn 在苹果 GPU 上每 TFLOPS 只跑出约 1.1，NVIDIA 和 AMD 是 1.9–2.5。Burn 每一步要算二十多次 `sin`/`cos`/`atan2`/`exp`/`pow`。这个小测试量的是这些函数相对 FMA 的速度。

```bash
bash scripts/metal-math-probe/run.sh
```

- 它是在 Windows 上写的，没有编译过。编不过就修这个小测试本身（只改 `scripts/metal-math-probe/` 里的文件）。
- 结果会存到 `out/test-results/metal-math-probe-<时间>/output.txt`，把表格抄到下面"结果"一栏。

**对的结果：**
- FMA 一行折算出的 FP32 算力，20 核的 M4 Pro 应该在 8–10 TFLOPS 左右。
- `GPU Burn step` 应该不低于真实 Burn 的 9.4–9.7 Gpix-step/s。真实 Burn 每像素还要多算一段收尾的着色，所以这一行会更高一些。
- 要看的是 `sin`、`atan2`、`exp`、`pow` 的 `vs fma` 是不是远小于 1。

### [ ] M3（可选，10-14 起就挂着）

- 用新的 `membw_v2_verified` 测法，补一条 Mac 的 Vulkan 显存带宽正式成绩。
- Mac 上 Vulkan 显示粒子 1M 的吞吐会在运行中往下掉，诊断还没收尾。

## iPhone / iPad 上的任务（等用户接上设备）

### [ ] I1. iPhone 14 Pro Max（A16）：补齐 9 个没跑的项目

- **未执行的项目：**
  - 被取消的 Vulkan GPU Burn 64；
  - Vulkan GPU Burn 256、Vulkan 显存带宽、GPU 缓存诊断；
  - CPU 算力、CPU 缓存/内存/虚拟内存/存储；
  - GLES2 GPU Burn 16/64/256。
- 只跑这 9 项，不要整套重跑。每项开始前要等系统热状态回到 0。
- **可选：** Metal GPU Burn 256（1.269，运行中掉了 16.7%）和流体（9.14）当时排在发热的测试后面，凉下来以后各补测一次。

### [ ] I2. iPad Pro 11 寸一代（iPad8,1）：重测 Metal 纯计算 16M 粒子

- 现在的 20.1 GB/s，比它自己的 Vulkan（28.8）和另一台 A12X 的 Metal（29.5）都低 30%，疑似有问题。只重测这一项。
- 测完核对 GPU 时间和帧间隔是否对得上。

### [ ] I3（可选）iPhone 13 Pro（A15）

- 用新的 `membw_v2_verified` 补一条 Vulkan 显存带宽成绩（现在只有旧版 v1 的 29.3）。
- 真机上完整点一次"一键测试全部项目"的验收，一直没做过。

## 苹果这边有问题或偏低的成绩（2026-10-16 汇总）

| 设备 | 项目 | 现象 | 判断 | 处理 |
|---|---|---|---|---|
| M4 Pro | CPU 大核 312、全核 3582 | 每单元只要 13 个周期，比其他平台都少 | 编译器把分支改成了条件选择 | 代码已修，待 M1 重测；旧成绩不参与比较 |
| M4 Pro | 10-14 之前的 Metal Burn（4.2–4.8） | 记下的 GPU 时间是帧间隔的两倍 | 旧的计时错误 | 已被 9.4–9.7 取代，不再使用 |
| 所有苹果 GPU | GPU Burn | 每 TFLOPS 约 1.1，NVIDIA/AMD 是 1.9–2.5；A15、A16、A12X 只到麒麟 9000 的水平 | Burn 主要量特殊函数，苹果在这上面相对弱（推测） | M2 确认 |
| A15 | 16M 粒子 19.0、显存带宽 30.9 | 低于同代的安卓旗舰 | 64 位 LPDDR4X，是这些旗舰里最慢的内存；把带宽用起来的效率不低（0.61） | 不用处理 |
| A15 | CPU 全核内存读 34.3–35.2 GB/s | 略高于按 LPDDR4X-4266 64 位算的理论值 34.1 | 数据集每线程 12–64 MiB，6 线程合计超过 72 MiB，已超出 32 MB 系统缓存，所以不是缓存造成的；原因不明（公开的内存规格不准，或计时/字节数有约 3% 偏差） | 待查 |
| A15、A12X | CPU 大核单线程 147 和 111 | 低于骁龙 865（179） | 测试特点：这两代苹果核每单元要 20–22 个周期；频率已实测确认正常 | 不用处理 |
| A12X（两台） | 流体 6.1–6.9 | 只有 A15 的一半 | 约 12.6 MB 的模拟网格放不进它的缓存 | 不用处理 |
| A16 | Metal Burn 256 1.269、流体 9.14 | 运行中受热掉速 | 排在发热的测试后面 | I1 可选冷机补测 |
| A16 | Vulkan 1M 显示粒子、Vulkan Burn 16 | 没有稳定段；运行中掉了 19% | 稳定性提示 | 不强制重测 |
| A16 | CPU 和其余共 9 项 | 没有成绩 | 测到一半拔了线 | I1 |
| iPad8,1 | Metal 纯计算 16M 粒子 20.1 | 比自己的 Vulkan 和另一台 A12X 低 30% | 疑似错误 | I2 |
| iPad8,7 | Metal 显示粒子 16M | 两轮都没有稳定段 | 稳定性提示，计时核对通过 | 保留 |
| 所有 iOS 设备 | 65K/1M 显示粒子、Burn 16 | 偏低（A16 的 65K：显示 7.1，纯计算 64.6） | 画面跟屏幕刷新走、一次只排一帧，GPU 空闲时会降频 | 只粗看，比较时用纯计算的结果 |
| 所有 iOS 设备 | Vulkan（MoltenVK）的各项 | 比 Metal 低 30–50% | 转译开销，加上每帧做完才计时 | 苹果设备一律拿 Metal 的结果比 |
| 三台 iOS 设备和 Mac | 小核单线程 | 三种测法：Mac 是后台优先级；A15 让大核陪跑；A12X 是 Utility 优先级 | 测法不同 | 不横向比较 |

## 背景文档

- [docs/cli-reference.md](docs/cli-reference.md) 里的 "What the score measures"：CPU 算力测试量的是什么，以及 macOS 编译那个分支的问题。
- [docs/interpreting-gpu-burn.md](docs/interpreting-gpu-burn.md)：GPU Burn 量的是特殊函数的速度。
- [docs/interpreting-stream-bandwidth.md](docs/interpreting-stream-bandwidth.md)：粒子分数跟着各设备的内存带宽走。
- [docs/interpreting-cinematic-liquid.md](docs/interpreting-cinematic-liquid.md)：流体模拟看缓存，A15 比 A12X 快一倍的原因。
- [docs/apple-retest-status-20261014.md](docs/apple-retest-status-20261014.md)、[docs/a16-partial-run-20261016.md](docs/a16-partial-run-20261016.md)：之前的复测记录。

## 结果

| 任务 | 日期 | 结果 | 原始数据 |
|---|---|---|---|
| M1 | | | |
| M2 | | | |
| I1 | | | |
| I2 | | | |
