import Foundation

let root = URL(fileURLWithPath: CommandLine.arguments[1])
let decoder = JSONDecoder()
let before = try decoder.decode([BenchResult].self, from: Data(contentsOf: root.appendingPathComponent("phone-before/results/results.json")))
let rows = try decoder.decode([BenchResult].self, from: Data(contentsOf: root.appendingPathComponent("phone-final/results/results.json")))
let report = try decoder.decode(MobileSuiteReport.self, from: Data(contentsOf: root.appendingPathComponent("manifest.json")))
let plan = MobileSuiteJob.makeJobs(includeVulkan: true, vulkanLiquid: false, gpuCache: true,
    gles2: true, gles2MaxSteps: 256, cpuCoreTypes: true, particleFixedCoverage: true,
    cpuEfficiencyForeground: true)
let selected = ["stream-65536", "stream_headless-65536", "stream-1048576", "stream_headless-1048576", "stream-4194304", "stream_headless-4194304", "stream-16777216", "stream_headless-16777216", "gpu_burn-16", "gpu_burn-64", "gpu_burn-256", "cinematic_liquid", "memory_sweep", "gpu_cache-v2"]
precondition(report.jobs.count == selected.count && rows.count == before.count + selected.count)
precondition(Set(rows.map(\.id)).count == rows.count)
let newIDs = Set(rows.map(\.id)).subtracting(before.map(\.id))
var used = Set<String>()
var audit: [[String: Any]] = []
for (id, savedJob) in zip(selected, report.jobs) {
    let job = plan.first { $0.id == id }!
    precondition(report.resumedFrom == nil)
    precondition(["passed", "review"].contains(report.status) && !report.finishedAt.isEmpty)
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
    if id == "memory_sweep" {
        precondition(charts[0].gpuMemoryPoints.count > 0)
        precondition(saved[0].workloadConfigValue("memoryPlateauVerified") == "0")
        precondition((saved[0].workloadVersion ?? "").hasPrefix("metal_memory_sweep_v1_preview"))
    }
    if id == "gpu_cache-v2" {
        precondition(charts[0].gpuCachePoints.count > 0)
        precondition(saved[0].workloadConfigValue("cacheAttribution") == "unverified")
        precondition(saved[0].workloadConfigValue("checksumValid") == "1")
    }

    audit.append(["job": id, "status": savedJob.status, "contractPassed": true,
        "resultIDs": savedJob.resultIDs, "reviewReasons": reasons,
        "vulkanMemoryPoints": charts.reduce(0) { $0 + $1.vulkanMemoryPoints.count },
        "metalMemoryPoints": charts.reduce(0) { $0 + $1.gpuMemoryPoints.count },
        "metalCachePoints": charts.reduce(0) { $0 + $1.gpuCachePoints.count },
        "thermalStart": savedJob.thermalStateStart, "thermalEnd": savedJob.thermalStateEnd])
}
precondition(used == newIDs)
try JSONSerialization.data(withJSONObject: audit, options: [.prettyPrinted, .sortedKeys]).write(to: root.appendingPathComponent("production-contract-audit.json"))
print("PASS: 14 native Metal production jobs and unique new records; requests, versions, groups, quality and saved curves verified.")
