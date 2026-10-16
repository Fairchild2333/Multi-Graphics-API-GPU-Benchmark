import Foundation
let folder = CommandLine.arguments[1]
let report = try JSONDecoder().decode(MobileSuiteReport.self, from: Data(contentsOf: URL(fileURLWithPath: folder + "/suite-runs/latest.json")))
let rows = try JSONDecoder().decode([BenchResult].self, from: Data(contentsOf: URL(fileURLWithPath: folder + "/results/results.json")))
let plan = MobileSuiteJob.makeJobs(includeVulkan: true, vulkanLiquid: false, gpuCache: false,
    gles2: true, gles2MaxSteps: 64, particleFixedCoverage: true, cpuEfficiencyForeground: true,
    vulkanMaxParticles: 8_388_608)
var failures: [String] = []
@MainActor func check(_ value: Bool, _ message: String) { if !value { failures.append(message) } }
check(["passed", "review"].contains(report.status), "Unfinished/failed suite")
check(report.jobs.map(\.id) == plan.map(\.id), "Missing or unsupported job")
check(plan.count == 27 && report.finishedCount == 27 && report.failedCount == 0, "Expected27 completed jobs and zero failures")
let ids = report.jobs.flatMap(\.resultIDs)
check(ids.count == 28 && Set(ids).count == 28, "Expected28 unique records")
let log = try String(contentsOf: URL(fileURLWithPath: folder + "/suite-runs/" + report.id + ".log"), encoding: .utf8)
var timings = 0
var curves: [String: Int] = [:]
for (index, pair) in zip(plan, report.jobs).enumerated() {
    let (job, entry) = pair
    let saved = rows.filter { entry.resultIDs.contains($0.id) }
    check(["passed", "review"].contains(entry.status), "Unsuccessful job: \(job.id)")
    check(job.matchesRecordGroup(saved), "Wrong saved contract/group: \(job.id)")
    check(saved.allSatisfy { $0.score.isFinite && $0.score > 0 }, "Invalid score: \(job.id)")
    for row in saved where ["membw", "cpu_membw"].contains(row.workload) {
        let restored = RunOutput.savedMemory(workload: row.workload, api: row.graphicsApi, configuration: row.workloadConfig)
        if row.workload == "cpu_membw" {
            check(restored.points.count >= 174 && restored.storage?.measured == true && (restored.pageMapping?.gbps ?? 0) > 0, "Missing CPU curves/page/storage")
            curves["CPU"] = restored.points.count
        } else {
            let count = restored.gpuMemoryPoints.count + restored.vulkanMemoryPoints.count
            check(count > 40, "Missing GPU memory curves: \(job.id)")
            curves[row.graphicsApi] = count
        }
    }
    if job.gpuRequest != nil && !job.id.contains("memory_sweep") {
        let marker = "\n=== \(index + 1)/\(plan.count) \(job.id):"
        if let start = log.range(of: marker) {
            let end = log.range(of: "\n=== \(index + 2)/\(plan.count) ", range: start.upperBound..<log.endIndex)?.lowerBound ?? log.endIndex
            let parsed = RunOutput.parse(String(log[start.lowerBound..<end]))
            let count = parsed.jobs.reduce(0) { $0 + $1.samples.count }
            check(count > 0, "Missing GPU timing curve: \(job.id)")
            timings += count
        } else { check(false, "Missing native log section: \(job.id)") }
    }
}
print("Complete button: \(report.finishedCount)/27 jobs, \(ids.count) records, failed\(report.failedCount), review\(report.reviewCount)")
print("Saved memory curves: \(curves); native GPU timing points: \(timings)")
print("Failures: \(failures)")
exit(failures.isEmpty ? 0 : 1)
