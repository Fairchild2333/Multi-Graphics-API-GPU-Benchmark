// Compile as main.swift with MobileBenchmark, BenchResult and Localization.
import Foundation
let fixture = try Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[1]))
let rows = try JSONDecoder().decode([BenchResult].self, from: fixture)
let native = MobileSuiteJob.makeJobs(includeVulkan: false)
let translated = MobileSuiteJob.makeJobs(includeVulkan: true)
precondition(native.count == 16 && translated.count == 29)
precondition(Set(translated.map(\.id)).count == 29)
precondition(translated.reduce(0) { $0 + $1.expectedRecords } == 30)
precondition(MobileSuiteJob.makeJobs(includeVulkan: true, gles2: true).count == 32)
precondition(MobileSuiteJob.makeJobs(includeVulkan: true, vulkanLiquid: false, gles2: true).count == 31)
let softwareGL = MobileSuiteJob.makeJobs(includeVulkan: true, gles2: true, gles2MaxSteps: 64)
precondition(softwareGL.count == 31 && !softwareGL.contains { $0.id == "gles2/gpu_burn-256" })
precondition(softwareGL.contains { $0.id == "gpu_burn-256" && $0.gpuRequest?.contains("api=metal") == true })
let restricted = MobileSuiteJob.makeJobs(includeVulkan: true, vulkanLiquid: false, gpuCache: false)
precondition(restricted.count == 27)
precondition(!restricted.contains { $0.id == "vulkan/cinematic_liquid" || $0.id == "gpu_cache-v2" })
let gpuOnly = translated.filter { MobileSuiteScope.gpu.includes($0) }
precondition(translated.filter { $0.gpuRequest == nil }.allSatisfy { !$0.requiresRenderSurface })
precondition(translated.filter { $0.id.contains("stream_headless") || $0.id.contains("memory_sweep") || $0.id == "gpu_cache-v2" }
    .allSatisfy { !$0.requiresRenderSurface })
precondition(translated.filter { $0.id.contains("gpu_burn-") || $0.id.contains("stream-") || $0.id.contains("cinematic_liquid") }
    .allSatisfy { $0.requiresRenderSurface })
let cpuOnly = translated.filter { MobileSuiteScope.cpu.includes($0) }
precondition(gpuOnly.count == 27 && cpuOnly.count == 2)
precondition(gpuOnly.allSatisfy { $0.gpuRequest != nil })
precondition(cpuOnly.reduce(0) { $0 + $1.expectedRecords } == 3)
let splitCPU = MobileSuiteJob.makeJobs(includeVulkan: false, cpuCoreTypes: true)
    .first { $0.id == "cpu_compute" }!
precondition(splitCPU.expectedRecords == 3)
let actualA15Plan = MobileSuiteJob.makeJobs(includeVulkan: true, vulkanLiquid: false,
    gles2: true, cpuCoreTypes: true, particleFixedCoverage: true)
precondition(actualA15Plan.count == 31 && actualA15Plan.reduce(0) { $0 + $1.expectedRecords } == 33)
let limitedVulkanPlan = MobileSuiteJob.makeJobs(includeVulkan: true, vulkanMaxParticles: 4_194_304)
precondition(!limitedVulkanPlan.contains { $0.id == "vulkan/stream-16777216" || $0.id == "vulkan/stream_headless-16777216" })
precondition(limitedVulkanPlan.contains { $0.id == "stream-16777216" } && limitedVulkanPlan.contains { $0.id == "vulkan/stream_headless-4194304" })
precondition(limitedVulkanPlan.count == translated.count - 2)
var p = rows.first { $0.workload == "cpu_single_core" }!
precondition(!splitCPU.matches(p), "Unverified scheduler-managed sample accepted as a core type")
p.workloadConfig = "coreType=performance;residencyVerified=1"
var e = p; e.workloadConfig = "coreType=efficiency;residencyVerified=1"
let allCPU = rows.first { $0.workload == "cpu_multi_core" }!
precondition(splitCPU.matchesRecordGroup([p, e, allCPU]))
let performanceCPU = MobileSuiteJob.makeJobs(includeVulkan: false, cpuCoreTypes: true,
    cpuEfficiencyForeground: true).first { $0.id == "cpu_compute" }!
