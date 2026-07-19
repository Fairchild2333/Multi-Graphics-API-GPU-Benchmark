// RunView.swift — WinUI-aligned GPU Run page (macOS SwiftUI).

import SwiftUI

enum BenchPreset: String, CaseIterable, Identifiable {
    case custom, fullOne, fullAll
    var id: String { rawValue }

    var label: String {
        switch self {
        case .custom:
            return Localization.tr(
                "Custom run (choose API / GPU / workload)",
                "自定义运行（选择 API / GPU / 负载）",
                "カスタム実行（API / GPU / ワークロードを選択）")
        case .fullOne:
            return Localization.tr(
                "Full analysis — selected workload / one GPU (selected APIs + capture + charts)",
                "完整分析 —— 所选负载 / 单 GPU（所选 API + 抓帧 + 图表）",
                "フル分析 — 選択ワークロード / 1 GPU（選択 API + キャプチャ + チャート）")
        case .fullAll:
            return Localization.tr(
                "Full analysis — selected workload / all GPUs × selected APIs (+ capture + charts)",
                "完整分析 —— 所选负载 / 全部 GPU × 所选 API（+ 抓帧 + 图表）",
                "フル分析 — 選択ワークロード / 全 GPU × 選択 API（+ キャプチャ + チャート）")
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

    /// Empty = Auto (best available). Otherwise selected API tokens.
    @State private var selectedApis: Set<String> = []
    @State private var showApiPicker = false

    @State private var logExpanded = false
    @State private var logScrollID = 0

    private let primaryWorkloads: [WorkloadChoice] = [
        .init(id: "stream", en: "Particle — Memory Throughput", zh: "粒子 —— 内存吞吐", ja: "パーティクル — メモリ帯域", primary: true),
        .init(id: "gpu_burn", en: "Plasma x Kaleidoscope — GPU Burn", zh: "等离子晶核 × 万花镜 —— GPU Burn", ja: "プラズマ核 × カレイドスコープ — GPU Burn", primary: true),
        .init(id: "cinematic_liquid", en: "Fluid — Interactive Pool", zh: "流体 —— 互动水池", ja: "流体 — インタラクティブプール", primary: true),
    ]
    private let advancedWorkloads: [WorkloadChoice] = [
        .init(id: "gpu_stress", en: "GraphicsBurn v1 / Component (Advanced)", zh: "GraphicsBurn v1 / 组件（高级）", ja: "GraphicsBurn v1 / コンポーネント（上級）", primary: false),
        .init(id: "nbody", en: "N-Body — Advanced Compute", zh: "N 体 —— 高级计算", ja: "N 体 — 高度計算", primary: false),
        .init(id: "synthpeak", en: "SynthPeak — Advanced Synthetic", zh: "SynthPeak —— 高级合成", ja: "SynthPeak — 高度合成", primary: false),
        .init(id: "stress", en: "Legacy Stress v1 — Fragment ALU/SFU", zh: "旧版压力 v1 —— 片元 ALU/SFU", ja: "旧ストレステスト v1", primary: false),
        .init(id: "render3d", en: "Legacy 3D Prototype — Billboards", zh: "旧版 3D 原型 —— 广告牌", ja: "旧 3D プロトタイプ", primary: false),
        .init(id: "volumetric", en: "Volumetric — Experimental Raymarch", zh: "体素 —— 实验性光线行进", ja: "ボリューメトリック — 実験", primary: false),
        .init(id: "fluid", en: "Other / Legacy 2D Fluid", zh: "其他 / 旧版 2D 流体", ja: "その他 / 旧 2D 流体", primary: false),
        .init(id: "cinematic_liquid_v1", en: "Other / Legacy Cinematic Liquid v1", zh: "其他 / 旧版电影化液体 v1", ja: "その他 / 旧シネマティック液体 v1", primary: false),
    ]

    private var visibleWorkloads: [WorkloadChoice] {
        showLegacy ? primaryWorkloads + advancedWorkloads : primaryWorkloads
    }

    private var supportedApis: [String] {
        var s: [String] = []
        if engine.gpus.contains(where: { $0.supportsMetal }) { s.append("metal") }
        if engine.gpus.contains(where: { $0.supportsVulkan }) { s.append("vulkan") }
        if engine.gpus.contains(where: { $0.supportsOpenGL }) { s.append("opengl") }
        if s.isEmpty { s = ["metal", "vulkan", "opengl"] }
        return s
    }

    private var unsupportedApis: [String] {
        let all = ["metal", "vulkan", "opengl", "dx12", "dx11"]
        return all.filter { !supportedApis.contains($0) }
    }

    private var apiSummary: String {
        if selectedApis.isEmpty {
            return Localization.tr("Auto (best available)", "自动（最佳可用）", "自動（最良）")
        }
        return selectedApis.sorted().map { $0.uppercased() }.joined(separator: ", ")
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
                "Fullscreen Plasma×Kaleidoscope burn; fixed steps/draw, 2 opaque draws/frame. Metal supported.",
                "全屏等离子×万花镜 Burn；固定步数/次绘制，每帧 2 次不透明绘制。Metal 已支持。",
                "全画面 Plasma×カレイド Burn。固定ステップ、2 不透明ドロー/フレーム。Metal 対応。")
        case "cinematic_liquid":
            return Localization.tr(
                "Interactive pool. Vulkan = formal contract; Metal = MLS-MPM + stub present (_metal_preview).",
                "互动水池。Vulkan = 正式合同；Metal = MLS-MPM + 占位呈现（_metal_preview）。",
                "インタラクティブプール。Vulkan=正式、Metal=MLS-MPM+スタブ（_metal_preview）。")
        case "gpu_stress":
            return Localization.tr(
                "GraphicsBurn component — not supported on Metal.",
                "GraphicsBurn 组件 —— Metal 不受支持。",
                "GraphicsBurn — Metal 非対応。")
        case "synthpeak":
            return Localization.tr(
                "Synthetic peak FLOPS/IOPS. FP64 unsupported on Apple GPUs.",
                "合成峰值 FLOPS/IOPS。Apple GPU 不支持 FP64。",
                "合成ピーク。Apple GPU は FP64 非対応。")
        case "fluid", "cinematic_liquid_v1":
            return Localization.tr(
                "Legacy / Vulkan-oriented path; Metal may reject.",
                "旧版 / 偏 Vulkan 路径；Metal 可能拒绝。",
                "レガシー / Vulkan 寄り。Metal は拒否する場合あり。")
        default:
            return Localization.tr("Advanced / experimental workload.", "高级 / 实验负载。", "高度 / 実験ワークロード。")
        }
    }

