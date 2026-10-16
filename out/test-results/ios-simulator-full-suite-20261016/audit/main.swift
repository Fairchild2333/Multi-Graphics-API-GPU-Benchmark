import Foundation
let root = URL(fileURLWithPath: CommandLine.arguments[1])
let docs = root.appendingPathComponent("final-documents")
let decoder = JSONDecoder()
let report = try decoder.decode(MobileSuiteReport.self, from: Data(contentsOf: docs.appendingPathComponent("suite-runs/latest.json")))
let rows = try decoder.decode([BenchResult].self, from: Data(contentsOf: docs.appendingPathComponent("results/results.json")))
let plan = MobileSuiteJob.makeJobs(includeVulkan: true, vulkanLiquid: false, gpuCache: false,
    gles2: true, gles2MaxSteps: 64, cpuCoreTypes: false, particleFixedCoverage: true,
    vulkanMaxParticles: 8_388_608)
precondition(report.resumedFrom == nil, "This must be a fresh real-button run")
precondition(plan.count == 27 && rows.count == 28)
precondition(plan.map(\.id) == report.jobs.map(\.id))
precondition(["passed", "review"].contains(report.status) && !report.finishedAt.isEmpty)
precondition(report.failedCount == 0 && report.finishedCount == plan.count)
precondition(Set(rows.map(\.id)).count == rows.count)
var audit: [[String: Any]] = []
var used = Set<String>()
for (job, savedJob) in zip(plan, report.jobs) {
    precondition(savedJob.expectedRecords == job.expectedRecords)
    precondition(savedJob.request == (job.gpuRequest ?? job.cpuArguments.joined(separator: " ")))
    let saved = rows.filter { savedJob.resultIDs.contains($0.id) }
    precondition(saved.count == job.expectedRecords && Set(savedJob.resultIDs).count == saved.count)
    precondition(job.matchesRecordGroup(saved) && saved.allSatisfy(job.matches), "Contract failed: \(job.id)")
    precondition(saved.allSatisfy { $0.score.isFinite && $0.score > 0 && ($0.workloadVersion ?? "").contains("ios_preview") })
    precondition(used.isDisjoint(with: savedJob.resultIDs), "Record reused by different jobs")
    used.formUnion(savedJob.resultIDs)
    let reasons = Array(Set(saved.flatMap(\.measurementReviewReasons))).sorted()
    precondition(savedJob.status == (reasons.isEmpty ? "passed" : "review"), "Quality status mismatch: \(job.id)")
    let charts = saved.map { RunOutput.savedMemory(workload: $0.workload, api: $0.graphicsApi, configuration: $0.workloadConfig) }
    if job.id == "cpu_memory" {
        precondition(charts[0].points.count > 0 && charts[0].pageMapping != nil && charts[0].storage != nil, "Missing CPU curves / VM / storage")
    }
    if job.id == "memory_sweep" { precondition(charts[0].gpuMemoryPoints.count > 0) }
    if job.id == "vulkan/memory_sweep" { precondition(charts[0].vulkanMemoryPoints.count > 0) }
    audit.append(["job": job.id, "status": savedJob.status, "contractPassed": true,
        "resultIDs": savedJob.resultIDs, "reviewReasons": reasons,
        "cpuCurvePoints": charts.reduce(0) { $0 + $1.points.count },
        "metalMemoryPoints": charts.reduce(0) { $0 + $1.gpuMemoryPoints.count },
        "vulkanMemoryPoints": charts.reduce(0) { $0 + $1.vulkanMemoryPoints.count },
        "hasPageMapping": charts.contains { $0.pageMapping != nil },
        "hasStorage": charts.contains { $0.storage != nil }])
}
precondition(used.count == rows.count)
try JSONSerialization.data(withJSONObject: audit, options: [.prettyPrinted,.sortedKeys]).write(to: root.appendingPathComponent("production-contract-audit.json"))
print("SIMULATOR FLOW AUDIT PASS: \(plan.count) jobs / \(rows.count) unique saved records; \(report.passedCount) passed, \(report.reviewCount) variability reviews; all contracts and persisted curves checked. Not physical-device performance acceptance.")
