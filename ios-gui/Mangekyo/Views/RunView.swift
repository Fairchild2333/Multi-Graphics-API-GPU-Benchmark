// RunView.swift — the GPU page of the iOS app.
//
// The same tests as the Android and HarmonyOS apps, started the same way: a
// test takes over the screen in landscape while it runs (GpuFullscreenRun),
// then this page shows its result and chart. The engine always renders
// 1280 x 720 and the system scales the picture to the screen.

import SwiftUI
import QuartzCore

/// A test the iOS engine host can start (src/ios_engine_host.cpp).
///
/// A test has lighter and heavier forms. When the light one does not keep the
/// GPU busy, the frame rate stops at the refresh rate of the display and the
/// score is too low; the heavier one is for that case.
struct GpuTest: Identifiable, Hashable {
    let id: String
    let en: String
    let zh: String
    let ja: String
    /// The name the engine saves a run of this test under.
    let savedAs: String

    var label: String { Localization.tr(en, zh, ja) }

    static let all: [GpuTest] = [
        GpuTest(id: "stream",
                en: "Particle — memory throughput",
                zh: "粒子 —— 内存吞吐",
                ja: "パーティクル — メモリ帯域",
                savedAs: "stream_v1_ios_preview"),
        GpuTest(id: "stream_heavy",
                en: "Particle — memory throughput (1M)",
                zh: "粒子 —— 内存吞吐（100 万）",
                ja: "パーティクル — メモリ帯域（100 万）",
                savedAs: "stream_v1_ios_preview"),
        GpuTest(id: "gpu_burn",
                en: "GPU Burn (light 16)",
                zh: "GPU Burn（轻量 16 步）",
                ja: "GPU Burn（ライト 16）",
                savedAs: "gpu_burn_v3_fixed_steps_16_kaleidoscope_ios_preview"),
        GpuTest(id: "gpu_burn_medium",
                en: "GPU Burn (medium 64)",
                zh: "GPU Burn（中等 64 步）",
                ja: "GPU Burn（ミディアム 64）",
                savedAs: "gpu_burn_v3_fixed_steps_64_kaleidoscope_ios_preview"),
        GpuTest(id: "gpu_burn_heavy",
                en: "GPU Burn (heavy 256)",
                zh: "GPU Burn（重度 256 步）",
                ja: "GPU Burn（ヘビー 256）",
                savedAs: "gpu_burn_v3_fixed_steps_256_kaleidoscope_ios_preview"),
        GpuTest(id: "cinematic_liquid",
                en: "Fluid — interactive pool",
                zh: "流体 —— 互动水池",
                ja: "流体 — インタラクティブプール",
                savedAs: "cinematic_liquid_v2_physical_scene_v8_metal_preview_ios_preview"),
    ]
}

struct RunView: View {
    @EnvironmentObject var engine: BenchEngine

    @State private var selectedTest = "stream"
    @State private var seconds = 3
    /// A test is on screen, or about to be.
    @State private var fullscreen = false

    @State private var logExpanded = false
    @State private var logScrollID = 0

    private var test: GpuTest {
        GpuTest.all.first { $0.id == selectedTest } ?? GpuTest.all[0]
    }

    var body: some View {
        VStack(spacing: 0) {
            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    fullscreenCard
                    testsCard
                    durationCard
                    summaryCard
                    GpuRunChartsCard(jobs: engine.runOutput.jobs)
                    cliCard
                }
                .padding(16)
            }

            actionBar
        }
        .fullScreenCover(isPresented: $fullscreen) {
            GpuFullscreenRun(test: test, seconds: seconds, isPresented: $fullscreen)
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
                                Text(item.savedAs)
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

    private var durationCard: some View {
        GlassCard(padding: 16) {
            VStack(alignment: .leading, spacing: 8) {
                Text(Localization.tr("Duration", "时长", "時間"))
                    .font(.headline)
                Picker("", selection: $seconds) {
                    Text(Localization.tr("3 s", "3 秒", "3 秒")).tag(3)
                    Text(Localization.tr("10 s", "10 秒", "10 秒")).tag(10)
                    Text(Localization.tr("15 s", "15 秒", "15 秒")).tag(15)
                }
                .labelsHidden()
                .pickerStyle(.segmented)
                .disabled(engine.isRunning)
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

    private var summaryCard: some View {
        AccentGlassCard {
            VStack(alignment: .leading, spacing: 8) {
                Text(Localization.tr("Last run", "最近一次运行", "直近の実行")).fontWeight(.semibold)
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
                Localization.tr("Raw engine output", "引擎原始输出", "エンジンの生出力"),
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
            Circle()
                .fill(engine.isRunning ? Color.orange : Color.green)
                .frame(width: 10, height: 10)
            Text(engine.statusText)
                .foregroundStyle(.secondary)
                .font(.callout)
                .lineLimit(1)
            Spacer()
            Button {
                fullscreen = true
            } label: {
                Text(Localization.tr("Run", "运行", "実行"))
            }
            .buttonStyle(.borderedProminent)
            .disabled(engine.isRunning || fullscreen || engine.workingDirectory.isEmpty)
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

    let test: GpuTest
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

            Button {
                engine.cancel()
            } label: {
                Label(Localization.tr("Stop", "停止", "停止"), systemImage: "stop.fill")
            }
            .buttonStyle(.borderedProminent)
            .padding(16)
        }
        .statusBarHidden(true)
        .persistentSystemOverlays(.hidden)
        .onAppear { ScreenOrientation.setLandscape(true) }
        .onDisappear { ScreenOrientation.setLandscape(false) }
        .onChange(of: engine.isRunning) { running in
            if started && !running { isPresented = false }
        }
    }

    private func start(in layer: CAMetalLayer) {
        guard !started else { return }
        started = true
        engine.attachMetalLayer(layer)
        if !engine.startPreview(workload: test.id, seconds: Double(seconds)) {
            isPresented = false
        }
    }
}
