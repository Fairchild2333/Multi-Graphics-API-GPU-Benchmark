import Foundation

// Uses the production result model; this program only reads and audits archives.
let root = URL(fileURLWithPath: CommandLine.arguments[1])
let documents = URL(fileURLWithPath: CommandLine.arguments[2])
let decoder = JSONDecoder()
let report = try decoder.decode(MobileSuiteReport.self, from: Data(contentsOf: root.appendingPathComponent("ui-watch-01/documents/suite-runs/latest.json")))
let latest = try decoder.decode(MobileSuiteReport.self, from: Data(contentsOf: documents.appendingPathComponent("suite-runs/latest.json")))
let results = try decoder.decode([BenchResult].self, from: Data(contentsOf: documents.appendingPathComponent("results/results.json")))
let ids = Set(report.jobs.map(\.id))
let parallel = report.jobs.first { $0.id.hasPrefix("gpu_cache-v3-parallel") }
    .flatMap { Int($0.id.dropFirst("gpu_cache-v3-parallel".count)) } ?? 0
let largestVulkan = report.jobs.filter { $0.id.hasPrefix("vulkan/stream-") }
    .compactMap { Int($0.id.split(separator: "-").last ?? "") }.max() ?? 16_777_216
let plan = MobileSuiteJob.makeJobs(includeVulkan: ids.contains { $0.hasPrefix("vulkan/") },
    vulkanLiquid: ids.contains("vulkan/cinematic_liquid"),
    gpuCache: ids.contains { $0.hasPrefix("gpu_cache-") },
    gles2: ids.contains { $0.hasPrefix("gles2/") },
    gpuCacheParallelSteps: parallel, cpuCoreTypes: true, particleFixedCoverage: true,
    cpuEfficiencyForeground: true, cpuMemoryEfficiencyForeground: false,
    vulkanMaxParticles: largestVulkan)
let completeMatrix = Set(plan.map(\.id)) == ids
var output: [[String: Any]] = []
var allValid = completeMatrix
for original in report.jobs {
    let saved = latest.jobs.first { $0.id == original.id } ?? original
    let rows = results.filter { saved.resultIDs.contains($0.id) }
    let job = plan.first { $0.id == saved.id }
    let valid = job?.matchesRecordGroup(rows) == true && rows.count == saved.expectedRecords &&
        rows.allSatisfy { $0.score.isFinite && $0.score > 0 && $0.deviceModel == "iPad8,1" && !$0.isSoftware }
    allValid = allValid && valid && ["passed", "review"].contains(saved.status)
    let memory = rows.map { RunOutput.savedMemory(workload: $0.workload, api: $0.graphicsApi, configuration: $0.workloadConfig) }
    output.append(["job": saved.id, "status": saved.status, "productionContractPassed": valid,
        "resultIDs": saved.resultIDs, "scores": rows.map(\.score),
        "reviewReasons": rows.map(\.measurementReviewReasons),
        "cpuCurvePoints": memory.reduce(0) { $0 + $1.points.count },
        "metalMemoryPoints": memory.reduce(0) { $0 + $1.gpuMemoryPoints.count },
        "vulkanMemoryPoints": memory.reduce(0) { $0 + $1.vulkanMemoryPoints.count },
        "gpuCachePoints": memory.reduce(0) { $0 + $1.gpuCachePoints.count },
        "thermalStart": saved.thermalStateStart, "thermalEnd": saved.thermalStateEnd,
        "error": saved.error, "originalUIStatus": original.status,
        "sourceManifestID": latest.jobs.contains { $0.id == saved.id } ? latest.id : report.id])
}
let audit: [String: Any] = ["productionContractsPassed": allValid,
    "completeSupportedMatrix": completeMatrix, "jobs": output,
    "historyRecords": results.count, "manifestID": report.id, "manifestStatus": report.status,
    "selection": "Original UI suite plus actual targeted headless retests; not a second full-button run"]
try JSONSerialization.data(withJSONObject: audit, options: [.prettyPrinted, .sortedKeys])
    .write(to: root.appendingPathComponent("production-contract-audit.json"))
print("Production contract audit: \(allValid ? "PASS" : "INCOMPLETE") | \(output.count) jobs | \(results.count) records")
if !allValid { exit(1) }
