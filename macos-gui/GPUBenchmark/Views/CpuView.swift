// CpuView.swift — WinUI-aligned CPU benchmark page.

import SwiftUI

struct CpuView: View {
    @EnvironmentObject var engine: BenchEngine

    @State private var mode: String = "all"          // per-core | multi | all
    @State private var durationPreset: String = "1"  // 1 | 15
    @State private var secondsPerTest: String = "1"
    @State private var warmupSec: String = "0.2"
    @State private var logScrollID = 0

    var body: some View {
        VStack(spacing: 0) {
            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    VStack(alignment: .leading, spacing: 4) {
                        Text(Localization.tr("CPU Benchmark", "CPU 跑分", "CPU ベンチマーク"))
                            .font(.largeTitle).fontWeight(.bold)
                        Text(Localization.tr(
                            "Measures CPU compute throughput with dense math loops on logical processors (single-core and multi-core).",
                            "用密集数学循环测量逻辑处理器上的 CPU 计算吞吐（单核与多核）。",
                            "論理プロセッサ上の密集数学ループで CPU スループットを測定（単一/複数コア）。"))
                            .font(.callout)
                            .foregroundStyle(.secondary)
                    }

                    GlassCard {
                        VStack(alignment: .leading, spacing: 14) {
                            Text(infoText)
                                .font(.caption)
                                .foregroundStyle(.secondary)
                                .padding(8)
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .background(RoundedRectangle(cornerRadius: 8).fill(Color.accentColor.opacity(0.08)))

                            HStack(spacing: 16) {
                                VStack(alignment: .leading, spacing: 4) {
                                    Text(Localization.tr("Test mode", "测试模式", "テストモード"))
                                        .font(.subheadline).foregroundStyle(.secondary)
                                    Picker("", selection: $mode) {
                                        Text(Localization.tr("Each logical processor", "每个逻辑处理器", "各論理プロセッサ")).tag("per-core")
                                        Text(Localization.tr("All cores together", "全部核心一起", "全コア同時")).tag("multi")
                                        Text(Localization.tr("Per-core + all-core", "单核 + 全核", "単一+全コア")).tag("all")
                                    }
                                    .labelsHidden()
                                    .frame(minWidth: 200)
                                }

                                VStack(alignment: .leading, spacing: 4) {
                                    Text(Localization.tr("Duration preset", "时长预设", "時間プリセット"))
                                        .font(.subheadline).foregroundStyle(.secondary)
                                    Picker("", selection: $durationPreset) {
                                        Text(Localization.tr("Quick (1 s)", "快速（1 秒）", "クイック（1 秒）")).tag("1")
                                        Text(Localization.tr("Formal (15 s)", "正式（15 秒）", "正式（15 秒）")).tag("15")
                                    }
                                    .labelsHidden()
                                    .frame(width: 160)
                                    .onChange(of: durationPreset) { v in
                                        secondsPerTest = v
                                    }
                                }

                                VStack(alignment: .leading, spacing: 4) {
                                    Text(Localization.tr("Seconds per test", "每项测试秒数", "テストごとの秒数"))
                                        .font(.subheadline).foregroundStyle(.secondary)
                                    TextField("1", text: $secondsPerTest)
                                        .textFieldStyle(.roundedBorder)
                                        .frame(width: 100)
                                }

                                VStack(alignment: .leading, spacing: 4) {
                                    Text(Localization.tr("Warm-up seconds", "预热秒数", "ウォームアップ秒数"))
                                        .font(.subheadline).foregroundStyle(.secondary)
                                    TextField("0.2", text: $warmupSec)
                                        .textFieldStyle(.roundedBorder)
                                        .frame(width: 100)
                                }
                            }

                            Text(Localization.tr(
                                "Per-core duration is applied separately to every logical processor; the formal preset can take a long time.",
                                "单核时长会对每个逻辑处理器分别计时；正式预设可能耗时较长。",
                                "単一コア時間は論理プロセッサごとに適用。正式プリセットは長時間かかる場合があります。"))
                                .font(.caption2)
                                .foregroundStyle(.secondary)
                        }
                    }

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

                    AccentGlassCard {
                        VStack(alignment: .leading, spacing: 8) {
                            Text(Localization.tr("Summary", "摘要", "要約")).fontWeight(.semibold)
                            Text(engine.lastScore.isEmpty ? "—" : engine.lastScore)
                                .font(.title2).fontWeight(.semibold)
                            Button(Localization.tr("Open results folder", "打开结果文件夹", "結果フォルダを開く")) {
                                engine.openResultsFolder()
                            }
                        }
                        .frame(maxWidth: .infinity, alignment: .leading)
                    }

                    GlassCard(padding: 12) {
                        VStack(alignment: .leading, spacing: 4) {
                            Text(Localization.tr("Output", "输出", "出力"))
                                .font(.subheadline).foregroundStyle(.secondary)
                            ScrollViewReader { proxy in
                                ScrollView {
                                    Text(engine.logOutput.isEmpty
                                         ? Localization.tr("CPU output will appear here…",
                                                          "CPU 输出将显示在此处…",
                                                          "CPU 出力はここに表示されます…")
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
                .padding(28)
            }

            HStack {
                Spacer()
                Button(Localization.tr("Cancel", "取消", "キャンセル")) {
                    engine.cancel()
                }
                .disabled(!engine.isRunning)
                Button(action: runCpu) {
                    Text(Localization.tr("Run CPU Benchmark", "开始 CPU 跑分", "CPU ベンチマークを実行"))
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.large)
                .disabled(engine.isRunning || engine.workingDirectory.isEmpty)
            }
            .padding(.horizontal, 28)
            .padding(.vertical, 12)
        }
    }

    private var infoText: String {
        Localization.tr(
            "Native mixed CPU kernel (per-core and/or all-core). Results use graphicsApi=CPU.",
            "原生混合 CPU 内核（单核和/或全核）。结果使用 graphicsApi=CPU。",
            "ネイティブ混合 CPU カーネル。結果は graphicsApi=CPU。")
    }

    private func runCpu() {
        let sec = secondsPerTest.trimmingCharacters(in: .whitespaces)
        let warm = warmupSec.trimmingCharacters(in: .whitespaces)
        var args = [
            "gpu_benchmark",
            "--cpu-benchmark", mode,
            "--cpu-time", sec.isEmpty ? "1" : sec,
            "--cpu-warmup", warm.isEmpty ? "0.2" : warm,
        ]
        // Prefer direct mode; engine CLI accepts --cpu-benchmark <mode>
        engine.run(jobs: [args], needCharts: false)
    }
}
