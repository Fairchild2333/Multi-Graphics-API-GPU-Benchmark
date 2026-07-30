// BenchEngine.swift — Swift wrapper around GpuBenchBridge for iOS.
// Embedded Metal host for stream/gpu_burn preview; cliMain kept for advanced jobs.

import UIKit
import Foundation
import SwiftUI
import QuartzCore
import Darwin

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
    @Published var engineVersion: String = ""
    @Published var lastError: String = ""

    /// Available graphics APIs on iOS (Metal only).
    static let availableAPIs = ["Metal"]

    private var cancelRequested = false
    private var metalLayer: CAMetalLayer?
    private var pollTask: Task<Void, Never>?

    init() {
        prepareSandboxAndShaders()
        if let ver = gpb_engine_version() {
            engineVersion = String(cString: ver)
            gpb_free(ver)
        }
        if !workingDirectory.isEmpty {
            refreshGpus()
            refreshResults()
        }
    }

    /// Copy bundled .metal sources into Documents/shaders and point the host at sandbox data.
    private func prepareSandboxAndShaders() {
        let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first
        guard let docs else { return }
        workingDirectory = docs.path
        gpb_set_working_dir(workingDirectory)

        let shaderDir = docs.appendingPathComponent("shaders", isDirectory: true)
        try? FileManager.default.createDirectory(at: shaderDir, withIntermediateDirectories: true)
        for name in ["particle.metal", "gpu_burn.metal"] {
            let dest = shaderDir.appendingPathComponent(name)
            if FileManager.default.fileExists(atPath: dest.path) { continue }
            if let src = Bundle.main.url(forResource: name, withExtension: nil)
                ?? Bundle.main.url(forResource: (name as NSString).deletingPathExtension,
                                   withExtension: (name as NSString).pathExtension) {
                try? FileManager.default.copyItem(at: src, to: dest)
            } else if let src = Bundle.main.url(forResource: "shaders/\(name)", withExtension: nil) {
                try? FileManager.default.copyItem(at: src, to: dest)
            }
        }
        gpb_init_paths(shaderDir.path, docs.path)
        setenv("GPU_BENCH_DATA_DIR", docs.path, 1)
    }

    func attachMetalLayer(_ layer: CAMetalLayer) {
        metalLayer = layer
        gpb_set_metal_layer(Unmanaged.passUnretained(layer).toOpaque())
    }

    func setWorkingDirectory(_ path: String) {
        workingDirectory = path
        gpb_set_working_dir(path)
        gpb_init_paths((URL(fileURLWithPath: path).appendingPathComponent("shaders")).path, path)
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
                    supportsMetal: dict["metal"] as? Bool ?? true,
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
        print("iOS Data Folder: \(path)")
    }

    func cancel() {
        cancelRequested = true
        gpb_stop_workload()
        statusText = Localization.tr("Cancel requested…", "已请求取消…", "キャンセル要求済み…")
    }

    func cancelIfRunning() {
        if isRunning || gpb_is_running() {
            cancel()
        }
    }

    /// 3s embedded Metal preview (stream / gpu_burn). Requires CAMetalLayer.
    @discardableResult
    func startPreview(workload: String, seconds: Double = 3.0) -> Bool {
        guard metalLayer != nil else {
            lastError = "Metal layer not ready"
            statusText = lastError
            return false
        }
        guard !isRunning else { return false }
        isRunning = true
        cancelRequested = false
        lastError = ""
        statusText = Localization.tr(
            "Running \(workload) preview (\(Int(seconds))s)…",
            "运行 \(workload) 预览（\(Int(seconds)) 秒）…",
            "\(workload) プレビュー実行中（\(Int(seconds))秒）…")
        logOutput += "=== Preview \(workload) \(seconds)s ===\n"

        let ok = gpb_start_workload(workload, seconds)
        if !ok {
            if let err = gpb_last_error() {
                lastError = String(cString: err)
                gpb_free(err)
            }
            statusText = lastError.isEmpty ? "Start failed" : lastError
            isRunning = false
            return false
        }
        startPolling()
        return true
    }

    private func startPolling() {
        pollTask?.cancel()
        pollTask = Task { [weak self] in
            while !Task.isCancelled {
                let running = gpb_is_running()
                await MainActor.run {
                    guard let self else { return }
                    self.isRunning = running
                    if !running {
                        if let err = gpb_last_error() {
                            let s = String(cString: err)
                            gpb_free(err)
                            if !s.isEmpty {
                                self.lastError = s
                                self.logOutput += "ERROR: \(s)\n"
                                self.statusText = s
                            } else if self.cancelRequested {
                                self.statusText = Localization.tr("Cancelled.", "已取消。", "キャンセルしました。")
                            } else {
                                self.statusText = Localization.tr(
                                    "Preview done (ios_preview — do not mix with desktop scores).",
                                    "预览完成（ios_preview — 勿与桌面成绩混排）。",
                                    "プレビュー完了（ios_preview — デスクトップ成績と混在禁止）。")
                            }
                        }
                        self.refreshResults()
                        self.pollTask = nil
                    }
                }
                if !running { break }
                try? await Task.sleep(nanoseconds: 250_000_000)
            }
        }
    }

    /// Run a sequence of benchmark jobs (legacy cliMain path).
    func run(jobs: [[String]], needCharts: Bool) {
        guard !isRunning, !jobs.isEmpty else { return }

        // Prefer embedded host for single stream/gpu_burn preview-style jobs.
        if jobs.count == 1 {
            let job = jobs[0]
            let wl = job.first(where: { ["stream", "gpu_burn", "particle"].contains($0) })
                ?? job.dropFirst().first { ["stream", "gpu_burn"].contains($0) }
            if let wl, metalLayer != nil {
                var seconds = 3.0
                if let i = job.firstIndex(of: "--time"), i + 1 < job.count,
                   let v = Double(job[i + 1]) {
                    seconds = min(max(v, 1.0), 15.0)
                }
                _ = startPreview(workload: wl == "particle" ? "stream" : wl, seconds: seconds)
                return
            }
        }

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
