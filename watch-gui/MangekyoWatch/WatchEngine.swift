import Foundation
import Combine

@MainActor
final class WatchEngine: ObservableObject {
    @Published private(set) var results: [WatchResult] = []
    @Published private(set) var running = false
    @Published private(set) var progress = 0.0
    @Published private(set) var stage = ""
    @Published private(set) var suite: WatchSuite?
    @Published private(set) var error = ""
    private var cancelled = false
    let smoke = ProcessInfo.processInfo.arguments.contains("--watch-smoke")
    let dataDirectory: URL
    var simulator: Bool {
        #if targetEnvironment(simulator)
        true
        #else
        false
        #endif
    }
    var finishedJobs: Int { suite?.jobs.filter { ["passed", "review"].contains($0.status) }.count ?? 0 }

    init() {
        dataDirectory = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("Mangekyo", isDirectory: true)
        do {
            try FileManager.default.createDirectory(at: dataDirectory.appendingPathComponent("suite-runs"), withIntermediateDirectories: true)
            dataDirectory.path.withCString { wcpu_initialize($0) }
            if let data = try? Data(contentsOf: manifestURL), var previous = try? JSONDecoder().decode(WatchSuite.self, from: data) {
                if previous.status == "running" {
                    previous.status = "interrupted"
                    for index in previous.jobs.indices where previous.jobs[index].status == "running" {
                        previous.jobs[index].status = "interrupted"
                    }
                    previous.updatedAt = Date()
                }
                suite = previous
                try persist()
            }
            try reload()
        } catch { self.error = error.localizedDescription }
    }
    private var manifestURL: URL { dataDirectory.appendingPathComponent("suite-runs/latest.json") }
    private func persist() throws {
        guard var value = suite else { return }
        value.updatedAt = Date(); suite = value
        let encoder = JSONEncoder(); encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        let data = try encoder.encode(value)
        try data.write(to: manifestURL, options: .atomic)
        try data.write(to: dataDirectory.appendingPathComponent("suite-runs/\(value.id).json"), options: .atomic)
    }
    private func text(_ pointer: UnsafeMutablePointer<CChar>?) -> String? {
        guard let pointer else { return nil }
        defer { wcpu_free(pointer) }
        return String(cString: pointer)
    }
    private func reload() throws {
        guard let value = text(wcpu_load_results()) else { throw failure("Unable to read results.json") }
        results = try JSONDecoder().decode([WatchResult].self, from: Data(value.utf8))
    }
    private func failure(_ message: String) -> NSError {
        NSError(domain: "MangekyoWatch", code: 1, userInfo: [NSLocalizedDescriptionKey: message])
    }
    func cancel() {
        guard running else { return }
        cancelled = true; wcpu_cancel()
        stage = watchText("正在停止…", "Stopping…")
    }
    func startAll() {
        guard !running else { return }
        guard ProcessInfo.processInfo.thermalState.rawValue < 2 else {
            error = watchText("当前热状态较高，请冷却后再测", "Thermal state is elevated; retry after cooling")
            return
        }
        running = true; cancelled = false; error = ""; progress = 0
        let now = Date()
        suite = WatchSuite(id: UUID().uuidString, startedAt: now, updatedAt: now, status: "running",
                           profile: smoke ? "smoke" : "standard", executionEnvironment: simulator ? "simulator" : "device",
                           jobs: [WatchJob(id: "cpu_compute"), WatchJob(id: "cpu_memory")])
        Task { await runAll() }
    }
    private func collectProgress(job: Int) {
        guard let output = text(wcpu_poll_output()) else { return }
        for line in output.split(separator: "\n") {
            guard line.hasPrefix("CPU_PROGRESS\t") || line.hasPrefix("CPUMEM_PROGRESS\t") else { continue }
            let values = Dictionary(line.split(separator: "\t").dropFirst().compactMap { entry -> (String, String)? in
                let p = entry.split(separator: "=", maxSplits: 1)
                return p.count == 2 ? (String(p[0]), String(p[1])) : nil
            }, uniquingKeysWith: { _, last in last })
            if let value = Double(values["overall_fraction"] ?? values["fraction"] ?? "") {
                progress = (Double(job) + min(1, max(0, value))) / 2
            }
        }
    }
    private func runAll() async {
        defer { running = false }
        do {
            try persist()
            for index in 0..<2 {
                if cancelled { break }
                let oldIDs = Set(results.map(\.id))
                suite?.jobs[index].status = "running"
                stage = index == 0 ? watchText("算力：单线程＋全核心", "Compute: single + all cores")
                                   : watchText("缓存／内存／页面／存储", "Cache / memory / pages / storage")
                try persist()
                guard wcpu_start(Int32(index), smoke) else { throw failure("CPU worker could not start") }
                while wcpu_running() {
                    collectProgress(job: index)
                    try? await Task.sleep(for: .milliseconds(200))
                }
                collectProgress(job: index)
                let code = wcpu_exit_code()
                try reload()
                let added = results.filter { !oldIDs.contains($0.id) }
                suite?.jobs[index].resultIDs = added.map(\.id)
                if cancelled || code == 130 {
                    cancelled = true; suite?.jobs[index].status = "cancelled"; break
                }
                guard code == 0, added.count == (index == 0 ? 2 : 1),
                      added.allSatisfy({ $0.platform == "watchOS" && $0.workloadVersion.contains("watchos_preview") }) else {
                    suite?.jobs[index].status = "failed"
                    throw failure(text(wcpu_error()).flatMap { $0.isEmpty ? nil : $0 } ?? "CPU run incomplete (\(code))")
                }
                let notes = added.flatMap(\.reviewReasons)
                suite?.jobs[index].notes = notes
                suite?.jobs[index].status = notes.isEmpty ? "passed" : "review"
                progress = Double(index + 1) / 2
                try persist()
            }
            suite?.status = cancelled ? "cancelled" : suite?.jobs.contains(where: { $0.status == "review" }) == true ? "review" : "passed"
            stage = cancelled ? watchText("已取消", "Cancelled") : watchText("已完成 2/2", "Completed 2/2")
            try persist()
        } catch {
            wcpu_cancel()
            self.error = error.localizedDescription
            suite?.status = "failed"
            try? persist()
        }
    }
}
