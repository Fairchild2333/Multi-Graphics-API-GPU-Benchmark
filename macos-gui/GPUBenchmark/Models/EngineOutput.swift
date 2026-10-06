// EngineOutput.swift — Turns the lines the engine prints into chart data.
//
// The engine prints tab-separated protocol lines (CPU_*, CPUMEM_*) and a
// periodic "[GPU Timing]" line. The Windows and Android apps read the same
// lines, so every front end shows the same numbers. No UI code lives here.

import Foundation

/// Number and size formatting shared by the tables and the charts.
enum EngineFormat {
    /// "48 KiB", "1.5 MiB", "1 GiB".
    static func bytes(_ value: Double) -> String {
        guard value.isFinite, value >= 0, value < 1e18 else { return "-" }
        let kib = value / 1024.0
        if kib < 1024.0 { return "\(Int(kib.rounded())) KiB" }
        let mib = kib / 1024.0
        if mib >= 1024.0, mib.truncatingRemainder(dividingBy: 1024.0) < 0.01 {
            return "\(Int((mib / 1024.0).rounded())) GiB"
        }
        if mib.truncatingRemainder(dividingBy: 1.0) < 0.01 { return "\(Int(mib.rounded())) MiB" }
        return String(format: "%.1f MiB", mib)
    }

    /// "1,863", "74.7", "0.024": three significant digits.
    static func number(_ value: Double) -> String {
        guard value.isFinite, value < 1e18 else { return "-" }
        if value >= 1000.0 { return grouped(value) }
        if value >= 100.0 { return String(format: "%.0f", value) }
        if value >= 10.0 { return String(format: "%.1f", value) }
        if value >= 1.0 { return String(format: "%.2f", value) }
        if value > 0.0 { return String(format: "%.3f", value) }
        return "0"
    }

    /// Axis tick: "0.5", "2", "200", "1,000".
    static func tick(_ value: Double) -> String {
        guard value.isFinite, value < 1e18 else { return "-" }
        if value >= 1000.0 { return grouped(value) }
        return String(format: "%g", value)
    }

    /// A whole number with thousands separators.
    static func grouped(_ value: Double) -> String {
        guard value.isFinite, abs(value) < 1e18 else { return "-" }
        let digits = String(Int64(abs(value).rounded()))
        var out = value < 0 ? "-" : ""
        for (offset, digit) in digits.enumerated() {
            if offset > 0, (digits.count - offset) % 3 == 0 { out.append(",") }
            out.append(digit)
        }
        return out
    }
}

/// One measured point of a cache and memory sweep.
struct CurvePoint: Hashable, Sendable {
    /// "<mode>/<kernel>", such as "single/read" or "all/copy".
    var series: String
    var blockBytes: Double
    var gbps: Double
}

/// The rates of one cache level, or of memory, for one mode of the sweep.
struct MemLevel: Hashable, Sendable {
    var mode: String
    var level: String
    var cacheBytes: Double
    var read: Double
    var write: Double
    var copy: Double
    var writeStream: Double
    var copyStream: Double
    var zeroFill: Double
    var measured: Bool
    var settled: Bool
    /// The system reports no size for this cache; the capacity range comes from the curve.
    var fromCurve: Bool
    var capacityLow: Double
    var capacityHigh: Double
}

struct MemCache: Hashable, Sendable {
    var cpuClass: Int
    var level: Int
    var sizeBytes: Double
    var sharedCores: Int
}

/// A class of cores, such as the performance or the efficiency cores.
struct MemClass: Hashable, Sendable {
    var rank: Int
    var mode: String
    var name: String
    var cores: Int
}

struct PageMapping: Hashable, Sendable {
    var gbps: Double
    var nsPerPage: Double
    var pageBytes: Double
}

struct StorageResult: Hashable, Sendable {
    var measured: Bool
    var directIo: Bool
    var backing: String
    var skipped: String
    var seqRead: Double
    var seqWrite: Double
    var randRead: Double
    var randWrite: Double
    var randReadIops: Double
    var randWriteIops: Double
}

struct CoreScore: Hashable, Identifiable, Sendable {
    var core: Int
    var score: Double
    var unit: String
    var label: String
    var id: Int { core }
}

struct ScoreValue: Hashable, Sendable {
    var value: Double
    var unit: String
    var threads: Int
}

struct GpuSample: Hashable, Sendable {
    var seconds: Double
    var computeMs: Double
    var renderMs: Double
    var warmup: Bool
}

