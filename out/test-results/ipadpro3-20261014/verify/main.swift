import Foundation
let folder = CommandLine.arguments[1]
let suite = try JSONDecoder().decode(MobileSuiteReport.self, from: Data(contentsOf: URL(fileURLWithPath: folder + "/suite-runs/latest.json")))
let rows = try JSONDecoder().decode([BenchResult].self, from: Data(contentsOf: URL(fileURLWithPath: folder + "/results/results.json")))
let jobs = MobileSuiteJob.makeJobs(includeVulkan: true, vulkanLiquid: false, gpuCache: true, gles2: true, gpuCacheParallelSteps: 512)
var failures: [String] = []
@MainActor func check(_ yes: Bool, _ message: String) { if !yes { failures.append(message) } }
check(suite.status == "passed", "Final suite incomplete")
check(suite.jobs.map(\.id) == jobs.map(\.id), "Unsupported or missing job")
check(jobs.count == 31 && suite.passedCount == 31, "Expected 31 supported jobs")
let ids = suite.jobs.flatMap(\.resultIDs)
check(ids.count == 32 && Set(ids).count == 32, "Expected 32 unique selected records")
for (job, entry) in zip(jobs, suite.jobs) {
    let saved = rows.filter { entry.resultIDs.contains($0.id) }
    check(entry.status == "passed" && entry.error.isEmpty, "Failed job: \(job.id)")
    check(saved.count == job.expectedRecords && saved.allSatisfy(job.matches), "Wrong work/API: \(job.id)")
    check(saved.allSatisfy { $0.score.isFinite && $0.score > 0 && $0.platform == "iOS" && $0.deviceModel == "iPad8,7" }, "Invalid physical score: \(job.id)")
    for row in saved where ["membw", "cpu_membw", "gpu_cache"].contains(row.workload) {
        let restored = RunOutput.savedMemory(workload: row.workload, api: row.graphicsApi, configuration: row.workloadConfig)
        switch row.workload {
        case "cpu_membw":
            check(restored.points.count >= 120 && restored.storage?.measured == true && (restored.pageMapping?.gbps ?? 0) > 0, "Missing CPU curves/VM/storage")
            print("CPU saved curve points: \(restored.points.count)")
        case "gpu_cache":
            check(restored.gpuCachePoints.count == 124, "Missing cache-state curves")
            check(Set(restored.gpuCachePoints.map(\.condition)).count == 4, "Missing cache conditions")
            for condition in Set(restored.gpuCachePoints.map(\.condition)) {
                check(restored.gpuCachePoints.filter { $0.condition == condition }.map(\.addressBytes).max() == 128 * 1048576, "Cache curve did not reach 128 MiB")
            }
            check(row.workloadConfigValue("warming") == "bounded_complete_cycle", "Missing bounded warmup contract")
            check(row.workloadConfigValue("parallelSteps") == "512", "Missing shorter parallel sampling contract")
            print("GPU cache saved curve points: \(restored.gpuCachePoints.count)")
        default:
            let count = restored.gpuMemoryPoints.count + restored.vulkanMemoryPoints.count
            check(count > 40, "Missing GPU memory curves")
            print("\(row.graphicsApi) saved memory points: \(count)")
        }
    }
}
print("Verified \(suite.passedCount)/31 jobs, \(ids.count) records. Failures: \(failures)")
exit(failures.isEmpty ? 0 : 1)