    var body: some View {
        VStack(spacing: 0) {
            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    Text(Localization.tr("GPU Benchmark", "GPU 跑分", "GPU ベンチマーク"))
                        .font(.largeTitle).fontWeight(.bold)

                    optionsCard
                    advancedCard
                    progressCard
                    summaryCard
                    cliCard
                }
                .padding(28)
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

                HStack(alignment: .top, spacing: 16) {
                    VStack(alignment: .leading, spacing: 4) {
                        Text("GPU").font(.subheadline).foregroundStyle(.secondary)
                        Picker("", selection: $selectedGpuIndex) {
                            Text(Localization.tr("(auto)", "（自动）", "（自動）")).tag(-1)
                            ForEach(engine.gpus) { g in
                                Text("\(g.index): \(g.name)").tag(g.index)
                            }
                        }
                        .labelsHidden()
                        .frame(minWidth: 200)
                    }

                    VStack(alignment: .leading, spacing: 4) {
                        Text(Localization.tr("Graphics API", "图形 API", "グラフィックス API"))
                            .font(.subheadline).foregroundStyle(.secondary)
                        Button {
                            showApiPicker.toggle()
                        } label: {
                            HStack {
                                Text(apiSummary).lineLimit(1)
                                Spacer()
                                Image(systemName: "chevron.up.chevron.down")
                                    .font(.caption)
                            }
                            .padding(.horizontal, 8)
                            .padding(.vertical, 6)
                            .background(RoundedRectangle(cornerRadius: 6).stroke(.secondary.opacity(0.4)))
                        }
                        .buttonStyle(.plain)
                        .frame(minWidth: 180)
                        .popover(isPresented: $showApiPicker, arrowEdge: .bottom) {
                            apiPickerPopover
                        }
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
                        .frame(minWidth: 240)
                    }

                    if selectedWorkload == "synthpeak" {
                        VStack(alignment: .leading, spacing: 4) {
                            Text(Localization.tr("Precision", "精度", "精度"))
                                .font(.subheadline).foregroundStyle(.secondary)
                            Picker("", selection: $selectedPrecision) {
                                Text("fp32").tag("fp32")
                                Text("fp16").tag("fp16")
                                Text("int32").tag("int32")
                                // fp64 hidden for Apple — still listed under legacy when showLegacy
                                if showLegacy { Text("fp64").tag("fp64") }
                            }
                            .labelsHidden()
                            .frame(width: 100)
                        }
                    }
                }

                Text(infoBanner)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .padding(8)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .background(RoundedRectangle(cornerRadius: 8).fill(Color.accentColor.opacity(0.08)))

                HStack(alignment: .top, spacing: 16) {
                    VStack(alignment: .leading, spacing: 4) {
                        Text(Localization.tr("Duration", "时长", "時間"))
                            .font(.subheadline).foregroundStyle(.secondary)
                        Picker("", selection: $durationUnit) {
                            ForEach(DurationUnit.allCases) { u in
                                Text(u.label).tag(u)
                            }
                        }
                        .labelsHidden()
                        .frame(width: 140)
                    }

                    if durationUnit != .unlimited {
                        VStack(alignment: .leading, spacing: 4) {
                            Text(Localization.tr("Value", "数值", "値"))
                                .font(.subheadline).foregroundStyle(.secondary)
                            TextField(durationUnit == .frames ? "600" : "15", text: $durationValue)
                                .textFieldStyle(.roundedBorder)
                                .frame(width: 100)
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
                            .frame(width: 180)
                        }
                        if particlePreset == "custom" {
                            TextField("multiple of 256", text: $customParticles)
                                .textFieldStyle(.roundedBorder)
                                .frame(width: 140)
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
                            .frame(width: 160)
                        }
                        if burnPreset == "custom" {
                            TextField("16–2048", text: $customBurnSteps)
                                .textFieldStyle(.roundedBorder)
                                .frame(width: 100)
                        }
                    }
                }
            }
        }
    }

    private var apiPickerPopover: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Button(Localization.tr("All supported", "全选支持项", "対応をすべて")) {
                    selectedApis = Set(supportedApis)
                }
                Button(Localization.tr("None (Auto)", "无（自动）", "なし（自動）")) {
                    selectedApis.removeAll()
                }
            }
            Text(Localization.tr("Supported", "支持", "対応"))
                .font(.headline)
            ForEach(supportedApis, id: \.self) { api in
                Toggle(api.uppercased(), isOn: bindingForApi(api))
            }
            if !unsupportedApis.isEmpty {
                Text(Localization.tr("Not reported as supported", "未报告为支持", "未対応として報告"))
                    .font(.headline)
                    .padding(.top, 4)
                ForEach(unsupportedApis, id: \.self) { api in
                    Toggle(api.uppercased(), isOn: bindingForApi(api))
                }
                Text(Localization.tr(
                    "Unsupported selections remain available and will be reported by the CLI.",
                    "仍可勾选不受支持的项；CLI 会报告失败原因。",
                    "非対応の選択も可能で、CLI が理由を報告します。"))
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                    .frame(maxWidth: 280)
            }
        }
        .padding(14)
        .toggleStyle(.checkbox)
    }

    private func bindingForApi(_ api: String) -> Binding<Bool> {
        Binding(
            get: { selectedApis.contains(api) },
            set: { on in
                if on { selectedApis.insert(api) } else { selectedApis.remove(api) }
            }
        )
    }

    private var advancedCard: some View {
        GlassCard(padding: 16) {
            VStack(alignment: .leading, spacing: 10) {
                Text(Localization.tr("Advanced", "高级选项", "詳細オプション"))
                    .font(.headline)
                HStack(spacing: 20) {
                    Toggle("Headless", isOn: $headless)
                        .disabled(usesBurnSteps || selectedWorkload == "cinematic_liquid")
                    Toggle("V-Sync", isOn: $vsync)
                    Toggle(Localization.tr("System memory", "系统内存", "システムメモリ"), isOn: $hostMemory)
                    Toggle(Localization.tr("Show legacy & advanced tests",
                                           "显示旧版与高级测试",
                                           "レガシー/高度テストを表示"),
                           isOn: $showLegacy)
                }
                .toggleStyle(.checkbox)

                HStack(spacing: 12) {
                    Toggle(Localization.tr("Capture at", "捕获于", "キャプチャ位置"), isOn: $captureOn)
                        .disabled(durationUnit == .unlimited)
                    if captureOn && durationUnit != .unlimited {
                        TextField("5", text: $captureSec)
                            .textFieldStyle(.roundedBorder)
                            .frame(width: 60)
                        Text("s").foregroundStyle(.secondary)
                    }
                }
                .toggleStyle(.checkbox)

                Text(Localization.tr(
                    "On macOS, capture records captureUnavailable until MTLCaptureManager is wired; the capture window is still excluded from scores when requested.",
                    "macOS 上在接入 MTLCaptureManager 前会诚实标记 captureUnavailable；请求抓帧时成绩仍排除该窗口。",
                    "macOS では MTLCaptureManager 接続前は captureUnavailable。要求時は成績からキャプチャ窓を除外。"))
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
                HStack(spacing: 8) {
                    Button(Localization.tr("Open results folder", "打开结果文件夹", "結果フォルダを開く")) {
                        engine.openResultsFolder()
                    }
                    Button(Localization.tr("Open captures folder", "打开抓帧文件夹", "キャプチャフォルダを開く")) {
                        engine.openCapturesFolder()
                    }
                }
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
                ProgressView().controlSize(.small)
            }
            Button(Localization.tr("Cancel", "取消", "キャンセル")) {
                engine.cancel()
            }
            .disabled(!engine.isRunning)
            Button(action: runBenchmark) {
                Text(Localization.tr("Run GPU Benchmark", "开始 GPU 跑分", "GPU ベンチマークを実行"))
            }
            .buttonStyle(.borderedProminent)
            .controlSize(.large)
            .disabled(engine.isRunning || engine.workingDirectory.isEmpty)
        }
        .padding(.horizontal, 28)
        .padding(.vertical, 12)
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

    private func backendsForJob() -> [String] {
        if selectedApis.isEmpty { return [""] } // Auto
        return selectedApis.sorted()
    }

    private func buildOne(backend: String, gpu: Int?, extra: [String] = []) -> [String] {
        var a = ["gpu_benchmark"] + durationArgs()
        if !backend.isEmpty { a += ["--backend", backend] }
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
        var needCharts = false
        let jobs = buildPresetJobs(needCharts: &needCharts)
        guard !jobs.isEmpty else {
            engine.logOutput += Localization.tr(
                "[GUI] Nothing to run — check API / workload selection.\n",
                "[GUI] 无可运行项 —— 请检查 API / 负载选择。\n",
                "[GUI] 実行対象なし — API / ワークロードを確認。\n")
            return
        }
        engine.run(jobs: jobs, needCharts: needCharts)
    }

    private func buildPresetJobs(needCharts: inout Bool) -> [[String]] {
        needCharts = false
        let gpu = selectedGpuIndex

        // Skip known-hopeless liquid×non-vulkan / stress×metal when Auto expands.
        func allow(_ backend: String) -> Bool {
            if backend.isEmpty { return true }
            if selectedWorkload == "cinematic_liquid" && backend != "vulkan" && backend != "metal" {
                return false
            }
            if selectedWorkload == "cinematic_liquid_v1" && backend != "vulkan" { return false }
            if selectedWorkload == "fluid" && backend != "vulkan" { return false }
            if selectedWorkload == "gpu_stress" && backend == "metal" { return false }
            return true
        }

        switch preset {
        case .custom:
            let backs = backendsForJob().filter(allow)
            if backs == [""] { return [buildOne(backend: "", gpu: gpu >= 0 ? gpu : nil)] }
            return backs.map { buildOne(backend: $0, gpu: gpu >= 0 ? gpu : nil) }

        case .fullOne:
            needCharts = true
            let backs = (selectedApis.isEmpty ? supportedApis : Array(selectedApis))
                .sorted()
                .filter(allow)
            return backs.map { buildOne(backend: $0, gpu: gpu >= 0 ? gpu : nil) }

        case .fullAll:
            needCharts = true
            if selectedWorkload.hasPrefix("cinematic_liquid") || selectedWorkload == "fluid" {
                // Cross-API suite rejected for liquid; run allowed backends only.
                let backs = (selectedApis.isEmpty ? ["vulkan", "metal"] : Array(selectedApis))
                    .sorted()
                    .filter(allow)
                return backs.map { buildOne(backend: $0, gpu: gpu >= 0 ? gpu : nil) }
            }
            var a = ["gpu_benchmark"] + durationArgs() + ["--run-all"]
            if selectedWorkload != "stream" { a += ["--workload", selectedWorkload] }
            if selectedWorkload == "synthpeak" { a += ["--precision", selectedPrecision] }
            a += particleArgs()
            a += burnArgs()
            a += captureArgs()
            return [a]
        }
    }
}
