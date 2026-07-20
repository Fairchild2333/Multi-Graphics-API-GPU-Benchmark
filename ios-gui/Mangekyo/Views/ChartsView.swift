// ChartsView.swift — Native SwiftUI charts for iOS benchmark comparison.

import Charts
import SwiftUI

struct ChartsView: View {
    @EnvironmentObject var engine: BenchEngine

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                if engine.results.isEmpty {
                    GlassCard {
                        VStack(spacing: 12) {
                            Image(systemName: "chart.bar.fill")
                                .font(.system(size: 48))
                                .foregroundStyle(.secondary)
                            Text(Localization.tr(
                                "No benchmark results yet. Run some tests to see comparison charts.",
                                "尚无跑分结果。运行一些测试以查看对比图表。",
                                "ベンチマーク結果がありません。テストを実行してチャートを表示します。"))
                                .multilineTextAlignment(.center)
                                .foregroundStyle(.secondary)
                        }
                        .frame(maxWidth: .infinity)
                        .padding(40)
                    }
                } else {
                    // GPU Metal scores
                    let metalResults = engine.results.filter { $0.graphicsApi.lowercased() == "metal" && $0.score > 0 }
                    if !metalResults.isEmpty {
                        GlassCard {
                            VStack(alignment: .leading, spacing: 12) {
                                Text(Localization.tr("Metal GPU Benchmark Scores", "Metal GPU 跑分对比", "Metal GPU スコア比較"))
                                    .font(.headline)
                                    .padding(.bottom, 4)

                                Chart(metalResults) { item in
                                    BarMark(
                                        x: .value("Score", item.score),
                                        y: .value("Workload", "\(item.workload.capitalized) (\(item.difficulty))")
                                    )
                                    .foregroundStyle(by: .value("Device", item.deviceName))
                                    .annotation(position: .trailing) {
                                        Text(String(format: "%.1f", item.score))
                                            .font(.caption2)
                                            .foregroundColor(.secondary)
                                    }
                                }
                                .frame(height: CGFloat(max(metalResults.count * 45, 180)))
                            }
                        }
                    }

                    // CPU scores
                    let cpuResults = engine.results.filter { $0.graphicsApi.lowercased() == "cpu" && $0.score > 0 }
                    if !cpuResults.isEmpty {
                        GlassCard {
                            VStack(alignment: .leading, spacing: 12) {
                                Text(Localization.tr("CPU Benchmark Scores", "CPU 跑分对比", "CPU スコア比較"))
                                    .font(.headline)
                                    .padding(.bottom, 4)

                                Chart(cpuResults) { item in
                                    BarMark(
                                        x: .value("Score", item.score),
                                        y: .value("Mode", item.workload.capitalized)
                                    )
                                    .foregroundStyle(by: .value("Device", item.deviceName))
                                    .annotation(position: .trailing) {
                                        Text(String(format: "%.1f", item.score))
                                            .font(.caption2)
                                            .foregroundColor(.secondary)
                                    }
                                }
                                .frame(height: CGFloat(max(cpuResults.count * 45, 150)))
                            }
                        }
                    }
                }
            }
            .padding(16)
        }
    }
}
