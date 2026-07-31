// RunView.swift — WinUI-aligned GPU Run page (macOS SwiftUI).

import SwiftUI

enum BenchPreset: String, CaseIterable, Identifiable {
    case custom, fullOne, fullAll, completeSuite, fillMissing
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
        case .completeSuite:
            return Localization.tr(
                "Complete test — all formal scores, headless and native captures",
                "完整测试 —— 全部正式成绩、无头模式与原生抓帧",
                "完全テスト — 全正式スコア、ヘッドレス、ネイティブキャプチャ")
        case .fillMissing:
            return Localization.tr(
                "Fill missing — incomplete formal scores and captures",
                "补齐缺失 —— 不完整的正式成绩与抓帧",
                "不足分を補完 — 未完了の正式スコアとキャプチャ")
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

    @State private var logExpanded = false
    @State private var logScrollID = 0
    @State private var confirmFillMissing = false

    /// This page only ever reads and writes the GPU channel.
    private var run: BenchChannelState { engine.gpuState }

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
        let all = ["metal", "vulkan", "opengl"]
        return all.filter { !supportedApis.contains($0) }
    }

    private var isFormalSuitePreset: Bool {
        preset == .completeSuite || preset == .fillMissing
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

    /// Particle buffer size in MB for the current selection, or nil when the
    /// count is not a number we can read.
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
    /// reported GB/s is then cache bandwidth and can exceed what the memory
    /// system can actually deliver.
    private var particleBufferCaution: String? {
        guard selectedWorkload == "stream", let mb = particleBufferMB, mb < 64 else {
            return nil
        }
        let size = String(format: "%.0f MB", mb)
        return Localization.tr(
            "The particle buffer is only \(size), small enough to sit in cache, so the reported GB/s will be well above what memory can actually deliver — it is not a memory-bandwidth reading. Use Heavy (128 MB) or Extreme (512 MB) for that, and only compare runs that used the same size.",
            "当前粒子缓冲只有 \(size)，基本装得进缓存，报出的 GB/s 会远高于内存实际能提供的带宽——它不是内存带宽读数。要测内存带宽请用 Heavy（128 MB）或 Extreme（512 MB），且只在相同档位之间比较。",
            "パーティクルバッファは \(size) しかなくキャッシュに収まるため、表示される GB/s はメモリ帯域ではありません。Heavy（128 MB）以上を使い、同じサイズ同士でのみ比較してください。")
    }

    // MARK: - Option explanations (mirrors the Windows GUI's info glyphs)

    private var headlessHint: String {
        Localization.tr(
            "Pure compute mode: no swapchain, no rendering, no present. Measures raw compute throughput, so scores are much higher than a windowed run of the same workload and are not comparable with it. GPU capture is unavailable while headless.",
            "纯计算模式：无交换链、无渲染、无 present。测量的是原始计算吞吐，因此成绩会明显高于同一负载的窗口运行，两者不可直接比较。Headless 下无法抓帧。",
            "純計算モード：スワップチェーン・描画・present なし。生のコンピュートスループットを測るため、同じワークロードのウィンドウ実行より大幅に高く出ます（相互比較不可）。ヘッドレス中はキャプチャできません。")
    }

    private var vsyncHint: String {
        Localization.tr(
            "Cap presentation to the display refresh rate. macOS then presents directly instead of using the decoupled presenter, so the frame rate is pinned to the panel (120 Hz on ProMotion). Leave it off for throughput runs.",
            "把呈现限制到显示器刷新率。macOS 下开启后会改为直接呈现、不走解耦呈现线程，帧率被锁定在屏幕刷新率（ProMotion 为 120Hz）。测吞吐时请保持关闭。",
            "呈示をディスプレイのリフレッシュレートに制限します。macOS では直接呈示に切り替わり、フレームレートがパネル（ProMotion なら 120Hz）に固定されます。")
    }

    private var hostMemoryHint: String {
        Localization.tr(
            "Keep the particle buffer in system RAM instead of VRAM. Apple Silicon has unified memory and no separate VRAM to overflow, so the Metal backend always uses shared storage and this switch has no effect here. It is meaningful only for the Vulkan and OpenGL backends on discrete GPUs.",
            "把粒子缓冲放在系统内存而非显存。Apple Silicon 是统一内存，没有独立显存可爆，Metal 后端始终使用 shared 存储，因此该开关在这里不起作用。它只对独立显卡上的 Vulkan 与 OpenGL 后端有意义。",
            "パーティクルバッファを VRAM ではなくシステム RAM に置きます。Apple Silicon はユニファイドメモリのため Metal バックエンドでは効果がありません。")
    }

    private var legacyHint: String {
        Localization.tr(
            "Show older and experimental workloads in the Workload list. They are kept for comparison and are not part of the formal score set; several are Vulkan-oriented and may be rejected by Metal.",
            "在负载列表中显示旧版与实验性测试项。它们仅供对比，不属于正式成绩集；其中若干偏 Vulkan 路径，Metal 可能会拒绝运行。",
            "ワークロード一覧に旧版・実験的な項目を表示します。比較用で、正式スコアには含まれません。")
    }

    private var captureHint: String {
        Localization.tr(
            "Record one GPU frame at the given second. On macOS this uses Metal's built-in MTLCaptureManager and writes a .gputrace to the Captures folder — nothing to install, though Xcode is needed to open the file. Capture adds overhead, but its time is excluded from the score. It runs no later than one second before the test ends and is unavailable for runs of one second or less, or while Headless is on.",
            "在指定秒数抓取一帧 GPU 数据。macOS 使用 Metal 内置的 MTLCaptureManager，把 .gputrace 写入 Captures 文件夹——无需安装任何东西，但打开该文件需要 Xcode。抓帧会带来开销，不过这段时间不计入成绩。自动抓帧最晚在测试结束前 1 秒；时长不超过 1 秒或开启 Headless 时不可用。",
            "指定秒数で GPU フレームを 1 枚記録します。macOS では Metal 内蔵の MTLCaptureManager を使い、.gputrace を Captures に保存します（導入不要、閲覧には Xcode が必要）。")
    }

    private var infoBanner: String {
        if preset == .completeSuite {
            return Localization.tr(
                "Runs the formal 15-second matrix on every detected GPU and compiled macOS API: four Particle sizes windowed + headless, GPU Burn, Cinematic Liquid, and native captures where supported.",
                "在每个已检测 GPU 与已编译的 macOS API 上运行正式 15 秒矩阵：四档粒子窗口/无头、GPU Burn、互动水池，并在支持时进行原生抓帧。",
                "検出した全 GPU とビルド済み macOS API で正式 15 秒マトリクス（4 段階 Particle、ウィンドウ/ヘッドレス、GPU Burn、Cinematic Liquid、対応時のネイティブキャプチャ）を実行します。")
        }
        if preset == .fillMissing {
            return Localization.tr(
                "Checks saved results and the real capture directory, then runs only missing formal passes. Known unsupported and previously failed combinations are reported without retrying.",
                "检查已保存成绩与真实抓帧目录，仅运行缺失的正式项目；已知不支持或此前失败的组合只报告、不重试。",
                "保存済み結果と実キャプチャフォルダを確認し、不足する正式パスだけを実行します。既知の非対応/失敗済み組み合わせは再試行せず報告します。")
        }
        switch selectedWorkload {
        case "stream":
            return Localization.tr(
                """
                Particle baseline: compute update + point sprite draw. The GB/s figure is derived — particles × an assumed 40 bytes moved each ÷ compute time — not a hardware counter. The kernel actually moves about 48 bytes per particle (reads 32, writes back 16), so the reported value runs roughly 17% conservative. Treat it as a number for comparing the same workload across devices and APIs, not against a vendor's peak-bandwidth spec: a simple streaming kernel typically reaches only 60-70% of that peak, which is shared with the CPU and display.
                """,
                """
                粒子基线：compute 更新 + 点精灵绘制。GB/s 是推算值——粒子数 × 假定的每粒子 40 字节 ÷ compute 时间——不是硬件计数器读数。内核实际每粒子搬运约 48 字节（读 32、回写 16），因此报出的数值偏保守约 17%。请把它当作同一负载在不同设备/API 之间的横向对比指标，不要和厂商标称的峰值带宽对比：简单流式内核通常只能达到标称峰值的 60–70%，而标称值还要与 CPU、显示输出共享。
                """,
                """
                パーティクル基線：compute 更新 + 点描画。GB/s は「粒子数 × 想定 40 バイト ÷ compute 時間」から導いた値でハードウェアカウンタではありません。実際は約 48 バイト/粒子を移動するため約 17% 控えめです。カタログのピーク帯域ではなく、同一ワークロードの機種間比較に使ってください。
                """)
        case "gpu_burn":
            return Localization.tr(
                "Fullscreen Plasma×Kaleidoscope burn; fixed steps/draw, 2 opaque draws/frame. Metal supported.",
                "全屏等离子×万花镜 Burn；固定步数/次绘制，每帧 2 次不透明绘制。Metal 已支持。",
                "全画面 Plasma×カレイド Burn。固定ステップ、2 不透明ドロー/フレーム。Metal 対応。")
        case "cinematic_liquid":
            return Localization.tr(
                "Interactive pool. Metal uses MLS-MPM simulation and native raymarch presentation; its score remains a separate _metal_preview contract.",
                "互动水池。Metal 使用 MLS-MPM 模拟与原生光线行进呈现；成绩仍属于独立的 _metal_preview 合同。",
                "インタラクティブプール。Metal は MLS-MPM とネイティブ raymarch 描画を使い、スコアは独立した _metal_preview 契約です。")
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
                    Text(Localization.tr("GPU Benchmark", "GPU 测试", "GPU ベンチマーク"))
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
        .confirmationDialog(
            Localization.tr(
                "Run missing formal tests?",
                "运行缺失的正式测试？",
                "不足する正式テストを実行しますか？"),
            isPresented: $confirmFillMissing
        ) {
            Button(Localization.tr("Run missing tests", "运行缺失项", "不足分を実行")) {
                startBenchmark()
            }
            Button(Localization.tr("Cancel", "取消", "キャンセル"), role: .cancel) {}
        } message: {
            Text(Localization.tr(
                "Mangekyo will inspect saved scores and capture files, then launch only genuinely missing runnable passes.",
                "Mangekyo 将检查已保存成绩与抓帧文件，然后只启动确实缺失且可运行的项目。",
                "保存済みスコアとキャプチャを確認し、本当に不足している実行可能なパスだけを開始します。"))
        }
    }

    // MARK: - Cards

    private var optionsCard: some View {
        GlassCard {
            VStack(alignment: .leading, spacing: FormMetrics.rowSpacing) {
                VStack(alignment: .leading, spacing: FormMetrics.labelSpacing) {
                    Text(Localization.tr("Preset", "预设", "プリセット"))
                        .font(.headline)
                    Picker("", selection: $preset) {
                        ForEach(BenchPreset.allCases) { p in
                            Text(p.label).tag(p)
                        }
                    }
                    .labelsHidden()
                    .pickerStyle(.menu)
                    .frame(maxWidth: 560, alignment: .leading)
                }

                FormRow {
                    FormField(title: "GPU", width: 190) {
                        Picker("", selection: $selectedGpuIndex) {
                            Text(Localization.tr("(auto)", "（自动）", "（自動）")).tag(-1)
                            ForEach(engine.gpus) { g in
                                Text("\(g.index): \(g.name)").tag(g.index)
                            }
                        }
                    }

                    FormField(
                        title: Localization.tr("Graphics API", "图形 API", "グラフィックス API"),
                        width: 175
                    ) {
                        apiMenu
                    }

                    FormField(
                        title: Localization.tr("Workload", "负载", "ワークロード"),
                        width: 250
                    ) {
                        Picker("", selection: $selectedWorkload) {
                            ForEach(visibleWorkloads) { w in
                                Text(w.label).tag(w.id)
                            }
                        }
                    }
                }
                .disabled(isFormalSuitePreset)

                FormBanner(text: infoBanner)

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

                FormRow {
                    FormField(
                        title: Localization.tr("Duration", "时长", "時間"),
                        width: 150
                    ) {
                        Picker("", selection: $durationUnit) {
                            ForEach(DurationUnit.allCases) { u in
                                Text(u.label).tag(u)
                            }
                        }
                    }

                    if durationUnit != .unlimited {
                        FormField(
                            title: Localization.tr("Value", "数值", "値"),
                            width: 110
                        ) {
                            TextField(durationUnit == .frames ? "600" : "15", text: $durationValue)
                                .textFieldStyle(.roundedBorder)
                        }
                    }

                    if selectedWorkload == "synthpeak" {
                        FormField(
                            title: Localization.tr("Precision", "精度", "精度"),
                            width: 110
                        ) {
                            Picker("", selection: $selectedPrecision) {
                                Text("fp32").tag("fp32")
                                Text("fp16").tag("fp16")
                                Text("int32").tag("int32")
                                // fp64 hidden for Apple — still listed under legacy when showLegacy
                                if showLegacy { Text("fp64").tag("fp64") }
                            }
                        }
                    }

                    if usesParticles {
                        FormField(
                            title: Localization.tr("Particles", "粒子数", "パーティクル数"),
                            width: 180
                        ) {
                            // Buffer size is shown because the presets straddle
                            // the cache/memory boundary: the small ones report
                            // cache bandwidth, not memory bandwidth.
                            Picker("", selection: $particlePreset) {
                                Text("Light — 65K · 2 MB").tag("65536")
                                Text("Medium — 1M · 32 MB").tag("1048576")
                                Text("Heavy — 4M · 128 MB").tag("4194304")
                                Text("Extreme — 16M · 512 MB").tag("16777216")
                                Text(Localization.tr("Custom…", "自定义…", "カスタム…")).tag("custom")
                            }
                        }
                        if particlePreset == "custom" {
                            FormField(
                                title: Localization.tr(
                                    "Custom count", "自定义数量", "カスタム数"),
                                width: 150
                            ) {
                                TextField("multiple of 256", text: $customParticles)
                                    .textFieldStyle(.roundedBorder)
                            }
                        }
                    }

                    if usesBurnSteps {
                        FormField(
                            title: Localization.tr(
                                "GPU Burn steps", "GPU Burn 步数", "GPU Burn ステップ"),
                            width: 170
                        ) {
                            Picker("", selection: $burnPreset) {
                                Text("Light — 16").tag("16")
                                Text("Medium — 64").tag("64")
                                Text("Heavy — 256").tag("256")
                                Text(Localization.tr("Custom…", "自定义…", "カスタム…")).tag("custom")
                            }
                        }
                        if burnPreset == "custom" {
                            FormField(
                                title: Localization.tr(
                                    "Custom steps", "自定义步数", "カスタムステップ"),
                                width: 130
                            ) {
                                TextField("16–2048", text: $customBurnSteps)
                                    .textFieldStyle(.roundedBorder)
                            }
                        }
                    }
                }
                .disabled(isFormalSuitePreset)
            }
        }
    }

    /// Native pull-down button so the API control matches the height and
    /// chrome of the pickers beside it while keeping multi-selection.
    private var apiMenu: some View {
        Menu {
            Button(Localization.tr("All supported", "全选支持项", "対応をすべて")) {
                selectedApis = Set(supportedApis)
            }
            Button(Localization.tr("None (Auto)", "无（自动）", "なし（自動）")) {
                selectedApis.removeAll()
            }
            Divider()
            Text(Localization.tr("Supported", "支持", "対応"))
            ForEach(supportedApis, id: \.self) { api in
                Toggle(api.uppercased(), isOn: bindingForApi(api))
            }
            if !unsupportedApis.isEmpty {
                Divider()
                Text(Localization.tr(
                    "Not reported as supported", "未报告为支持", "未対応として報告"))
                ForEach(unsupportedApis, id: \.self) { api in
                    Toggle(api.uppercased(), isOn: bindingForApi(api))
                }
            }
        } label: {
            Text(apiSummary).lineLimit(1)
        }
        .help(Localization.tr(
            "Unsupported selections remain available and will be reported by the CLI.",
            "仍可勾选不受支持的项；CLI 会报告失败原因。",
            "非対応の選択も可能で、CLI が理由を報告します。"))
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
            VStack(alignment: .leading, spacing: 12) {
                Text(Localization.tr("Advanced", "高级选项", "詳細オプション"))
                    .font(.headline)
                HStack(alignment: .firstTextBaseline, spacing: 18) {
                    HintedToggle(title: "Headless", hint: headlessHint, isOn: $headless)
                        .disabled(usesBurnSteps || selectedWorkload == "cinematic_liquid")
                    HintedToggle(title: "V-Sync", hint: vsyncHint, isOn: $vsync)
                    HintedToggle(
                        title: Localization.tr("System memory", "系统内存", "システムメモリ"),
                        hint: hostMemoryHint,
                        isOn: $hostMemory)
                    HintedToggle(
                        title: Localization.tr("Show legacy & advanced tests",
                                               "显示旧版与高级测试",
                                               "レガシー/高度テストを表示"),
                        hint: legacyHint,
                        isOn: $showLegacy)
                    Spacer(minLength: 0)
                }
                .disabled(isFormalSuitePreset)

                HStack(alignment: .firstTextBaseline, spacing: 10) {
                    HintedToggle(
                        title: Localization.tr("Capture at", "捕获于", "キャプチャ位置"),
                        hint: captureHint,
                        isOn: $captureOn)
                        // Headless has no swapchain and no render pass, so
                        // there is nothing to capture; the worker reports
                        // captureUnavailable rather than producing a trace.
                        .disabled(durationUnit == .unlimited || headless)
                    if captureOn && !headless && durationUnit != .unlimited {
                        TextField("5", text: $captureSec)
                            .textFieldStyle(.roundedBorder)
                            .labelsHidden()
                            .frame(width: 64)
                        Text("s").foregroundStyle(.secondary)
                    }
                    Spacer(minLength: 0)
                }
                .disabled(isFormalSuitePreset)

                Text(headless
                     ? Localization.tr(
                        "Headless runs have no swapchain or render pass, so GPU capture is unavailable. Clear Headless to capture a frame.",
                        "Headless 没有交换链和渲染流程，因此无法抓帧。需要抓帧请取消勾选 Headless。",
                        "ヘッドレスにはスワップチェーンも描画パスもないため、キャプチャできません。")
                     : Localization.tr(
                        "Metal capture uses MTLCaptureManager and writes a .gputrace file to Captures. If a requested capture backend is unavailable, the run records captureUnavailable; capture time is excluded from the score.",
                        "Metal 抓帧已使用 MTLCaptureManager，并将 .gputrace 写入 Captures。若所请求的抓帧后端不可用，运行会记录 captureUnavailable；抓帧时间不计入成绩。",
                        "Metal キャプチャは MTLCaptureManager を使い、.gputrace を Captures に保存します。要求したキャプチャ手段が使えない場合は captureUnavailable を記録し、キャプチャ時間はスコアから除外します。"))
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
            }
        }
    }

    private var progressCard: some View {
        GlassCard(padding: 16) {
            VStack(alignment: .leading, spacing: 8) {
                HStack {
                    Text(run.statusText).fontWeight(.semibold).lineLimit(1)
                    Spacer(minLength: 12)
                    Text(run.progressLabel).foregroundStyle(.secondary).monospacedDigit()
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
                if run.runSummary.completed > 0 ||
                    run.runSummary.skipped > 0 ||
                    !run.runSummary.issues.isEmpty {
                    HStack(spacing: 14) {
                        summaryMetric(
                            Localization.tr("Succeeded", "成功", "成功"),
                            value: run.runSummary.succeeded,
                            color: .green)
                        summaryMetric(
                            Localization.tr("Failed", "失败", "失敗"),
                            value: run.runSummary.failed,
                            color: run.runSummary.failed > 0 ? .red : .secondary)
                        summaryMetric(
                            Localization.tr("Skipped", "跳过", "スキップ"),
                            value: run.runSummary.skipped,
                            color: run.runSummary.skipped > 0 ? .orange : .secondary)
                    }
                    .font(.caption)
                }
                if !run.runSummary.issues.isEmpty {
                    Divider()
                    Text(Localization.tr("Run issues", "运行问题", "実行時の問題"))
                        .font(.subheadline)
                        .fontWeight(.semibold)
                    ForEach(Array(run.runSummary.issues.prefix(8))) { issue in
                        Label {
                            VStack(alignment: .leading, spacing: 2) {
                                Text(issue.message)
                                if !issue.target.isEmpty {
                                    Text(issue.target)
                                        .font(.caption2)
                                        .foregroundStyle(.secondary)
                                }
                            }
                        } icon: {
                            Image(systemName: issue.kind == .workerTimeout
                                  ? "clock.badge.exclamationmark"
                                  : "exclamationmark.triangle.fill")
                                .foregroundStyle(.orange)
                        }
                        .font(.caption)
                    }
                    if run.runSummary.issues.count > 8 {
                        Text(Localization.tr(
                            "…and \(run.runSummary.issues.count - 8) more; see Raw CLI output.",
                            "……另有 \(run.runSummary.issues.count - 8) 项；请查看原始 CLI 输出。",
                            "ほか \(run.runSummary.issues.count - 8) 件。生の CLI 出力を確認してください。"))
                            .font(.caption2)
                            .foregroundStyle(.secondary)
                    }
                }
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
                Localization.tr("Raw GPU CLI output", "GPU 原始 CLI 输出", "GPU の生 CLI 出力"),
                isExpanded: $logExpanded
            ) {
                ScrollViewReader { proxy in
                    ScrollView {
                        Text(run.logOutput.isEmpty
                             ? Localization.tr("GPU benchmark output will appear here…",
                                              "GPU 测试输出将显示在此处…",
                                              "GPU ベンチマーク出力はここに表示されます…")
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
                ProgressView().controlSize(.small)
            }
            Button(Localization.tr("Cancel", "取消", "キャンセル")) {
                engine.cancel(channel: .gpu)
            }
            .disabled(!run.isRunning)
            Button(action: runBenchmark) {
                Text(isFormalSuitePreset
                     ? Localization.tr("Run Formal Suite", "运行正式套件", "正式スイートを実行")
                     : Localization.tr("Run GPU Test", "开始 GPU 测试", "GPU ベンチマークを実行"))
            }
            .buttonStyle(.borderedProminent)
            .controlSize(.large)
            .disabled(engine.isRunning)
        }
        .padding(.horizontal, 28)
        .padding(.vertical, 12)
    }

    private func summaryMetric(_ label: String, value: Int, color: Color) -> some View {
        HStack(spacing: 4) {
            Circle().fill(color).frame(width: 7, height: 7)
            Text("\(label): \(value)").monospacedDigit()
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
        if preset == .fillMissing {
            confirmFillMissing = true
            return
        }
        startBenchmark()
    }

    private func startBenchmark() {
        var needCharts = false
        let jobs = buildPresetJobs(needCharts: &needCharts)
        guard !jobs.isEmpty else {
            engine.gpuState.logOutput += Localization.tr(
                "[GUI] Nothing to run — check API / workload selection.\n",
                "[GUI] 无可运行项 —— 请检查 API / 负载选择。\n",
                "[GUI] 実行対象なし — API / ワークロードを確認。\n")
            return
        }
        engine.run(jobs: jobs, needCharts: needCharts, channel: .gpu)
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

        case .completeSuite:
            return [["gpu_benchmark", "--gui-worker", "--complete-suite"]]

        case .fillMissing:
            return [["gpu_benchmark", "--gui-worker", "--fill-missing", "--yes"]]
        }
    }
}