precondition(!performanceCPU.matchesRecordGroup([p, e, allCPU]), "Background E score reused as performance-priority E")
var foregroundE = e
foregroundE.workloadConfig = "coreType=efficiency;residencyVerified=1;efficiencyPolicy=foreground_with_performance_load;residencyMethod=thread_selfcounts_perf_level_spi;scheduling=user_initiated"
precondition(performanceCPU.matchesRecordGroup([p, foregroundE, allCPU]))
var utilityE = foregroundE
utilityE.workloadConfig = "coreType=efficiency;residencyVerified=1;efficiencyPolicy=foreground_utility_without_helpers;residencyMethod=thread_selfcounts_perf_level_spi;scheduling=utility;placementHelperThreads=0;minimumWorkerBusyFraction=0.90"
precondition(!performanceCPU.matches(utilityE), "Unversioned utility policy accepted")
utilityE.workloadVersion = "cpu_mixed_v1_foreground_e_utility_v1_ios_preview"
var utilityAll = allCPU
utilityAll.workloadVersion = "cpu_mixed_v1_apple_pe_qos_workers_v1_ios_preview"
utilityAll.workloadConfig = (utilityAll.workloadConfig ?? "") + ";workerSchedulingPolicy=apple_pe_qos_v1;allCorePlacementVerified=1"
precondition(performanceCPU.matchesRecordGroup([p, utilityE, utilityAll]))
var migratedAll = utilityAll
migratedAll.workloadConfig = "workerSchedulingPolicy=apple_pe_qos_v1;allCorePlacementVerified=0;allCoreValidation=aggregate_cpu_time_v1;allCoreParticipationVerified=1;minimumRoundPerformanceOccupancy=0.97;minimumRoundEfficiencyOccupancy=0.94;minimumRoundWorkerOccupancy=0.92"
precondition(migratedAll.measurementReviewReasons.isEmpty, "Balanced all-core migration is not a failed single-core purity measurement")
var starvedAll = migratedAll
starvedAll.workloadConfig = starvedAll.workloadConfig?.replacingOccurrences(of: "minimumRoundEfficiencyOccupancy=0.94", with: "minimumRoundEfficiencyOccupancy=0.74")
precondition(!starvedAll.measurementReviewReasons.isEmpty, "An idle core group must remain review even with a claimed verified flag")
precondition(!performanceCPU.matchesRecordGroup([p, utilityE, allCPU]), "Inherited-QoS all-core score reused for typed workers")
var invalidUtility = utilityE
invalidUtility.workloadConfig = invalidUtility.workloadConfig?.replacingOccurrences(of: "residencyVerified=1", with: "residencyVerified=0")
precondition(!performanceCPU.matches(invalidUtility))
let backgroundPlan = MobileSuiteJob.makeJobs(includeVulkan: false, cpuCoreTypes: true,
    cpuEfficiencyForeground: false)
let backgroundCPU = backgroundPlan.first { $0.id == "cpu_compute" }!
var backgroundE = foregroundE
backgroundE.workloadConfig = backgroundE.workloadConfig?.replacingOccurrences(of: "foreground_with_performance_load", with: "background")
    .replacingOccurrences(of: "scheduling=user_initiated", with: "scheduling=background")
precondition(backgroundCPU.matchesRecordGroup([p, backgroundE, allCPU]))
precondition(!backgroundCPU.matches(foregroundE), "Foreground E reused as background E")
precondition(!performanceCPU.matches(backgroundE), "Background E reused as foreground E")
let performanceMemory = MobileSuiteJob.makeJobs(includeVulkan: false, cpuCoreTypes: true,
    cpuEfficiencyForeground: true).first { $0.id == "cpu_memory" }!
let oldMemory = rows.first { $0.workload == "cpu_membw" }!
precondition(!performanceMemory.matches(oldMemory), "Background/unchecked curves reused as foreground E")
var newMemory = oldMemory
newMemory.workloadConfig = "coreType=both;residencyVerified=1;efficiencyPolicy=foreground_with_performance_load;efficiencyResidencyMethod=thread_selfcounts_perf_level_spi;efficiencyScheduling=user_initiated;" +
    ["single", "single_1", "all"].flatMap { mode in
        ["read", "write", "copy"].map { "curve_\(mode)_\($0)=4:1" }
    }.joined(separator: ";")
