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
    /// Model of the machine and its memory in MiB; missing in results saved by older versions.
    var deviceModel: String?
    var systemMemoryMB: UInt32?
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

    /// "Mac14,7 · macOS 15.1 · 16.0 GB RAM", from what the result has; empty for older results.
    var machineLine: String {
        var parts: [String] = []
        let model = deviceModel ?? ""
        if !model.isEmpty { parts.append(model) }
        // A CPU result already shows the chip as its device name, and an
        // iPhone's chip is only known through the model.
        if !cpuName.isEmpty && cpuName != deviceName && !cpuName.hasPrefix("Unknown")
            && (model.isEmpty || !cpuName.contains(model)) {
            parts.append(cpuName)
        }
        if !osVersion.isEmpty && osVersion != "Unknown" { parts.append(osVersion) }
        if let megabytes = systemMemoryMB, megabytes > 0 {
            parts.append(String(format: "%.1f GB RAM", Double(megabytes) / 1024.0))
        }
        return parts.joined(separator: " · ")
    }

    /// Short display string for the result.
    var displaySummary: String {
        String(format: "%@ | %@ | %@ | %.0f FPS | %.2f ms",
               graphicsApi, deviceName, difficulty, avgFps, avgTotalGpuMs)
    }

    /// Read a `key=value` item from the semicolon-delimited workload contract.
    /// A completed run may still need metrology or stability review.
    var measurementReviewReasons: [String] {
        if workload == "cpu_multi_core" && workloadConfigValue("workerSchedulingPolicy") == "apple_pe_qos_v1" &&
           workloadConfigValue("allCorePlacementVerified") != "1" {
            return [Localization.tr("All-core thread placement or occupancy was not fully verified", "全核线程的驻留或占用未全部验证", "全スレッドの配置または稼働率が未検証です")]
        }
        guard ["stream", "gpu_burn", "cinematic_liquid"].contains(workload) else { return [] }
        var reasons: [String] = []
        if avgFrameTimeMs > 0 && avgTotalGpuMs > avgFrameTimeMs * 1.1 {
            reasons.append(Localization.tr("GPU timing exceeds the frame interval", "GPU 计时跨度超过帧间隔", "GPU 計時がフレーム間隔を超えます"))
        }
        if avgComputeMs + avgRenderMs > avgTotalGpuMs + 0.1 {
            reasons.append(Localization.tr("Compute and render timing overlap", "计算与绘制计时重叠", "計算と描画の計時が重複しています"))
        }
        if headless && graphicsApi == "Vulkan" && particleCount >= 1_048_576 && avgComputeMs < avgFrameTimeMs * 0.5 {
            reasons.append(Localization.tr("Compute timing needs wall-clock verification", "纯计算计时需要与实际耗时核对", "計算計時を実時間で確認してください"))
        }
        if workload != "cinematic_liquid" {
            if (throttlePct ?? 0) > 5 {
                reasons.append(Localization.tr("Throughput fell by more than 5%", "运行中吞吐下降超过 5%", "実行中のスループットが 5% 以上低下しました"))
            }
            if (workload == "gpu_burn" || particleCount >= 1_048_576) && (stableScore ?? 0) <= 0 {
                reasons.append(Localization.tr("Stable throughput was not reached", "尚未达到稳定平台", "安定したスループットに達していません"))
            }
        }
        return reasons
    }

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
        if graphicsApi == "CPU" {
            switch workloadConfigValue("coreType") {
            case "performance": return Localization.tr("Performance core", "大核", "性能コア")
            case "efficiency":
                return (workloadConfigValue("efficiencyPolicy") ?? "").hasPrefix("foreground_")
                    ? Localization.tr("Efficiency core", "小核", "効率コア")
                    : Localization.tr("Efficiency core (background)", "小核（后台）", "効率コア（背景）")
            case "both": return Localization.tr("P/E + All-core", "大核／小核＋全核", "性能／効率＋全コア")
            default:
                if workload == "cpu_multi_core" { return Localization.tr("All-core", "全核", "全コア") }
                if workloadConfigValue("affinityCapability") == "scheduler_managed" ||
                   workloadConfigValue("affinity") == "scheduler_managed" {
                    return Localization.tr("System scheduling", "系统调度", "システム選択")
                }
                return workload == "cpu_membw"
                    ? Localization.tr("Cache & memory", "缓存与内存", "キャッシュ / メモリ")
                    : Localization.tr("Single-thread tests", "单线程测试", "1 スレッドテスト")
            }
        }
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