/// One GPU run: its timing samples and the score lines of its summary.
struct GpuJob: Hashable, Identifiable, Sendable {
    var id: Int
    var label: String
    var samples: [GpuSample] = []
    var scoreLine = ""
    var fpsLine = ""

    /// "VRAM rate:   1863.24 GB/s  (0.023 ms compute)" -> "VRAM rate", 1863.24, "GB/s".
    var score: (name: String, value: Double, unit: String)? {
        guard let colon = scoreLine.firstIndex(of: ":") else { return nil }
        let words = scoreLine[scoreLine.index(after: colon)...].split(separator: " ")
        guard let first = words.first, let value = Double(String(first)), value.isFinite, value > 0 else {
            return nil
        }
        return (String(scoreLine[..<colon]), value, words.count > 1 ? String(words[1]) : "")
    }
}

/// Everything parsed from the output of one run.
struct RunOutput: Sendable {
    enum Phase: Hashable, Sendable {
        case warmUp, measure, finish
    }

    /// What the engine is doing, for the status line.
    enum Stage: Hashable, Sendable {
        case cpuCore(label: String, phase: Phase)
        case cpuAllCores(phase: Phase)
        case sweepOneThread
        case sweepAllCores
        case storage
        case pageMapping
        case error(String)
    }

    // CPU, both tests.
    var cpuName = ""
    var logicalCount = 0
    var physicalCount = 0
    /// How the engine places a thread: "strict_..." when it binds the thread to
    /// one logical processor, "scheduler_managed" when the system picks the core.
    var affinityCapability = ""
    /// Fraction of the run that is done, once the engine has reported one.
    var progress: Double?
    var stage: Stage?

    // CPU compute test.
    var coreScores: [CoreScore] = []
    var singleAverage: ScoreValue?
    var multiScore: ScoreValue?

    // Cache and memory sweep.
    var vectorName = ""
    var allThreads = 0
    var classes: [MemClass] = []
    var caches: [MemCache] = []
    var levels: [MemLevel] = []
    var points: [CurvePoint] = []
    var pageMapping: PageMapping?
    var storage: StorageResult?

    // GPU runs, one entry per job or per test of a job.
    var jobs: [GpuJob] = []

    private var coreLabels: [Int: String] = [:]
    private var jobBaseLabel = ""
    private var jobSegment = 1

    /// True on macOS and iOS: no thread can be bound to a core there, so the
    /// "per-core" runs are repeated one-thread runs on cores the system picks.
    var schedulerManaged: Bool { affinityCapability == "scheduler_managed" }

    var hasComputeResults: Bool {
        !coreScores.isEmpty || singleAverage != nil || multiScore != nil
    }

    var hasMemoryResults: Bool { !levels.isEmpty || !points.isEmpty }

    /// Parses a whole log, as kept by the output view.
    static func parse(_ log: String) -> RunOutput {
        var output = RunOutput()
        // "\r\n" is one Character, so split on any line break.
        for line in log.split(whereSeparator: { $0.isNewline }) {
            output.consume(String(line))
        }
        return output
    }

    /// "TAG\tkey=value\tkey=value" -> [key: value].
    static func fields(_ line: String) -> [String: String] {
        var out: [String: String] = [:]
        for item in line.split(separator: "\t").dropFirst() {
            guard let equals = item.firstIndex(of: "="), equals != item.startIndex else { continue }
            out[String(item[..<equals])] = String(item[item.index(after: equals)...])
        }
        return out
    }

    mutating func consume(_ rawLine: String) {
        let line = rawLine.hasSuffix("\r") ? String(rawLine.dropLast()) : rawLine
        if line.hasPrefix("=== ") {
            beginJob(header: line)
        } else if line.hasPrefix("[GPU Timing] Compute:") || line.hasPrefix("[Timing] Compute:") {
            addSample(line)
        } else if line.hasPrefix("CPU") {
            consumeProtocol(line)
        } else {
            consumeSummary(line)
        }
    }

    // MARK: - GPU

    private static let rateKeys = [
        "RAM rate:", "VRAM rate:", "Memory rate:", "Compute rate:", "Burn rate:", "Stress rate:",
        "Fill rate:", "Render rate:", "Vol rate:", "Fluid rate:", "Liquid rate:", "Peak FP", "Peak INT",
    ]