precondition(performanceMemory.matches(newMemory))
var utilityMemory = newMemory
utilityMemory.workloadConfig = utilityMemory.workloadConfig?.replacingOccurrences(of: "foreground_with_performance_load", with: "foreground_utility_without_helpers")
    .replacingOccurrences(of: "efficiencyScheduling=user_initiated", with: "efficiencyScheduling=utility")
utilityMemory.workloadConfig = (utilityMemory.workloadConfig ?? "") + ";placementHelperThreads=0;minimumWorkerBusyFraction=0.90"
precondition(!performanceMemory.matches(utilityMemory), "Unversioned utility curves accepted")
utilityMemory.workloadVersion = "cpu_membw_v1_foreground_e_utility_v1_ios_preview"
precondition(performanceMemory.matches(utilityMemory))
let backgroundMemory = backgroundPlan.first { $0.id == "cpu_memory" }!
precondition(!backgroundMemory.matches(oldMemory), "Unclassified curves reused as verified P/E")
precondition(!backgroundMemory.matches(newMemory), "Foreground curves reused as background E")
var backgroundCurves = newMemory
backgroundCurves.workloadConfig = backgroundCurves.workloadConfig?.replacingOccurrences(of: "foreground_with_performance_load", with: "background")
    .replacingOccurrences(of: "efficiencyScheduling=user_initiated", with: "efficiencyScheduling=background")
precondition(backgroundMemory.matches(backgroundCurves))
let mixedPolicies = MobileSuiteJob.makeJobs(includeVulkan: false, cpuCoreTypes: true,
    cpuEfficiencyForeground: true, cpuMemoryEfficiencyForeground: false)
precondition(mixedPolicies.first { $0.id == "cpu_compute" }!.matchesRecordGroup([p, utilityE, utilityAll]))
precondition(mixedPolicies.first { $0.id == "cpu_memory" }!.matches(backgroundCurves))
newMemory.workloadConfig = newMemory.workloadConfig?.replacingOccurrences(of: "curve_single_1_read=4:1", with: "curve_single_1_read=")
precondition(!performanceMemory.matches(newMemory), "Incomplete E curves accepted")
precondition(!splitCPU.matchesRecordGroup([p, p, allCPU]), "Duplicate P result accepted in place of E")
e.workloadConfig = "coreType=efficiency;residencyVerified=0"
precondition(!splitCPU.matchesRecordGroup([p, e, allCPU]), "Unverified residency accepted")
let fixedStream = MobileSuiteJob.makeJobs(includeVulkan: false, particleFixedCoverage: true)
    .first { $0.id == "stream-1048576" }!
