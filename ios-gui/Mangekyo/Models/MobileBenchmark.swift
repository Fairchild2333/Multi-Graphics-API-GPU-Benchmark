// MobileBenchmark.swift — iOS/iPadOS product capabilities and run state.

import Foundation

enum MobileWorkload: String, CaseIterable, Identifiable, Codable {
    case stream
    case gpuBurn = "gpu_burn"
    case cinematicLiquid = "cinematic_liquid"

    var id: String { rawValue }

    var title: String {
        switch self {
        case .stream:
            return Localization.tr(
                "Particle — Memory Throughput",
                "粒子 —— 内存吞吐",
                "パーティクル — メモリ帯域")
        case .gpuBurn:
            return Localization.tr(
                "Plasma × Kaleidoscope — GPU Burn",
                "等离子晶核 × 万花镜 —— GPU Burn",
                "プラズマ核 × カレイドスコープ — GPU Burn")
        case .cinematicLiquid:
            return Localization.tr(
                "Cinematic Liquid — Interactive Pool",
                "电影化液体 —— 互动水池",
                "Cinematic Liquid — インタラクティブプール")
        }
    }

    var shortTitle: String {
        switch self {
        case .stream: return Localization.tr("Particle", "粒子", "パーティクル")
        case .gpuBurn: return "GPU Burn"
        case .cinematicLiquid: return Localization.tr("Liquid", "液体", "液体")
        }
    }

    var systemImage: String {
        switch self {
        case .stream: return "circle.grid.cross"
        case .gpuBurn: return "flame.fill"
        case .cinematicLiquid: return "drop.fill"
        }
    }

    var expectedContract: String {
        switch self {
        case .stream:
            return "stream_v1_ios_preview"
        case .gpuBurn:
            return "gpu_burn_v3_fixed_steps_16_kaleidoscope_ios_preview"
        case .cinematicLiquid:
            return "cinematic_liquid_v2_…_metal_preview_ios_preview"
        }
    }

    var fixedProfile: String {
        switch self {
        case .stream:
            return Localization.tr(
                "262,144 particles · embedded preview profile",
                "262,144 粒子 · 嵌入式预览配置",
                "262,144 パーティクル · 埋め込みプレビュー")
        case .gpuBurn:
            return Localization.tr(
                "Light · 16 fixed raymarch steps",
                "轻量 · 固定 16 次光线步进",
                "Light · 16 固定レイマーチステップ")
        case .cinematicLiquid:
            return Localization.tr(
                "320,920 particles · Metal preview profile",
                "320,920 粒子 · Metal 预览配置",
                "320,920 パーティクル · Metal プレビュー")
        }
    }

    var shaderResourceNames: [String] {
        switch self {
        case .stream: return ["particle.metal"]
        case .gpuBurn: return ["gpu_burn.metal"]
        case .cinematicLiquid: return ["cinematic_liquid_v2.metal"]
        }
    }
}

struct MobileBenchmarkRequest: Equatable {
    var workload: MobileWorkload
    var durationSeconds: Double
    var captureRequested: Bool
    var captureAtSeconds: Double?
}

enum MobileRunPhase: String, Equatable {
    case idle
    case preparing
    case running
    case cancelling
    case completed
    case failed
}

enum CancellationReason: Equatable {
    case user
    case background
}

enum NativeCaptureCapability: Equatable {
    case unavailable(reason: String)
    case available

    var isAvailable: Bool {
        if case .available = self { return true }
        return false
    }

    var explanation: String {
        switch self {
        case .available:
            return Localization.tr(
                "Native Metal capture is available.",
                "原生 Metal 抓帧可用。",
                "ネイティブ Metal キャプチャを利用できます。")
        case .unavailable(let reason):
            return reason
        }
    }
}

struct BenchIssue: Identifiable, Equatable {
    enum Code: String {
        case metalUnavailable
        case layerUnavailable
        case workloadUnavailable
        case invalidDuration
        case captureUnavailable
        case alreadyRunning
        case nativeStartFailed
        case nativeRunFailed
        case resultDecodeFailed
        case exportFailed
        case storageUnavailable
    }

    let code: Code
    let title: String
    let message: String
    let recoverySuggestion: String?

    var id: String {
        "\(code.rawValue):\(message)"
    }

    var combinedMessage: String {
        guard let recoverySuggestion, !recoverySuggestion.isEmpty else {
            return message
        }
        return "\(message)\n\n\(recoverySuggestion)"
    }
}

/// The supported iOS matrix. A CPU compute job saves both single- and multi-thread results.
enum MobileSuiteScope: String, Sendable {
    case all, gpu, cpu
    func includes(_ job: MobileSuiteJob) -> Bool {
        self == .all || (job.gpuRequest == nil ? self == .cpu : self == .gpu)
    }
}