    /// The number that follows `key` in `line`.
    private static func number(in line: String, after key: String) -> Double? {
        guard let range = line.range(of: key) else { return nil }
        let tail = line[range.upperBound...].drop(while: { $0 == " " })
        let digits = tail.prefix(while: { ("0"..."9").contains($0) || $0 == "." || $0 == "-" })
        guard let value = Double(String(digits)), value.isFinite else { return nil }
        return value
    }

    /// "=== Job 2 of 3: gpu_benchmark --time 15 --backend vulkan ===" -> "Vulkan".
    private static func jobLabel(header: String) -> String {
        let words = header.split(separator: " ").map(String.init)
        func value(after flag: String) -> String? {
            guard let index = words.firstIndex(of: flag), index + 1 < words.count else { return nil }
            return words[index + 1]
        }
        if words.count > 2, words[1] == "Preview" { return words[2] }
        var parts: [String] = []
        if let backend = value(after: "--backend") {
            switch backend.lowercased() {
            case "metal": parts.append("Metal")
            case "vulkan": parts.append("Vulkan")
            case "opengl": parts.append("OpenGL")
            default: parts.append(backend)
            }
        }
        if let gpu = value(after: "--gpu") { parts.append("GPU \(gpu)") }
        if parts.isEmpty { parts.append(value(after: "--workload") ?? "") }
        return parts.filter { !$0.isEmpty }.joined(separator: " · ")
    }

    private mutating func beginJob(header: String) {
        jobBaseLabel = Self.jobLabel(header: header)
        jobSegment = 1
        jobs.append(GpuJob(id: jobs.count, label: jobBaseLabel))
    }

    private mutating func addSample(_ line: String) {
        if jobs.isEmpty { beginJob(header: "") }
        let sample = GpuSample(
            // An engine without the "T:" field prints one line a second.
            seconds: Self.number(in: line, after: "| T:") ?? Double(jobs[jobs.count - 1].samples.count + 1),
            computeMs: Self.number(in: line, after: "Compute:") ?? 0,
            renderMs: Self.number(in: line, after: "Render:") ?? 0,
            warmup: line.contains("(warm-up)"))
        if let last = jobs[jobs.count - 1].samples.last, sample.seconds + 0.5 < last.seconds {
            // A job that runs several tests in a row: every test restarts its clock.
            if jobSegment == 1 { jobs[jobs.count - 1].label = segmentLabel(1) }
            jobSegment += 1
            jobs.append(GpuJob(id: jobs.count, label: segmentLabel(jobSegment)))
        }
        jobs[jobs.count - 1].samples.append(sample)
    }

    private func segmentLabel(_ number: Int) -> String {
        jobBaseLabel.isEmpty ? "#\(number)" : "\(jobBaseLabel) #\(number)"
    }

    private mutating func consumeSummary(_ line: String) {
        guard !jobs.isEmpty else { return }
        let trimmed = line.trimmingCharacters(in: .whitespaces)
        let isScore = Self.rateKeys.contains { trimmed.hasPrefix($0) }
        guard isScore || trimmed.hasPrefix("Avg FPS:") else { return }
        let compact = trimmed.split(separator: " ").joined(separator: " ")
        if isScore {
            if jobs[jobs.count - 1].scoreLine.isEmpty { jobs[jobs.count - 1].scoreLine = compact }
        } else if jobs[jobs.count - 1].fpsLine.isEmpty {
            jobs[jobs.count - 1].fpsLine = compact
        }
    }

    // MARK: - CPU

    private func runLabel(_ core: Int) -> String {
        schedulerManaged ? "#\(core + 1)" : "LP \(core + 1)"
    }

