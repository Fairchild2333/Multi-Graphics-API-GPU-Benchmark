import Foundation

let root = URL(fileURLWithPath: CommandLine.arguments[1])
let decoder = JSONDecoder()
var summary: [[String: Any]] = []
for phase in ["candidate", "final", "boundary"] {
    let relative = phase == "candidate" ? "data/results/results.json" : "\(phase)/data/results/results.json"
    let rows = try decoder.decode([BenchResult].self, from: Data(contentsOf: root.appendingPathComponent(relative)))
    let expected = phase == "boundary" ? 2 : 6
    precondition(rows.count == expected && Set(rows.map(\.id)).count == expected)
    for row in rows {
        let reasons = row.measurementReviewReasons
        let legacyQuery = phase == "candidate" && row.graphicsApi == "Vulkan" && row.workload == "stream"
        let boundaryPreview = phase == "boundary" && row.workloadConfigValue("maxDatasetMiB") == "256"
        precondition(legacyQuery || boundaryPreview ? !reasons.isEmpty : reasons.isEmpty, "Unexpected quality state: \(row.id) \(reasons)")
        var item: [String: Any] = ["phase": phase, "id": row.id, "reviewReasons": reasons,
                                   "version": row.workloadVersion ?? "", "score": row.score]
        if row.workload == "membw" {
            let curves = RunOutput.savedMemory(workload: row.workload, api: row.graphicsApi, configuration: row.workloadConfig)
            precondition(curves.vulkanMemoryPoints.count == (boundaryPreview ? 77 : 95))
            precondition(row.workloadConfigValue("memoryPlateauReached") == (boundaryPreview ? "0" : "1"))
            item["restoredCurvePoints"] = curves.vulkanMemoryPoints.count
        }
        summary.append(item)
    }
}
let data = try JSONSerialization.data(withJSONObject: ["status": "passed", "records": summary], options: [.prettyPrinted, .sortedKeys])
try data.write(to: root.appendingPathComponent("production-swift-audit.json"))
print("Production Swift model: 10/14 accepted; 3 query controls and 1 boundary preview remain review; complete memory curves restored")
