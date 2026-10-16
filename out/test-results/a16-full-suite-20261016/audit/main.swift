import Foundation
let root = URL(fileURLWithPath: CommandLine.arguments[1])
let decoder = JSONDecoder()
let rows = try decoder.decode([BenchResult].self, from: Data(contentsOf: root.appendingPathComponent("curated-suite-results.json")))
let sources = try decoder.decode([String: String].self, from: Data(contentsOf: root.appendingPathComponent("selected-run-sources.json")))
let plan = MobileSuiteJob.makeJobs(includeVulkan: true, vulkanLiquid: false, gpuCache: true,
    gles2: true, gpuCacheParallelSteps: 0, cpuCoreTypes: true, particleFixedCoverage: true,
    cpuEfficiencyForeground: true, cpuMemoryEfficiencyForeground: false, vulkanMaxParticles: 100_663_296)
precondition(plan.count == 31 && rows.count == 33 && sources.count == 31)
precondition(Set(rows.map(\.id)).count == rows.count)
var used = Set<String>()
var output: [[String: Any]] = []
for job in plan {
    let source = sources[job.id]!
    let report = try decoder.decode(MobileSuiteReport.self, from: Data(contentsOf: root.appendingPathComponent("phone-final/suite-runs/" + source + ".json")))
    let native = report.jobs.first { $0.id == job.id }!
    precondition(native.expectedRecords == job.expectedRecords && native.request == (job.gpuRequest ?? job.cpuArguments.joined(separator: " ")))
    let saved = rows.filter { native.resultIDs.contains($0.id) }
    precondition(saved.count == job.expectedRecords && Set(native.resultIDs).count == saved.count)
    precondition(saved.allSatisfy(job.matches) && job.matchesRecordGroup(saved), "Contract failed: \(job.id)")
    precondition(saved.allSatisfy { $0.deviceModel == "iPhone15,3" && $0.score.isFinite && $0.score > 0 && ($0.workloadVersion ?? "").contains("ios_preview") })
    precondition(used.isDisjoint(with: native.resultIDs))
    used.formUnion(native.resultIDs)
    let reasons = Array(Set(saved.flatMap(\.measurementReviewReasons))).sorted()
    precondition(native.status == (reasons.isEmpty ? "passed" : "review"), "Quality status mismatch: \(job.id)")
    let restored = saved.map { RunOutput.savedMemory(workload: $0.workload, api: $0.graphicsApi, configuration: $0.workloadConfig) }
    if job.id == "cpu_memory" { precondition(restored[0].points.count > 0 && restored[0].pageMapping != nil && restored[0].storage != nil) }
    if job.id == "memory_sweep" { precondition(restored[0].gpuMemoryPoints.count > 0) }
    if job.id == "vulkan/memory_sweep" { precondition(restored[0].vulkanMemoryPoints.count > 0) }
    if job.id == "gpu_cache-v2" { precondition(restored[0].gpuCachePoints.count > 0) }
    output.append(["job": job.id, "sourceManifest": source, "nativeStatus": native.status,
        "productionContractPassed": true, "resultIDs": saved.map(\.id), "reviewReasons": reasons,
        "scores": saved.map(\.score), "units": saved.map(\.scoreUnit),
        "cpuCurvePoints": restored.reduce(0) { $0 + $1.points.count },
        "metalMemoryPoints": restored.reduce(0) { $0 + $1.gpuMemoryPoints.count },
        "vulkanMemoryPoints": restored.reduce(0) { $0 + $1.vulkanMemoryPoints.count },
        "gpuCachePoints": restored.reduce(0) { $0 + $1.gpuCachePoints.count },
        "hasPageMapping": restored.contains { $0.pageMapping != nil }, "hasStorage": restored.contains { $0.storage != nil }])
}
precondition(used.count == rows.count)
try JSONSerialization.data(withJSONObject: output, options: [.prettyPrinted,.sortedKeys]).write(to: root.appendingPathComponent("production-contract-audit.json"))
print("Production contract and persisted-curve audit PASS: 31 jobs / 33 selected real iPhone15,3 records; \(output.filter { ($0["nativeStatus"] as? String) == "review" }.count) quality reviews retained. Sources are actual native manifests, not a synthetic run.")
