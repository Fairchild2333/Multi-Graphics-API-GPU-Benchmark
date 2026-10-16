// Compile as main.swift with MobileBenchmark, BenchResult and Localization.
import Foundation
let fixture = try Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[1]))
let rows = try JSONDecoder().decode([BenchResult].self, from: fixture)
let native = MobileSuiteJob.all
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
            other.workloadVersion = job.id == "memory_sweep" ? "membw_v1_ios_preview_moltenvk" : (row.workloadVersion ?? "") + "_moltenvk"
            other.workloadConfig = (row.workloadConfig ?? "") + ";apiImplementation=MoltenVK;translatedTo=Metal"
            precondition(vk.matches(other), "Valid translated contract rejected")
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
    precondition(report.reusableJobs(for: plan, results: rows).count == 31)
    var failed = report
    failed.jobs[25].status = "failed"
    failed.jobs[25].resultIDs = []
    precondition(failed.reusableJobs(for: plan, results: rows).count == 30)
    let bounded = MobileSuiteJob.makeJobs(includeVulkan: true, vulkanLiquid: false,
        gles2: true, gpuCacheParallelSteps: 512)
    precondition(failed.reusableJobs(for: bounded, results: rows).count == 30)
    precondition(report.reusableJobs(for: bounded, results: rows).count == 25)
    var zeroScore = rows
    for index in zeroScore.indices where report.jobs[0].resultIDs.contains(zeroScore[index].id) {
        zeroScore[index].score = 0
    }
    precondition(report.reusableJobs(for: plan, results: zeroScore).count == 30)
    print("Resume: retain verified later jobs, replace failed cache profile, reject incompatible passed profiles and zero scores")
}