var legacyStream = rows.first { $0.graphicsApi == "Metal" && $0.workload == "stream" && $0.particleCount == 1_048_576 && !$0.headless }!
precondition(!fixedStream.matches(legacyStream), "Legacy coverage reused for fixed coverage")
legacyStream.workloadConfig = (legacyStream.workloadConfig ?? "") + ";renderCoverage=wrapped_ndc_v2"
precondition(fixedStream.matches(legacyStream))
precondition(translated.allSatisfy { MobileSuiteScope.all.includes($0) })
var overlap = rows.first { $0.graphicsApi == "Metal" && $0.workload == "stream" }!
overlap.avgTotalGpuMs = overlap.avgFrameTimeMs * 2
precondition(!overlap.measurementReviewReasons.isEmpty)
for job in native where job.id != "gpu_cache-v2" {
    let matched = rows.filter(job.matches)
    precondition(matched.count == job.expectedRecords, "Old iOS result no longer matches \(job.id)")
    for row in matched {
        var other = row
        other.graphicsApi = "OpenGL"
        precondition(!job.matches(other), "Wrong API accepted")
        if job.id.hasPrefix("stream") {
            other = row; other.headless.toggle()
            precondition(!job.matches(other), "Wrong execution mode accepted")
            other = row; other.particleCount += 256
            precondition(!job.matches(other), "Wrong particle size accepted")
        }
        if let vk = translated.first(where: { $0.id == "vulkan/" + job.id }) {
            other = row; other.graphicsApi = "Vulkan"
            other.workloadVersion = job.id == "memory_sweep" ? "membw_v3_streaming_ios_preview_moltenvk" : (row.workloadVersion ?? "") + "_moltenvk"
            other.workloadConfig = (row.workloadConfig ?? "") + ";apiImplementation=MoltenVK;translatedTo=Metal"
            if job.id == "memory_sweep" { other.workloadConfig = (other.workloadConfig ?? "") + ";dataVerified=1;writeCopyChecks=2;timestampAnomalies=0;largeDatasetTraversal=ordered_full_dataset_passes" }
            if job.id.hasPrefix("gpu_burn-") || job.id.hasPrefix("stream") {
                other.workloadConfig = (other.workloadConfig ?? "").split(separator: ";")
                    .filter { !$0.hasPrefix("timingMode=") }.joined(separator: ";") + ";timingMode=synchronized_wall_clock"
            }
            precondition(vk.matches(other), "Valid translated contract rejected")
            if job.id.hasPrefix("stream") {
                var unchecked = other
                unchecked.workloadConfig = unchecked.workloadConfig?.replacingOccurrences(of: "timingMode=synchronized_wall_clock", with: "timingMode=gpu_timestamp_query")
                precondition(!vk.matches(unchecked), "Startup-only MoltenVK compute timestamps reused")
            }
            if job.id == "memory_sweep" {
                var unchecked = other
                unchecked.workloadVersion = "membw_v1_ios_preview_moltenvk"
                precondition(!vk.matches(unchecked), "Unchecked old Vulkan sweep reused")
                unchecked.workloadVersion = "membw_v2_verified_ios_preview_moltenvk"
                precondition(!vk.matches(unchecked), "Private-chunk repeats reused as whole-dataset streaming")
                var noPlateau = other
                noPlateau.workloadConfig = (noPlateau.workloadConfig ?? "").split(separator: ";")
                    .filter { !$0.hasPrefix("memoryPlateauReached=") }.joined(separator: ";") + ";memoryPlateauReached=0"
                precondition(!noPlateau.measurementReviewReasons.isEmpty, "A completed preview must retain its memory-platform warning")
                unchecked = other
                unchecked.workloadConfig = unchecked.workloadConfig?.replacingOccurrences(of: "timestampAnomalies=0", with: "timestampAnomalies=1")
                precondition(!vk.matches(unchecked), "Mixed-timing sweep reused")
            }
            other.workloadConfig = ""
            precondition(!vk.matches(other), "Translation metadata required")
        }
    }
}
print("iOS manifest: native 16 / translated 29 / restricted 27; preserved existing 15-job scores; API and translation boundaries verified")

// An optional real completed manifest exercises resume against saved results.
if CommandLine.arguments.count > 2 {
    let data = try Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[2]))
    let report = try JSONDecoder().decode(MobileSuiteReport.self, from: data)
    let plan = MobileSuiteJob.makeJobs(includeVulkan: true, vulkanLiquid: false, gles2: true)
    let reusable = report.reusableJobs(for: plan, results: rows)
    precondition(reusable.allSatisfy { job in
        rows.filter { job.resultIDs.contains($0.id) }.allSatisfy { $0.measurementReviewReasons.isEmpty }
    })
    var failed = report
    failed.jobs[25].status = "failed"
    failed.jobs[25].resultIDs = []
    precondition(failed.reusableJobs(for: plan, results: rows).count == reusable.filter { $0.id != report.jobs[25].id }.count)
    let bounded = MobileSuiteJob.makeJobs(includeVulkan: true, vulkanLiquid: false,
        gles2: true, gpuCacheParallelSteps: 512)
    precondition(failed.reusableJobs(for: bounded, results: rows).count == reusable.filter { $0.id != report.jobs[25].id }.count)
    precondition(report.reusableJobs(for: bounded, results: rows).count <= 25)
    var zeroScore = rows
    for index in zeroScore.indices where report.jobs[0].resultIDs.contains(zeroScore[index].id) {
        zeroScore[index].score = 0
    }
    precondition(report.reusableJobs(for: plan, results: zeroScore).count == reusable.filter { $0.id != report.jobs[0].id }.count)
    print("Resume: retain verified later jobs, replace failed cache profile, reject flagged measurements, incompatible profiles and zero scores")
}
