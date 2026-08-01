// RunView.swift — GPU Run page adapted for iOS.

import SwiftUI

enum BenchPreset: String, CaseIterable, Identifiable {
    case custom
    var id: String { rawValue }

    var label: String {
        switch self {
        case .custom:
            return Localization.tr(
                "Custom run (choose GPU / workload)",
                "自定义运行（选择 GPU / 负载）",
                "カスタム実行（GPU / ワークロードを選択）")
        }
    }
}

enum DurationUnit: String, CaseIterable, Identifiable {
    case seconds, minutes, hours, frames, unlimited
    var id: String { rawValue }
    var label: String {
        switch self {
        case .seconds:   return Localization.tr("Seconds", "秒", "秒")
        case .minutes:   return Localization.tr("Minutes", "分钟", "分")
        case .hours:     return Localization.tr("Hours", "小时", "時間")
        case .frames:    return Localization.tr("Frames", "帧", "フレーム")
        case .unlimited: return Localization.tr("Until Cancel", "直到取消", "キャンセルまで")
        }
    }
}

struct WorkloadChoice: Identifiable, Hashable {
    let id: String
    let en: String
    let zh: String
    let ja: String
    let primary: Bool
    var label: String { Localization.tr(en, zh, ja) }
}

struct RunView: View {
    @EnvironmentObject var engine: BenchEngine

    /// This page only ever reads and writes the GPU channel.
    private var run: BenchChannelState { engine.gpuState }

    @State private var showStage = false
    @State private var pendingJobs: [[String]] = []

    @State private var preset: BenchPreset = .custom
    @State private var selectedGpuIndex: Int = -1
    @State private var selectedWorkload: String = "stream"
    @State private var selectedPrecision: String = "fp32"
    @State private var showLegacy = false

    @State private var durationUnit: DurationUnit = .seconds
    @State private var durationValue: String = "3"

    @State private var particlePreset: String = "1048576"
    @State private var customParticles: String = ""
    @State private var burnPreset: String = "16"
    @State private var customBurnSteps: String = ""

    @State private var headless = false
    @State private var vsync = false
    @State private var hostMemory = false
    @State private var captureOn = true
    @State private var captureSec: String = "5"

    @State private var logExpanded = false
    @State private var logScrollID = 0

    private let primaryWorkloads: [WorkloadChoice] = [
        .init(id: "stream", en: "Particle — Memory Throughput", zh: "粒子 —— 内存吞吐", ja: "パーティクル — メモリ帯域", primary: true),
        .init(id: "gpu_burn", en: "Plasma x Kaleidoscope — GPU Burn", zh: "等离子晶核 × 万花镜 —— GPU Burn", ja: "プラズマ核 × カレイドスコープ — GPU Burn", primary: true),
        .init(id: "cinematic_liquid", en: "Fluid — Interactive Pool", zh: "流体 —— 互动水池", ja: "流体 — インタラクティブプール", primary: true),
    ]
    private let advancedWorkloads: [WorkloadChoice] = [
        .init(id: "nbody", en: "N-Body — Advanced Compute", zh: "N 体 —— 高级计算", ja: "N 体 — 高度計算", primary: false),
        .init(id: "synthpeak", en: "SynthPeak — Advanced Synthetic", zh: "SynthPeak —— 高级合成", ja: "SynthPeak — 高度合成", primary: false),
        .init(id: "stress", en: "Legacy Stress v1 — Fragment ALU/SFU", zh: "旧版压力 v1 —— 片元 ALU/SFU", ja: "旧ストレステスト v1", primary: false),
        .init(id: "render3d", en: "Legacy 3D Prototype — Billboards", zh: "旧版 3D 原型 —— 广告牌", ja: "旧 3D プロトタイプ", primary: false),
        .init(id: "volumetric", en: "Volumetric — Experimental Raymarch", zh: "体素 —— 实验性光线行进", ja: "ボリューメトリック — 実験", primary: false),
    ]

