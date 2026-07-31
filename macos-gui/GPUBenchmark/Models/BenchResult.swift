// BenchResult.swift — Codable model mirroring BenchmarkResult from C++.

import Foundation

struct BenchResult: Codable, Identifiable, Hashable {
    var id: String
    var timestamp: String
    var resultSchemaVersion: UInt32?
    var appVersion: String?

    var workload: String
    var workloadVersion: String?
    var workloadConfig: String?
    var graphicsApi: String
    var deviceName: String
    var driverVersion: String
    var cpuName: String
    var osVersion: String
    var platform: String?
    var osArchitecture: String?
    var processArchitecture: String?
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
    var stableScore: Double?
    var stableVariancePct: Double?
    var throttlePct: Double?

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

    /// Read a `key=value` item from the semicolon-delimited workload contract.
    func workloadConfigValue(_ key: String) -> String? {
        guard let workloadConfig else { return nil }
        return workloadConfig
            .split(separator: ";")
            .lazy
            .compactMap { component -> (String, String)? in
                let pair = component.split(separator: "=", maxSplits: 1)
                guard pair.count == 2 else { return nil }
                return (
                    pair[0].trimmingCharacters(in: .whitespacesAndNewlines),
                    pair[1].trimmingCharacters(in: .whitespacesAndNewlines)
                )
            }
            .first(where: { $0.0 == key })?
            .1
    }

    /// Execution mode shown by the Windows history table, expressed honestly
    /// for macOS too. Imported AFR/SFR records remain distinguishable without
    /// implying that the macOS runner can create them.
    var executionMode: String {
        var mode = workloadConfigValue("multiGpu")
        if mode == nil {
            if workloadVersion?.contains("_afr2") == true {
                mode = "afr"
            } else if workloadVersion?.contains("_sfr2") == true {
                mode = "sfr"
            }
        }

        if mode == "afr" {
            switch workloadConfigValue("afrControl") {
            case "implicit_driver_unverified":
                return Localization.tr("AFR (unverified)", "AFR（未验证）", "AFR（未検証）")
            case "explicit_vulkan_device_group":
                return Localization.tr("AFR (experimental)", "AFR（实验）", "AFR（実験）")
            case nil, "":
                return Localization.tr("AFR (legacy)", "AFR（旧记录）", "AFR（旧記録）")
            default:
                return "AFR ×2"
            }
        }
        if mode == "sfr" { return "SFR ×2" }
        if headless { return "Headless" }
        return Localization.tr("Single GPU", "单 GPU", "単一 GPU")
    }

    /// Very old bandwidth rows can contain impossible values produced by
    /// broken timestamp queries. Keep the original JSON, but do not rank or
    /// present the value as a valid score.
    var hidesImplausibleLegacyScore: Bool {
        let legacy = (resultSchemaVersion ?? 1) < 4 ||
            appVersion?.isEmpty != false ||
            appVersion == "Unknown (legacy)"
        let bandwidthScore = scoreUnit.localizedCaseInsensitiveContains("GB/s")
        guard legacy, bandwidthScore else { return false }
        return !score.isFinite ||
            !avgFps.isFinite ||
            !avgComputeMs.isFinite ||
            score > 10_000 ||
            (score > 0 && avgFps <= 0)
    }

    var normalizedScore: Double {
        guard !hidesImplausibleLegacyScore, score.isFinite, score > 0 else {
            return 0
        }
        return score
    }

    var scoreDisplay: String {
        if workload == "fluid" {
            return Localization.tr(
                "Unverified legacy",
                "未验证旧版",
                "未検証の旧版")
        }
        guard normalizedScore > 0 else { return "—" }
        let unit = scoreUnit.isEmpty ? "" : " \(scoreUnit)"
        let precisionSuffix = precision.isEmpty ? "" : " (\(precision))"
        return String(format: "%.1f", normalizedScore) + unit + precisionSuffix
    }
}
