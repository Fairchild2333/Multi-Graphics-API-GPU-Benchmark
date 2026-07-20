// BenchResult.swift — Codable model mirroring BenchmarkResult from C++.

import Foundation

struct BenchResult: Codable, Identifiable, Hashable {
    var id: String
    var timestamp: String

    var workload: String
    var workloadVersion: String?
    var workloadConfig: String?
    var graphicsApi: String
    var deviceName: String
    var driverVersion: String
    var cpuName: String
    var osVersion: String
    var memory: String
    var vramMB: UInt32
    var resWidth: UInt32
    var resHeight: UInt32
    var particleCount: UInt32
    var difficulty: String
    var vsync: Bool
    var isSoftware: Bool
    var headless: Bool
    var framesInFlight: UInt32

    var durationSec: Double
    var warmupSec: Double
    var measuredFrames: UInt32
    var timingSamples: UInt32

    var avgComputeMs: Double
    var minComputeMs: Double
    var maxComputeMs: Double
    var avgRenderMs: Double
    var minRenderMs: Double
    var maxRenderMs: Double
    var avgTotalGpuMs: Double
    var minTotalGpuMs: Double
    var maxTotalGpuMs: Double

    var avgFps: Double
    var avgFrameTimeMs: Double
    var gpuUtilisation: Double
    var bottleneck: String

    var score: Double
    var scoreUnit: String
    var precision: String

    /// Parsed timestamp for sorting/filtering.
    var date: Date? {
        // id format: "20260627-101245-760" → "2026-06-27 10:12:45"
        guard id.count >= 15 else { return nil }
        let idx = id.index(id.startIndex, offsetBy: 15)
        let prefix = String(id[..<idx])
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyyMMdd-HHmmss"
        return formatter.date(from: prefix)
    }

    /// Short display string for the result.
    var displaySummary: String {
        String(format: "%@ | %@ | %@ | %.0f FPS | %.2f ms",
               graphicsApi, deviceName, difficulty, avgFps, avgTotalGpuMs)
    }
}
