// RunView.swift — the GPU page of the iOS app.
//
// The same tests as the Android and HarmonyOS apps, started the same way: a
// test takes over the screen in landscape while it runs (GpuFullscreenRun),
// then this page shows its result and chart. The engine always renders
// 1280 x 720 and the system scales the picture to the screen.

import SwiftUI
import QuartzCore

/// A test the iOS engine host can start (src/ios_engine_host.cpp), named as
/// in the desktop app.
struct GpuTest: Identifiable, Hashable {
    let id: String
    let en: String
    let zh: String
    let ja: String

    var label: String { Localization.tr(en, zh, ja) }

    static let all: [GpuTest] = [
        GpuTest(id: "stream",
                en: "Particle — Memory Throughput",
                zh: "粒子 —— 内存吞吐",
                ja: "パーティクル — メモリスループット"),
        GpuTest(id: "stream_headless",
                en: "Particle — Compute only",
                zh: "粒子 —— 纯计算",
                ja: "パーティクル — コンピュートのみ"),
        GpuTest(id: "gpu_burn",
                en: "Plasma x Kaleidoscope — GPU Burn",
                zh: "等离子晶核 × 万花镜 —— GPU Burn",
                ja: "プラズマ核 × カレイドスコープ — GPU Burn"),
        GpuTest(id: "cinematic_liquid",
                en: "Fluid — Interactive Pool",
                zh: "流体 —— 互动水池",
                ja: "流体 — インタラクティブプール"),
    ]
}

/// A preset of a setting: its value and its name.
struct GpuPreset: Identifiable, Hashable {
    let value: Int
    let en: String
    let zh: String
    let ja: String

    var id: Int { value }
    var label: String { Localization.tr(en, zh, ja) }
}

/// The size of a GPU test, chosen as in the desktop app: the same presets,
/// the same names, the same limits. The engine host takes the choice as part
/// of the name of the test, for example "stream?particles=4194304"
/// (src/embedded_request.h).
enum GpuOptions {
    /// Stands for "Custom…" in a list of presets.
    static let custom = -1

    static let particlePresets: [GpuPreset] = [
        GpuPreset(value: 65_536, en: "Light — 65K", zh: "轻量 —— 65K", ja: "Light — 65K"),
        GpuPreset(value: 1_048_576, en: "Medium — 1M", zh: "中等 —— 1M", ja: "Medium — 1M"),
        GpuPreset(value: 4_194_304, en: "Heavy — 4M", zh: "重载 —— 4M", ja: "Heavy — 4M"),
        GpuPreset(value: 16_777_216, en: "Extreme — 16M", zh: "极限 —— 16M", ja: "Extreme — 16M"),
    ]
    static let stepPresets: [GpuPreset] = [
        GpuPreset(value: 16, en: "Light — 16", zh: "轻量 — 16", ja: "Light — 16"),
        GpuPreset(value: 64, en: "Medium — 64", zh: "中等 — 64", ja: "Medium — 64"),
        GpuPreset(value: 256, en: "Heavy — 256", zh: "重载 — 256", ja: "Heavy — 256"),
    ]
    static let secondPresets: [GpuPreset] = [
        GpuPreset(value: 3, en: "3 s", zh: "3 秒", ja: "3 秒"),
        GpuPreset(value: 15, en: "15 s", zh: "15 秒", ja: "15 秒"),
        GpuPreset(value: 60, en: "60 s", zh: "60 秒", ja: "60 秒"),
    ]

    // What the desktop app selects at first.
    static let defaultParticles = 1_048_576
    static let defaultSteps = 16
    static let defaultSeconds = 15

    static let particleRange = 1...67_108_864
    static let stepRange = 16...2048
    static let secondRange = 1...3600

    /// The value of a choice: the preset, or the number typed for "Custom…";
    /// nil when that is not valid.
    static func resolve(choice: Int, custom text: String, range: ClosedRange<Int>) -> Int? {
        if choice != custom { return choice }
        guard let value = Int(text.trimmingCharacters(in: .whitespaces)), range.contains(value) else { return nil }
        return value
    }

