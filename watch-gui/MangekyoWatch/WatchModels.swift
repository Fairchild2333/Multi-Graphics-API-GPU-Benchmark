import Foundation

func watchText(_ chinese: String, _ english: String) -> String {
    Locale.preferredLanguages.first?.hasPrefix("zh") == true ? chinese : english
}

struct WatchResult: Codable, Identifiable {
    let id: String
    let timestamp: String
    let workload: String
    let workloadVersion: String
    let workloadConfig: String
    let score: Double
    let scoreUnit: String
    let platform: String
    let deviceModel: String
    let cpuName: String
    let isSoftware: Bool

    var config: [String: String] {
        Dictionary(workloadConfig.split(separator: ";").compactMap { entry in
            let pair = entry.split(separator: "=", maxSplits: 1, omittingEmptySubsequences: false)
            return pair.count == 2 ? (String(pair[0]), String(pair[1])) : nil
        }, uniquingKeysWith: { _, last in last })
    }
    var title: String {
        switch workload {
        case "cpu_single_core": return watchText("单线程算力", "Single-thread compute")
        case "cpu_multi_core": return watchText("全核心算力", "All-core compute")
        default: return watchText("缓存／内存", "Cache / memory")
        }
    }
    var reviewReasons: [String] {
        guard workload == "cpu_membw" else { return [] }
        let values = config
        var reasons: [String] = []
        if !memoryVerified {
            reasons.append(watchText("内存平台未确认；保留原始曲线", "Memory plateau unconfirmed; curves retained"))
        }
        if values["pageFaultMeasured"] != "1" { reasons.append(watchText("页面首次访问未完成", "First-page touch incomplete")) }
        if values["storageMeasured"] != "1" { reasons.append(watchText("存储测试未完成", "Storage test incomplete")) }
        return reasons
    }
    var memoryVerified: Bool {
        let source = config["score"] == "single_thread_memory_read" ? "single_memory" : "all_memory"
        return score > 0 && config[source + "_settled"] == "1"
    }
    var curves: [WatchCurvePoint] {
        config.filter { $0.key.hasPrefix("curve_") && ["read", "write", "copy"].contains(String($0.key.split(separator: "_").last ?? "")) }
            .sorted { $0.key < $1.key }.flatMap { name, data in
                data.split(separator: ",").enumerated().compactMap { index, item in
                    let pair = item.split(separator: ":")
                    guard pair.count == 2, let size = Double(pair[0]), let rate = Double(pair[1]),
                          size > 0, rate.isFinite, rate >= 0 else { return nil }
                    return WatchCurvePoint(id: "\(name)-\(index)", series: String(name.dropFirst(6)), kib: size, gbps: rate)
                }
            }
    }
}
struct WatchCurvePoint: Identifiable {
    let id: String
    let series: String
    let kib: Double
    let gbps: Double
}
struct WatchJob: Codable, Identifiable {
    let id: String
    var status = "pending"
    var resultIDs: [String] = []
    var notes: [String] = []
}
struct WatchSuite: Codable {
    let id: String
    let startedAt: Date
    var updatedAt: Date
    var status: String
    let profile: String
    let executionEnvironment: String
    var jobs: [WatchJob]
    var statusText: String {
        switch status {
        case "running": return watchText("测试中", "Running")
        case "passed": return watchText("已完成", "Completed")
        case "review": return watchText("已完成，有结果待确认", "Completed; review needed")
        case "cancelled": return watchText("已取消", "Cancelled")
        case "interrupted": return watchText("已中断", "Interrupted")
        default: return watchText("测试失败", "Failed")
        }
    }
}
