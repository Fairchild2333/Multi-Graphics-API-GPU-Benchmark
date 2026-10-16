import Foundation
let path = CommandLine.arguments[1]
let rows = try JSONDecoder().decode([BenchResult].self, from: Data(contentsOf: URL(fileURLWithPath: path)))
let plan = MobileSuiteJob.makeJobs(includeVulkan: true, vulkanLiquid: false, gles2: true,
    cpuCoreTypes: true, particleFixedCoverage: true, cpuEfficiencyForeground: true)
var failures: [String] = []
@MainActor func check(_ ok: Bool, _ message: String) { if !ok { failures.append(message) } }
check(rows.count == 33 && Set(rows.map(\.id)).count == 33, "Selected33 records missing/duplicated")
var curves: [String: Int] = [:]
for job in plan {
    let matched = rows.filter(job.matches)
    check(job.matchesRecordGroup(matched), "Invalid saved contract/group: \(job.id)")
    for row in matched {
        check(row.deviceModel == "iPhone14,2" && row.score.isFinite && row.score > 0, "Invalid physical result")
        let output = RunOutput.savedMemory(workload: row.workload, api: row.graphicsApi, configuration: row.workloadConfig)
        if row.workload == "cpu_membw" {
            check(output.points.filter { $0.series.hasPrefix("single/") || $0.series.hasPrefix("single_1/") }.count == 186 && output.points.count == 267 && output.memoryEfficiencyForeground, "Missing foreground P/E curves")
            check(output.storage?.measured == true && (output.pageMapping?.gbps ?? 0) > 0, "Missing pages/storage")
            curves["CPU"] = output.points.count
        } else if row.workload == "membw" {
            let n = output.gpuMemoryPoints.count + output.vulkanMemoryPoints.count
            check(n > 40, "Missing \(row.graphicsApi) memory curves")
            curves[row.graphicsApi] = n
        } else if row.workload == "gpu_cache" {
            check(output.gpuCachePoints.count == 124, "Missing four cache-state curves")
            check(row.workloadConfigValue("cacheAttribution") == "unverified", "False attribution")
            curves["cache states"] = output.gpuCachePoints.count
        }
    }
}
print("A15 selected: \(plan.count) measured jobs / \(rows.count) records; \(curves); failures \(failures)")
exit(failures.isEmpty ? 0 : 1)
