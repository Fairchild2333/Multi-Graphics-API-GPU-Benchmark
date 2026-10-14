// BenchEngine.swift — Swift wrapper around GpuBenchBridge for iOS.
// Embedded Metal GPU host and a shared CPU-only entry point.

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
    let supportsVulkanLiquid: Bool
    let supportsGpuCache: Bool
    let supportsGles2: Bool
    let gles2MaxSteps: Int
    let gpuCacheParallelSteps: Int
}

/// GPU and CPU runs own separate console output, progress, and scores so one
/// page never shows the other page's results.
enum BenchChannel: String, Sendable, Hashable {
    case gpu
    case cpu
}

/// Everything a single benchmark page displays about its own last run.
struct BenchChannelState: Sendable {
    var logOutput: String = ""
    var lastScore: String = ""
    var statusText: String = ""
    /// False once a run has written a real status, so a language switch does
    /// not overwrite a completion message with the idle string.
    var statusIsIdle: Bool = true
    var progressFraction: Double = 0
    var progressLabel: String = "0%"
    var isRunning: Bool = false
    var runOutput = RunOutput()
}

/// Observable engine state for SwiftUI binding.
@MainActor
final class BenchEngine: ObservableObject {
    @Published var gpus: [GpuDevice] = []
    @Published var workingDirectory: String = ""
    @Published var results: [BenchResult] = []
    @Published var engineVersion: String = ""
    @Published var lastError: String = ""

    /// Per-page run state. The GPU and CPU pages never share console output,
    /// progress, scores, or status.
    @Published var gpuState = BenchChannelState()
    @Published var cpuState = BenchChannelState()
    @Published private(set) var completeSuiteRunning = false
    @Published private(set) var completeSuiteReport: MobileSuiteReport?
    @Published private(set) var completeSuiteIndex = 0
    @Published private(set) var completeSuiteJobs: [MobileSuiteJob] = MobileSuiteJob.all
    @Published var allowTranslatedApis = UserDefaults.standard.bool(forKey: "allowTranslatedApis") {
        didSet { UserDefaults.standard.set(allowTranslatedApis, forKey: "allowTranslatedApis") }
    }
    private var startingSuiteJob = false
    private var suiteCancelled = false
    private var suiteLaunchHandled = false

    /// Available graphics APIs on iOS (Metal only).
    var vulkanAvailable: Bool { gpus.contains { $0.supportsVulkan } }
    var vulkanLiquidAvailable: Bool { gpus.contains { $0.supportsVulkanLiquid } }
    var gpuCacheAvailable: Bool { gpus.contains { $0.supportsGpuCache } }
    var gles2Available: Bool { gpus.contains { $0.supportsGles2 } }
    var gles2MaxSteps: Int { gpus.map(\.gles2MaxSteps).max() ?? 0 }
    var gpuCacheParallelSteps: Int { gpus.map(\.gpuCacheParallelSteps).max() ?? 0 }
    var availableAPIs: [String] {
        ["metal"] + (allowTranslatedApis && vulkanAvailable ? ["vulkan"] : []) + (gles2Available ? ["gles2"] : [])
    }
    var supportedSuiteJobs: [MobileSuiteJob] {
        MobileSuiteJob.makeJobs(includeVulkan: allowTranslatedApis && vulkanAvailable,
            vulkanLiquid: vulkanLiquidAvailable, gpuCache: gpuCacheAvailable, gles2: gles2Available,
            gles2MaxSteps: gles2MaxSteps, gpuCacheParallelSteps: gpuCacheParallelSteps)
    }

    static var readyText: String {
        Localization.tr("Ready", "就绪", "準備完了")
    }

    /// True while any channel is running; only one workload runs at a time.
    var isRunning: Bool { workloadRunning || completeSuiteRunning }
    private var workloadRunning: Bool { gpuState.isRunning || cpuState.isRunning }

    func state(for channel: BenchChannel) -> BenchChannelState {
        channel == .gpu ? gpuState : cpuState
    }

    private func mutate(_ channel: BenchChannel, _ body: (inout BenchChannelState) -> Void) {
        switch channel {
        case .gpu: body(&gpuState)
        case .cpu: body(&cpuState)
        }
        UIApplication.shared.isIdleTimerDisabled = isRunning
    }

    /// Re-render idle status strings after the UI language changes.
    func refreshLocalizedStatus() {
        if gpuState.statusIsIdle { gpuState.statusText = Self.readyText }
        if cpuState.statusIsIdle { cpuState.statusText = Self.readyText }
    }

