// CpuView.swift — iOS CPU benchmark page.

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

                            VStack(alignment: .leading, spacing: 12) {
                                VStack(alignment: .leading, spacing: 4) {
                                    Text(Localization.tr("Test mode", "测试模式", "テストモード"))
                                        .font(.subheadline).foregroundStyle(.secondary)
                                    Picker("", selection: $mode) {
                                        Text(Localization.tr("Single-core", "单核", "コア別")).tag("per-core")
                                        Text(Localization.tr("All-core", "全核", "全コア")).tag("multi")
                                        Text(Localization.tr("Single-core + All-core", "单核 + 全核", "コア別 + 全コア")).tag("all")
                                    }
                                    .labelsHidden()
                                }

                                VStack(alignment: .leading, spacing: 4) {
                                    Text(Localization.tr("Duration preset", "时长预设", "时间プリセット"))
                                        .font(.subheadline).foregroundStyle(.secondary)
                                    Picker("", selection: $durationPreset) {
                                        Text(Localization.tr("Quick (1 s)", "快速（1 秒）", "クイック（1 秒）")).tag("1")
                                        Text(Localization.tr("Formal (15 s)", "正式（15 秒）", "正式（15 秒）")).tag("15")
                                    }
                                    .labelsHidden()
                                    .onChange(of: durationPreset) { v in
                                        secondsPerTest = v
                                    }
                                }

                                VStack(alignment: .leading, spacing: 4) {
                                    Text(Localization.tr("Seconds per test", "每项测试秒数", "テストごとの秒数"))
                                        .font(.subheadline).foregroundStyle(.secondary)
                                    TextField("1", text: $secondsPerTest)
                                        .textFieldStyle(.roundedBorder)
                                        .keyboardType(.decimalPad)
                                }

                                VStack(alignment: .leading, spacing: 4) {
                                    Text(Localization.tr("Warm-up seconds", "预热秒数", "ウォームアップ秒数"))
                                        .font(.subheadline).foregroundStyle(.secondary)
                                    TextField("0.2", text: $warmupSec)
                                        .textFieldStyle(.roundedBorder)
                                        .keyboardType(.decimalPad)
                                }
                            }

                            Text(Localization.tr(
                                "Single-core duration is applied once to each logical processor; the formal preset can take a long time.",
                                "单核测试会依次测试每个逻辑处理器；正式测试可能耗时较长。",
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
                .padding(16)
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
                .disabled(engine.isRunning || engine.workingDirectory.isEmpty)
            }
            .padding(.horizontal, 16)
            .padding(.vertical, 12)
            .background(.ultraThinMaterial)
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
        let args = [
            "gpu_benchmark",
            "--cpu-benchmark", mode,
            "--cpu-time", sec.isEmpty ? "1" : sec,
            "--cpu-warmup", warm.isEmpty ? "0.2" : warm,
        ]
        engine.run(jobs: [args], needCharts: false)
    }
}
