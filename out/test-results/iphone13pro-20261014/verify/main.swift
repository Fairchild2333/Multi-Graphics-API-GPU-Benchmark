import Foundation

let folder = CommandLine.arguments[1]
let report = try JSONDecoder().decode(MobileSuiteReport.self, from: Data(contentsOf: URL(fileURLWithPath: folder + "/suite-runs/latest.json")))
let results = try JSONDecoder().decode([BenchResult].self, from: Data(contentsOf: URL(fileURLWithPath: folder + "/results/results.json")))
var failures: [String] = []
@MainActor func expect(_ ok: Bool, _ message: String) { if !ok { failures.append(message) } }
expect(report.status == "passed", "Suite did not finish successfully")
expect(report.jobs.count == 15 && MobileSuiteJob.all.count == 15, "Incomplete matrix")
expect(report.jobs.map(\.id) == MobileSuiteJob.all.map(\.id), "Incorrect job order or identity")
expect(Set(results.map(\.id)).count == results.count, "Duplicate result IDs")
let ids = report.jobs.flatMap(\.resultIDs)
expect(ids.count == 16 && Set(ids).count == 16, "Expected 16 unique result records")
for (job, saved) in zip(MobileSuiteJob.all, report.jobs) {
    let rows = results.filter { saved.resultIDs.contains($0.id) }
    expect(saved.status == "passed" && saved.error.isEmpty, "Job failed: \(job.id)")
    expect(rows.count == job.expectedRecords && rows.allSatisfy(job.matches), "Wrong records: \(job.id)")
    expect(rows.allSatisfy { $0.score.isFinite && $0.score > 0 && $0.platform == "iOS" && $0.deviceModel == "iPhone14,2" && ($0.workloadVersion ?? "").hasSuffix("_ios_preview") }, "Invalid device/score/contract: \(job.id)")
    if let first = rows.first {
        var other = first
        other.graphicsApi = "Vulkan"
        expect(!job.matches(other), "Accepts a different API: \(job.id)")
        if job.id.hasPrefix("stream") {
            other = first; other.particleCount += 256
            expect(!job.matches(other), "Accepts a different particle size: \(job.id)")
            other = first; other.headless.toggle()
            expect(!job.matches(other), "Accepts a different execution mode: \(job.id)")
        }
        if job.id.hasPrefix("gpu_burn-") {
            other = first; other.workloadVersion = "gpu_burn_v3_fixed_steps_2048_kaleidoscope_ios_preview"
            expect(!job.matches(other), "Accepts the wrong GPU Burn steps: \(job.id)")
        }
    }
}
for result in results where result.workload == "membw" || result.workload == "cpu_membw" {
    let output = RunOutput.savedMemory(workload: result.workload, api: result.graphicsApi, configuration: result.workloadConfig)
    if result.graphicsApi == "CPU" {
        expect(output.hasMemoryResults && output.points.count > 100, "Saved CPU curves could not be restored")
        expect(output.pageMapping?.gbps ?? 0 > 0, "No measured virtual memory/page mapping")
        expect(output.storage?.measured == true, "No measured storage")
        print("CPU saved points: \(output.points.count), levels: \(output.levels.count), page mapping and storage restored")
    } else {
        expect(output.hasGpuMemoryResults && !output.gpuMemoryPoints.isEmpty, "Saved GPU curves could not be restored")
        print("GPU saved points: \(output.gpuMemoryPoints.count)")
    }
}
print("Verified \(report.passedCount)/15 jobs, \(ids.count) records; failures: \(failures)")
exit(failures.isEmpty ? 0 : 1)