    /// Channel that owns the currently running workload.
    private var activeChannel: BenchChannel = .gpu

    private var cancelRequested = false
    private var runFailed = false
    /// Output that does not end in a line break yet.
    private var pendingOutput = ""
    private var shownStage: RunOutput.Stage?
    private var metalLayer: CAMetalLayer?
    private var glesLayer: CAEAGLLayer?
    private var pollTask: Task<Void, Never>?

    init() {
        if ProcessInfo.processInfo.arguments.contains("--ios-allow-translated-api") {
            allowTranslatedApis = true
        }
        gpuState.statusText = Self.readyText
        cpuState.statusText = Self.readyText
        prepareSandboxAndShaders()
        if let ver = gpb_engine_version() {
            engineVersion = String(cString: ver)
            gpb_free(ver)
        }
        if !workingDirectory.isEmpty {
            refreshGpus()
            refreshResults()
            let latest = URL(fileURLWithPath: workingDirectory).appendingPathComponent("suite-runs/latest.json")
            if let data = try? Data(contentsOf: latest),
               let report = try? JSONDecoder().decode(MobileSuiteReport.self, from: data),
               report.status != "running" {
                completeSuiteReport = report
            }
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
        let resources = ["particle.metal", "gpu_burn.metal", "cinematic_liquid_v2.metal", "gpu_burn_es2.vert", "gpu_burn_es2.frag"] +
            (Bundle.main.urls(forResourcesWithExtension: "spv", subdirectory: nil) ?? []).map { $0.lastPathComponent }
        for name in resources {
            let dest = shaderDir.appendingPathComponent(name)
            guard let src = Bundle.main.url(forResource: name, withExtension: nil)
                ?? Bundle.main.url(forResource: (name as NSString).deletingPathExtension,
                                   withExtension: (name as NSString).pathExtension)
                ?? Bundle.main.url(forResource: "shaders/\(name)", withExtension: nil) else { continue }
            // Copied at every launch: an update of the app may bring a newer source.
            try? FileManager.default.removeItem(at: dest)
            try? FileManager.default.copyItem(at: src, to: dest)
        }
        gpb_init_paths(shaderDir.path, docs.path)
        setenv("GPU_BENCH_DATA_DIR", docs.path, 1)
    }

    func attachMetalLayer(_ layer: CAMetalLayer) {
        metalLayer = layer
        gpb_set_metal_layer(Unmanaged.passUnretained(layer).toOpaque())
    }

    func attachGlesLayer(_ layer: CAEAGLLayer) {
        glesLayer = layer
        gpb_set_gles_layer(Unmanaged.passUnretained(layer).toOpaque())
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
            print("IOS_GPU_CAPS \(jsonStr)")

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
                    supportsOpenGL: dict["opengl"] as? Bool ?? false,
                    supportsVulkanLiquid: dict["vulkanLiquid"] as? Bool ?? false,
                    supportsGpuCache: dict["gpuCache"] as? Bool ?? false,
                    supportsGles2: dict["gles2"] as? Bool ?? false,
                    gles2MaxSteps: dict["gles2MaxSteps"] as? Int ?? 0,
                    gpuCacheParallelSteps: dict["gpuCacheParallelSteps"] as? Int ?? 0
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

    func cancel(channel: BenchChannel) {
        guard state(for: channel).isRunning else { return }
        if completeSuiteRunning {
            suiteCancelled = true
            completeSuiteReport?.cancellationReason = "stop_\(channel.rawValue)"
        }
        cancelRequested = true
        gpb_stop_workload()
        mutate(channel) {
            $0.statusIsIdle = false
            $0.statusText = Localization.tr(
                "Cancel requested…", "已请求取消…", "キャンセル要求済み…")
        }
    }

    func cancelIfRunning() {
        if completeSuiteRunning {
            cancelCompleteSuite(reason: "background")
            return
        }
        if isRunning || gpb_is_running() {
            cancel(channel: activeChannel)
        }
    }

    /// An explicit device launch argument starts the same flow as the GUI button, once per launch.
    func claimCompleteSuiteLaunch() -> Bool {
        guard !suiteLaunchHandled,
              ProcessInfo.processInfo.arguments.contains(where: {
                  ["--ios-complete-suite", "--ios-complete-suite-resume"].contains($0)
              }) else { return false }
        suiteLaunchHandled = true
        return true
    }

    func cancelCompleteSuite(reason: String = "user") {
        guard completeSuiteRunning else { return }
        suiteCancelled = true
        cancel(channel: activeChannel)
        completeSuiteReport?.cancellationReason = reason
        print("IOS_SUITE_CANCEL \(reason) job=\(completeSuiteIndex + 1) workloadRunning=\(workloadRunning)")
    }

    func startCompleteSuite(resume: Bool = false, scope: MobileSuiteScope = .all) {
        guard !isRunning, !workingDirectory.isEmpty, scope == .cpu || metalLayer != nil else { return }
        var jobs = supportedSuiteJobs.filter { scope.includes($0) }
        // Device automation can target an explicit subset without changing the
        // GUI's complete-suite button or touching unrelated saved measurements.
        let launchArguments = ProcessInfo.processInfo.arguments
        if let index = launchArguments.firstIndex(of: "--ios-suite-jobs") {
            guard index + 1 < launchArguments.count else {
                lastError = "Missing targeted suite job IDs"; return
            }
            let selected = Set(launchArguments[index + 1].split(separator: ",").map(String.init))
            guard !selected.isEmpty, selected.isSubset(of: Set(jobs.map(\.id))) else {
                lastError = "Unsupported targeted suite job IDs"; return
            }
            jobs = jobs.filter { selected.contains($0.id) }
        }
        guard !jobs.isEmpty else { lastError = "No supported jobs in this scope"; return }
        print("IOS_SUITE_CAPS translated=\(allowTranslatedApis) vulkan=\(vulkanAvailable) liquid=\(vulkanLiquidAvailable) cache=\(gpuCacheAvailable)")
        completeSuiteJobs = jobs
        let now = ISO8601DateFormatter().string(from: Date())
        let id = "complete-suite-" + UUID().uuidString
        completeSuiteReport = MobileSuiteReport(id: id, startedAt: now, jobs: jobs.map {
            MobileSuiteReport.Job(id: $0.id, title: $0.title,
                request: $0.gpuRequest ?? $0.cpuArguments.joined(separator: " "),
                expectedRecords: $0.expectedRecords)
        })
        var priorLog = ""
        if resume {
            let directory = URL(fileURLWithPath: workingDirectory).appendingPathComponent("suite-runs")
            if let data = try? Data(contentsOf: directory.appendingPathComponent("latest.json")),
               let prior = try? JSONDecoder().decode(MobileSuiteReport.self, from: data) {
                refreshResults()
                completeSuiteReport?.resumedFrom = prior.id
                for old in prior.reusableJobs(for: jobs, results: results) {
                    if let index = jobs.firstIndex(where: { $0.id == old.id }) {
                        completeSuiteReport?.jobs[index] = old
                    }
                }
                if prior.id.hasPrefix("complete-suite-"), !prior.id.contains("/"), !prior.id.contains("..") {
                    priorLog = (try? String(contentsOf: directory.appendingPathComponent(prior.id + ".log"), encoding: .utf8)) ?? ""
                }
            }
        }
        completeSuiteRunning = true
        completeSuiteIndex = 0
        suiteCancelled = false
        UIApplication.shared.isIdleTimerDisabled = true

        Task { [self] in
            var combinedCPUOutput = RunOutput()
            var log = "iOS complete suite \(id)\nStarted: \(now)\n" + priorLog
            persistCompleteSuite(log: log)
            for (index, job) in jobs.enumerated() {
                if suiteCancelled { break }
                if completeSuiteReport?.jobs[index].status == "passed" { continue }
                completeSuiteIndex = index
                do {
                    // All one-click suites start each measurement from the
                    // same reported thermal state, including ordinary GUI runs.
                    var coolingSeconds = 0
                    if ProcessInfo.processInfo.thermalState != .nominal {
                        mutate(job.gpuRequest == nil ? .cpu : .gpu) {
                            $0.statusText = Localization.tr("Cooling before the next test…", "正在等待设备降温…", "次のテストの前に冷却を待っています…")
                        }
                    }
                    while ProcessInfo.processInfo.thermalState != .nominal &&
                          !suiteCancelled && coolingSeconds < 600 {
                        if coolingSeconds % 30 == 0 {
                            print("IOS_SUITE_COOLING \(job.id) thermal=\(ProcessInfo.processInfo.thermalState.rawValue) waited=\(coolingSeconds)s")
                        }
                        try? await Task.sleep(nanoseconds: 5_000_000_000)
                        coolingSeconds += 5
                    }
                    if suiteCancelled { break }
                    if ProcessInfo.processInfo.thermalState != .nominal {
                        completeSuiteReport?.jobs[index].status = "failed"
                        completeSuiteReport?.jobs[index].error = "Device did not cool to nominal within 600 seconds; measurement not run"
                        log += "\nThermal cooldown timed out for \(job.id); no measurement saved.\n"
                        persistCompleteSuite(log: log)
                        continue
                    }
                    log += "\nThermal precheck \(job.id): nominal after \(coolingSeconds)s cooling.\n"
                }
                if job.gpuRequest?.contains("api=gles2") == true {
                    try? await Task.sleep(nanoseconds: 100_000_000)
                }
                refreshResults()
                let before = Set(results.map(\.id))
                completeSuiteReport?.jobs[index].status = "running"
                completeSuiteReport?.jobs[index].startedAt = ISO8601DateFormatter().string(from: Date())
                completeSuiteReport?.jobs[index].thermalStateStart = ProcessInfo.processInfo.thermalState.rawValue
                persistCompleteSuite(log: log)
                print("IOS_SUITE_START \(index + 1)/\(jobs.count) \(job.id)")
                startingSuiteJob = true
                let channel: BenchChannel = job.gpuRequest == nil ? .cpu : .gpu
                let started: Bool
                if let request = job.gpuRequest {
                    started = startPreview(workload: request, seconds: job.seconds)
                } else {
                    run(jobs: [job.cpuArguments], needCharts: false, channel: .cpu)
                    started = cpuState.isRunning
                }
                startingSuiteJob = false
                while workloadRunning {
                    try? await Task.sleep(nanoseconds: 250_000_000)
                }
                if channel == .cpu {
                    for line in cpuState.logOutput.split(whereSeparator: \.isNewline) {
                        combinedCPUOutput.consume(String(line))
                    }
                    cpuState.runOutput = combinedCPUOutput
                }
                refreshResults()
                let added = results.filter { !before.contains($0.id) }
                let valid = added.count == job.expectedRecords && added.allSatisfy {
                    job.matches($0) && $0.score.isFinite && $0.score > 0 &&
                    ($0.workloadVersion ?? $0.workload).contains("ios_preview")
                } && (job.id != "cpu_compute" || Set(added.map(\.workload)) ==
                    Set(["cpu_single_core", "cpu_multi_core"]))
                let failed = channel == .cpu ? runFailed : !lastError.isEmpty
                // A late dismissal after a native job has saved its complete result
                // must not turn that completed job into a cancelled measurement.
                let reviewReasons = added.flatMap(\.measurementReviewReasons)
                let status = started && !failed && valid && !cancelRequested ?
                    (reviewReasons.isEmpty ? "passed" : "review") :
                    suiteCancelled ? "cancelled" : "failed"
                completeSuiteReport?.jobs[index].status = status
                completeSuiteReport?.jobs[index].resultIDs = added.map(\.id)
                completeSuiteReport?.jobs[index].finishedAt = ISO8601DateFormatter().string(from: Date())
                completeSuiteReport?.jobs[index].thermalStateEnd = ProcessInfo.processInfo.thermalState.rawValue
                if status == "review" {
                    completeSuiteReport?.jobs[index].error = Array(Set(reviewReasons)).sorted().joined(separator: "; ")
                }
                if status == "failed" {
                    completeSuiteReport?.jobs[index].error = !lastError.isEmpty ? lastError :
                        "Expected \(job.expectedRecords) matching saved results; found \(added.count). See log."
                }
                log += "\n=== \(index + 1)/\(jobs.count) \(job.id): \(status) ===\n" + state(for: channel).logOutput
                persistCompleteSuite(log: log)
                print("IOS_SUITE_END \(index + 1)/\(jobs.count) \(job.id) \(status)")
                // Let the previous native host and drawable pool finish releasing resources.
                if !suiteCancelled { try? await Task.sleep(nanoseconds: 500_000_000) }
            }
            completeSuiteReport?.finishedAt = ISO8601DateFormatter().string(from: Date())
            let savedCount = (completeSuiteReport?.passedCount ?? 0) + (completeSuiteReport?.reviewCount ?? 0)
            completeSuiteReport?.status = savedCount == jobs.count ?
                ((completeSuiteReport?.reviewCount ?? 0) > 0 ? "review" : "passed") :
                suiteCancelled ? "cancelled" : "failed"
            if suiteCancelled {
                for index in jobs.indices where completeSuiteReport?.jobs[index].status == "pending" {
                    completeSuiteReport?.jobs[index].status = "not_run"
                }
            }
            persistCompleteSuite(log: log)
            print("IOS_SUITE_DONE \(completeSuiteReport?.status ?? "failed") \(completeSuiteReport?.passedCount ?? 0)/\(jobs.count)")
            completeSuiteRunning = false
            UIApplication.shared.isIdleTimerDisabled = false
        }
    }

    private func persistCompleteSuite(log: String) {
        guard let report = completeSuiteReport else { return }
        let directory = URL(fileURLWithPath: workingDirectory).appendingPathComponent("suite-runs", isDirectory: true)
        do {
            try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
            let encoder = JSONEncoder()
            encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
            try encoder.encode(report).write(to: directory.appendingPathComponent(report.id + ".json"), options: .atomic)
            try log.write(to: directory.appendingPathComponent(report.id + ".log"), atomically: true, encoding: .utf8)
            // A stable pointer makes retrieval possible without guessing a UUID.
            try encoder.encode(report).write(to: directory.appendingPathComponent("latest.json"), options: .atomic)
        } catch {
            lastError = "Cannot save suite log: \(error.localizedDescription)"
            suiteCancelled = true
        }
    }

    /// Starts a GPU test of the embedded host in the attached CAMetalLayer.
    /// `workload` is a test of `GpuTest.all`, with its size where it has one:
    /// "stream?particles=4194304", "gpu_burn?steps=64".
    @discardableResult
    func startPreview(workload: String, seconds: Double = 3.0) -> Bool {
        let test = workload.components(separatedBy: "?").first ?? workload
        guard metalLayer != nil || ["memory_sweep", "stream_headless", "gpu_cache"].contains(test) else {
            lastError = "Metal layer not ready"
            mutate(.gpu) { $0.statusIsIdle = false; $0.statusText = "Metal layer not ready" }
            return false
        }
        guard !isRunning || startingSuiteJob else { return false }
        activeChannel = .gpu
        cancelRequested = false
        lastError = ""
        resetRunOutput()
        if let stale = gpb_poll_output() { gpb_free(stale) }
        mutate(.gpu) {
            $0.isRunning = true
            $0.logOutput = ""
            $0.lastScore = ""
            $0.statusIsIdle = false
            $0.statusText = Localization.tr(
                "Running \(workload) preview (\(Int(seconds))s)…",
                "运行 \(workload) 预览（\(Int(seconds)) 秒）…",
                "\(workload) プレビュー実行中（\(Int(seconds))秒）…")
            $0.logOutput += "=== Preview \(workload) \(seconds)s ===\n"
        }

        let ok = gpb_start_workload_with_options(workload, seconds, allowTranslatedApis)
        if !ok {
            if let err = gpb_last_error() {
                lastError = String(cString: err)
                gpb_free(err)
            }
            mutate(.gpu) {
                $0.statusIsIdle = false
                $0.statusText = self.lastError.isEmpty ? "Start failed" : self.lastError
                $0.isRunning = false
            }
            return false
        }
        startPolling(channel: .gpu)
        return true
    }

    private func startPolling(channel: BenchChannel) {
        pollTask?.cancel()
        pollTask = Task { [weak self] in
            while !Task.isCancelled {
                let running = gpb_is_running()
                await MainActor.run {
                    guard let self else { return }
                    self.drainEmbeddedOutput()
                    self.mutate(channel) { $0.isRunning = running }
                    if !running {
                        self.mutate(channel) { $0.progressFraction = 1; $0.progressLabel = "100%" }
                        self.flushOutput()
                        if let err = gpb_last_error() {
                            let s = String(cString: err)
                            gpb_free(err)
                            if !s.isEmpty {
                                self.lastError = s
                                self.mutate(channel) {
                                    $0.logOutput += "ERROR: \(s)\n"
                                    $0.statusIsIdle = false
                                    $0.statusText = s
                                }
                            } else if self.cancelRequested {
                                self.mutate(channel) {
                                    $0.statusIsIdle = false
                                    $0.statusText = Localization.tr("Cancelled.", "已取消。", "キャンセルしました。")
                                }
                            } else {
                                self.mutate(channel) {
                                    $0.statusIsIdle = false
                                    $0.statusText = Localization.tr(
                                    "Preview done (ios_preview — do not mix with desktop scores).",
                                    "预览完成（ios_preview — 勿与桌面成绩混排）。",
                                    "プレビュー完了（ios_preview — デスクトップ成績と混在禁止）。")
                                }
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

    /// Run CPU jobs without a Metal surface.
    func run(jobs: [[String]], needCharts: Bool, channel: BenchChannel) {
        guard (!isRunning || startingSuiteJob), !jobs.isEmpty else { return }
        activeChannel = channel

        cancelRequested = false
        runFailed = false
        resetRunOutput()
        mutate(channel) {
            $0.isRunning = true
            $0.logOutput = ""
            $0.lastScore = ""
            $0.progressFraction = 0
            $0.progressLabel = "0%"
            $0.statusIsIdle = false
            $0.statusText = jobs.count > 1
                ? Localization.tr("Running… (multiple passes)",
                                   "运行中…（多趟）",
                                   "実行中…（複数パス）")
                : Localization.tr("Running…", "运行中…", "実行中…")
        }

        let cwd = workingDirectory
        Task.detached { [weak self] in
            guard let self else { return }
            gpb_set_working_dir(cwd)

            for (index, job) in jobs.enumerated() {
                if await MainActor.run(body: { self.cancelRequested }) { break }

                await MainActor.run {
                    let pct = Double(index) / Double(max(jobs.count, 1))
                    self.mutate(channel) {
                        $0.progressFraction = pct
                        $0.progressLabel = String(format: "%.0f%%", pct * 100)
                        $0.logOutput += "=== Job \(index + 1) of \(jobs.count): "
                            + "\(job.joined(separator: " ")) ===\n"
                    }
                }

                let code = await self.runViaBridge(job: job, channel: channel)
                if code != 0 {
                    await MainActor.run { self.runFailed = true }
                    break
                }
            }

            await MainActor.run {
                self.flushOutput()
                self.refreshResults()
                let cancelled = self.cancelRequested
                let failed = self.runFailed
                self.mutate(channel) { st in
                    st.isRunning = false
                    st.progressFraction = 1
                    st.progressLabel = "100%"
                    st.statusIsIdle = false
                    if cancelled {
                        st.statusText = Localization.tr("Cancelled.", "已取消。", "キャンセルしました。")
                    } else if failed {
                        st.statusText = Localization.tr("Run failed — see output.", "测试失败 —— 请查看输出。", "実行失敗 — 出力を確認してください。")
                    } else if st.lastScore.isEmpty {
                        st.statusText = Localization.tr(
                            "Done — see output / History.", "完成 —— 见输出/历史。",
                            "完了 — 出力/履歴を確認。")
                    } else {
                        st.statusText = Localization.tr(
                            "Done. \(st.lastScore)", "完成。\(st.lastScore)",
                            "完了。\(st.lastScore)")
                    }
                }
            }
        }
    }

    nonisolated private func runViaBridge(job: [String], channel: BenchChannel) async -> Int32 {
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

        let selfRefBits = await MainActor.run {
            UInt(bitPattern: Unmanaged.passUnretained(self).toOpaque())
        }
        let selfRef = UnsafeMutableRawPointer(bitPattern: selfRefBits)
        let code = cPtrs.withUnsafeMutableBufferPointer { buf in
            gpb_run(buf.baseAddress, argc, callback, selfRef)
        }
        for ptr in cStrings { free(ptr) }
        // A FIFO barrier drains every callback before a later job can change ownership.
        await withCheckedContinuation { (continuation: CheckedContinuation<Void, Never>) in
            DispatchQueue.main.async { continuation.resume() }
        }
        return code
    }

    /// Collects what the embedded run printed since the last call.
    private func drainEmbeddedOutput() {
        guard let text = gpb_poll_output() else { return }
        let chunk = String(cString: text)
        gpb_free(text)
        if !chunk.isEmpty { appendOutput(chunk) }
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
        mutate(activeChannel) { state in
            state.logOutput += line + "\n"
            if let score = RunOutput.gpuMemoryScoreLine(line) { state.lastScore = score }
            if line.hasPrefix("Avg FPS:") || line.contains("Score:") {
                state.lastScore = Localization.scoreLine(line.trimmingCharacters(in: .whitespacesAndNewlines))
            }
            state.runOutput.consume(line)
            if let fraction = state.runOutput.progress {
                state.progressFraction = fraction
                state.progressLabel = String(format: "%.0f%%", fraction * 100)
            }
            if let stage = state.runOutput.stage, stage != shownStage {
                shownStage = stage
                state.statusText = Self.stageText(stage)
            }
        }
    }

    private func resetRunOutput() {
        mutate(activeChannel) { $0.runOutput = RunOutput() }
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