    private var visibleWorkloads: [WorkloadChoice] {
        showLegacy ? primaryWorkloads + advancedWorkloads : primaryWorkloads
    }

    private var usesParticles: Bool {
        ["stream", "nbody", "render3d"].contains(selectedWorkload)
    }
    private var usesBurnSteps: Bool { selectedWorkload == "gpu_burn" }

    // MARK: - Option explanations (ported from the macOS RunView)

    private var headlessHint: String {
        Localization.tr(
            "Pure compute mode: no swapchain, no rendering, no present. Scores come out much higher than a windowed run of the same workload and are not comparable with it. GPU capture is unavailable while headless.",
            "纯计算模式：无交换链、无渲染、无 present。成绩会明显高于同一负载的窗口运行，两者不可直接比较。Headless 下无法抓帧。",
            "純計算モード。ウィンドウ実行より大幅に高く出るため相互比較はできません。")
    }

    private var vsyncHint: String {
        Localization.tr(
            "Cap presentation to the display refresh rate. Leave it off for throughput runs.",
            "把呈现限制到屏幕刷新率。测吞吐时请保持关闭。",
            "呈示をリフレッシュレートに制限します。スループット測定ではオフに。")
    }

    private var hostMemoryHint: String {
        Localization.tr(
            "Keep the particle buffer in system RAM instead of VRAM. Apple silicon has unified memory and no separate VRAM to overflow, so this switch has no effect here. It is meaningful only for the Vulkan and OpenGL backends on discrete GPUs.",
            "把粒子缓冲放在系统内存而非显存。Apple 芯片是统一内存，没有独立显存可爆，该开关在这里不起作用。它只对独立显卡上的 Vulkan 与 OpenGL 后端有意义。",
            "パーティクルバッファをシステム RAM に置きます。Apple silicon はユニファイドメモリのため効果はありません。")
    }

    /// Particle buffer size in MB for the current selection.
    private var particleBufferMB: Double? {
        guard usesParticles else { return nil }
        let raw = particlePreset == "custom"
            ? customParticles.trimmingCharacters(in: .whitespaces)
            : particlePreset
        guard let count = Double(raw), count > 0 else { return nil }
        // Particle = float4 position + float4 velocity.
        return count * 32.0 / (1024.0 * 1024.0)
    }

    /// Warn when the working set is small enough to live in cache, because the
    /// reported GB/s is then cache bandwidth rather than memory bandwidth.
    private var particleBufferCaution: String? {
        guard selectedWorkload == "stream", let mb = particleBufferMB, mb < 64
        else { return nil }
        let size = String(format: "%.0f MB", mb)
        return Localization.tr(
            "The particle buffer is only \(size), small enough to sit in cache, so the reported GB/s will be well above what memory can deliver. Use Heavy (128 MB) or Extreme (512 MB) for a memory reading, and only compare runs that used the same size.",
            "当前粒子缓冲只有 \(size)，基本装得进缓存，报出的 GB/s 会远高于内存实际能提供的带宽。要测内存带宽请用 Heavy（128 MB）或 Extreme（512 MB），且只在相同档位之间比较。",
            "パーティクルバッファは \(size) しかなくキャッシュに収まるため、メモリ帯域ではありません。")
    }

