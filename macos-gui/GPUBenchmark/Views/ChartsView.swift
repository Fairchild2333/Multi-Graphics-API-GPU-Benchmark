// ChartsView.swift — Comparison charts drawn from the saved results.
// One chart per test; no Python or matplotlib is needed.

import SwiftUI

struct ChartsView: View {
    @EnvironmentObject var engine: BenchEngine

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(spacing: 12) {
                Text(Localization.tr("Charts", "图表", "チャート"))
                    .font(.largeTitle)
                    .fontWeight(.bold)

                Button(action: { engine.refreshResults() }) {
                    Label(Localization.tr("Refresh", "刷新", "更新"), systemImage: "arrow.clockwise")
                }
                .disabled(engine.workingDirectory.isEmpty)

                Spacer()

                Button(Localization.tr("Open results folder", "打开结果文件夹", "結果フォルダを開く")) {
                    engine.openResultsFolder()
                }
                .disabled(engine.workingDirectory.isEmpty)

                // The full-analysis presets still write PNG charts here.
                Button(Localization.tr("Open PNG charts folder", "打开 PNG 图表文件夹", "PNG チャートフォルダを開く")) {
                    let dir = URL(fileURLWithPath: engine.workingDirectory)
                        .appendingPathComponent("docs/images").path
                    engine.openFolder(dir)
                }
                .disabled(engine.workingDirectory.isEmpty)
            }

            ScrollView {
                SavedResultCharts(results: engine.results)
                    .padding(.bottom, 12)
            }
        }
        .padding(28)
        .onAppear {
            if !engine.workingDirectory.isEmpty {
                engine.refreshResults()
            }
        }
    }
}
