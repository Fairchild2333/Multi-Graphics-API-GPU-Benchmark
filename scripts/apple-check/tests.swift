// Tests of the Swift code that has no UI: the parser of engine output, the
// chart geometry and the translation of engine phrases. check.sh compiles this
// file with EngineOutput.swift, ChartGeometry.swift and Localization.swift and
// runs it on the engine output saved in fixtures/ (a Ryzen 7 9800X3D, Windows:
// a cache and memory sweep capped at 64 MiB, a CPU compute run, two GPU runs).

import Foundation

struct Checker {
    var failures = 0
    var count = 0

    mutating func expect(_ condition: Bool, _ what: String, line: Int = #line) {
        count += 1
        if condition { return }
        failures += 1
        print("FAILED (tests.swift:\(line)): \(what)")
    }

    mutating func equal<T: Equatable>(_ actual: T, _ wanted: T, _ what: String, line: Int = #line) {
        expect(actual == wanted, "\(what): got \(actual), wanted \(wanted)", line: line)
    }

    mutating func near(_ actual: Double, _ wanted: Double, _ what: String, line: Int = #line) {
        expect(abs(actual - wanted) <= 1e-6 * max(1, abs(wanted)), "\(what): got \(actual), wanted \(wanted)",
               line: line)
    }
}

let fixtureDirectory = CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : "fixtures"

@MainActor func fixture(_ name: String) -> String {
    guard let data = FileManager.default.contents(atPath: fixtureDirectory + "/" + name) else {
        fatalError("missing fixture \(name) in \(fixtureDirectory)")
    }
    return String(decoding: data, as: UTF8.self)
}

var check = Checker()
let tab = "\t"

// MARK: - Formatting

check.equal([48.0 * 1024, 1.5 * 1048576, 96.0 * 1048576, 1024.0 * 1048576, 1536.0 * 1048576].map(EngineFormat.bytes),
            ["48 KiB", "1.5 MiB", "96 MiB", "1 GiB", "1536 MiB"], "sizes")
check.equal([1863.24, 588.1, 74.7, 2.5, 0.0237, 0.0, 1234567.0].map(EngineFormat.number),
            ["1,863", "588", "74.7", "2.50", "0.024", "0", "1,234,567"], "numbers")
check.equal([0.5, 2.0, 20.0, 200.0, 1000.0, 0.0, 1.5].map(EngineFormat.tick),
            ["0.5", "2", "20", "200", "1,000", "0", "1.5"], "axis ticks")
check.equal(EngineFormat.number(Double.nan), "-", "not a number")

// MARK: - Cache and memory sweep

let sweepLog = fixture("cpumem.txt")
let sweep = RunOutput.parse(sweepLog)
check.equal(sweep.cpuName, "AMD Ryzen 7 9800X3D 8-Core Processor", "sweep: CPU name")
check.equal(sweep.logicalCount, 16, "sweep: logical processors")
check.equal(sweep.physicalCount, 8, "sweep: physical cores")
check.equal(sweep.vectorName, "AVX-512", "sweep: vector width")
check.equal(sweep.allThreads, 8, "sweep: threads of the all-core run")
check.equal(sweep.classes.map { $0.mode }, ["single"], "sweep: core classes")
check.equal(sweep.caches.map { "\($0.level):\(Int($0.sizeBytes)):\($0.sharedCores)" },
            ["1:49152:1", "2:1048576:1", "3:100663296:8"], "sweep: caches")
check.equal(sweep.points.count, 150, "sweep: points")
check.equal(Set(sweep.points.map { $0.series }).sorted(),
            ["all/copy", "all/read", "all/write", "single/copy", "single/read", "single/write"], "sweep: series")
check.equal(sweep.levels.map { $0.mode + "/" + $0.level },
            ["single/L1", "single/L2", "single/L3", "single/memory", "all/L1", "all/L2", "all/L3", "all/memory"],
            "sweep: levels")
