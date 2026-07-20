// BenchEngine.swift — Swift wrapper around GpuBenchBridge for iOS (Bridge-only, no sub-processes).
// Manages benchmark state, stdout capture, cancel, and GPU enumeration.

import UIKit
import Foundation
import SwiftUI

/// Represents a detected GPU from the engine.
struct GpuDevice: Identifiable, Hashable {
    let id: Int
    let index: Int
    let name: String
    let vramMB: Int
    let supportsMetal: Bool
    let supportsVulkan: Bool
    let supportsOpenGL: Bool
}

/// Observable engine state for SwiftUI binding.
@MainActor
final class BenchEngine: ObservableObject {
    @Published var gpus: [GpuDevice] = []
    @Published var logOutput: String = ""
    @Published var isRunning: Bool = false
    @Published var lastScore: String = ""
    @Published var statusText: String = "Ready"
    @Published var workingDirectory: String = ""
    @Published var results: [BenchResult] = []
    @Published var progressFraction: Double = 0
    @Published var progressLabel: String = "0%"

    /// Available graphics APIs on iOS (Metal only).
    static let availableAPIs = ["Metal"]

    private var cancelRequested = false

    init() {
        // On iOS, we use Application Support / Documents directory as the data root.
        // We can retrieve it through path_service or default directories.
        let paths = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)
        if let docsDir = paths.first {
            workingDirectory = docsDir.path
            gpb_set_working_dir(workingDirectory)
        }
        if !workingDirectory.isEmpty {
            refreshGpus()
            refreshResults()
        }
    }

    func setWorkingDirectory(_ path: String) {
        workingDirectory = path
        gpb_set_working_dir(path)
        refreshGpus()
        refreshResults()
    }

    func refreshGpus() {
        guard !workingDirectory.isEmpty else { return }
        let cwd = workingDirectory
        Task.detached {
            gpb_set_working_dir(cwd)
            guard let json = gpb_list_gpus() else { return }
            let jsonStr = String(cString: json)
            gpb_free(json)

            guard let data = jsonStr.data(using: .utf8),
                  let arr = try? JSONSerialization.jsonObject(with: data) as? [[String: Any]]
            else { return }

            let devices = arr.compactMap { dict -> GpuDevice? in
                guard let idx = dict["index"] as? Int,
                      let name = dict["name"] as? String else { return nil }
                return GpuDevice(
                    id: idx,
                    index: idx,
                    name: name,
                    vramMB: dict["vramMB"] as? Int ?? dict["vram"] as? Int ?? 0,
                    supportsMetal: dict["metal"] as? Bool ?? false,
                    supportsVulkan: dict["vulkan"] as? Bool ?? false,
                    supportsOpenGL: dict["opengl"] as? Bool ?? false
                )
            }
            await MainActor.run {
                self.gpus = devices
            }
        }
    }

    var resultsDirectory: String {
        guard let p = gpb_results_dir() else { return "" }
        let s = String(cString: p)
        gpb_free(p)
        return s
    }

    var capturesDirectory: String {
        guard let p = gpb_captures_dir() else { return "" }
        let s = String(cString: p)
        gpb_free(p)
        return s
    }

    func openResultsFolder() {
        openFolder(resultsDirectory)
    }

    func openCapturesFolder() {
        openFolder(capturesDirectory)
    }

    func openFolder(_ path: String) {
        guard !path.isEmpty else { return }
        let url = URL(fileURLWithPath: path)
        try? FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        // iOS: cannot open directory in Files app directly in a standard way,
        // but we can share or display the path. For sandbox sharing, iOS apps
        // typically export files using ShareSheet or DocumentPicker.
        print("iOS Data Folder: \(path)")
    }

    func cancel() {
        cancelRequested = true
        statusText = Localization.tr("Cancel requested…", "已请求取消…", "キャンセル要求済み…")
        // iOS in-process run cannot be cleanly stopped via Process.terminate.
        // It relies on app_base.cpp checking cancel signals or event loop exits.
    }

    func cancelIfRunning() {
        if isRunning {
            cancel()
        }
    }

    /// Run a sequence of benchmark jobs.
    func run(jobs: [[String]], needCharts: Bool) {
        guard !isRunning, !jobs.isEmpty else { return }
        isRunning = true
        cancelRequested = false
        logOutput = ""
        lastScore = ""
        progressFraction = 0
        progressLabel = "0%"

        statusText = jobs.count > 1
            ? Localization.tr("Running… (multiple passes)",
                               "运行中…（多趟）",
                               "実行中…（複数パス）")
            : Localization.tr("Running…",
                               "运行中…",
                               "実行中…")

        let cwd = workingDirectory
        Task.detached { [weak self] in
            guard let self else { return }
            gpb_set_working_dir(cwd)

            for (index, job) in jobs.enumerated() {
                if await MainActor.run(body: { self.cancelRequested }) { break }

                await MainActor.run {
                    let pct = Double(index) / Double(max(jobs.count, 1))
                    self.progressFraction = pct
                    self.progressLabel = String(format: "%.0f%%", pct * 100)
                    self.logOutput += "=== Job \(index + 1) of \(jobs.count): \(job.joined(separator: " ")) ===\n"
                }

                // On iOS, we only run via in-process bridge.
                await self.runViaBridge(job: job)
            }

            await MainActor.run {
                self.isRunning = false
                self.progressFraction = 1
                self.progressLabel = "100%"
                self.refreshResults()
                if self.cancelRequested {
                    self.statusText = Localization.tr("Cancelled.", "已取消。", "キャンセルしました。")
                } else if self.lastScore.isEmpty {
                    self.statusText = Localization.tr("Done — see output / History.", "完成 —— 见输出/历史。", "完了 — 出力/履歴を確認。")
                } else {
                    self.statusText = Localization.tr("Done. \(self.lastScore)", "完成。\(self.lastScore)", "完了。\(self.lastScore)")
                }
            }
        }
    }

    private func runViaBridge(job: [String]) async {
        let cStrings = job.map { strdup($0) }
        let argc = Int32(cStrings.count)
        var cPtrs = cStrings.map { UnsafePointer($0) }

        let callback: gpb_line_callback = { line, ctx in
            guard let line = line, let ctx = ctx else { return }
            let str = String(cString: line)
            let eng = Unmanaged<BenchEngine>.fromOpaque(ctx).takeUnretainedValue()
            DispatchQueue.main.async { [eng, str] in
                eng.logOutput += str + "\n"
                if str.hasPrefix("Avg FPS:") || str.contains("Score:") {
                    eng.lastScore = str.trimmingCharacters(in: .whitespaces)
                }
            }
        }

        let selfRef = await MainActor.run {
            Unmanaged.passUnretained(self).toOpaque()
        }
        _ = cPtrs.withUnsafeMutableBufferPointer { buf in
            gpb_run(buf.baseAddress, argc, callback, selfRef)
        }
        for ptr in cStrings { free(ptr) }
    }

    func refreshResults() {
        gpb_set_working_dir(workingDirectory)
        guard let json = gpb_load_results() else { return }
        let jsonStr = String(cString: json)
        gpb_free(json)
        guard let data = jsonStr.data(using: .utf8) else { return }
        if let decoded = try? JSONDecoder().decode([BenchResult].self, from: data) {
            self.results = decoded
        }
    }

    func deleteResult(id: String) -> Bool {
        gpb_set_working_dir(workingDirectory)
        let success = gpb_delete_result(id)
        if success { refreshResults() }
        return success
    }

    func clearAllResults() -> Bool {
        gpb_set_working_dir(workingDirectory)
        let success = gpb_clear_results()
        if success { refreshResults() }
        return success
    }
}
