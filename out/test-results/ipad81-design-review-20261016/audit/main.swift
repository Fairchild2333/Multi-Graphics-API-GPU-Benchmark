import Foundation
let root = URL(fileURLWithPath: CommandLine.arguments[1])
let rows = try JSONDecoder().decode([BenchResult].self, from: Data(contentsOf: root.appendingPathComponent("curated-suite-results.json")))
let plan = MobileSuiteJob.makeJobs(includeVulkan: true, vulkanLiquid: false, gpuCache: true, gles2: true,
    gpuCacheParallelSteps: 512, cpuCoreTypes: true, particleFixedCoverage: true,
    cpuEfficiencyForeground: true, cpuMemoryEfficiencyForeground: false)
precondition(plan.count == 31 && rows.count == 33)
var output: [[String: Any]] = []
for job in plan {
    let saved = rows.filter(job.matches)
    precondition(job.matchesRecordGroup(saved), "Production contract failed: \(job.id)")
    precondition(saved.allSatisfy { $0.score.isFinite && $0.score > 0 && $0.deviceModel == "iPad8,1" && $0.measurementReviewReasons.isEmpty }, "Quality review remains: \(job.id)")
    let restored = saved.map { RunOutput.savedMemory(workload: $0.workload, api: $0.graphicsApi, configuration: $0.workloadConfig) }
    output.append(["job": job.id, "status": "passed", "resultIDs": saved.map(\.id),
        "productionContractPassed": true, "reviewReasons": saved.map(\.measurementReviewReasons),
        "cpuCurvePoints": restored.reduce(0) { $0 + $1.points.count },
        "metalMemoryPoints": restored.reduce(0) { $0 + $1.gpuMemoryPoints.count },
        "vulkanMemoryPoints": restored.reduce(0) { $0 + $1.vulkanMemoryPoints.count },
        "gpuCachePoints": restored.reduce(0) { $0 + $1.gpuCachePoints.count }])
}
try JSONSerialization.data(withJSONObject: output, options: [.prettyPrinted,.sortedKeys]).write(to: root.appendingPathComponent("production-contract-audit.json"))
print("Production contract and quality audit PASS: 31 jobs / 33 real selected records. This is a combined selection, not a new full-button run.")
