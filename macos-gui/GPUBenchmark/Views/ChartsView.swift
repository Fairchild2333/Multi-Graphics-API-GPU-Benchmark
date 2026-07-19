// ChartsView.swift — Generate and display workload comparison charts.
// Calls scripts/plot_workloads.py via Process and displays resulting PNGs.

import SwiftUI

struct ChartsView: View {
    @EnvironmentObject var engine: BenchEngine

    @State private var isGenerating = false
    @State private var statusText = ""
    @State private var chartImages: [NSImage] = []
    @State private var chartPaths: [String] = []

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(spacing: 12) {
                Text(Localization.tr("Charts", "图表", "チャート"))
                    .font(.largeTitle)
                    .fontWeight(.bold)

                Button(action: generateCharts) {
                    Label(Localization.tr("Generate Charts", "生成图表", "チャート生成"),
                          systemImage: "chart.bar.xaxis")
                }
                .disabled(isGenerating || engine.workingDirectory.isEmpty)

                if isGenerating {
                    ProgressView()
                        .controlSize(.small)
                }

                if !statusText.isEmpty {
                    Text(statusText)
                        .foregroundStyle(.secondary)
                        .font(.callout)
                }

                Spacer()

                Button(Localization.tr("Open charts folder", "打开图表文件夹", "チャートフォルダを開く")) {
                    let dir = URL(fileURLWithPath: engine.workingDirectory)
                        .appendingPathComponent("docs/images").path
                    engine.openFolder(dir)
                }
                .disabled(engine.workingDirectory.isEmpty)
            }

            if chartImages.isEmpty && !isGenerating {
                GlassCard {
                    VStack(spacing: 12) {
                        Image(systemName: "chart.bar.fill")
                            .font(.system(size: 48))
                            .foregroundStyle(.secondary)
                        Text(Localization.tr(
                            "Click \"Generate Charts\" to create workload comparison plots.\nRequires Python 3 + matplotlib.",
                            "点击「生成图表」创建负载对比图。\n需要 Python 3 + matplotlib。",
                            "「チャート生成」で負荷比較図を作成。\nPython 3 + matplotlib が必要です。"))
                            .multilineTextAlignment(.center)
                            .foregroundStyle(.secondary)
                    }
                    .frame(maxWidth: .infinity)
                    .padding(40)
                }
            } else {
                ScrollView {
                    VStack(spacing: 16) {
                        ForEach(Array(chartImages.enumerated()), id: \.offset) { idx, img in
                            GlassCard(padding: 12) {
                                VStack(alignment: .leading, spacing: 8) {
                                    if idx < chartPaths.count {
                                        Text(URL(fileURLWithPath: chartPaths[idx]).lastPathComponent)
                                            .font(.caption)
                                            .foregroundStyle(.secondary)
                                    }
                                    Image(nsImage: img)
                                        .resizable()
                                        .aspectRatio(contentMode: .fit)
                                        .frame(maxWidth: .infinity)
                                }
                            }
                        }
                    }
                }
            }
        }
        .padding(28)
        .onAppear { loadExistingCharts() }
    }

    // MARK: - Helpers

    private func generateCharts() {
        isGenerating = true
        statusText = Localization.tr("Generating…", "生成中…", "生成中…")

        let cwd = engine.workingDirectory
        Task.detached {
            let process = Process()
            process.executableURL = URL(fileURLWithPath: "/usr/bin/env")
            process.arguments = ["python3", "scripts/plot_workloads.py"]
            process.currentDirectoryURL = URL(fileURLWithPath: cwd)

            let pipe = Pipe()
            process.standardOutput = pipe
            process.standardError = pipe

            do {
                try process.run()
                process.waitUntilExit()
                let data = pipe.fileHandleForReading.readDataToEndOfFile()
                let output = String(data: data, encoding: .utf8) ?? ""

                await MainActor.run {
                    if process.terminationStatus == 0 {
                        statusText = Localization.tr("Done.", "完成。", "完了。")
                        loadExistingCharts()
                    } else {
                        statusText = Localization.tr("Error — check Python/matplotlib.",
                                                    "出错 — 请检查 Python/matplotlib。",
                                                    "エラー — Python/matplotlib を確認。")
                        if !output.isEmpty { print("[Charts] \(output)") }
                    }
                    isGenerating = false
                }
            } catch {
                await MainActor.run {
                    statusText = "Error: \(error.localizedDescription)"
                    isGenerating = false
                }
            }
        }
    }

    private func loadExistingCharts() {
        let imagesDir = URL(fileURLWithPath: engine.workingDirectory)
            .appendingPathComponent("docs/images")
        guard FileManager.default.fileExists(atPath: imagesDir.path) else { return }

        do {
            let files = try FileManager.default.contentsOfDirectory(
                at: imagesDir, includingPropertiesForKeys: nil)
                .filter { $0.pathExtension == "png" && $0.lastPathComponent.hasPrefix("workload_") }
                .sorted { $0.lastPathComponent < $1.lastPathComponent }

            var images: [NSImage] = []
            var paths: [String] = []
            for file in files {
                if let img = NSImage(contentsOf: file) {
                    images.append(img)
                    paths.append(file.path)
                }
            }
            chartImages = images
            chartPaths = paths
        } catch {
            // Ignore — no charts yet
        }
    }
}