if let first = sweep.levels.first {
    check.near(first.read, 587.259, "sweep: L1 read")
    check.near(first.cacheBytes, 49152, "sweep: L1 size")
    check.expect(first.measured && first.settled && !first.fromCurve, "sweep: L1 flags")
}
if let memory = sweep.levels.first(where: { $0.mode == "single" && $0.level == "memory" }) {
    // The sweep of the fixture stops at 64 MiB, inside the 96 MiB L3.
    check.expect(memory.measured && !memory.settled, "sweep: memory is measured but not settled")
} else {
    check.expect(false, "sweep: memory level present")
}
check.near(sweep.pageMapping?.gbps ?? 0, 15.853, "sweep: page mapping rate")
check.near(sweep.pageMapping?.pageBytes ?? 0, 4096, "sweep: page size")
check.equal(sweep.storage?.backing ?? "", "page file on C:", "sweep: paging target")
check.near(sweep.storage?.seqRead ?? 0, 5872.5, "sweep: sequential read")
check.near(sweep.storage?.randWriteIops ?? 0, 59904.3, "sweep: random write requests")
check.expect(sweep.storage?.measured == true && sweep.storage?.directIo == true, "sweep: storage flags")
check.near(sweep.progress ?? 0, 1.0, "sweep: progress at the end")
check.expect(sweep.hasMemoryResults && !sweep.hasComputeResults, "sweep: kind of results")

// The same log with Windows line endings: "\r\n" is one Character in Swift.
let windowsLog = sweepLog.replacingOccurrences(of: "\r\n", with: "\n").replacingOccurrences(of: "\n", with: "\r\n")
check.equal(RunOutput.parse(windowsLog).points.count, 150, "sweep: CRLF line endings")

// MARK: - CPU compute test

let compute = RunOutput.parse(fixture("cpu.txt"))
check.equal(compute.coreScores.count, 16, "compute: scores")
check.equal(compute.coreScores.first?.label ?? "", "LP 0 · core 1 · InferredUniform", "compute: first label")
check.near(compute.singleAverage?.value ?? 0, 195.935749, "compute: single-core average")
check.equal(compute.singleAverage?.unit ?? "", "MWork/s", "compute: unit")
check.near(compute.multiScore?.value ?? 0, 2603.895952, "compute: all-core score")
check.equal(compute.multiScore?.threads ?? 0, 16, "compute: all-core threads")
check.equal(compute.affinityCapability, "strict_group_affinity", "compute: affinity")
check.expect(!compute.schedulerManaged && compute.hasComputeResults, "compute: flags")

// macOS and iOS: the engine reports that it cannot bind a thread to a core.
var unbound = RunOutput()
for line in ["CPU_META" + tab + "name=Apple M2" + tab + "logical_count=8" + tab + "physical_count=8" + tab
                 + "affinity_capability=scheduler_managed",
             "CPU_TOPOLOGY" + tab + "core_index=0" + tab + "logical_index=0" + tab + "physical_core=0",
             "CPU_PROGRESS" + tab + "mode=per_core" + tab + "core_index=2" + tab + "phase=measure" + tab
                 + "overall_fraction=0.3",
             "CPU_RESULT" + tab + "kind=core" + tab + "core_index=0" + tab + "score=210.5" + tab + "unit=MWork/s"] {
    unbound.consume(line)
}
check.expect(unbound.schedulerManaged, "unbound: scheduler managed")
check.equal(unbound.stage, RunOutput.Stage.cpuCore(label: "#3", phase: .measure), "unbound: stage names the run")
check.equal(unbound.coreScores.map { $0.label }, [""], "unbound: no processor label")
check.near(unbound.progress ?? 0, 0.3, "unbound: progress")

// MARK: - GPU runs

let gpu = RunOutput.parse(fixture("gpu.txt"))
check.equal(gpu.jobs.map { $0.label }, ["Vulkan · GPU 0", "dx12 · GPU 0"], "gpu: job labels")
check.equal(gpu.jobs.map { $0.samples.count }, [6, 6], "gpu: samples")
check.equal(gpu.jobs.map { job in job.samples.filter { !$0.warmup }.count }, [3, 3], "gpu: measured samples")
check.near(gpu.jobs.first?.samples.last?.seconds ?? 0, 3.9, "gpu: time of the last sample")
check.near(gpu.jobs.first?.samples.last?.computeMs ?? 0, 0.022, "gpu: compute time of the last sample")
check.equal(gpu.jobs.first?.score?.name ?? "", "VRAM rate", "gpu: score name")
check.near(gpu.jobs.first?.score?.value ?? 0, 1876.32, "gpu: score")
check.equal(gpu.jobs.first?.score?.unit ?? "", "GB/s", "gpu: score unit")
check.near(gpu.jobs.last?.score?.value ?? 0, 2505.34, "gpu: second score")
check.equal(gpu.jobs.first?.fpsLine ?? "", "Avg FPS: 18768", "gpu: frame rate line")

