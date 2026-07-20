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

    @State private var preset: BenchPreset = .custom
    @State private var selectedGpuIndex: Int = -1
    @State private var selectedWorkload: String = "stream"
    @State private var selectedPrecision: String = "fp32"
    @State private var showLegacy = false

    @State private var durationUnit: DurationUnit = .seconds
    @State private var durationValue: String = "15"

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

    private var infoBanner: String {
        switch selectedWorkload {
        case "stream":
            return Localization.tr(
                "Particle baseline: compute update + point sprite draw. Score is memory-throughput oriented.",
                "粒子基线：计算更新 + 点精灵绘制。分数偏内存吞吐。",
                "パーティクル基線：計算更新 + 点描画。スコアは帯域寄り。")
        case "gpu_burn":
            return Localization.tr(
                "Fullscreen Plasma×Kaleidoscope burn; fixed steps/draw, 2 draws/frame. Metal supported.",
                "全屏等离子×万花镜 Burn；固定步数/次绘制，每帧 2 次不透明绘制。Metal 已支持。",
                "全画面 Plasma×カレイド Burn。固定ステップ、2 ドロー/フレーム。Metal 対応。")
        case "cinematic_liquid":
            return Localization.tr(
                "Interactive pool. Metal = MLS-MPM + preview render (_metal_preview).",
                "互动水池。Metal = MLS-MPM + 预览渲染（_metal_preview）。",
                "インタラクティブプール。Metal=MLS-MPM+プレビュー（_metal_preview）。")
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
        .onAppear {
            if !showLegacy && !primaryWorkloads.contains(where: { $0.id == selectedWorkload }) {
                selectedWorkload = "stream"
            }
        }
    }

    // MARK: - Cards

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
                    .padding(8)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .background(RoundedRectangle(cornerRadius: 8).fill(Color.accentColor.opacity(0.08)))

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
                                Text("Light — 65K").tag("65536")
                                Text("Medium — 1M").tag("1048576")
                                Text("Heavy — 4M").tag("4194304")
                                Text("Extreme — 16M").tag("16777216")
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

                Toggle("Headless", isOn: $headless)
                    .disabled(usesBurnSteps || selectedWorkload == "cinematic_liquid")
                    .toggleStyle(.switch)

                Toggle("V-Sync", isOn: $vsync)
                    .toggleStyle(.switch)

                Toggle(Localization.tr("System memory", "系统内存", "システムメモリ"), isOn: $hostMemory)
                    .toggleStyle(.switch)

                Toggle(Localization.tr("Show legacy & advanced tests",
                                       "显示旧版与高级测试",
                                       "レガシー/高度テストを表示"),
                       isOn: $showLegacy)
                    .toggleStyle(.switch)

                Toggle(Localization.tr("Capture at", "捕获于", "キャプチャ位置"), isOn: $captureOn)
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
                    Text(engine.statusText).fontWeight(.semibold)
                    Spacer()
                    Text(engine.progressLabel).foregroundStyle(.secondary)
                }
                ProgressView(value: engine.progressFraction)
            }
        }
    }

    private var summaryCard: some View {
        AccentGlassCard {
            VStack(alignment: .leading, spacing: 8) {
                Text(Localization.tr("Summary", "摘要", "要約")).fontWeight(.semibold)
                Text(engine.lastScore.isEmpty ? "—" : engine.lastScore)
                    .font(.title2).fontWeight(.semibold)
                    .foregroundStyle(engine.lastScore.isEmpty ? .secondary : .primary)
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
                        Text(engine.logOutput.isEmpty
                             ? Localization.tr("Benchmark output will appear here…",
                                              "基准测试输出将显示在此处…",
                                              "ベンチマーク出力はここに表示されます…")
                             : engine.logOutput)
                            .font(.system(.caption, design: .monospaced))
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .textSelection(.enabled)
                            .id(logScrollID)
                    }
                    .frame(height: 180)
                    .onChange(of: engine.logOutput) { _ in
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
                .fill(engine.isRunning ? Color.orange : Color.green)
                .frame(width: 10, height: 10)
            Text(engine.statusText)
                .foregroundStyle(.secondary)
                .font(.callout)
                .lineLimit(1)
            if engine.isRunning {
                ProgressView()
            }
            Button(Localization.tr("Cancel", "取消", "キャンセル")) {
                engine.cancel()
            }
            .disabled(!engine.isRunning)
            Button(action: runBenchmark) {
                Text(Localization.tr("Run GPU Benchmark", "开始 GPU 跑分", "GPU ベンチマークを実行"))
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
            engine.logOutput += Localization.tr(
                "[GUI] Nothing to run — check workload selection.\n",
                "[GUI] 无可运行项 —— 请检查负载选择。\n",
                "[GUI] 実行対象なし — ワークロードを確認。\n")
            return
        }
        engine.run(jobs: jobs, needCharts: false)
    }

    private func buildPresetJobs() -> [[String]] {
        let gpu = selectedGpuIndex
        return [buildOne(backend: "metal", gpu: gpu >= 0 ? gpu : nil)]
    }
}