    private mutating func consumeProtocol(_ line: String) {
        guard let tag = line.split(separator: "\t", maxSplits: 1).first else { return }
        let f = Self.fields(line)
        func number(_ key: String) -> Double {
            guard let text = f[key], let value = Double(text), value.isFinite else { return 0 }
            return value
        }
        func whole(_ key: String) -> Int? {
            guard let text = f[key] else { return nil }
            return Int(text)
        }
        func flag(_ key: String) -> Bool { number(key) > 0.5 }
        func fraction(_ key: String) -> Double? {
            guard let text = f[key], let value = Double(text), value.isFinite else { return nil }
            return min(max(value, 0), 1)
        }

        switch tag {
        case "CPU_META":
            cpuName = f["name"] ?? cpuName
            logicalCount = whole("logical_count") ?? logicalCount
            physicalCount = whole("physical_count") ?? physicalCount
            affinityCapability = f["affinity_capability"] ?? affinityCapability

        case "CPU_TOPOLOGY":
            // Without binding, a processor number says nothing about where a run lands.
            guard !schedulerManaged, let core = whole("core_index") else { return }
            var label = "LP \(f["logical_index"] ?? String(core)) · core \((whole("physical_core") ?? core) + 1)"
            if let coreClass = f["core_class"], !coreClass.isEmpty { label += " · \(coreClass)" }
            coreLabels[core] = label

        case "CPU_PROGRESS":
            progress = fraction("overall_fraction") ?? fraction("fraction") ?? progress
            let phase: Phase
            switch f["phase"] {
            case "warmup": phase = .warmUp
            case "measure": phase = .measure
            default: phase = .finish
            }
            let core = whole("core_index") ?? -1
            if f["mode"] == "multi" || core < 0 {
                stage = .cpuAllCores(phase: phase)
            } else {
                stage = .cpuCore(label: coreLabels[core] ?? runLabel(core), phase: phase)
            }

        case "CPU_ERROR":
            stage = .error((f["message"] ?? "CPU engine error").replacingOccurrences(of: "_", with: " "))

        case "CPU_RESULT":
            let unit = f["unit"] ?? ""
            switch f["kind"] {
            case "core":
                guard let core = whole("core_index") else { return }
                coreScores.append(CoreScore(core: core, score: number("score"), unit: unit,
                                            label: coreLabels[core] ?? (schedulerManaged ? "" : runLabel(core))))
            case "summary":
                let average = number("average_score")
                singleAverage = ScoreValue(value: average > 0 ? average : number("score"), unit: unit, threads: 1)
            case "multi":
                multiScore = ScoreValue(value: number("score"), unit: unit, threads: whole("thread_count") ?? 0)
            default:
                break
            }

        case "CPUMEM_META":
            cpuName = f["name"] ?? cpuName
            logicalCount = whole("logical_count") ?? logicalCount
            physicalCount = whole("physical_count") ?? physicalCount
            vectorName = f["vector"] ?? vectorName
            allThreads = Int(number("all_threads"))

        case "CPUMEM_CLASS":
            classes.append(MemClass(rank: Int(number("rank")), mode: f["mode"] ?? "",
                                    name: f["name"] ?? "", cores: Int(number("cores"))))

        case "CPUMEM_CACHE":
            caches.append(MemCache(cpuClass: Int(number("class")), level: Int(number("level")),
                                   sizeBytes: number("size_bytes"),
                                   sharedCores: max(1, Int(number("shared_cores")))))

        case "CPUMEM_POINT":
            let bytes = number("block_bytes")
            let rate = number("median_gbps")
            if bytes > 0, rate > 0 {
                points.append(CurvePoint(series: "\(f["mode"] ?? "")/\(f["kernel"] ?? "")",
                                         blockBytes: bytes, gbps: rate))
            }

        case "CPUMEM_PROGRESS":
            progress = fraction("fraction") ?? progress
            let mode = f["mode"] ?? ""
            if mode.hasPrefix("single") {
                stage = .sweepOneThread
            } else if mode == "all" {
                stage = .sweepAllCores
            } else if mode == "storage" {
                stage = .storage
            } else {
                stage = .pageMapping
            }

        case "CPUMEM_LEVEL":
            levels.append(MemLevel(
                mode: f["mode"] ?? "", level: f["level"] ?? "", cacheBytes: number("cache_bytes"),
                read: number("read_gbps"), write: number("write_gbps"), copy: number("copy_gbps"),
                writeStream: number("write_nt_gbps"), copyStream: number("copy_nt_gbps"),
                zeroFill: number("zero_gbps"),
                measured: flag("measured"), settled: flag("settled"), fromCurve: flag("from_curve"),
                capacityLow: number("capacity_low_bytes"), capacityHigh: number("capacity_high_bytes")))

        case "CPUMEM_PAGEFAULT":
            if flag("measured") {
                pageMapping = PageMapping(gbps: number("gbps"), nsPerPage: number("ns_per_page"),
                                          pageBytes: number("page_bytes"))
            }

        case "CPUMEM_STORAGE":
            storage = StorageResult(
                measured: flag("measured"), directIo: flag("direct_io"),
                backing: f["backing"] ?? "", skipped: f["skipped"] ?? "",
                seqRead: number("seq_read_mbps"), seqWrite: number("seq_write_mbps"),
                randRead: number("rand_read_mbps"), randWrite: number("rand_write_mbps"),
                randReadIops: number("rand_read_iops"), randWriteIops: number("rand_write_iops"))

        default:
            break
        }
    }
}
