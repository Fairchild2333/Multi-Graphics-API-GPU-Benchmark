import Foundation
let rows = try JSONDecoder().decode([BenchResult].self, from: Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[1])))
var selected: [BenchResult] = []
for job in MobileSuiteJob.makeJobs(includeVulkan: false) where job.id != "gpu_cache-v2" {
 let matching = rows.filter(job.matches)
 if job.id == "cpu_compute" {
  selected.append(matching.first { $0.workload == "cpu_single_core" && $0.workloadConfigValue("coreType") == nil }!)
  selected.append(matching.first { $0.workload == "cpu_multi_core" }!)
 } else { selected.append(matching.last!) }
}
precondition(selected.count == 16)
let encoder = JSONEncoder(); encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
try encoder.encode(selected).write(to: URL(fileURLWithPath: CommandLine.arguments[2]))
print("Selected 16 real legacy records for the 15-job compatibility fixture; no history mutated")
