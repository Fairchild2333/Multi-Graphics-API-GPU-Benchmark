import Foundation
let root = URL(fileURLWithPath: CommandLine.arguments[1])
let rows = try JSONDecoder().decode([BenchResult].self, from: Data(contentsOf: root))
let plan = MobileSuiteJob.makeJobs(includeVulkan: true, vulkanLiquid: false, gpuCache: true,
    gles2: true, gles2MaxSteps: 256, gpuCacheParallelSteps: 512, cpuCoreTypes: true, particleFixedCoverage: true,
    cpuEfficiencyForeground: true, cpuMemoryEfficiencyForeground: false)
var output: [[String: Any]] = []
for job in plan {
    let matching = rows.filter(job.matches)
    let selected: [BenchResult]
    if job.id == "cpu_compute" {
        selected = [matching.last(where: { $0.workload == "cpu_single_core" && $0.workloadConfigValue("coreType") == "performance" }),
            matching.last(where: { $0.workload == "cpu_single_core" && $0.workloadConfigValue("coreType") == "efficiency" }),
            matching.last(where: { $0.workload == "cpu_multi_core" })].compactMap { $0 }
    } else { selected = Array(matching.suffix(job.expectedRecords)) }
    let complete = job.matchesRecordGroup(selected)
    output.append(["job": job.id, "request": job.gpuRequest ?? job.cpuArguments.joined(separator: " "),
        "complete": complete, "expectedRecords": job.expectedRecords, "resultIDs": selected.map(\.id),
        "scores": selected.map(\.score), "versions": selected.map { $0.workloadVersion ?? "" },
        "reviewReasons": Array(Set(selected.flatMap(\.measurementReviewReasons))).sorted(),
        "cpuMemoryPoints": selected.reduce(0) { $0 + RunOutput.savedMemory(workload: $1.workload, api: $1.graphicsApi, configuration: $1.workloadConfig).points.count },
        "vulkanMemoryPoints": selected.reduce(0) { $0 + RunOutput.savedMemory(workload: $1.workload, api: $1.graphicsApi, configuration: $1.workloadConfig).vulkanMemoryPoints.count }])
}
let result: [String:Any] = ["jobs": output, "planCount": plan.count, "completeCount": output.filter { $0["complete"] as? Bool == true }.count, "historyRows": rows.count]
FileHandle.standardOutput.write(try JSONSerialization.data(withJSONObject: result, options: [.prettyPrinted, .sortedKeys]))
print("")
