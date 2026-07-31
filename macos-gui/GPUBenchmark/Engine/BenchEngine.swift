// BenchEngine.swift — Swift wrapper around GpuBenchBridge / external CLI.
// Manages benchmark state, stdout capture, cancel, and GPU enumeration.

import AppKit
import Darwin
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

enum BenchRunIssueKind: String, Hashable, Sendable {
    case unsupported
    case apiUnavailable
    case vulkanRuntimeMissing
    case openGLVersion
    case workloadUnsupported
    case workerTimeout
    case deviceLost
    case shaderPipeline
    case swapchainOutOfDate
    case resourceAllocation
    case captureUnavailable
    case suspectResult
    case postProcessing
    case unknown
}

struct BenchRunIssue: Identifiable, Hashable, Sendable {
    let kind: BenchRunIssueKind
    let target: String

    var id: String { "\(kind.rawValue)|\(target)" }

    var message: String {
        switch kind {
        case .unsupported:
            return Localization.tr(
                "The selected GPU/API combination was not reported as supported.",
                "系统未报告支持所选的 GPU/API 组合。",
                "選択した GPU/API の組み合わせは対応対象として報告されていません。")
        case .apiUnavailable:
            return Localization.tr(
                "The graphics API or requested adapter could not be initialised.",
                "图形 API 或所请求的设备无法初始化。",
                "グラフィックス API または要求したアダプタを初期化できませんでした。")
        case .vulkanRuntimeMissing:
            return Localization.tr(
                "The Vulkan loader or MoltenVK runtime is unavailable; install it or use Metal.",
                "Vulkan Loader 或 MoltenVK Runtime 不可用；请安装它们或改用 Metal。",
                "Vulkan Loader / MoltenVK Runtime がありません。インストールするか Metal を使用してください。")
        case .openGLVersion:
            return Localization.tr(
                "The active OpenGL renderer does not provide the required OpenGL 4.3 features.",
                "当前 OpenGL renderer 不提供测试所需的 OpenGL 4.3 功能。",
                "現在の OpenGL レンダラは必要な OpenGL 4.3 機能を提供しません。")
        case .workloadUnsupported:
            return Localization.tr(
                "This workload or requested feature is not supported by the selected API/device.",
                "所选 API/设备不支持该测试项目或请求的功能。",
                "選択した API/デバイスはこのワークロードまたは要求機能に対応していません。")
        case .workerTimeout:
            return Localization.tr(
                "The benchmark exceeded its safety timeout and the worker was stopped.",
                "测试超过安全超时时间，worker 已停止。",
                "ベンチマークが安全タイムアウトを超えたため、ワーカーを停止しました。")
        case .deviceLost:
            return Localization.tr(
                "The GPU device/driver was lost or reset during the run; restart the app and check system stability.",
                "运行期间 GPU 设备丢失或驱动重置；请重启程序并检查系统稳定性。",
                "実行中に GPU デバイス/ドライバが消失またはリセットされました。アプリを再起動し、安定性を確認してください。")
        case .shaderPipeline:
            return Localization.tr(
                "Shader compilation, program linking, or graphics/compute pipeline creation failed.",
                "Shader 编译、程序链接或图形/计算管线创建失败。",
                "シェーダのコンパイル、リンク、またはパイプライン作成に失敗しました。")
        case .swapchainOutOfDate:
            return Localization.tr(
                "The render surface became out of date, usually after a resize or display change; run the test again.",
                "渲染表面已失效，通常由调整窗口或显示配置变化引起；请重新运行。",
                "描画サーフェスが無効になりました。ウィンドウや表示設定を戻して再実行してください。")
        case .resourceAllocation:
            return Localization.tr(
                "GPU memory/resource allocation failed; reduce the workload or close other GPU-heavy apps.",
                "GPU 内存或资源分配失败；请降低负载或关闭其他占用 GPU 的应用。",
                "GPU メモリ/リソースの割り当てに失敗しました。負荷を下げるか、他の GPU アプリを閉じてください。")
        case .captureUnavailable:
            return Localization.tr(
                "The score completed, but the requested GPU capture was unavailable or was not written.",
                "成绩已完成，但请求的 GPU 抓帧不可用或未写入。",
                "スコアは完了しましたが、要求した GPU キャプチャは利用できないか保存されませんでした。")
        case .suspectResult:
            return Localization.tr(
                "The reported bandwidth/timing looks implausible. The original value was retained for diagnosis.",
                "报告的带宽或计时不符合合理范围；原始值已保留以便排查。",
                "報告された帯域幅/計測値が妥当ではありません。診断用に元の値を保持しました。")
        case .postProcessing:
            return Localization.tr(
                "Chart generation did not finish; benchmark scores may still be valid.",
                "图表生成未完成；测试成绩可能仍然有效。",
                "チャート生成が完了しませんでした。ベンチマークスコアは有効な場合があります。")
        case .unknown:
            return Localization.tr(
                "The worker exited unexpectedly; expand Raw CLI output for the original error.",
                "Worker 意外退出；请展开原始 CLI 输出查看错误。",
                "ワーカーが予期せず終了しました。生の CLI 出力で元のエラーを確認してください。")
        }
    }
}