    /// What the engine host is asked to run.
    static func request(test: String, particles: Int, steps: Int, vsync: Bool = false) -> String {
        switch test {
        case "stream", "stream_headless": return "\(test)?particles=\(particles)&vsync=\(vsync ? 1 : 0)"
        case "gpu_burn": return "gpu_burn?steps=\(steps)&vsync=\(vsync ? 1 : 0)"
        default: return "\(test)?vsync=\(vsync ? 1 : 0)"
        }
    }

    /// The name the engine saves a run under.
    static func savedAs(test: String, steps: Int) -> String {
        switch test {
        case "stream", "stream_headless": return "stream_v1_ios_preview"
        case "gpu_burn": return "gpu_burn_v3_fixed_steps_\(steps)_kaleidoscope_ios_preview"
        case "cinematic_liquid": return "cinematic_liquid_v2_physical_scene_v8_metal_preview_ios_preview"
        default: return test
        }
    }
}

struct RunView: View {
    @EnvironmentObject var engine: BenchEngine
    private var run: BenchChannelState { engine.gpuState }

    @State private var selectedTest = "stream"
    @State private var vsync = false
    // The size of the selected test and its duration, as in the desktop app.
    @State private var particleChoice = GpuOptions.defaultParticles
    @State private var customParticles = ""
    @State private var stepChoice = GpuOptions.defaultSteps
    @State private var customSteps = ""
    @State private var secondChoice = GpuOptions.defaultSeconds
    @State private var customSeconds = ""
    /// A test is on screen, or about to be.
    @State private var fullscreen = false

    @State private var logExpanded = false
    @State private var logScrollID = 0

    private var test: GpuTest {
        GpuTest.all.first { $0.id == selectedTest } ?? GpuTest.all[0]
    }

    private var particles: Int? {
        GpuOptions.resolve(choice: particleChoice, custom: customParticles, range: GpuOptions.particleRange)
    }
    private var steps: Int? {
        GpuOptions.resolve(choice: stepChoice, custom: customSteps, range: GpuOptions.stepRange)
    }
    private var seconds: Int? {
        GpuOptions.resolve(choice: secondChoice, custom: customSeconds, range: GpuOptions.secondRange)
    }
    private var optionsValid: Bool {
        guard seconds != nil else { return false }
        switch selectedTest {
        case "stream", "stream_headless": return particles != nil
        case "gpu_burn": return steps != nil
        default: return true
        }
    }

    var body: some View {
        VStack(spacing: 0) {
            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    fullscreenCard
                    testsCard
                    settingsCard
                    summaryCard
                    GpuRunChartsCard(jobs: run.runOutput.jobs)
                    GpuMemoryResultsCard(output: run.runOutput)
                    GlassCard {
                        Button(Localization.tr("Run GPU Cache & Memory Probe", "运行 GPU 缓存与内存探测", "GPU キャッシュ / メモリ測定")) {
                            _ = engine.startPreview(workload: "memory_sweep", seconds: 0)
                        }
                        .disabled(engine.isRunning)
                    }
                    cliCard
                }
                .padding(16)
            }