// One job that runs two tests in a row: the clock restarts.
var suite = RunOutput()
for line in ["=== Job 1 of 1: gpu_benchmark --run-all ===",
             "[GPU Timing] Compute: 1.0 ms | Render: 2.0 ms | Total: 3 ms | FPS: 10 | T: 1.0 s",
             "[GPU Timing] Compute: 1.0 ms | Render: 2.0 ms | Total: 3 ms | FPS: 10 | T: 2.0 s",
             "VRAM rate:   10.5 GB/s",
             "[GPU Timing] Compute: 1.0 ms | Render: 2.0 ms | Total: 3 ms | FPS: 10 | T: 1.0 s",
             "[GPU Timing] Compute: 1.0 ms | Render: 2.0 ms",
             "VRAM rate:   12.5 GB/s"] {
    suite.consume(line)
}
check.equal(suite.jobs.map { $0.label }, ["#1", "#2"], "suite: one entry per test")
check.equal(suite.jobs.map { $0.samples.map { $0.seconds } }, [[1.0, 2.0], [1.0, 2.0]], "suite: sample times")
check.equal(suite.jobs.map { $0.scoreLine }, ["VRAM rate: 10.5 GB/s", "VRAM rate: 12.5 GB/s"], "suite: scores")

// Suite protocol keeps sizes and tiers identifiable without adding CPU work to GPU charts.
let previousLanguage = Localization.current
Localization.current = .en
let nativeSuite = RunOutput.parse("""
SUITE_RUN\tindex=1\ttotal=18\tbackend=metal\tworkload=stream\tparticles=65536\tsteps=0\theadless=1
[GPU Timing] Compute: 1.0 ms | Render: 0 ms | Total: 1 ms | FPS: 100 | T: 3.0 s
VRAM rate: 10 GB/s
SUITE_RUN\tindex=2\ttotal=18\tbackend=opengl\tworkload=gpu_burn\tparticles=0\tsteps=64\theadless=0
[GPU Timing] Compute: 0 ms | Render: 2.0 ms | Total: 2 ms | FPS: 50 | T: 3.0 s
Burn rate: 20 Gpix-step/s
SUITE_RUN\tindex=17\ttotal=18\tbackend=cpu\tworkload=cpu_all\tparticles=0\tsteps=0\theadless=1
""")
check.equal(nativeSuite.jobs.count, 2, "native suite: CPU does not create a GPU chart")
check.equal(nativeSuite.jobs.map { $0.label }, ["Metal · Particle · 65K · Headless", "OpenGL · GPU Burn 64"], "native suite: variant labels")
check.equal(nativeSuite.jobs.map { $0.samples.count }, [1, 1], "native suite: independent timing samples")
check.equal(nativeSuite.jobs.last?.score?.unit ?? "", "Gpix-step/s", "native suite: Burn score unit")
Localization.current = previousLanguage

// MARK: - Chart geometry