    private var infoBanner: String {
        switch selectedWorkload {
        case "stream":
            return Localization.tr(
                "Particle baseline; the embedded host writes stream_v1_ios_preview, not the desktop 15 s contract. The GB/s figure is derived — particles x an assumed 40 bytes each / compute time — not a hardware counter. The kernel actually moves about 48 B per particle, so the value runs roughly 17% conservative. Compare the same workload across devices, not against a vendor peak.",
                "粒子基线；嵌入宿主写出 stream_v1_ios_preview，不是桌面的 15 秒合同。GB/s 是推算值——粒子数 × 假定的每粒子 40 字节 ÷ compute 时间——不是硬件计数器读数。内核实际每粒子搬运约 48 字节，因此偏保守约 17%。请横向比较同一负载在不同设备上的表现，不要和厂商标称峰值对比。",
                "パーティクル基線。GB/s は想定 40 バイト/粒子から導いた値で、実際は約 48 バイト。カタログのピークとは比較しないでください。")
        case "gpu_burn":
            return Localization.tr(
                "Burn light=16. Writes gpu_burn_v3_fixed_steps_16_kaleidoscope_ios_preview.",
                "Burn light=16。写出 gpu_burn_v3_fixed_steps_16_kaleidoscope_ios_preview。",
                "Burn light=16。書き出し gpu_burn_v3_fixed_steps_16_kaleidoscope_ios_preview。")
        case "cinematic_liquid":
            return Localization.tr(
                "Interactive pool. Not on ios_engine_host yet; desktop Metal uses …_metal_preview.",
                "互动水池。尚未接 ios_engine_host；桌面 Metal 为 …_metal_preview。",
                "インタラクティブプール。ios_engine_host 未接続。デスクトップ Metal は …_metal_preview。")
        case "synthpeak":
            return Localization.tr(
                "Synthetic peak FLOPS/IOPS. FP64 unsupported on Apple GPUs.",
                "合成峰值 FLOPS/IOPS。Apple GPU 不支持 FP64。",
                "合成ピーク。Apple GPU は FP64 非対応。")
        default:
            return Localization.tr("Advanced / experimental workload.", "高级 / 实验负载。", "高度 / 実験ワークロード。")
        }
    }

    var body: some View {
        VStack(spacing: 0) {
            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    engineCard
                    optionsCard
                    advancedCard
                    progressCard
                    summaryCard
                    cliCard
                }
                .padding(16)
            }

