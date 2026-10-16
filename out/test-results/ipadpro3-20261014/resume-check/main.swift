import Foundation
let folder = CommandLine.arguments[1]
let report = try JSONDecoder().decode(MobileSuiteReport.self, from: Data(contentsOf: URL(fileURLWithPath: folder + "/final-phone/suite-runs/latest.json")))
let results = try JSONDecoder().decode([BenchResult].self, from: Data(contentsOf: URL(fileURLWithPath: folder + "/suite-results.json")))
let jobs = MobileSuiteJob.makeJobs(includeVulkan: true, vulkanLiquid: false, gpuCache: true, gles2: true)
precondition(report.reusableJobs(for: jobs, results: results).count == 31)
var failed = report
failed.jobs[25].status = "failed"
failed.jobs[25].resultIDs = []
let reusable = failed.reusableJobs(for: jobs, results: results)
precondition(reusable.count == 30 && !reusable.contains { $0.id == "gpu_cache-v2" })
precondition(reusable.contains { $0.id == "cpu_memory" } && reusable.contains { $0.id == "gles2/gpu_burn-256" })
var changed = jobs
changed[25] = MobileSuiteJob(id: "gpu_cache-new-contract", title: "New contract", gpuRequest: "gpu_cache?api=metal", cpuArguments: [], seconds: 0, expectedRecords: 1)
precondition(report.reusableJobs(for: changed, results: results).count == 25)
var corrupt = results
for index in corrupt.indices where report.jobs[0].resultIDs.contains(corrupt[index].id) { corrupt[index].score = 0 }
precondition(report.reusableJobs(for: jobs, results: corrupt).count == 30)
let olderGPU = MobileSuiteJob.makeJobs(includeVulkan: true, vulkanLiquid: false, gpuCache: true, gles2: true, gpuCacheParallelSteps: 512)
precondition(olderGPU.count == 31)
precondition(failed.reusableJobs(for: olderGPU, results: results).count == 30)
precondition(report.reusableJobs(for: olderGPU, results: results).count == 25)
print("Verified identical-plan retries retain later passed jobs; changed contracts retain only a valid prefix; zero scores rejected")