@MainActor func curves(_ output: RunOutput, _ mode: String) -> [PlotSeries] {
    ["read", "write", "copy"].enumerated().map { index, kernel in
        PlotSeries(label: kernel, slot: index,
                   points: output.points.filter { $0.series == "\(mode)/\(kernel)" }
                       .map { PlotPoint(x: $0.blockBytes, y: $0.gbps) })
    }
}
let cacheMarks = sweep.caches.map { PlotMarker(x: $0.sizeBytes, label: "L\($0.level)") }
for width in [300.0, 520.0, 900.0] {
    let size = CGSize(width: width, height: 206)
    guard let plot = LinePlotGeometry.make(series: curves(sweep, "single"), markers: cacheMarks, size: size,
                                           logBytes: true) else {
        check.expect(false, "log plot at width \(width)")
        continue
    }
    check.equal(plot.lines.map { $0.points.count }, [27, 27, 27], "log plot \(width): points per line")
    let points = plot.lines.flatMap { $0.points }
    check.expect(points.allSatisfy { $0.x >= 39.9 && $0.x <= width - 9.9 && $0.y >= 19.9 && $0.y <= 186.1 },
                 "log plot \(width): every point inside the plot area")
    // L3 (96 MiB) lies beyond the 64 MiB end of this sweep.
    check.equal(plot.markerLines.count, 2, "log plot \(width): cache marks in range")
    let texts = plot.labels.map { $0.text }
    check.expect(texts.contains("L1") && texts.contains("L2") && !texts.contains("L3"), "log plot \(width): mark labels")
    check.expect(texts.contains("200") && texts.contains("500"), "log plot \(width): value ticks")
    check.expect(texts.contains("64 MiB"), "log plot \(width): size ticks")
}
if let job = gpu.jobs.first {
    let measured = job.samples.filter { !$0.warmup }
    let timing = [PlotSeries(label: "Compute", slot: 0,
                             points: measured.map { PlotPoint(x: $0.seconds, y: $0.computeMs) })]
    if let plot = LinePlotGeometry.make(series: timing, markers: [], size: CGSize(width: 340, height: 190),
                                        logBytes: false) {
        check.equal(plot.labels.map { $0.text }, ["0", "0.01", "0.02", "2 s", "2.5 s", "3 s", "3.5 s"],
                    "time plot: ticks")
        check.equal(plot.lines.first?.points.count ?? 0, 3, "time plot: points")
    } else {
        check.expect(false, "time plot")
    }
}
let scores = compute.coreScores.sorted { $0.core < $1.core }
if let columns = ColumnPlotGeometry.make(values: scores.map { $0.score }, labels: scores.map { String($0.core + 1) },
                                         size: CGSize(width: 300, height: 190)) {
    check.equal(columns.bars.count, 16, "column plot: bars")
    check.expect(columns.bars.allSatisfy { $0.rect.width >= 2 && $0.rect.maxY <= 170.1 && $0.rect.minY >= 5.9 },
                 "column plot: bars inside the plot area")
    check.expect(columns.labels.map { $0.text }.contains("200"), "column plot: value ticks")
} else {
    check.expect(false, "column plot")
}
check.expect(LinePlotGeometry.make(series: [], markers: [], size: CGSize(width: 300, height: 190), logBytes: true) == nil,
             "log plot without data")
check.expect(ColumnPlotGeometry.make(values: [0, 0], labels: ["1", "2"], size: CGSize(width: 300, height: 190)) == nil,
             "column plot without a positive value")

// MARK: - Translation of engine phrases

Localization.current = .en
check.equal(Localization.scoreLine("VRAM rate:   1755.45 GB/s  (0.024 ms compute)"),
            "VRAM rate:   1755.45 GB/s  (0.024 ms compute)", "en: score line unchanged")
check.equal(Localization.engineText("page file on C:"), "page file on C:", "en: paging target unchanged")

Localization.current = .zh
check.equal(Localization.scoreLine("VRAM rate:   1755.45 GB/s  (0.024 ms compute)"),
            "显存速率: 1755.45 GB/s  (0.024 ms 计算)", "zh: VRAM score line")
check.equal(Localization.scoreLine("RAM rate: 24.1 GB/s"), "内存速率: 24.1 GB/s", "zh: RAM is not VRAM")
check.equal(Localization.scoreLine("Avg FPS:      18768"), "平均帧率: 18768", "zh: frame rate")
check.equal(Localization.scoreLine("Render rate: 12.5 Gpix/s (1.2 ms render)"), "渲染速率: 12.5 Gpix/s (1.2 ms 渲染)",
            "zh: render score line")
check.equal(Localization.scoreLine("Something else: 1"), "Something else: 1", "zh: unknown line unchanged")
check.equal(Localization.engineText("page file on C:"), "C: 盘上的页面文件", "zh: page file")
check.equal(Localization.engineText("swap on /dev/sdb"), "/dev/sdb 上的交换分区", "zh: swap device")
check.equal(Localization.engineText("no swap"), "没有交换分区", "zh: no swap")
check.equal(Localization.engineText("the directory is on tmpfs, which is memory and not a drive"),
            "目录位于 tmpfs，它是内存而不是硬盘", "zh: tmpfs")
check.equal(Localization.engineText("anything else"), "anything else", "zh: unknown phrase unchanged")

Localization.current = .ja
check.equal(Localization.scoreLine("VRAM rate:   1755.45 GB/s  (0.024 ms compute)"),
            "VRAM レート: 1755.45 GB/s  (0.024 ms コンピュート)", "ja: VRAM score line")
check.equal(Localization.engineText("page file on C:"), "C: のページファイル", "ja: page file")
check.equal(Localization.engineText("unknown"), "不明", "ja: unknown")

print("\(check.count - check.failures) of \(check.count) checks passed")
exit(check.failures == 0 ? 0 : 1)