struct BenchRunSummary: Equatable, Sendable {
    var planned = 0
    var completed = 0
    var succeeded = 0
    var failed = 0
    var skipped = 0
    var issues: [BenchRunIssue] = []

    mutating func addIssue(_ issue: BenchRunIssue) {
        guard !issues.contains(issue) else { return }
        issues.append(issue)
    }
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
    var runSummary = BenchRunSummary()
    /// All-core score of the current CPU run, in MWork/s.
    var cpuAllCoreScore: Double?
    /// Mean per-logical-processor score of the current CPU run, in MWork/s.
    var cpuSingleCoreScore: Double?
}

private struct BenchJobOutcome: Sendable {
    let exitCode: Int32?
    let output: String
    let timedOut: Bool
    let cancelled: Bool
    let launchError: String?
}

private final class ThreadSafeText: @unchecked Sendable {
    private let lock = NSLock()
    private var value = ""

    func append(_ text: String) {
        lock.lock()
        value += text
        lock.unlock()
    }

    func snapshot() -> String {
        lock.lock()
        defer { lock.unlock() }
        return value
    }
}

private final class BridgeOutputContext: @unchecked Sendable {
    weak var engine: BenchEngine?
    /// Carried here rather than captured: the bridge callback is a C function
    /// pointer and cannot close over anything but its opaque context.
    let channel: BenchChannel
    let output = ThreadSafeText()

    init(engine: BenchEngine, channel: BenchChannel) {
        self.engine = engine
        self.channel = channel
    }
}

/// Observable engine state for SwiftUI binding.
@MainActor
final class BenchEngine: ObservableObject {
    @Published var gpus: [GpuDevice] = []
    @Published var workingDirectory: String = ""
    @Published var results: [BenchResult] = []

    /// Per-page run state. The GPU and CPU pages never share console output,
    /// progress, scores, or issue lists.
    @Published var gpuState = BenchChannelState()
    @Published var cpuState = BenchChannelState()

    /// Available graphics APIs on macOS (no DX).
    static let availableAPIs = ["Auto", "Metal", "Vulkan", "OpenGL"]

    static var readyText: String {
        Localization.tr("Ready", "就绪", "準備完了")
    }

    /// True while any channel is running; only one worker runs at a time.
    var isRunning: Bool { gpuState.isRunning || cpuState.isRunning }

    func state(for channel: BenchChannel) -> BenchChannelState {
        channel == .gpu ? gpuState : cpuState
    }

    private func mutate(_ channel: BenchChannel, _ body: (inout BenchChannelState) -> Void) {
        switch channel {
        case .gpu: body(&gpuState)
        case .cpu: body(&cpuState)
        }
    }

    /// Re-render idle status strings after the UI language changes.
    func refreshLocalizedStatus() {
        if gpuState.statusIsIdle { gpuState.statusText = Self.readyText }
        if cpuState.statusIsIdle { cpuState.statusText = Self.readyText }
    }

    private var activeProcess: Process?
    private var activeChannel: BenchChannel?
    private var cancelRequested = false

