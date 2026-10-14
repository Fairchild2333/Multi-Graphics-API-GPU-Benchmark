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

struct GpuMemorySummary: Hashable, Sendable {
    var kernel: String
    var memory: Double
    var cache: Double
    var cacheFromBytes: Double
    var cacheToBytes: Double
    var settled: Bool?
}

struct GpuLocalCacheSummary: Hashable, Sendable {
    var fullLoad: Double
    var singleGroup: Double
    var capacityLow: Double
    var capacityHigh: Double
}

struct GpuCachePoint: Hashable, Sendable {
    var condition: String
    var addressBytes: Double
    /// ns per dependent load; GB/s of 4-byte payload for parallel_gpu_warm.
    var value: Double
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
    var coreType = "auto"
    var residencyLowerBound: Double?
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
    /// The engine found that every frame waited for the display, so the score may be too low.
    var displayLimited = false

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
    var coreTypeSelection = "auto"
    var memoryResidencyLowerBound: Double?
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
    var gpuMemoryPoints: [CurvePoint] = []
    var vulkanMemoryPoints: [CurvePoint] = []
    var gpuMemorySummaries: [GpuMemorySummary] = []
    var gpuLocalCache: GpuLocalCacheSummary?
    var gpuCachePoints: [GpuCachePoint] = []

    var hasGpuMemoryResults: Bool {
        !gpuMemoryPoints.isEmpty || !vulkanMemoryPoints.isEmpty ||
            !gpuMemorySummaries.isEmpty || gpuLocalCache != nil || !gpuCachePoints.isEmpty
    }

