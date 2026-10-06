// BenchEngine.swift — Swift wrapper around GpuBenchBridge / external CLI.
// Manages benchmark state, stdout capture, cancel, and GPU enumeration.

import AppKit
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
    @Published var statusText: String = Localization.tr("Ready", "就绪", "準備完了")
    @Published var workingDirectory: String = ""
    @Published var results: [BenchResult] = []
    @Published var progressFraction: Double = 0
    @Published var progressLabel: String = "0%"
    /// The output of the current or last run, parsed for the result tables and charts.
    @Published var runOutput = RunOutput()

    /// Available graphics APIs on macOS (no DX).
    static let availableAPIs = ["Auto", "Metal", "Vulkan", "OpenGL"]

    private var activeProcess: Process?
    private var cancelRequested = false
    /// Output that does not end in a line break yet.
    private var pendingOutput = ""
    private var shownStage: RunOutput.Stage?

    init() {
        if let execURL = Bundle.main.executableURL {
            let repoMarkers = ["CMakeLists.txt", "src/gpu_engine.h"]
            var dir = execURL.deletingLastPathComponent()
            for _ in 0..<8 {
                let hasMarkers = repoMarkers.allSatisfy { marker in
                    FileManager.default.fileExists(atPath: dir.appendingPathComponent(marker).path)
                }
                if hasMarkers {
                    workingDirectory = dir.path
                    gpb_set_working_dir(workingDirectory)
                    break
                }
                dir = dir.deletingLastPathComponent()
            }
        }
        if !workingDirectory.isEmpty {
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
        NSWorkspace.shared.open(url)
    }

    /// Resolve `gpu_benchmark` next to the repo / common CMake outs.
    func resolveCliExecutable() -> String? {
        let fm = FileManager.default
        let candidates = [
            "\(workingDirectory)/build/gpu_benchmark",
            "\(workingDirectory)/build/Release/gpu_benchmark",
            "\(workingDirectory)/cmake-build-release/gpu_benchmark",
            "\(workingDirectory)/out/build/gpu_benchmark",
            "\(workingDirectory)/gpu_benchmark",
        ]
        for c in candidates where fm.isExecutableFile(atPath: c) {
            return c
        }
        return nil
    }

    func cancel() {
        cancelRequested = true
        if let proc = activeProcess, proc.isRunning {
            proc.terminate()
            statusText = Localization.tr("Cancel requested…", "已请求取消…", "キャンセル要求済み…")
        } else {
            statusText = Localization.tr(
                "Cancel: close the render window if the run is in-process / unlimited.",
                "取消：若为进程内或无限时长运行，请关闭渲染窗口。",
                "キャンセル：プロセス内/無制限実行なら描画ウィンドウを閉じてください。")
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
        resetRunOutput()

        statusText = jobs.count > 1
            ? Localization.tr("Running… (multiple passes; render windows may appear)",
                              "运行中…（多趟；可能弹出渲染窗口）",
                              "実行中…（複数パス；描画ウィンドウが出る場合あり）")
            : Localization.tr("Running… (a render window may appear)",
                              "运行中…（可能弹出渲染窗口）",
                              "実行中…（描画ウィンドウが出る場合あり）")

        let cwd = workingDirectory
        let cli = resolveCliExecutable()
        Task.detached { [weak self] in
            guard let self else { return }
            gpb_set_working_dir(cwd)

            for (index, job) in jobs.enumerated() {
                if await MainActor.run(body: { self.cancelRequested }) { break }

                await MainActor.run {
                    let pct = Double(index) / Double(max(jobs.count, 1))
                    self.progressFraction = pct
                    self.progressLabel = String(format: "%.0f%%", pct * 100)
                    self.flushOutput()
                    self.appendLine("=== Job \(index + 1) of \(jobs.count): \(job.joined(separator: " ")) ===")
                }

                if let cliPath = cli {
                    await self.runViaProcess(cliPath: cliPath, job: job, cwd: cwd)
                } else {
                    await self.runViaBridge(job: job)
                }
            }

            let cancelledBeforeCharts = await MainActor.run(body: { self.cancelRequested })
            if needCharts && !cancelledBeforeCharts {
                await MainActor.run {
                    self.statusText = Localization.tr("Generating charts…", "正在生成图表…", "チャート生成中…")
                }
                let process = Process()
                process.executableURL = URL(fileURLWithPath: "/usr/bin/env")
                process.arguments = ["python3", "scripts/plot_workloads.py"]
                process.currentDirectoryURL = URL(fileURLWithPath: cwd)
                try? process.run()
                process.waitUntilExit()
            }

            await MainActor.run {
                self.flushOutput()
                self.activeProcess = nil
                self.isRunning = false
                self.progressFraction = 1
                self.progressLabel = "100%"
                self.refreshResults()
                if self.cancelRequested {
                    self.statusText = Localization.tr("Cancelled.", "已取消。", "キャンセルしました。")
                } else if self.lastScore.isEmpty {
                    self.statusText = needCharts
                        ? Localization.tr("Done (charts regenerated).", "完成（已重新生成图表）。", "完了（チャート再生成）。")
                        : Localization.tr("Done — see output / History.", "完成 —— 见输出/历史。", "完了 — 出力/履歴を確認。")
                } else {
                    self.statusText = needCharts
                        ? Localization.tr("Done (charts regenerated). \(self.lastScore)",
                                          "完成（已重新生成图表）。\(self.lastScore)",
                                          "完了（チャート再生成）。\(self.lastScore)")
                        : Localization.tr("Done. \(self.lastScore)", "完成。\(self.lastScore)", "完了。\(self.lastScore)")
                }
            }
        }
    }

    /// Hands the child process to the main actor, which only ever terminates it.
    private struct ProcessBox: @unchecked Sendable {
        let process: Process
    }

    // Not on the main actor: this blocks until the child process has exited.
    nonisolated private func runViaProcess(cliPath: String, job: [String], cwd: String) async {
        // job[0] is usually "gpu_benchmark"; replace with absolute CLI.
        var args = Array(job.dropFirst())
        if job.first == "gpu_benchmark" {
            // keep args as-is after program name
        } else {
            args = job
        }

        let process = Process()
        process.executableURL = URL(fileURLWithPath: cliPath)
        process.arguments = args
        process.currentDirectoryURL = URL(fileURLWithPath: cwd)

        let pipe = Pipe()
        process.standardOutput = pipe
        process.standardError = pipe

        let box = ProcessBox(process: process)
        await MainActor.run { self.activeProcess = box.process }

        let outputEnded = DispatchSemaphore(value: 0)
        pipe.fileHandleForReading.readabilityHandler = { handle in
            let data = handle.availableData
            if data.isEmpty {
                // End of the output: every writer has closed the pipe.
                handle.readabilityHandler = nil
                outputEnded.signal()
                return
            }
            // A read can end in the middle of a line; appendOutput joins the pieces.
            let chunk = String(decoding: data, as: UTF8.self)
            DispatchQueue.main.async {
                self.appendOutput(chunk)
            }
        }

        do {
            try process.run()
            // Close our copy of the write end, so the read end sees the end of the output.
            try? pipe.fileHandleForWriting.close()
            process.waitUntilExit()
            // The last lines may still be on their way.
            _ = outputEnded.wait(timeout: .now() + 2)
        } catch {
            await MainActor.run {
                self.logOutput += "[GUI] Failed to launch CLI: \(error.localizedDescription)\n"
            }
        }
        pipe.fileHandleForReading.readabilityHandler = nil
        await MainActor.run { self.activeProcess = nil }
    }

    // Not on the main actor: gpb_run blocks until the run has finished.
    nonisolated private func runViaBridge(job: [String]) async {
        await MainActor.run {
            self.logOutput += "[GUI] gpu_benchmark binary not found — using in-process bridge (Cancel limited).\n"
        }
        let cStrings = job.map { strdup($0) }
        let argc = Int32(cStrings.count)
        var cPtrs = cStrings.map { UnsafePointer($0) }

        let callback: gpb_line_callback = { line, ctx in
            guard let line = line, let ctx = ctx else { return }
            let str = String(cString: line)
            let eng = Unmanaged<BenchEngine>.fromOpaque(ctx).takeUnretainedValue()
            DispatchQueue.main.async { [eng, str] in
                eng.appendLine(str)
            }
        }

        let selfRef = Unmanaged.passUnretained(self).toOpaque()
        _ = cPtrs.withUnsafeMutableBufferPointer { buf in
            gpb_run(buf.baseAddress, argc, callback, selfRef)
        }
        for ptr in cStrings { free(ptr) }
    }

    // MARK: - Engine output

    /// Adds engine output that may end in the middle of a line.
    private func appendOutput(_ chunk: String) {
        pendingOutput += chunk
        while let lineEnd = pendingOutput.firstIndex(where: { $0.isNewline }) {
            let line = String(pendingOutput[..<lineEnd])
            pendingOutput.removeSubrange(...lineEnd)
            appendLine(line)
        }
    }

    /// Adds the rest of the output once a run has ended.
    private func flushOutput() {
        guard !pendingOutput.isEmpty else { return }
        let line = pendingOutput
        pendingOutput = ""
        appendLine(line)
    }

    /// Adds one line to the log and to the parsed results that feed the charts.
    private func appendLine(_ line: String) {
        logOutput += line + "\n"
        if line.hasPrefix("Avg FPS:") || line.contains("Score:") {
            lastScore = Localization.scoreLine(line.trimmingCharacters(in: .whitespacesAndNewlines))
        }
        runOutput.consume(line)
        // The CPU tests report their own progress.
        if let fraction = runOutput.progress {
            progressFraction = fraction
            progressLabel = String(format: "%.0f%%", fraction * 100)
        }
        if let stage = runOutput.stage, stage != shownStage {
            shownStage = stage
            statusText = Self.stageText(stage)
        }
    }

    private func resetRunOutput() {
        runOutput = RunOutput()
        pendingOutput = ""
        shownStage = nil
    }

    private static func stageText(_ stage: RunOutput.Stage) -> String {
        func phaseText(_ phase: RunOutput.Phase) -> String {
            switch phase {
            case .warmUp: return Localization.tr("warming up", "预热", "ウォームアップ")
            case .measure: return Localization.tr("measuring", "测量", "測定中")
            case .finish: return Localization.tr("finishing", "收尾", "仕上げ")
            }
        }
        switch stage {
        case .cpuCore(let label, let phase):
            return label + " · " + phaseText(phase)
        case .cpuAllCores(let phase):
            return Localization.tr("All cores", "全核", "全コア") + " · " + phaseText(phase)
        case .sweepOneThread:
            return Localization.tr("One thread · sweeping data-set sizes", "单线程 · 扫描数据量",
                                   "1 スレッド · データセットサイズをスイープ")
        case .sweepAllCores:
            return Localization.tr("All cores · sweeping data-set sizes", "全核 · 扫描数据量",
                                   "全コア · データセットサイズをスイープ")
        case .storage:
            return Localization.tr("Storage · reading and writing a test file", "存储 · 读写测试文件",
                                   "ストレージ · テストファイルの読み書き")
        case .pageMapping:
            return Localization.tr("Virtual memory · mapping pages", "虚拟内存 · 映射页面",
                                   "仮想メモリ · ページの割り当て")
        case .error(let message):
            return message
        }
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
