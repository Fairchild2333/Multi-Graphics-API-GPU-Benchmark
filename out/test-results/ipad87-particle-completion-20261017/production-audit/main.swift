import Foundation

let root = URL(fileURLWithPath: CommandLine.arguments[1])
let decoder = JSONDecoder()
let before = try decoder.decode([BenchResult].self, from: Data(contentsOf: root.appendingPathComponent("phone-before/results/results.json")))
let rows = try decoder.decode([BenchResult].self, from: Data(contentsOf: root.appendingPathComponent("phone-final/results/results.json")))
let report = try decoder.decode(MobileSuiteReport.self, from: Data(contentsOf: root.appendingPathComponent("phone-final/suite-runs/latest.json")))
let plan = MobileSuiteJob.makeJobs(includeVulkan: true, vulkanLiquid: false, gpuCache: true,
    gles2: true, gles2MaxSteps: 256, cpuCoreTypes: true, particleFixedCoverage: true,
    cpuEfficiencyForeground: true)
let selected = ["vulkan/stream-65536", "vulkan/stream_headless-65536", "vulkan/stream-1048576", "vulkan/stream-4194304", "vulkan/stream-16777216"]
precondition(rows.count == before.count + selected.count)
precondition(report.resumedFrom == nil && report.jobs.count == selected.count)
precondition(["passed", "review"].contains(report.status) && !report.finishedAt.isEmpty)
precondition(Set(rows.map(\.id)).count == rows.count)
let newIDs = Set(rows.map(\.id)).subtracting(before.map(\.id))
var used = Set<String>()
var audit: [[String: Any]] = []
for (id, savedJob) in zip(selected, report.jobs) {
    let job = plan.first { $0.id == id }!
    precondition(savedJob.id == job.id && savedJob.expectedRecords == job.expectedRecords)
    precondition(savedJob.request == (job.gpuRequest ?? job.cpuArguments.joined(separator: " ")))
    let saved = rows.filter { savedJob.resultIDs.contains($0.id) }
    precondition(saved.count == job.expectedRecords && Set(savedJob.resultIDs).count == saved.count)
    precondition(job.matchesRecordGroup(saved) && saved.allSatisfy(job.matches))
    precondition(saved.allSatisfy { $0.score.isFinite && $0.score > 0 && ($0.workloadVersion ?? "").contains("ios_preview") })
    precondition(used.isDisjoint(with: savedJob.resultIDs))
    used.formUnion(savedJob.resultIDs)
    let reasons = Array(Set(saved.flatMap(\.measurementReviewReasons))).sorted()
    precondition(savedJob.status == (reasons.isEmpty ? "passed" : "review"))
    let charts = saved.map { RunOutput.savedMemory(workload: $0.workload, api: $0.graphicsApi, configuration: $0.workloadConfig) }
    audit.append(["job": id, "status": savedJob.status, "contractPassed": true,
        "resultIDs": savedJob.resultIDs, "reviewReasons": reasons,
        "vulkanMemoryPoints": charts.reduce(0) { $0 + $1.vulkanMemoryPoints.count },
        "thermalStart": savedJob.thermalStateStart, "thermalEnd": savedJob.thermalStateEnd])
}
precondition(used == newIDs)
try JSONSerialization.data(withJSONObject: audit, options: [.prettyPrinted, .sortedKeys]).write(to: root.appendingPathComponent("production-contract-audit.json"))
print("PASS: 5 production jobs and unique new records; requests, versions, groups, quality and curves verified.")
