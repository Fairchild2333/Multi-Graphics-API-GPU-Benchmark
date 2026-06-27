// BenchEngine.swift — Swift wrapper around GpuBenchBridge C functions.
// Manages benchmark state, stdout capture, and GPU enumeration.

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

    /// Available graphics APIs on macOS (no DX).
    static let availableAPIs = ["Auto", "Metal", "Vulkan", "OpenGL"]

    init() {
        // Try to auto-detect the repo root
        if let execURL = Bundle.main.executableURL {
            // If running from Xcode, executable is in DerivedData; try to find repo
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

    /// Set working directory and refresh GPU list.
    func setWorkingDirectory(_ path: String) {
        workingDirectory = path
        gpb_set_working_dir(path)
        refreshGpus()
        refreshResults()
    }

    /// Enumerate GPUs from the engine.
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
                    vramMB: dict["vramMB"] as? Int ?? 0,
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

    /// Run a sequence of benchmark jobs.
    func run(jobs: [[String]], needCharts: Bool) {
        guard !isRunning, !jobs.isEmpty else { return }
        isRunning = true
        logOutput = ""
        lastScore = ""
        
        statusText = jobs.count > 1
            ? Localization.tr("Running… (multiple passes; render windows may appear)",
                              "运行中…（多趟；可能弹出渲染窗口）")
            : Localization.tr("Running… (a render window may appear)",
                              "运行中…（可能弹出渲染窗口）")

        let cwd = workingDirectory
        Task.detached {
            gpb_set_working_dir(cwd)

            for (index, job) in jobs.enumerated() {
                await MainActor.run {
                    self.logOutput += "=== Job \(index + 1) of \(jobs.count): \(job.joined(separator: " ")) ===\n"
                }

                // Convert Swift strings to C strings
                let cStrings = job.map { strdup($0) }
                let argc = Int32(cStrings.count)

                // Build a C array of const char* pointers
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

                let selfRef = Unmanaged.passUnretained(self).toOpaque()
                _ = cPtrs.withUnsafeMutableBufferPointer { buf in
                    gpb_run(buf.baseAddress, argc, callback, selfRef)
                }

                // Free C strings
                for ptr in cStrings { free(ptr) }
            }

            // Generate charts if requested
            if needCharts {
                await MainActor.run {
                    self.statusText = Localization.tr("Generating charts…", "正在生成图表…")
                }
                let process = Process()
                process.executableURL = URL(fileURLWithPath: "/usr/bin/env")
                process.arguments = ["python3", "scripts/plot_workloads.py"]
                process.currentDirectoryURL = URL(fileURLWithPath: cwd)
                try? process.run()
                process.waitUntilExit()
            }

            await MainActor.run {
                self.isRunning = false
                self.refreshResults()
                if self.lastScore.isEmpty {
                    self.statusText = needCharts
                        ? Localization.tr("Done (charts regenerated).", "完成（已重新生成图表）。")
                        : Localization.tr("Done — see output / History.", "完成 —— 见输出/历史。")
                } else {
                    self.statusText = needCharts
                        ? Localization.tr("Done (charts regenerated). \(self.lastScore)", "完成（已重新生成图表）。\(self.lastScore)")
                        : Localization.tr("Done. \(self.lastScore)", "完成。\(self.lastScore)")
                }
            }
        }
    }

    /// Load/Reload saved benchmark results.
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

    /// Delete a result by ID.
    func deleteResult(id: String) -> Bool {
        gpb_set_working_dir(workingDirectory)
        let success = gpb_delete_result(id)
        if success {
            refreshResults()
        }
        return success
    }

    /// Clear all results.
    func clearAllResults() -> Bool {
        gpb_set_working_dir(workingDirectory)
        let success = gpb_clear_results()
        if success {
            refreshResults()
        }
        return success
    }
}