    var hasMemoryCurves: Bool {
        !points.isEmpty || !gpuMemoryPoints.isEmpty || !vulkanMemoryPoints.isEmpty || !gpuCachePoints.isEmpty
    }

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
        if line.hasPrefix("SUITE_RUN\t") {
            let f = Self.fields(line)
            let workload = f["workload"] ?? ""
            if (f["backend"] ?? "") != "cpu" && workload != "membw" {
                beginJob(header: "=== gpu_benchmark --backend " + (f["backend"] ?? "")
                    + " --workload " + workload + " --particles " + (f["particles"] ?? "0")
                    + " --iter " + (f["steps"] ?? "0") + (f["headless"] == "1" ? " --headless" : "") + " ===")
                let api = f["backend"] == "metal" ? "Metal" : f["backend"] == "opengl" ? "OpenGL" : (f["backend"] ?? "").uppercased()
                var details = [api]
                switch workload {
                case "stream":
                    details.append(Localization.tr("Particle", "粒子", "パーティクル"))
                    let count = f["particles"] ?? ""
                    details.append(["65536": "65K", "1048576": "1M", "4194304": "4M", "16777216": "16M"][count] ?? count)
                    if f["headless"] == "1" { details.append("Headless") }
                case "gpu_burn": details.append("GPU Burn " + (f["steps"] ?? ""))
                default: details.append(Localization.tr("Fluid", "流体", "流体"))
                }
                jobBaseLabel = details.joined(separator: " · ")
                jobs[jobs.count - 1].label = jobBaseLabel
            }
        } else if line.hasPrefix("=== ") {
            beginJob(header: line)
        } else if line.hasPrefix("[GPU Timing] Compute:") || line.hasPrefix("[Timing] Compute:") {
            addSample(line)
        } else if line.hasPrefix("MEMSWEEP_POINT") {
            let f = Self.fields(line)
            if let bytes = Double(f["bytes"] ?? ""), let rate = Double(f["gbps"] ?? ""),
               bytes.isFinite, bytes > 0, rate.isFinite, rate > 0 {
                gpuMemoryPoints.append(CurvePoint(series: (f["storage"] ?? "") + "/" + (f["access"] ?? ""),
                                                  blockBytes: bytes, gbps: rate))
            }
        } else if line.hasPrefix("MEMBW_POINT") {
            let f = Self.fields(line)
            if let kernel = f["kernel"], !kernel.isEmpty,
               let bytes = Double(f["dataset_bytes"] ?? ""), let rate = Double(f["median_gbps"] ?? ""),
               bytes.isFinite, bytes > 0, rate.isFinite, rate > 0 {
                vulkanMemoryPoints.append(CurvePoint(series: kernel, blockBytes: bytes, gbps: rate))
            }
        } else if line.hasPrefix("MEMBW_SUMMARY") {
            let f = Self.fields(line)
            if let kernel = f["kernel"], ["read", "write", "copy"].contains(kernel) {
                gpuMemorySummaries.removeAll { $0.kernel == kernel }
                gpuMemorySummaries.append(GpuMemorySummary(kernel: kernel,
                    memory: Self.finiteNumber(f, "memory_gbps"), cache: Self.finiteNumber(f, "cache_gbps"),
                    cacheFromBytes: Self.finiteNumber(f, "cache_from_bytes"),
                    cacheToBytes: Self.finiteNumber(f, "cache_to_bytes"),
                    settled: f["memory_plateau_reached"].map { $0 == "1" }))
            }
        } else if line.hasPrefix("MEMBW_L1") {
            let f = Self.fields(line)
            gpuLocalCache = GpuLocalCacheSummary(fullLoad: Self.finiteNumber(f, "full_load_gbps"),
                singleGroup: Self.finiteNumber(f, "single_group_gbps"),
                capacityLow: Self.finiteNumber(f, "capacity_low_bytes"),
                capacityHigh: Self.finiteNumber(f, "capacity_high_bytes"))
        } else if line.hasPrefix("GCACHE_POINT") {
            let f = Self.fields(line)
            let bytes = Self.finiteNumber(f, "bytes"), value = Self.finiteNumber(f, "median")
            if let condition = f["condition"], bytes > 0, value > 0 {
                gpuCachePoints.append(GpuCachePoint(condition: condition, addressBytes: bytes, value: value))
            }
        } else if line.hasPrefix("CPU") {
            consumeProtocol(line)
        } else {
            consumeSummary(line)
        }
    }

    private static func finiteNumber(_ fields: [String: String], _ key: String) -> Double {
        guard let number = Double(fields[key] ?? ""), number.isFinite, number >= 0, number < 1e18 else { return 0 }
        return number
    }

    /// Reconstruct the data already stored in results.json; never measures or
    /// rewrites it. CPU x values are KiB per thread, GPU data sets are MiB, and
    /// the GPU local-cache curve uses KiB per workgroup.
    static func savedMemory(workload: String, api: String, configuration: String?) -> RunOutput {
        var output = RunOutput()
        var f: [String: String] = [:]
        for item in (configuration ?? "").split(separator: ";") {
            let pair = item.split(separator: "=", maxSplits: 1, omittingEmptySubsequences: false)
            if pair.count == 2 {
                let key = pair[0].trimmingCharacters(in: .whitespacesAndNewlines)
                if f[key] == nil { f[key] = pair[1].trimmingCharacters(in: .whitespacesAndNewlines) }
            }
        }
        output.coreTypeSelection = f["coreType"] ?? "auto"
        if f["residencyVerified"] == "1" {
            let bounds = f.filter { $0.key.hasPrefix("residency_single") }.flatMap { item in
                item.value.split(separator: ",").compactMap { sample -> Double? in
                    let fields = sample.split(separator: ":")
                    guard fields.count >= 4, let value = Double(fields[2]),
                          value.isFinite, value >= 0.99, value <= 1 else { return nil }
                    return value
                }
            }
            output.memoryResidencyLowerBound = bounds.min()
        }
        func number(_ key: String) -> Double { finiteNumber(f, key) }
        func count(_ key: String) -> Int { Int(min(65536, number(key))) }
        func curve(_ key: String, series: String, unit: Double) -> [CurvePoint] {
            var samples: [Double: Double] = [:]
            for pair in (f[key] ?? "").split(separator: ",") {
                let parts = pair.split(separator: ":", maxSplits: 1)
                guard parts.count == 2, let size = Double(parts[0]), let rate = Double(parts[1]),
                      size.isFinite, size > 0, size * unit < 1e18, rate.isFinite, rate > 0, rate < 1e18 else { continue }
                if samples[size * unit] == nil { samples[size * unit] = rate }
            }
            return samples.keys.sorted().map { CurvePoint(series: series, blockBytes: $0, gbps: samples[$0]!) }
        }
        if workload == "membw", api == "Vulkan" {
            for kernel in ["read", "write", "copy", "host_read", "host_write", "l1", "l1_single"] {
                output.vulkanMemoryPoints += curve("curve_" + kernel, series: kernel,
                    unit: kernel.hasPrefix("l1") ? 1024 : 1048576)
            }
            for (kernel, prefix) in [("read", "read"), ("write", "write"), ("copy", "copy")] {
                if f[prefix + "MemoryGBps"] != nil {
                    output.gpuMemorySummaries.append(GpuMemorySummary(kernel: kernel,
                        memory: number(prefix + "MemoryGBps"), cache: number(prefix + "CacheGBps"),
                        cacheFromBytes: number(prefix + "CacheFromMiB") * 1048576,
                        cacheToBytes: number(prefix + "CacheToMiB") * 1048576,
                        settled: kernel == "read" ? f["memoryPlateauReached"].map { $0 == "1" } : nil))
                }
            }
            if f["l1FullLoadGBps"] != nil || f["l1SingleGroupGBps"] != nil {
                output.gpuLocalCache = GpuLocalCacheSummary(fullLoad: number("l1FullLoadGBps"),
                    singleGroup: number("l1SingleGroupGBps"), capacityLow: number("l1CapacityLowKiB") * 1024,
                    capacityHigh: number("l1CapacityHighKiB") * 1024)
            }
        } else if workload == "membw", api == "Metal" {
            for storage in ["device_local", "host_visible"] {
                for access in ["read_only", "read_modify_write"] {
                    let prefix = storage + "_" + access + "_"
                    let points = f.compactMap { key, value -> CurvePoint? in
                        guard key.hasPrefix(prefix), let bytes = Double(key.dropFirst(prefix.count)),
                              bytes.isFinite, bytes > 0, bytes < 1e18, let rate = Double(value),
                              rate.isFinite, rate > 0, rate < 1e18 else { return nil }
                        return CurvePoint(series: storage + "/" + access, blockBytes: bytes, gbps: rate)
                    }
                    output.gpuMemoryPoints += points.sorted { $0.blockBytes < $1.blockBytes }
                }
            }
        } else if workload == "gpu_cache" {
            for condition in ["gpu_warm", "eviction_pass", "cpu_warm", "parallel_gpu_warm"] {
                output.gpuCachePoints += curve("curve_" + condition, series: condition, unit: 1024).map {
                    GpuCachePoint(condition: condition, addressBytes: $0.blockBytes, value: $0.gbps)
                }
            }
        } else if workload == "cpu_membw" {
            output.vectorName = f["vector"] ?? ""
            output.logicalCount = count("logicalProcessors")
            output.physicalCount = count("physicalCores")
            output.allThreads = count("allCoreThreads")
            output.affinityCapability = f["affinity"] ?? ""
            let ranks = 0..<max(1, min(128, count("coreClasses")))
            for rank in ranks {
                let mode = rank == 0 ? "single" : "single_\(rank)"
                output.classes.append(MemClass(rank: rank, mode: mode,
                    name: f["class\(rank)Name"] ?? "", cores: count("class\(rank)Cores")))
            }
            for rank in ranks {
                for level in 1...4 {
                    let typedKey = "class\(rank)CacheL\(level)"
                    let key = f[typedKey + "KiB"] != nil ? typedKey : rank == 0 ? "cacheL\(level)" : ""
                    guard !key.isEmpty, number(key + "KiB") > 0 else { continue }
                    output.caches.append(MemCache(cpuClass: rank, level: level,
                        sizeBytes: number(key + "KiB") * 1024, sharedCores: max(1, count(key + "SharedCores"))))
                }
            }
            for mode in output.classes.map(\.mode) + ["all"] {
                for kernel in ["read", "write", "copy", "write_nt", "copy_nt", "zero"] {
                    output.points += curve("curve_" + mode + "_" + kernel, series: mode + "/" + kernel, unit: 1024)
                }
                for name in ["L1", "L2", "L3", "L4", "SoC", "memory"] {
                    let prefix = mode + "_" + name
                    guard f[prefix + "_read"] != nil || f[prefix + "_write"] != nil || f[prefix + "_copy"] != nil else { continue }
                    output.levels.append(MemLevel(mode: mode, level: name,
                        cacheBytes: output.caches.first(where: {
                            $0.level == Int(name.dropFirst()) && $0.cpuClass == output.classes.first(where: { $0.mode == mode })?.rank
                        })?.sizeBytes ?? 0,
                        read: number(prefix + "_read"), write: number(prefix + "_write"), copy: number(prefix + "_copy"),
                        writeStream: number(prefix + "_writeStream"), copyStream: number(prefix + "_copyStream"),
                        zeroFill: number(prefix + "_zeroFill"), measured: true,
                        settled: name != "memory" || f[prefix + "_settled"] == "1",
                        fromCurve: f[prefix + "_capacityLowKiB"] != nil || f[prefix + "_capacityHighKiB"] != nil,
                        capacityLow: number(prefix + "_capacityLowKiB") * 1024,
                        capacityHigh: number(prefix + "_capacityHighKiB") * 1024))
                }
            }
            if f["pageFaultMeasured"] == "1" {
                output.pageMapping = PageMapping(gbps: number("pageFaultGBps"), nsPerPage: number("pageFaultNsPerPage"), pageBytes: number("pageBytes"))
            }
            if f["storageMeasured"] != nil {
                output.storage = StorageResult(measured: f["storageMeasured"] == "1", directIo: f["storageDirectIo"] == "1",
                    backing: f["storageBacking"] ?? "", skipped: "", seqRead: number("storageSeqReadMBps"),
                    seqWrite: number("storageSeqWriteMBps"), randRead: number("storageRandReadMBps"),
                    randWrite: number("storageRandWriteMBps"), randReadIops: 0, randWriteIops: 0)
            }
        }
        return output
    }

    /// Both memory implementations keep their own meaning and plateau status.
    static func gpuMemoryScoreLine(_ line: String) -> String? {
        let f = fields(line)
        if line.hasPrefix("MEMSWEEP_RESULT"), let rate = f["read_only_gbps"] {
            return Localization.tr("GPU memory probe: ", "GPU 内存探测：", "GPU メモリ測定: ") + rate + " GB/s"
        }
        if line.hasPrefix("MEMBW_SUMMARY"), f["kernel"] == "read", let rate = f["memory_gbps"] {
            let label = f["memory_plateau_reached"] == "1"
                ? Localization.tr("GPU memory read: ", "GPU 内存读取：", "GPU メモリ読込: ")
                : Localization.tr("Largest working sets (plateau not reached): ", "最大工作集（未达到内存平台）：", "最大サイズ（メモリ帯域未確定）: ")
            return label + rate + " GB/s"
        }
        return nil
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
        if trimmed.hasPrefix(">> Display-bound") { jobs[jobs.count - 1].displayLimited = true }
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

        case "CPU_SCHEDULING":
            coreTypeSelection = f["core_type"] ?? "auto"
            if coreTypeSelection != "auto" {
                coreLabels[0] = Localization.tr("Performance core", "大核", "性能コア")
                coreLabels[1] = Localization.tr("Efficiency core", "小核", "効率コア")
            }

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
                guard f["valid"] != "0" else { return }
                let type = f["core_type"] ?? "auto"
                if type != "auto" {
                    guard f["residency_verified"] == "1",
                          let lower = fraction("requested_lower_bound"), lower >= 0.99 else { return }
                }
                coreScores.append(CoreScore(core: core, score: number("score"), unit: unit,
                    label: coreLabels[core] ?? (schedulerManaged ? "" : runLabel(core)),
                    coreType: type, residencyLowerBound: type == "auto" ? nil : fraction("requested_lower_bound")))
            case "summary":
                guard f["valid"] != "0", coreTypeSelection == "auto" else { return }
                let average = number("average_score")
                singleAverage = ScoreValue(value: average > 0 ? average : number("score"), unit: unit, threads: 1)
            case "multi":
                guard f["valid"] != "0" else { return }
                multiScore = ScoreValue(value: number("score"), unit: unit, threads: whole("thread_count") ?? 0)
            default:
                break
            }

        case "CPUMEM_META":
            coreTypeSelection = f["core_type"] ?? "auto"
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
            if f["core_type"] != nil, f["core_type"] != "auto" {
                guard f["residency_verified"] == "1",
                      let lower = fraction("requested_lower_bound"), lower >= 0.99 else { return }
                memoryResidencyLowerBound = min(memoryResidencyLowerBound ?? 1, lower)
            }
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
