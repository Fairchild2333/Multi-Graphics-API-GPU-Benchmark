import Foundation
let p = ProcessInfo.processInfo
let names = ["nominal", "fair", "serious", "critical"]
let raw = p.thermalState.rawValue
let obj: [String: Any] = ["timestamp": ISO8601DateFormatter().string(from: Date()), "thermalState": raw, "thermalStateName": raw < names.count ? names[raw] : "unknown", "lowPowerMode": p.isLowPowerModeEnabled, "activeProcessorCount": p.activeProcessorCount]
let data = try JSONSerialization.data(withJSONObject: obj, options: [.sortedKeys])
print(String(data: data, encoding: .utf8)!)