struct MobileSuiteJob: Identifiable, Sendable {
    let id: String
    let title: String
    let gpuRequest: String?
    let cpuArguments: [String]
    let seconds: Double
    let expectedRecords: Int

    static var all: [MobileSuiteJob] { makeJobs(includeVulkan: false) }

    static func makeJobs(includeVulkan: Bool, vulkanLiquid: Bool = true, gpuCache: Bool = true,
                         gles2: Bool = false, gles2MaxSteps: Int = 2048, gpuCacheParallelSteps: Int = 0) -> [MobileSuiteJob] {
        var jobs: [MobileSuiteJob] = []
        for api in includeVulkan ? ["metal", "vulkan"] : ["metal"] {
            let prefix = api == "vulkan" ? "vulkan/" : ""
            for particles in [65_536, 1_048_576, 4_194_304, 16_777_216] {
                for headless in [false, true] {
                    let test = headless ? "stream_headless" : "stream"
                    jobs.append(MobileSuiteJob(id: "\(prefix)\(test)-\(particles)",
                        title: "\(api == "vulkan" ? "Vulkan (MoltenVK)" : "Metal") · Particle \(particles) · \(headless ? "Compute" : "Render")",
                        gpuRequest: "\(test)?particles=\(particles)&vsync=0&api=\(api)", cpuArguments: [],
                        seconds: 15, expectedRecords: 1))
                }
            }
            for steps in [16, 64, 256] {
                jobs.append(MobileSuiteJob(id: "\(prefix)gpu_burn-\(steps)", title: "\(api.capitalized) · GPU Burn · \(steps)",
                    gpuRequest: "gpu_burn?steps=\(steps)&vsync=0&api=\(api)", cpuArguments: [],
                    seconds: 15, expectedRecords: 1))
            }
            if api == "metal" || vulkanLiquid {
                jobs.append(MobileSuiteJob(id: "\(prefix)cinematic_liquid", title: "\(api.capitalized) · Cinematic Liquid",
                gpuRequest: "cinematic_liquid?vsync=0&api=\(api)", cpuArguments: [], seconds: 15, expectedRecords: 1))
            }
            jobs.append(MobileSuiteJob(id: "\(prefix)memory_sweep", title: "\(api.capitalized) · GPU Cache & Memory",
                gpuRequest: "memory_sweep?api=\(api)", cpuArguments: [], seconds: 0, expectedRecords: 1))
        }
        if gpuCache {
            let cacheID = gpuCacheParallelSteps > 0 ? "gpu_cache-v3-parallel\(gpuCacheParallelSteps)" : "gpu_cache-v2"
            let request = "gpu_cache?api=metal" + (gpuCacheParallelSteps > 0 ? "&parallel_steps=\(gpuCacheParallelSteps)" : "")
            jobs.append(MobileSuiteJob(id: cacheID, title: "Metal · GPU Cache-state Diagnostic",
            gpuRequest: request, cpuArguments: [], seconds: 0, expectedRecords: 1))
        }
        jobs.append(MobileSuiteJob(id: "cpu_compute", title: "CPU Compute · Single + All Cores",
            gpuRequest: nil, cpuArguments: ["gpu_benchmark", "--cpu-benchmark", "all",
                "--cpu-time", "15", "--cpu-warmup", "0.2"], seconds: 0, expectedRecords: 2))
        jobs.append(MobileSuiteJob(id: "cpu_memory", title: "CPU Cache, Memory, Virtual Memory & Storage",
            gpuRequest: nil, cpuArguments: ["gpu_benchmark", "--cpu-membw"],
            seconds: 0, expectedRecords: 1))
        if gles2 {
            for steps in [16, 64, 256] where steps <= gles2MaxSteps {
                jobs.append(MobileSuiteJob(id: "gles2/gpu_burn-\(steps)", title: "OpenGL ES 2.0 · GPU Burn · \(steps)",
                    gpuRequest: "gpu_burn?steps=\(steps)&api=gles2&vsync=0", cpuArguments: [], seconds: 15, expectedRecords: 1))
            }
        }
        return jobs
    }

