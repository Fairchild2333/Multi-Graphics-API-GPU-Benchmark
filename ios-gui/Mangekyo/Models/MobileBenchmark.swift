// MobileBenchmark.swift — iOS/iPadOS product capabilities and run state.

import Foundation

enum MobileWorkload: String, CaseIterable, Identifiable, Codable {
    case stream
    case gpuBurn = "gpu_burn"
    case cinematicLiquid = "cinematic_liquid"

    var id: String { rawValue }

    var title: String {
        switch self {
        case .stream:
            return Localization.tr(
                "Particle — Memory Throughput",
                "粒子 —— 内存吞吐",
                "パーティクル — メモリ帯域")
        case .gpuBurn:
            return Localization.tr(
                "Plasma × Kaleidoscope — GPU Burn",
                "等离子晶核 × 万花镜 —— GPU Burn",
                "プラズマ核 × カレイドスコープ — GPU Burn")
        case .cinematicLiquid:
            return Localization.tr(
                "Cinematic Liquid — Interactive Pool",
                "电影化液体 —— 互动水池",
                "Cinematic Liquid — インタラクティブプール")
        }
    }

    var shortTitle: String {
        switch self {
        case .stream: return Localization.tr("Particle", "粒子", "パーティクル")
        case .gpuBurn: return "GPU Burn"
        case .cinematicLiquid: return Localization.tr("Liquid", "液体", "液体")
        }
    }

    var systemImage: String {
        switch self {
        case .stream: return "circle.grid.cross"
        case .gpuBurn: return "flame.fill"
        case .cinematicLiquid: return "drop.fill"
        }
    }

    var expectedContract: String {
        switch self {
        case .stream:
            return "stream_v1_ios_preview"
        case .gpuBurn:
            return "gpu_burn_v3_fixed_steps_16_kaleidoscope_ios_preview"
        case .cinematicLiquid:
            return "cinematic_liquid_v2_…_metal_preview_ios_preview"
        }
    }

    var fixedProfile: String {
        switch self {
        case .stream:
            return Localization.tr(
                "262,144 particles · embedded preview profile",
                "262,144 粒子 · 嵌入式预览配置",
                "262,144 パーティクル · 埋め込みプレビュー")
        case .gpuBurn:
            return Localization.tr(
                "Light · 16 fixed raymarch steps",
                "轻量 · 固定 16 次光线步进",
                "Light · 16 固定レイマーチステップ")
        case .cinematicLiquid:
            return Localization.tr(
                "320,920 particles · Metal preview profile",
                "320,920 粒子 · Metal 预览配置",
                "320,920 パーティクル · Metal プレビュー")
        }
    }

    var shaderResourceNames: [String] {
        switch self {
        case .stream: return ["particle.metal"]
        case .gpuBurn: return ["gpu_burn.metal"]
        case .cinematicLiquid: return ["cinematic_liquid_v2.metal"]
        }
    }
}

struct MobileBenchmarkRequest: Equatable {
    var workload: MobileWorkload
    var durationSeconds: Double
    var captureRequested: Bool
    var captureAtSeconds: Double?
}

enum MobileRunPhase: String, Equatable {
    case idle
    case preparing
    case running
    case cancelling
    case completed
    case failed
}

enum CancellationReason: Equatable {
    case user
    case background
}

enum NativeCaptureCapability: Equatable {
    case unavailable(reason: String)
    case available

    var isAvailable: Bool {
        if case .available = self { return true }
        return false
    }

    var explanation: String {
        switch self {
        case .available:
            return Localization.tr(
                "Native Metal capture is available.",
                "原生 Metal 抓帧可用。",
                "ネイティブ Metal キャプチャを利用できます。")
        case .unavailable(let reason):
            return reason
        }
    }
}

struct BenchIssue: Identifiable, Equatable {
    enum Code: String {
        case metalUnavailable
        case layerUnavailable
        case workloadUnavailable
        case invalidDuration
        case captureUnavailable
        case alreadyRunning
        case nativeStartFailed
        case nativeRunFailed
        case resultDecodeFailed
        case exportFailed
        case storageUnavailable
    }

    let code: Code
    let title: String
    let message: String
    let recoverySuggestion: String?

    var id: String {
        "\(code.rawValue):\(message)"
    }

    var combinedMessage: String {
        guard let recoverySuggestion, !recoverySuggestion.isEmpty else {
            return message
        }
        return "\(message)\n\n\(recoverySuggestion)"
    }
}