            actionBar
        }
        .numericKeyboardDoneButton()
        .fullScreenCover(isPresented: $fullscreen) {
            GpuFullscreenRun(
                title: test.label,
                request: GpuOptions.request(test: selectedTest,
                                            particles: particles ?? GpuOptions.defaultParticles,
                                            steps: steps ?? GpuOptions.defaultSteps, vsync: vsync),
                seconds: seconds ?? GpuOptions.defaultSeconds,
                isPresented: $fullscreen)
                .environmentObject(engine)
        }
    }

    // MARK: - Cards

    private var fullscreenCard: some View {
        GlassCard {
            VStack(alignment: .leading, spacing: 8) {
                Text(Localization.tr("Runs full screen", "全屏运行", "全画面で実行"))
                    .font(.headline)
                Text(Localization.tr(
                    "A test takes over the screen in landscape. It always renders 1280 × 720, as the desktop "
                        + "version does, and the system scales the picture to the screen, so a score does not "
                        + "depend on the size or resolution of the display.",
                    "测试开始后横屏占满屏幕。渲染尺寸固定为 1280 × 720，和桌面版相同，再由系统缩放到屏幕，"
                        + "所以成绩不随屏幕大小和分辨率变化。",
                    "テストは横向きで画面全体を使います。描画サイズはデスクトップ版と同じ 1280 × 720 に固定され、"
                        + "システムが画面に合わせて拡大縮小するため、スコアは画面の大きさや解像度に左右されません。"))
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Text(Localization.tr("API: Metal", "图形 API：Metal", "API：Metal")
                     + " · "
                     + Localization.tr("Engine: ", "引擎：", "エンジン：")
                     + (engine.engineVersion.isEmpty ? "…" : engine.engineVersion))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    private var testsCard: some View {
        GlassCard {
            VStack(alignment: .leading, spacing: 12) {
                Text(Localization.tr("Tests", "测试项目", "テスト"))
                    .font(.headline)
                ForEach(GpuTest.all) { item in
                    Button {
                        selectedTest = item.id
                    } label: {
                        HStack(alignment: .top, spacing: 10) {
                            Image(systemName: item.id == selectedTest ? "largecircle.fill.circle" : "circle")
                                .foregroundStyle(item.id == selectedTest ? Color.accentColor : Color.gray)
                            VStack(alignment: .leading, spacing: 2) {
                                Text(item.label)
                                    .foregroundStyle(.primary)
                                Text(GpuOptions.savedAs(test: item.id, steps: steps ?? GpuOptions.defaultSteps))
                                    .font(.caption2)
                                    .foregroundStyle(.secondary)
                            }
                            Spacer(minLength: 0)
                        }
                    }
                    .buttonStyle(.plain)
                    .disabled(engine.isRunning)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    private var settingsCard: some View {
        GlassCard(padding: 16) {
            VStack(alignment: .leading, spacing: 14) {
                Text(Localization.tr("Settings", "测试设置", "設定"))
                    .font(.headline)
                if selectedTest == "stream" || selectedTest == "stream_headless" {
                    presetRow(title: Localization.tr("Particles", "粒子数", "パーティクル数"),
                              presets: GpuOptions.particlePresets,
                              choice: $particleChoice,
                              custom: $customParticles,
                              hint: Localization.tr("multiple of 256", "256 的倍数", "256 の倍数"),
                              valid: particles != nil)
                }
                if selectedTest == "gpu_burn" {
                    presetRow(title: Localization.tr("GPU Burn steps", "GPU Burn 步数", "GPU Burn ステップ数"),
                              presets: GpuOptions.stepPresets,
                              choice: $stepChoice,
                              custom: $customSteps,
                              hint: "16–2048",
                              valid: steps != nil)
                }
                presetRow(title: Localization.tr("Duration", "运行时长", "実行時間"),
                          presets: GpuOptions.secondPresets,
                          choice: $secondChoice,
                          custom: $customSeconds,
                          hint: Localization.tr("seconds, 1–3600", "秒，1–3600", "秒、1–3600"),
                          valid: seconds != nil)
                if selectedTest != "stream_headless" {
                    Toggle(Localization.tr("V-Sync", "垂直同步", "垂直同期"), isOn: $vsync)
                        .disabled(engine.isRunning)
                }
                if selectedTest == "stream" || selectedTest == "stream_headless" {
                    Text(Localization.tr(
                        "Particle size is part of the score: 65K = 2 MiB, 1M = 32 MiB, 4M = 128 MiB, 16M = 512 MiB. Small buffers may measure cache bandwidth; compare the same size and execution mode.",
                        "粒子档位也是成绩的一部分：65K = 2 MiB，1M = 32 MiB，4M = 128 MiB，16M = 512 MiB。小缓冲可能测到缓存带宽，请比较相同档位和运行模式。",
                        "サイズと実行モードが同じ結果を比較してください。小さいバッファはキャッシュ帯域を測る場合があります。"))
                        .font(.caption).foregroundStyle(.secondary)
                }
                Text(Localization.tr(
                    "Results of this app are saved as previews and are never ranked with desktop scores.",
                    "这个 App 的成绩按预览保存，不和桌面成绩一起排名。",
                    "このアプリの結果はプレビューとして保存され、デスクトップのスコアとは並べられません。"))
                    .font(.caption2)
                    .foregroundStyle(.secondary)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    /// One setting: its presets with "Custom…" at the end, and a number field
    /// while that is selected.
    private func presetRow(title: String, presets: [GpuPreset], choice: Binding<Int>, custom: Binding<String>,
                           hint: String, valid: Bool) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(title)
                .font(.subheadline)
                .foregroundStyle(.secondary)
            Picker("", selection: choice) {
                ForEach(presets) { preset in
                    Text(preset.label).tag(preset.value)
                }
                Text(Localization.tr("Custom…", "自定义…", "カスタム…")).tag(GpuOptions.custom)
            }
            .labelsHidden()
            .pickerStyle(.menu)
            .disabled(engine.isRunning)
            if choice.wrappedValue == GpuOptions.custom {
                TextField(hint, text: custom)
                    .textFieldStyle(.roundedBorder)
                    .keyboardType(.numberPad)
                    .disabled(engine.isRunning)
                if !valid {
                    Text(Localization.tr("This number is not in the allowed range.", "这个数值不在允许的范围内。",
                                         "この数値は許可された範囲にありません。"))
                        .font(.caption2)
                        .foregroundStyle(Color.orange)
                }
            }
        }
    }

    private var summaryCard: some View {
        AccentGlassCard {
            VStack(alignment: .leading, spacing: 8) {
                Text(Localization.tr("Last run", "最近一次运行", "直近の実行")).fontWeight(.semibold)
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
                Localization.tr("Raw engine output", "引擎原始输出", "エンジンの生出力"),
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
            Circle()
                .fill(engine.isRunning ? Color.orange : Color.green)
                .frame(width: 10, height: 10)
            Text(run.statusText)
                .foregroundStyle(.secondary)
                .font(.callout)
                .lineLimit(1)
            Spacer()
            if run.isRunning {
                Button(Localization.tr("Stop", "停止", "停止")) { engine.cancel(channel: .gpu) }
            }
            Button {
                if selectedTest == "stream_headless" {
                    _ = engine.startPreview(workload: GpuOptions.request(test: selectedTest,
                        particles: particles ?? GpuOptions.defaultParticles,
                        steps: steps ?? GpuOptions.defaultSteps), seconds: Double(seconds ?? GpuOptions.defaultSeconds))
                } else {
                    fullscreen = true
                }
            } label: {
                Text(Localization.tr("Run", "运行", "実行"))
            }
            .buttonStyle(.borderedProminent)
            .disabled(engine.isRunning || fullscreen || engine.workingDirectory.isEmpty || !optionsValid)
        }
        .padding(.horizontal, 16)
        .padding(.vertical, 12)
        .background(.ultraThinMaterial)
    }
}

/// The picture of a running test over the whole screen.
///
/// The test starts in the layer of this view as soon as the layer exists, and
/// the view closes itself when the run has ended. The layer keeps the 16 : 9
/// shape of the 1280 x 720 picture the engine renders.
private struct GpuFullscreenRun: View {
    @EnvironmentObject var engine: BenchEngine
    private var run: BenchChannelState { engine.gpuState }

    /// The name of the test, for the reader.
    let title: String
    /// The test and its size, for the engine host.
    let request: String
    let seconds: Int
    @Binding var isPresented: Bool

    /// The test was started in the layer of this view.
    @State private var started = false

    var body: some View {
        ZStack(alignment: .topTrailing) {
            Color.black.ignoresSafeArea()

            // MetalBenchView reports its layer once, outside any view update.
            MetalBenchView { layer in
                start(in: layer)
            }
            .aspectRatio(16.0 / 9.0, contentMode: .fit)
            .frame(maxWidth: .infinity, maxHeight: .infinity)


        }
        .contentShape(Rectangle())
        .onTapGesture { engine.cancel(channel: .gpu) }
        .statusBarHidden(true)
        .persistentSystemOverlays(.hidden)
        .onAppear { ScreenOrientation.setLandscape(true) }
        .onDisappear { engine.cancel(channel: .gpu); ScreenOrientation.setLandscape(false) }
        .onChange(of: run.isRunning) { running in
            if started && !running { isPresented = false }
        }
    }

    private func start(in layer: CAMetalLayer) {
        guard !started else { return }
        started = true
        engine.attachMetalLayer(layer)
        if !engine.startPreview(workload: request, seconds: Double(seconds)) {
            isPresented = false
        }
    }
}