    func matches(_ result: BenchResult) -> Bool {
        if id == "cpu_compute" {
            return result.graphicsApi == "CPU" &&
                ["cpu_single_core", "cpu_multi_core"].contains(result.workload)
        }
        if id == "cpu_memory" {
            return result.graphicsApi == "CPU" && result.workload == "cpu_membw"
        }
        if id.hasPrefix("gles2/") {
            let steps = id.split(separator: "-").last ?? ""
            return result.graphicsApi == "OpenGL ES 2.0" &&
                (result.workloadVersion ?? "").hasPrefix("gpu_burn_es2_legacy_v1_fixed_steps_\(steps)_ios_preview")
        }
        let translated = id.hasPrefix("vulkan/")
        let testID = translated ? String(id.dropFirst(7)) : id
        guard result.graphicsApi == (translated ? "Vulkan" : "Metal") else { return false }
        let version = result.workloadVersion ?? result.workload
        guard version.contains("ios_preview") else { return false }
        if translated {
            guard version.contains("moltenvk"),
                  result.workloadConfigValue("apiImplementation") == "MoltenVK",
                  result.workloadConfigValue("translatedTo") == "Metal" else { return false }
        }
        if testID.hasPrefix("stream") {
            let parts = testID.split(separator: "-")
            return version.hasPrefix("stream_v1") &&
                result.particleCount == UInt32(parts.last ?? "") &&
                result.headless == testID.hasPrefix("stream_headless")
        }
        if testID.hasPrefix("gpu_burn-") {
            let steps = testID.split(separator: "-").last ?? ""
            return version.hasPrefix("gpu_burn_v3_fixed_steps_\(steps)_")
        }
        if testID == "cinematic_liquid" { return version.hasPrefix("cinematic_liquid_v2") }
        if testID.hasPrefix("gpu_cache-v") {
            let expectedVersion: String
            if testID.hasPrefix("gpu_cache-v3-parallel") {
                let parallel = String(testID.dropFirst("gpu_cache-v3-parallel".count))
                expectedVersion = "metal_dependent_cache_v3_parallel_\(parallel)_preview"
                guard result.workloadConfigValue("parallelSteps") == parallel else { return false }
            } else { expectedVersion = "metal_dependent_cache_v2_preview" }
            return version.hasPrefix(expectedVersion) &&
                result.workloadConfigValue("checksumValid") == "1" &&
                result.workloadConfigValue("cacheAttribution") == "unverified"
        }
        return version.hasPrefix(translated ? "membw_v1" : "metal_memory_sweep_v1")
    }

}

struct MobileSuiteReport: Codable, Sendable {
    struct Job: Codable, Sendable {
        let id: String
        let title: String
        let request: String
        let expectedRecords: Int
        var status = "pending"
        var resultIDs: [String] = []
        var error = ""
        var startedAt = ""
        var finishedAt = ""
        var thermalStateStart = 0
        var thermalStateEnd = 0
    }
    let id: String
    let startedAt: String
    var resumedFrom: String?
    var cancellationReason: String?
    var finishedAt = ""
    var status = "running"
    var jobs: [Job]

    var passedCount: Int { jobs.filter { $0.status == "passed" }.count }
    var reviewCount: Int { jobs.filter { $0.status == "review" }.count }
    var finishedCount: Int { jobs.filter { ["passed", "review", "failed", "cancelled"].contains($0.status) }.count }

    /// Retry an unchanged plan, or replace only a failed cache profile, while
    /// retaining later verified measurements. Other changes reuse a prefix.
    func reusableJobs(for plan: [MobileSuiteJob], results: [BenchResult]) -> [Job] {
        let samePlan = jobs.count == plan.count && zip(jobs, plan).allSatisfy { old, next in
            let sameRequest = old.id == next.id && old.expectedRecords == next.expectedRecords &&
                old.request == (next.gpuRequest ?? next.cpuArguments.joined(separator: " "))
            let replacedFailedCache = old.status == "failed" && old.request.hasPrefix("gpu_cache?") &&
                next.gpuRequest?.hasPrefix("gpu_cache?") == true && old.expectedRecords == next.expectedRecords
            return sameRequest || replacedFailedCache
        }
        var reused: [Job] = []
        for next in plan {
            guard let old = jobs.first(where: { $0.id == next.id }) else {
                if samePlan { continue }
                break
            }
            let saved = results.filter { old.resultIDs.contains($0.id) }
            let valid = old.status == "passed" && saved.count == next.expectedRecords &&
                Set(old.resultIDs).count == next.expectedRecords &&
                saved.allSatisfy { next.matches($0) && $0.score.isFinite && $0.score > 0 &&
                    ($0.workloadVersion ?? "").contains("ios_preview") } &&
                (next.id != "cpu_compute" || Set(saved.map(\.workload)) == Set(["cpu_single_core", "cpu_multi_core"]))
            if valid && saved.allSatisfy({ $0.measurementReviewReasons.isEmpty }) { reused.append(old) }
            else if !samePlan { break }
        }
        return reused
    }
}
