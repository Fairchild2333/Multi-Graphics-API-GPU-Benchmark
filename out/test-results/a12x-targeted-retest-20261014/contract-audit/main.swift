import Foundation
let root = URL(fileURLWithPath: CommandLine.arguments[1])
let decoder = JSONDecoder()
let results = try decoder.decode([BenchResult].self, from: Data(contentsOf: root.appendingPathComponent("phone-final/results/results.json")))
let manifests = try decoder.decode([MobileSuiteReport].self, from: Data(contentsOf: root.appendingPathComponent("completed-manifests.json")))
let plan = MobileSuiteJob.makeJobs(includeVulkan: true, vulkanLiquid: false, gpuCache: true, gles2: true, gpuCacheParallelSteps: 512, cpuCoreTypes: true, particleFixedCoverage: true, cpuEfficiencyForeground: true, cpuMemoryEfficiencyForeground: false)
var output: [[String: Any]] = []
for report in manifests {
    precondition(["passed", "review"].contains(report.status))
    precondition(report.jobs.count == 1)
    let run = report.jobs[0]
    let job = plan.first { $0.id == run.id }!
    let rows = results.filter { run.resultIDs.contains($0.id) }
    precondition(job.matchesRecordGroup(rows), "Production result contract failed: \(run.id)")
    precondition(rows.allSatisfy { $0.score.isFinite && $0.score > 0 && $0.deviceModel == "iPad8,7" })
    output.append(["job": run.id, "status": run.status, "resultIDs": run.resultIDs,
        "productionContractPassed": true, "reviewReasons": rows.map { $0.measurementReviewReasons },
        "thermalStart": run.thermalStateStart, "thermalEnd": run.thermalStateEnd])
}
precondition(output.count == 14)
let data = try JSONSerialization.data(withJSONObject: output, options: [.prettyPrinted, .sortedKeys])
try data.write(to: root.appendingPathComponent("production-contract-audit.json"))
print("Production contract audit passed: \(output.count) jobs")

let curated = try decoder.decode([BenchResult].self, from: Data(contentsOf: root.appendingPathComponent("curated-suite-results.json")))
precondition(plan.count == 31 && curated.count == 33)
for job in plan {
    let saved = curated.filter(job.matches)
    precondition(job.matchesRecordGroup(saved), "Curated full-suite contract failed: \(job.id)")
}
print("Curated full suite passed: 31 jobs / 33 records")