            actionBar
        }
        .numericKeyboardDoneButton()
        .fullScreenCover(isPresented: $showStage) {
            BenchmarkStageView {
                engine.run(jobs: pendingJobs, needCharts: false, channel: .gpu)
            }
            .environmentObject(engine)
        }
        .onAppear {
            if !showLegacy && !primaryWorkloads.contains(where: { $0.id == selectedWorkload }) {
                selectedWorkload = "stream"
            }
        }
    }

    // MARK: - Cards

    private var engineCard: some View {
        GlassCard {
            VStack(alignment: .leading, spacing: 8) {
                Text(Localization.tr(
                    "Engine: \(engine.engineVersion.isEmpty ? "…" : engine.engineVersion). stream/gpu_burn use embedded gpu_engine (ios_preview).",
                    "引擎：\(engine.engineVersion.isEmpty ? "…" : engine.engineVersion)。stream/gpu_burn 走嵌入式 gpu_engine（ios_preview）。",
                    "エンジン：\(engine.engineVersion.isEmpty ? "…" : engine.engineVersion)。stream/gpu_burn は埋め込み gpu_engine（ios_preview）。"))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }

    private var optionsCard: some View {
        GlassCard {
            VStack(alignment: .leading, spacing: 16) {
                labeledPicker(Localization.tr("Preset", "预设", "プリセット"), selection: $preset) {
                    ForEach(BenchPreset.allCases) { p in
                        Text(p.label).tag(p)
                    }
                }

                VStack(alignment: .leading, spacing: 4) {
                    Text("GPU").font(.subheadline).foregroundStyle(.secondary)
                    Picker("", selection: $selectedGpuIndex) {
                        Text(Localization.tr("(auto)", "（自动）", "（自動）")).tag(-1)
                        ForEach(engine.gpus) { g in
                            Text("\(g.index): \(g.name)").tag(g.index)
                        }
                    }
                    .labelsHidden()
                }

                VStack(alignment: .leading, spacing: 4) {
                    Text(Localization.tr("Workload", "负载", "ワークロード"))
                        .font(.subheadline).foregroundStyle(.secondary)
                    Picker("", selection: $selectedWorkload) {
                        ForEach(visibleWorkloads) { w in
                            Text(w.label).tag(w.id)
                        }
                    }
                    .labelsHidden()
                }

                if selectedWorkload == "synthpeak" {
                    VStack(alignment: .leading, spacing: 4) {
                        Text(Localization.tr("Precision", "精度", "精度"))
                            .font(.subheadline).foregroundStyle(.secondary)
                        Picker("", selection: $selectedPrecision) {
                            Text("fp32").tag("fp32")
                            Text("fp16").tag("fp16")
                            Text("int32").tag("int32")
                        }
                        .labelsHidden()
                    }
                }

                Text(infoBanner)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(8)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .background(RoundedRectangle(cornerRadius: 8).fill(Color.accentColor.opacity(0.08)))

                if let caution = particleBufferCaution {
                    Label {
                        Text(caution).fixedSize(horizontal: false, vertical: true)
                    } icon: {
                        Image(systemName: "exclamationmark.triangle.fill")
                            .foregroundStyle(.orange)
                    }
                    .font(.caption)
                    .foregroundStyle(.secondary)
                }

                VStack(alignment: .leading, spacing: 12) {
                    VStack(alignment: .leading, spacing: 4) {
                        Text(Localization.tr("Duration", "时长", "时间"))
                            .font(.subheadline).foregroundStyle(.secondary)
                        Picker("", selection: $durationUnit) {
                            ForEach(DurationUnit.allCases) { u in
                                Text(u.label).tag(u)
                            }
                        }
                        .labelsHidden()
                    }

                    if durationUnit != .unlimited {
                        VStack(alignment: .leading, spacing: 4) {
                            Text(Localization.tr("Value", "数值", "値"))
                                .font(.subheadline).foregroundStyle(.secondary)
                            TextField(durationUnit == .frames ? "600" : "15", text: $durationValue)
                                .textFieldStyle(.roundedBorder)
                                .keyboardType(.numberPad)
                        }
                    }

                    if usesParticles {
                        VStack(alignment: .leading, spacing: 4) {
                            Text(Localization.tr("Particles", "粒子数", "パーティクル数"))
                                .font(.subheadline).foregroundStyle(.secondary)
                            Picker("", selection: $particlePreset) {
                                Text("Light — 65K · 2 MB").tag("65536")
                                Text("Medium — 1M · 32 MB").tag("1048576")
                                Text("Heavy — 4M · 128 MB").tag("4194304")
                                Text("Extreme — 16M · 512 MB").tag("16777216")
                                Text(Localization.tr("Custom…", "自定义…", "カスタム…")).tag("custom")
                            }
                            .labelsHidden()
                        }
                        if particlePreset == "custom" {
                            TextField("multiple of 256", text: $customParticles)
                                .textFieldStyle(.roundedBorder)
                                .keyboardType(.numberPad)
                        }
                    }

                    if usesBurnSteps {
                        VStack(alignment: .leading, spacing: 4) {
                            Text(Localization.tr("GPU Burn steps", "GPU Burn 步数", "GPU Burn ステップ"))
                                .font(.subheadline).foregroundStyle(.secondary)
                            Picker("", selection: $burnPreset) {
                                Text("Light — 16").tag("16")
                                Text("Medium — 64").tag("64")
                                Text("Heavy — 256").tag("256")
                                Text(Localization.tr("Custom…", "自定义…", "カスタム…")).tag("custom")
                            }
                            .labelsHidden()
                        }
                        if burnPreset == "custom" {
                            TextField("16–2048", text: $customBurnSteps)
                                .textFieldStyle(.roundedBorder)
                                .keyboardType(.numberPad)
                        }
                    }
                }
            }
        }
    }

    private var advancedCard: some View {
        GlassCard(padding: 16) {
            VStack(alignment: .leading, spacing: 14) {
                Text(Localization.tr("Advanced", "高级选项", "詳細オプション"))
                    .font(.headline)

                HintedToggle(title: "Headless", hint: headlessHint, isOn: $headless)
                    .disabled(usesBurnSteps || selectedWorkload == "cinematic_liquid")
                    .toggleStyle(.switch)

                HintedToggle(title: "V-Sync", hint: vsyncHint, isOn: $vsync)
                    .toggleStyle(.switch)

                HintedToggle(
                    title: Localization.tr("System memory", "系统内存", "システムメモリ"),
                    hint: hostMemoryHint, isOn: $hostMemory)
                    .toggleStyle(.switch)

                Toggle(Localization.tr("Show legacy & advanced tests",
                                       "显示旧版与高级测试",
                                       "レガシー/高度テストを表示"),
                       isOn: $showLegacy)
                    .toggleStyle(.switch)

                Toggle(Localization.tr("Capture at", "捕获于", "キャプチャ位置"), isOn: $captureOn)
                    .disabled(headless)
                    .disabled(durationUnit == .unlimited)
                    .toggleStyle(.switch)

                if captureOn && durationUnit != .unlimited {
                    HStack {
                        TextField("5", text: $captureSec)
                            .textFieldStyle(.roundedBorder)
                            .keyboardType(.numberPad)
                            .frame(width: 80)
                        Text("s").foregroundStyle(.secondary)
                    }
                }

                Text(Localization.tr(
                    "On iOS, capture records captureUnavailable until MTLCaptureManager is wired; the capture window is still excluded from scores when requested.",
                    "iOS 上在接入 MTLCaptureManager 前会诚实标记 captureUnavailable；请求抓帧时成绩仍排除该窗口。",
                    "iOS では MTLCaptureManager 接続前は captureUnavailable。要求時は成績からキャプチャ窓を除外。"))
                    .font(.caption2)
                    .foregroundStyle(.secondary)
            }
        }
    }

    private var progressCard: some View {
        GlassCard(padding: 16) {
            VStack(alignment: .leading, spacing: 8) {
                HStack {
                    Text(run.statusText).fontWeight(.semibold)
                    Spacer()
                    Text(run.progressLabel).foregroundStyle(.secondary)
                }
                ProgressView(value: run.progressFraction)
            }
        }
    }

    private var summaryCard: some View {
        AccentGlassCard {
            VStack(alignment: .leading, spacing: 8) {
                Text(Localization.tr("Summary", "摘要", "要約")).fontWeight(.semibold)
                Text(run.lastScore.isEmpty ? "—" : run.lastScore)
                    .font(.title2).fontWeight(.semibold)
                    .foregroundStyle(run.lastScore.isEmpty ? .secondary : .primary)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    private var cliCard: some View {
        GlassCard(padding: 12) {
            DisclosureGroup(
                Localization.tr("Raw CLI output", "原始 CLI 输出", "生の CLI 出力"),
                isExpanded: $logExpanded
            ) {
                ScrollViewReader { proxy in
                    ScrollView {
                        Text(run.logOutput.isEmpty
                             ? Localization.tr("Benchmark output will appear here…",
                                              "基准测试输出将显示在此处…",
                                              "ベンチマーク出力はここに表示されます…")
                             : run.logOutput)
                            .font(.system(.caption, design: .monospaced))
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .textSelection(.enabled)
                            .id(logScrollID)
                    }
                    .frame(height: 180)
                    .onChange(of: run.logOutput) { _ in
                        logScrollID += 1
                        proxy.scrollTo(logScrollID, anchor: .bottom)
                    }
                }
            }
        }
    }

    private var actionBar: some View {
        HStack {
            Spacer()
            Circle()
                .fill(run.isRunning ? Color.orange : Color.green)
                .frame(width: 10, height: 10)
            Text(run.statusText)
                .foregroundStyle(.secondary)
                .font(.callout)
                .lineLimit(1)
            if run.isRunning {
                ProgressView()
            }
            Button(Localization.tr("Cancel", "取消", "キャンセル")) {
                engine.cancel(channel: .gpu)
            }
            .disabled(!run.isRunning)
            Button(action: runBenchmark) {
                Text(Localization.tr("Run GPU Test", "开始 GPU 测试", "GPU ベンチマークを実行"))
            }
            .buttonStyle(.borderedProminent)
            .disabled(engine.isRunning || engine.workingDirectory.isEmpty)
        }
        .padding(.horizontal, 16)
        .padding(.vertical, 12)
        .background(.ultraThinMaterial)
    }

    private func labeledPicker<V: Hashable, Content: View>(
        _ title: String, selection: Binding<V>, @ViewBuilder content: () -> Content
    ) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(title).font(.headline)
            Picker("", selection: selection, content: content)
                .labelsHidden()
                .pickerStyle(.menu)
                .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    // MARK: - Job building

    private func durationArgs() -> [String] {
        if durationUnit == .unlimited { return ["--no-time-limit"] }
        let raw = Double(durationValue.trimmingCharacters(in: .whitespaces)) ?? 15
        switch durationUnit {
        case .frames:
            return ["--benchmark", "\(Int(raw))"]
        case .minutes:
            return ["--time", "\(Int(raw * 60))"]
        case .hours:
            return ["--time", "\(Int(raw * 3600))"]
        default:
            if raw == floor(raw) { return ["--time", "\(Int(raw))"] }
            return ["--time", String(raw)]
        }
    }

    private func particleArgs() -> [String] {
        guard usesParticles else { return [] }
        let n = particlePreset == "custom"
            ? customParticles.trimmingCharacters(in: .whitespaces)
            : particlePreset
        guard !n.isEmpty else { return [] }
        let flag = selectedWorkload == "nbody" ? "--bodies" : "--particles"
        return [flag, n]
    }

    private func burnArgs() -> [String] {
        guard usesBurnSteps else { return [] }
        let n = burnPreset == "custom"
            ? customBurnSteps.trimmingCharacters(in: .whitespaces)
            : burnPreset
        guard !n.isEmpty else { return [] }
        return ["--iter", n]
    }

    private func captureArgs() -> [String] {
        guard captureOn, durationUnit != .unlimited else { return [] }
        let s = captureSec.trimmingCharacters(in: .whitespaces)
        return ["--capture", s.isEmpty ? "5" : s]
    }

    private func buildOne(backend: String, gpu: Int?, extra: [String] = []) -> [String] {
        var a = ["gpu_benchmark"] + durationArgs()
        a += ["--backend", "metal"] // iOS: metal-only
        if let g = gpu, g >= 0 { a += ["--gpu", "\(g)"] }
        if selectedWorkload != "stream" { a += ["--workload", selectedWorkload] }
        if selectedWorkload == "synthpeak" { a += ["--precision", selectedPrecision] }
        a += particleArgs()
        a += burnArgs()
        if headless { a.append("--headless") }
        if vsync { a.append("--vsync") }
        if hostMemory { a.append("--host-memory") }
        a += captureArgs()
        a += extra
        return a
    }

    private func runBenchmark() {
        let jobs = buildPresetJobs()
        guard !jobs.isEmpty else {
            engine.gpuState.logOutput += Localization.tr(
                "[GUI] Nothing to run — check workload selection.\n",
                "[GUI] 无可运行项 —— 请检查负载选择。\n",
                "[GUI] 実行対象なし — ワークロードを確認。\n")
            return
        }
        pendingJobs = jobs
        showStage = true
    }

    private func buildPresetJobs() -> [[String]] {
        let gpu = selectedGpuIndex
        return [buildOne(backend: "metal", gpu: gpu >= 0 ? gpu : nil)]
    }
}