    init() {
        gpuState.statusText = Self.readyText
        cpuState.statusText = Self.readyText
        if let bundledCli = Self.bundleCliCandidates()
            .first(where: { FileManager.default.isExecutableFile(atPath: $0.path) }) {
            // A distributed app keeps the CLI and all shaders in Contents/Helpers.
            workingDirectory = bundledCli.deletingLastPathComponent().path
        } else {
            let starts = [
                URL(fileURLWithPath: FileManager.default.currentDirectoryPath),
                Bundle.main.executableURL?.deletingLastPathComponent(),
                URL(fileURLWithPath: #filePath).deletingLastPathComponent(),
            ].compactMap { $0 }
            workingDirectory = starts.lazy
                .compactMap(Self.findRepositoryRoot(startingAt:))
                .first?
                .path ?? Bundle.main.resourceURL?.path ?? ""
        }
        applyWorkingDirectory()
        // PathService stores results under Application Support, independent of
        // a repository checkout, so standalone builds can load history now.
        refreshResults()
    }

    func setWorkingDirectory(_ path: String) {
        workingDirectory = path
        applyWorkingDirectory()
        refreshGpus()
        refreshResults()
    }

    private func applyWorkingDirectory() {
        guard !workingDirectory.isEmpty,
              FileManager.default.fileExists(atPath: workingDirectory)
        else { return }
        gpb_set_working_dir(workingDirectory)
    }

    private static func findRepositoryRoot(startingAt start: URL) -> URL? {
        let markers = ["CMakeLists.txt", "src/gpu_engine.h"]
        var directory = start.standardizedFileURL
        for _ in 0..<10 {
            if markers.allSatisfy({
                FileManager.default.fileExists(
                    atPath: directory.appendingPathComponent($0).path)
            }) {
                return directory
            }
            let parent = directory.deletingLastPathComponent()
            if parent.path == directory.path { break }
            directory = parent
        }
        return nil
    }

    private static func bundleCliCandidates() -> [URL] {
        let bundle = Bundle.main.bundleURL
        var candidates = [
            bundle.appendingPathComponent("Contents/Helpers/gpu_benchmark"),
            bundle.appendingPathComponent("Contents/MacOS/gpu_benchmark"),
            bundle.deletingLastPathComponent().appendingPathComponent("gpu_benchmark"),
        ]
        if let executable = Bundle.main.executableURL {
            candidates.append(
                executable.deletingLastPathComponent()
                    .appendingPathComponent("gpu_benchmark"))
        }
        if let resources = Bundle.main.resourceURL {
            candidates += [
                resources.appendingPathComponent("gpu_benchmark"),
                resources.appendingPathComponent("Helpers/gpu_benchmark"),
            ]
        }
        return candidates
    }

    func refreshGpus() {
        let cwd = workingDirectory
        Task.detached {
            if !cwd.isEmpty {
                gpb_set_working_dir(cwd)
            }
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

    /// Resolve the packaged helper first, then development-tree builds.
    func resolveCliExecutable() -> String? {
        let fm = FileManager.default
        var candidates = Self.bundleCliCandidates()
        if !workingDirectory.isEmpty {
            let root = URL(fileURLWithPath: workingDirectory)
            candidates += [
                root.appendingPathComponent("gpu_benchmark"),
                root.appendingPathComponent("build/gpu_benchmark"),
                root.appendingPathComponent("build/Release/gpu_benchmark"),
                root.appendingPathComponent("cmake-build-release/gpu_benchmark"),
                root.appendingPathComponent("out/build/gpu_benchmark"),
            ]
        }
        var seen = Set<String>()
        for candidate in candidates {
            let path = candidate.standardizedFileURL.path
            guard seen.insert(path).inserted else { continue }
            if fm.isExecutableFile(atPath: path) {
                return path
            }
        }
        return nil
    }

    func cancel(channel: BenchChannel) {
        guard state(for: channel).isRunning else { return }
        cancelRequested = true
        if let proc = activeProcess, proc.isRunning, activeChannel == channel {
            proc.terminate()
            mutate(channel) {
                $0.statusIsIdle = false
                $0.statusText = Localization.tr(
                    "Cancel requested…", "已请求取消…", "キャンセル要求済み…")
            }
        } else {
            mutate(channel) {
                $0.statusIsIdle = false
                $0.statusText = Localization.tr(
                    "Cancel: close the render window if the run is in-process / unlimited.",
                    "取消：若为进程内或无限时长运行，请关闭渲染窗口。",
                    "キャンセル：プロセス内/無制限実行なら描画ウィンドウを閉じてください。")
            }
        }
    }

    /// Run a sequence of benchmark jobs on one channel.
    func run(jobs: [[String]], needCharts: Bool, channel: BenchChannel) {
        guard !isRunning, !jobs.isEmpty else { return }
        cancelRequested = false
        activeChannel = channel
        mutate(channel) {
            $0.isRunning = true
            $0.logOutput = ""
            $0.lastScore = ""
            $0.progressFraction = 0
            $0.progressLabel = "0%"
            $0.runSummary = BenchRunSummary(planned: jobs.count)
            $0.cpuAllCoreScore = nil
            $0.cpuSingleCoreScore = nil
            $0.statusIsIdle = false
            $0.statusText = Self.runningText(jobCount: jobs.count, channel: channel)
        }

        let cwd = workingDirectory
        let cli = resolveCliExecutable()
        Task.detached { [weak self] in
            guard let self else { return }
            if !cwd.isEmpty {
                gpb_set_working_dir(cwd)
            }

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

                let outcome: BenchJobOutcome
                if let cliPath = cli {
                    outcome = await self.runViaProcess(
                        cliPath: cliPath, job: job, channel: channel)
                } else {
                    outcome = await self.runViaBridge(
                        job: job, workingDirectory: cwd, channel: channel)
                }
                await MainActor.run {
                    self.record(outcome: outcome, job: job, channel: channel)
                }
            }

            let cancelledBeforeRefresh = await MainActor.run {
                self.cancelRequested
            }
            if needCharts && !cancelledBeforeRefresh {
                await MainActor.run {
                    self.mutate(channel) {
                        $0.statusText = Localization.tr(
                            "Refreshing native charts…",
                            "正在刷新原生图表…",
                            "ネイティブチャートを更新中…")
                    }
                }
            }

            await MainActor.run {
                self.activeProcess = nil
                self.activeChannel = nil
                self.refreshResults()
                let cancelled = self.cancelRequested
                self.mutate(channel) { state in
                    state.isRunning = false
                    state.progressFraction = 1
                    state.progressLabel = "100%"
                    state.statusText = Self.completionText(
                        state: state, cancelled: cancelled)
                }
            }
        }
    }

    private static func runningText(jobCount: Int, channel: BenchChannel) -> String {
        if channel == .cpu {
            return Localization.tr(
                "Running… (CPU only; no render window)",
                "运行中…（仅 CPU，不会弹出渲染窗口）",
                "実行中…（CPU のみ。描画ウィンドウなし）")
        }
        return jobCount > 1
            ? Localization.tr("Running… (multiple passes; render windows may appear)",
                              "运行中…（多趟；可能弹出渲染窗口）",
                              "実行中…（複数パス；描画ウィンドウが出る場合あり）")
            : Localization.tr("Running… (a render window may appear)",
                              "运行中…（可能弹出渲染窗口）",
                              "実行中…（描画ウィンドウが出る場合あり）")
    }

    private static func completionText(
        state: BenchChannelState,
        cancelled: Bool
    ) -> String {
        if cancelled {
            return Localization.tr("Cancelled.", "已取消。", "キャンセルしました。")
        }
        if state.runSummary.failed > 0 && state.runSummary.succeeded == 0 {
            return Localization.tr(
                "Failed — see Summary / Raw CLI output.",
                "运行失败 —— 请查看摘要 / 原始 CLI 输出。",
                "失敗 — 要約 / 生の CLI 出力を確認してください。")
        }
        if state.runSummary.failed > 0 {
            return Localization.tr(
                "Completed with errors.",
                "部分完成。",
                "エラーありで完了。")
        }
        if state.runSummary.skipped > 0 {
            return Localization.tr(
                "Completed — unsupported combinations skipped.",
                "完成 —— 已跳过不受支持的组合。",
                "完了 — 非対応の組み合わせをスキップしました。")
        }
        if !state.runSummary.issues.isEmpty {
            return Localization.tr(
                "Completed with warnings.",
                "完成，但有警告。",
                "警告ありで完了。")
        }
        if state.lastScore.isEmpty {
            return Localization.tr(
                "Done — see output / History.",
                "完成 —— 见输出/历史。",
                "完了 — 出力/履歴を確認。")
        }
        return Localization.tr(
            "Done. \(state.lastScore)",
            "完成。\(state.lastScore)",
            "完了。\(state.lastScore)")
    }

    private nonisolated func runViaProcess(
        cliPath: String,
        job: [String],
        channel: BenchChannel
    ) async -> BenchJobOutcome {
        // job[0] is usually "gpu_benchmark"; replace with absolute CLI.
        var args = Array(job.dropFirst())
        if job.first != "gpu_benchmark" {
            args = job
        }

        let cliURL = URL(fileURLWithPath: cliPath)
        return await runExternalProcess(
            executable: cliPath,
            arguments: args,
            currentDirectory: cliURL.deletingLastPathComponent().path,
            timeout: workerTimeoutSeconds(for: job),
            channel: channel)
    }

    private nonisolated func runExternalProcess(
        executable: String,
        arguments: [String],
        currentDirectory: String,
        timeout: TimeInterval?,
        channel: BenchChannel
    ) async -> BenchJobOutcome {
        let process = Process()
        process.executableURL = URL(fileURLWithPath: executable)
        process.arguments = arguments
#if os(macOS)
        // Programmatic .gputrace capture is opt-in.  The app bundle's
        // MetalCaptureEnabled key covers Monterey/Ventura and in-process runs;
        // macOS 14+ also accepts this environment variable for the external
        // CLI worker.  Suite children inherit it from their orchestration
        // process.
        let requestsCapture = arguments.contains("--capture") ||
            arguments.contains("--capture-frame") ||
            arguments.contains("--renderdoc") ||
            arguments.contains("--complete-suite") ||
            arguments.contains("--fill-missing") ||
            arguments.contains("--full-analysis")
        if requestsCapture {
            var environment = ProcessInfo.processInfo.environment
            environment["MTL_CAPTURE_ENABLED"] = "1"
            process.environment = environment
        }
#endif
        if FileManager.default.fileExists(atPath: currentDirectory) {
            process.currentDirectoryURL = URL(fileURLWithPath: currentDirectory)
        }

        let pipe = Pipe()
        process.standardOutput = pipe
        process.standardError = pipe
        let output = ThreadSafeText()

        await MainActor.run {
            self.activeProcess = process
            self.activeChannel = channel
        }

        pipe.fileHandleForReading.readabilityHandler = { handle in
            let data = handle.availableData
            guard !data.isEmpty, let chunk = String(data: data, encoding: .utf8) else { return }
            output.append(chunk)
            Task { @MainActor [weak self] in
                self?.consumeOutputChunk(chunk, channel: channel)
            }
        }

        var launchError: String?
        var timedOut = false
        do {
            try process.run()
        } catch {
            launchError = error.localizedDescription
            let line = "[GUI] Failed to launch process: \(error.localizedDescription)\n"
            output.append(line)
            await MainActor.run {
                self.consumeOutputChunk(line, channel: channel)
            }
        }

        let started = Date()
        while process.isRunning {
            if let timeout, Date().timeIntervalSince(started) >= timeout {
                timedOut = true
                let line = "[Process] Timed out and terminated.\n"
                output.append(line)
                await MainActor.run {
                    self.consumeOutputChunk(line, channel: channel)
                }
                process.terminate()
                break
            }
            try? await Task.sleep(nanoseconds: 200_000_000)
        }

        if process.isRunning {
            for _ in 0..<10 where process.isRunning {
                try? await Task.sleep(nanoseconds: 100_000_000)
            }
            if process.isRunning {
                kill(process.processIdentifier, SIGKILL)
                while process.isRunning {
                    try? await Task.sleep(nanoseconds: 50_000_000)
                }
            }
        }

        pipe.fileHandleForReading.readabilityHandler = nil
        let remainder = pipe.fileHandleForReading.readDataToEndOfFile()
        if !remainder.isEmpty, let chunk = String(data: remainder, encoding: .utf8) {
            output.append(chunk)
            await MainActor.run {
                self.consumeOutputChunk(chunk, channel: channel)
            }
        }
        let cancelled = await MainActor.run { self.cancelRequested }
        await MainActor.run { self.activeProcess = nil }
        return BenchJobOutcome(
            exitCode: launchError == nil ? process.terminationStatus : nil,
            output: output.snapshot(),
            timedOut: timedOut,
            cancelled: cancelled,
            launchError: launchError)
    }

    private nonisolated func runViaBridge(
        job: [String],
        workingDirectory: String,
        channel: BenchChannel
    ) async -> BenchJobOutcome {
        await MainActor.run {
            self.mutate(channel) {
                $0.logOutput += "[GUI] gpu_benchmark binary not found — "
                    + "using in-process bridge (Cancel limited).\n"
            }
        }
        if !workingDirectory.isEmpty {
            gpb_set_working_dir(workingDirectory)
        }
        let cStrings = job.map { strdup($0) }
        let argc = Int32(cStrings.count)
        var cPtrs = cStrings.map { UnsafePointer($0) }
        let context = BridgeOutputContext(engine: self, channel: channel)

        let callback: gpb_line_callback = { line, ctx in
            guard let line = line, let ctx = ctx else { return }
            let str = String(cString: line)
            let context = Unmanaged<BridgeOutputContext>
                .fromOpaque(ctx)
                .takeUnretainedValue()
            context.output.append(str + "\n")
            let target = context.channel
            Task { @MainActor [weak engine = context.engine] in
                engine?.consumeOutputChunk(str + "\n", channel: target)
            }
        }

        let contextRef = Unmanaged.passUnretained(context).toOpaque()
        let exitCode = cPtrs.withUnsafeMutableBufferPointer { buf in
            gpb_run(buf.baseAddress, argc, callback, contextRef)
        }
        for ptr in cStrings { free(ptr) }
        let cancelled = await MainActor.run { self.cancelRequested }
        return BenchJobOutcome(
            exitCode: exitCode,
            output: context.output.snapshot(),
            timedOut: false,
            cancelled: cancelled,
            launchError: nil)
    }

    private nonisolated func workerTimeoutSeconds(for job: [String]) -> TimeInterval? {
        if job.contains("--no-time-limit") { return nil }
        if job.contains("--complete-suite") || job.contains("--fill-missing") {
            return 24 * 60 * 60
        }
        // A per-core CPU sweep applies the requested time to every logical
        // processor, so the GPU-oriented default would kill long formal runs.
        if job.contains("--cpu-benchmark") {
            let seconds = job.firstIndex(of: "--cpu-time")
                .flatMap { job.indices.contains($0 + 1) ? Double(job[$0 + 1]) : nil }
                .map { $0.isFinite && $0 > 0 ? $0 : 1 } ?? 1
            let stages = Double(ProcessInfo.processInfo.activeProcessorCount + 1)
            return min(24 * 60 * 60, max(10 * 60, seconds * stages * 2 + 10 * 60))
        }
        if let index = job.firstIndex(of: "--time"),
           job.indices.contains(index + 1),
           let seconds = Double(job[index + 1]),
           seconds.isFinite, seconds >= 0 {
            return min(24 * 60 * 60, max(6 * 60, seconds + 5 * 60))
        }
        return 60 * 60
    }

    private func consumeOutputChunk(_ chunk: String, channel: BenchChannel) {
        mutate(channel) { $0.logOutput += chunk }
        for rawLine in chunk.split(
            omittingEmptySubsequences: true,
            whereSeparator: { $0.isNewline }
        ) {
            let line = String(rawLine)
            switch channel {
            case .gpu: consumeGpuLine(line)
            case .cpu: consumeCpuLine(line)
            }
        }
    }

    private func consumeGpuLine(_ line: String) {
        if line.hasPrefix("Avg FPS:") ||
            line.contains("Score:") ||
            line.contains("VRAM rate:") ||
            line.contains("RAM rate:") {
            gpuState.lastScore = line.trimmingCharacters(in: .whitespacesAndNewlines)
        }
        guard line.hasPrefix("SUITE_RUN"),
              let index = Self.recordInteger(line, key: "index"),
              let total = Self.recordInteger(line, key: "total"),
              total > 0
        else { return }
        gpuState.progressFraction = min(1, Double(index - 1) / Double(total))
        gpuState.progressLabel = "\(index)/\(total)"
    }

    /// The CPU worker reports its own TAB-separated records; the GPU FPS and
    /// suite formats never appear in a CPU run.
    private func consumeCpuLine(_ line: String) {
        if line.hasPrefix("CPU_PROGRESS\t") {
            guard let fraction = Self.recordDouble(line, key: "overall_fraction")
            else { return }
            let clamped = min(1, max(0, fraction))
            cpuState.progressFraction = clamped
            cpuState.progressLabel = String(format: "%.0f%%", clamped * 100)
            return
        }
        guard line.hasPrefix("CPU_RESULT\t") else { return }
        switch Self.recordField(line, key: "kind") {
        case "multi":
            cpuState.cpuAllCoreScore = Self.recordDouble(line, key: "score")
        case "summary":
            cpuState.cpuSingleCoreScore = Self.recordDouble(line, key: "average_score")
        default:
            return
        }
        cpuState.lastScore = Self.cpuScoreSummary(state: cpuState)
    }

    private static func cpuScoreSummary(state: BenchChannelState) -> String {
        var parts: [String] = []
        if let allCore = state.cpuAllCoreScore {
            parts.append(Localization.tr("All-core", "全核", "全コア")
                + String(format: ": %.1f MWork/s", allCore))
        }
        if let singleCore = state.cpuSingleCoreScore {
            parts.append(Localization.tr("Single-core avg", "单核均值", "単一コア平均")
                + String(format: ": %.1f MWork/s", singleCore))
        }
        return parts.joined(separator: "   ·   ")
    }

    private func record(outcome: BenchJobOutcome, job: [String], channel: BenchChannel) {
        guard !outcome.cancelled else { return }
        var summary = state(for: channel).runSummary
        let lines = outcome.output.components(separatedBy: .newlines)
        let suiteResults = lines.filter { $0.hasPrefix("SUITE_RESULT\t") }
        let suiteSkips = lines.filter { $0.hasPrefix("SUITE_SKIP\t") }
        let isSuite = !suiteResults.isEmpty ||
            !suiteSkips.isEmpty ||
            job.contains("--complete-suite") ||
            job.contains("--fill-missing")

        if isSuite {
            if let plannedLine = lines.first(where: {
                $0.trimmingCharacters(in: .whitespaces).hasPrefix("Planned now:")
            }), let planned = Self.firstInteger(after: "Planned now:", in: plannedLine) {
                summary.planned = planned
            }
            for line in suiteSkips {
                summary.completed += 1
                summary.skipped += 1
                let status = Self.recordField(line, key: "status") ?? "unsupported"
                let target = Self.suiteTarget(line)
                if status.contains("capture") {
                    summary.addIssue(BenchRunIssue(kind: .captureUnavailable, target: target))
                } else if status == "unsupported" {
                    summary.addIssue(BenchRunIssue(kind: .unsupported, target: target))
                }
            }
            for line in suiteResults {
                summary.completed += 1
                let status = Self.recordField(line, key: "status") ?? "failed"
                let target = Self.suiteTarget(line)
                switch status {
                case "success":
                    summary.succeeded += 1
                case "capture_unsupported":
                    summary.succeeded += 1
                    summary.addIssue(BenchRunIssue(kind: .captureUnavailable, target: target))
                case "unsupported":
                    summary.skipped += 1
                    summary.addIssue(BenchRunIssue(kind: .unsupported, target: target))
                case "capture_missing", "capture_failed":
                    summary.failed += 1
                    summary.addIssue(BenchRunIssue(kind: .captureUnavailable, target: target))
                default:
                    summary.failed += 1
                    summary.addIssue(BenchRunIssue(kind: .unknown, target: target))
                }
            }
        } else {
            summary.completed += 1
            if outcome.exitCode == 0 && outcome.launchError == nil && !outcome.timedOut {
                summary.succeeded += 1
            } else {
                summary.failed += 1
            }
        }

        let target = Self.jobTarget(job)
        for issue in Self.detectIssues(
            output: outcome.output,
            target: target,
            timedOut: outcome.timedOut,
            launchError: outcome.launchError,
            needsUnknownFallback: !isSuite && outcome.exitCode != 0
        ) {
            summary.addIssue(issue)
        }
        mutate(channel) { $0.runSummary = summary }
    }

    private static func detectIssues(
        output: String,
        target: String,
        timedOut: Bool,
        launchError: String?,
        needsUnknownFallback: Bool
    ) -> [BenchRunIssue] {
        let lower = output.lowercased()
        var kinds: [BenchRunIssueKind] = []
        func add(_ kind: BenchRunIssueKind) {
            if !kinds.contains(kind) { kinds.append(kind) }
        }

        if timedOut ||
            lower.contains("[process] timed out") ||
            lower.contains("exceeded its safety timeout") {
            add(.workerTimeout)
        }
        if lower.contains("vulkan runtime") ||
            lower.contains("vulkan loader") && lower.contains("not found") ||
            lower.contains("libvulkan") && lower.contains("not found") {
            add(.vulkanRuntimeMissing)
        }
        if lower.contains("opengl 4.3") &&
            (lower.contains("not support") || lower.contains("requires")) {
            add(.openGLVersion)
        }
        if lower.contains("device lost") ||
            lower.contains("vk_error_device_lost") ||
            lower.contains("gpu reset") {
            add(.deviceLost)
        }
        if lower.contains("shader compilation failed") ||
            lower.contains("program link failed") ||
            lower.contains("pipeline creation failed") ||
            lower.contains("vkcreateshadermodule failed") ||
            lower.contains("newcomputepipelinestate") && lower.contains("failed") {
            add(.shaderPipeline)
        }
        if lower.contains("swapchain") &&
            (lower.contains("out of date") || lower.contains("suboptimal")) {
            add(.swapchainOutOfDate)
        }
        if lower.contains("out of device memory") ||
            lower.contains("out of host memory") ||
            lower.contains("allocation failed") ||
            lower.contains("failed to find suitable memory type") {
            add(.resourceAllocation)
        }
        if lower.contains("captureunavailable") ||
            lower.contains("capture_missing") ||
            lower.contains("capture_failed") ||
            lower.contains("capture_unsupported") ||
            lower.contains("no gpu capture") {
            add(.captureUnavailable)
        }
        if lower.contains("not supported by") ||
            lower.contains("workload is not supported") ||
            lower.contains("available only on") {
            add(.workloadUnsupported)
        } else if lower.contains("not compiled in or failed to initialise") ||
                    lower.contains("failed to initialise") ||
                    launchError != nil {
            add(.apiUnavailable)
        } else if lower.contains("does not report") && lower.contains("support") {
            add(.unsupported)
        }
        if lower.contains("score may be abnormal") ||
            lower.contains("implausible") {
            add(.suspectResult)
        }
        if needsUnknownFallback && kinds.isEmpty {
            add(.unknown)
        }
        return kinds.map { BenchRunIssue(kind: $0, target: target) }
    }

    /// Read `key=value` from a TAB-separated CLI record (SUITE_* and CPU_*).
    private static func recordField(_ line: String, key: String) -> String? {
        let prefix = key + "="
        return line
            .split(separator: "\t")
            .dropFirst()
            .map(String.init)
            .first(where: { $0.hasPrefix(prefix) })
            .map { String($0.dropFirst(prefix.count)) }
    }

    private static func recordInteger(_ line: String, key: String) -> Int? {
        recordField(line, key: key).flatMap(Int.init)
    }

    private static func recordDouble(_ line: String, key: String) -> Double? {
        guard let value = recordField(line, key: key).flatMap(Double.init),
              value.isFinite
        else { return nil }
        return value
    }

    private static func suiteTarget(_ line: String) -> String {
        let key = recordField(line, key: "key") ?? ""
        guard !key.isEmpty else {
            return Localization.tr("Formal suite", "正式测试套件", "正式スイート")
        }
        return key.replacingOccurrences(of: "|", with: " · ")
    }

    private static func firstInteger(after marker: String, in line: String) -> Int? {
        guard let range = line.range(of: marker) else { return nil }
        return line[range.upperBound...]
            .trimmingCharacters(in: .whitespaces)
            .split(separator: " ")
            .first
            .flatMap { Int($0) }
    }

    private static func jobTarget(_ job: [String]) -> String {
        func value(after flag: String) -> String? {
            guard let index = job.firstIndex(of: flag),
                  job.indices.contains(index + 1)
            else { return nil }
            return job[index + 1]
        }
        if job.contains("--complete-suite") {
            return Localization.tr("Complete suite", "完整测试", "完全テスト")
        }
        if job.contains("--fill-missing") {
            return Localization.tr("Fill missing", "补齐缺失", "不足分を補完")
        }
        var parts: [String] = []
        if let backend = value(after: "--backend") {
            parts.append(backend.uppercased())
        }
        if let gpu = value(after: "--gpu") {
            parts.append("GPU \(gpu)")
        }
        parts.append(value(after: "--workload") ?? "stream")
        return parts.joined(separator: " · ")
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
