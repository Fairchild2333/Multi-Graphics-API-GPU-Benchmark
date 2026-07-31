// ChartsView.swift — dependency-free benchmark comparison charts.
// Swift Charts is used on macOS 13+; Monterey receives a native SwiftUI bar
// list so the standalone app never depends on Python or matplotlib.

import Charts
import SwiftUI

private enum BenchmarkChartMetric: String, CaseIterable, Identifiable {
    case score
    case fps
    case gpuTime

    var id: String { rawValue }

    var label: String {
        switch self {
        case .score:
            return Localization.tr("Score", "分数", "スコア")
        case .fps:
            return "FPS"
        case .gpuTime:
            return Localization.tr("GPU time", "GPU 时间", "GPU 時間")
        }
    }

    var prefersLowerValue: Bool { self == .gpuTime }

    func value(for result: BenchResult) -> Double {
        switch self {
        case .score:   return result.normalizedScore
        case .fps:     return result.avgFps
        case .gpuTime: return result.avgTotalGpuMs
        }
    }

    func formattedValue(_ value: Double, unit: String) -> String {
        switch self {
        case .score:
            return String(format: "%.1f", value) + (unit.isEmpty ? "" : " \(unit)")
        case .fps:
            return String(format: "%.0f FPS", value)
        case .gpuTime:
            return String(format: "%.3f ms", value)
        }
    }
}

private enum ExecutionModeFilter: String, CaseIterable, Identifiable {
    case all
    case single
    case headless
    case multiGPU

    var id: String { rawValue }

    var label: String {
        switch self {
        case .all:      return Localization.tr("All modes", "所有模式", "すべてのモード")
        case .single:   return Localization.tr("Single GPU", "单 GPU", "単一 GPU")
        case .headless: return "Headless"
        case .multiGPU: return Localization.tr("Imported multi-GPU", "导入的多 GPU", "読み込み済みマルチ GPU")
        }
    }

    func includes(_ result: BenchResult) -> Bool {
        switch self {
        case .all:
            return true
        case .single:
            return !result.headless &&
                result.workloadConfigValue("multiGpu") == nil &&
                result.workloadVersion?.contains("_afr2") != true &&
                result.workloadVersion?.contains("_sfr2") != true
        case .headless:
            return result.headless
        case .multiGPU:
            return result.workloadConfigValue("multiGpu") != nil ||
                result.workloadVersion?.contains("_afr2") == true ||
                result.workloadVersion?.contains("_sfr2") == true
        }
    }
}

private struct BenchmarkChartRow: Identifiable {
    let id: String
    let label: String
    let api: String
    let value: Double
    let unit: String
    let timestamp: String
}

struct ChartsView: View {
    @EnvironmentObject var engine: BenchEngine

    @State private var selectedWorkload = "stream"
    @State private var selectedAPI = "All"
    @State private var selectedMode: ExecutionModeFilter = .single
    @State private var selectedMetric: BenchmarkChartMetric = .score
    @State private var particleFilter: UInt32 = 0

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(spacing: 12) {
                Text(Localization.tr("Charts", "图表", "チャート"))
                    .font(.largeTitle)
                    .fontWeight(.bold)

                Button {
                    engine.refreshResults()
                } label: {
                    Label(
                        Localization.tr("Refresh", "刷新", "更新"),
                        systemImage: "arrow.clockwise")
                }

                Spacer()

                Button(Localization.tr(
                    "Open results folder",
                    "打开结果文件夹",
                    "結果フォルダを開く")) {
                    engine.openResultsFolder()
                }
            }

            filterCard

            if mixesParticleSizes {
                Label {
                    Text(Localization.tr(
                        "\"All sizes\" charts different working sets together. A small particle buffer sits in cache and reads far above memory bandwidth, so those bars are not comparable with the large ones. Pick a single size to compare fairly.",
                        "「所有档位」会把不同大小的工作集画在一起。小粒子缓冲装在缓存里，读数远高于内存带宽，与大档位的柱子不可比。要公平比较请选定单一档位。",
                        "「すべてのサイズ」は異なるワーキングセットを同時に描画します。小さいバッファはキャッシュに収まり比較できません。"))
                        .fixedSize(horizontal: false, vertical: true)
                } icon: {
                    Image(systemName: "exclamationmark.triangle.fill")
                        .foregroundStyle(.orange)
                }
                .font(.caption)
                .foregroundStyle(.secondary)
            }

            // A tall chart (up to 24 rows) has to scroll rather than push the
            // page out of the pane.
            ScrollView {
                VStack(alignment: .leading, spacing: 12) {
                    if chartRows.isEmpty {
                        emptyState
                    } else {
                        summaryStrip
                        chartCard
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(.bottom, 4)
            }
        }
        .padding(28)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .onAppear {
            engine.refreshResults()
            repairSelections()
        }
        .onChange(of: engine.results) { _ in repairSelections() }
        .onChange(of: selectedWorkload) { _ in particleFilter = 0 }
    }

    private var filterCard: some View {
        GlassCard(padding: 14) {
            FormRow {
                FormField(
                    title: Localization.tr("Workload", "负载", "ワークロード"),
                    width: 190
                ) {
                    Picker("", selection: $selectedWorkload) {
                        if workloads.isEmpty {
                            Text(Localization.tr("No results yet", "暂无成绩", "結果なし"))
                                .tag("")
                        }
                        ForEach(workloads, id: \.self) { workload in
                            Text(workloadLabel(workload)).tag(workload)
                        }
                    }
                }

                FormField(title: "API", width: 140) {
                    Picker("", selection: $selectedAPI) {
                        Text(Localization.tr("All APIs", "所有 API", "すべての API"))
                            .tag("All")
                        ForEach(apis, id: \.self) { api in
                            Text(api).tag(api)
                        }
                    }
                }

                FormField(
                    title: Localization.tr("Mode", "模式", "モード"),
                    width: 170
                ) {
                    Picker("", selection: $selectedMode) {
                        ForEach(ExecutionModeFilter.allCases) { mode in
                            Text(mode.label).tag(mode)
                        }
                    }
                }

                FormField(
                    title: Localization.tr("Metric", "指标", "指標"),
                    width: 140
                ) {
                    Picker("", selection: $selectedMetric) {
                        ForEach(BenchmarkChartMetric.allCases) { metric in
                            Text(metric.label).tag(metric)
                        }
                    }
                }

                if !particleCounts.isEmpty {
                    FormField(
                        title: Localization.tr("Particles", "粒子数", "パーティクル数"),
                        width: 150
                    ) {
                        Picker("", selection: $particleFilter) {
                            Text(Localization.tr("All sizes", "所有档位", "すべてのサイズ"))
                                .tag(UInt32(0))
                            ForEach(particleCounts, id: \.self) { count in
                                Text(particleLabel(count)).tag(count)
                            }
                        }
                    }
                }
            }
        }
    }

    private var emptyState: some View {
        GlassCard {
            VStack(spacing: 12) {
                Image(systemName: "chart.bar.xaxis")
                    .font(.system(size: 46))
                    .foregroundStyle(.secondary)
                Text(Localization.tr(
                    "No comparable results match these filters.",
                    "没有符合当前筛选条件的可比较成绩。",
                    "このフィルタに一致する比較可能な結果はありません。"))
                    .font(.headline)
                Text(Localization.tr(
                    "Run a benchmark or choose a different workload, API, mode, or metric.",
                    "请先运行测试，或选择其他负载、API、模式或指标。",
                    "ベンチマークを実行するか、別のワークロード/API/モード/指標を選択してください。"))
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .multilineTextAlignment(.center)
            }
            .frame(maxWidth: .infinity)
            .padding(42)
        }
    }

    private var summaryStrip: some View {
        HStack(spacing: 12) {
            summaryTile(
                title: Localization.tr("Results", "成绩", "結果"),
                value: "\(chartRows.count)",
                icon: "list.number")
            summaryTile(
                title: Localization.tr("Devices", "设备", "デバイス"),
                value: "\(Set(chartRows.map(\.label)).count)",
                icon: "display")
            summaryTile(
                title: Localization.tr("Best", "最佳", "ベスト"),
                value: bestValueText,
                icon: selectedMetric.prefersLowerValue
                    ? "arrow.down.circle.fill"
                    : "arrow.up.circle.fill")
        }
    }

    private var chartCard: some View {
        GlassCard(padding: 14) {
            VStack(alignment: .leading, spacing: 10) {
                HStack {
                    Text("\(workloadLabel(selectedWorkload)) · \(selectedMetric.label)")
                        .font(.headline)
                    Spacer()
                    Text(Localization.tr(
                        "Best result per configuration · up to 24 rows",
                        "每种配置的最佳成绩 · 最多 24 行",
                        "構成ごとのベスト · 最大 24 行"))
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                }

                if #available(macOS 13.0, *) {
                    NativeBenchmarkChart(
                        rows: chartRows,
                        metric: selectedMetric)
                } else {
                    MontereyBenchmarkBars(
                        rows: chartRows,
                        metric: selectedMetric)
                }
            }
        }
    }

    private var workloads: [String] {
        Array(Set(engine.results.map(\.workload))).sorted()
    }

    private var apis: [String] {
        Array(Set(engine.results
            .filter { selectedWorkload.isEmpty || $0.workload == selectedWorkload }
            .map(\.graphicsApi))).sorted()
    }

    private var particleCounts: [UInt32] {
        Array(Set(engine.results
            .filter { $0.workload == selectedWorkload && $0.particleCount > 0 }
            .map(\.particleCount))).sorted()
    }

    private var chartRows: [BenchmarkChartRow] {
        let candidates = engine.results.filter { result in
            result.workload == selectedWorkload &&
                (selectedAPI == "All" || result.graphicsApi == selectedAPI) &&
                selectedMode.includes(result) &&
                (particleFilter == 0 || result.particleCount == particleFilter) &&
                selectedMetric.value(for: result).isFinite &&
                selectedMetric.value(for: result) > 0 &&
                !(selectedMetric == .score && result.hidesImplausibleLegacyScore)
        }

        var bestByConfiguration: [String: BenchResult] = [:]
        for result in candidates {
            let configuration = configurationKey(result)
            guard let current = bestByConfiguration[configuration] else {
                bestByConfiguration[configuration] = result
                continue
            }
            let newValue = selectedMetric.value(for: result)
            let currentValue = selectedMetric.value(for: current)
            let isBetter = selectedMetric.prefersLowerValue
                ? newValue < currentValue
                : newValue > currentValue
            if isBetter { bestByConfiguration[configuration] = result }
        }

        return bestByConfiguration.values
            .map { result in
                BenchmarkChartRow(
                    id: result.id,
                    label: configurationLabel(result),
                    api: result.graphicsApi,
                    value: selectedMetric.value(for: result),
                    unit: selectedMetric == .score ? result.scoreUnit : "",
                    timestamp: result.timestamp)
            }
            .sorted {
                if $0.value == $1.value { return $0.label < $1.label }
                return selectedMetric.prefersLowerValue
                    ? $0.value < $1.value
                    : $0.value > $1.value
            }
            .prefix(24)
            .map { $0 }
    }

    private var bestValueText: String {
        guard let first = chartRows.first else { return "—" }
        return selectedMetric.formattedValue(first.value, unit: first.unit)
    }

    private func configurationKey(_ result: BenchResult) -> String {
        [
            result.graphicsApi,
            result.deviceName,
            result.executionMode,
            result.particleCount > 0 ? "\(result.particleCount)" : "",
            result.workloadConfigValue("steps") ?? "",
            result.precision,
        ].joined(separator: "\u{1f}")
    }

    private func configurationLabel(_ result: BenchResult) -> String {
        var details: [String] = [result.graphicsApi, result.deviceName, result.executionMode]
        if particleFilter == 0, result.particleCount > 0 {
            details.append(particleLabel(result.particleCount))
        }
        if let steps = result.workloadConfigValue("steps"), !steps.isEmpty {
            details.append("\(steps) steps")
        }
        if !result.precision.isEmpty { details.append(result.precision) }
        return details.joined(separator: " · ")
    }

    private func repairSelections() {
        if !workloads.contains(selectedWorkload) {
            selectedWorkload = workloads.contains("stream")
                ? "stream"
                : workloads.first ?? ""
        }
        if selectedAPI != "All" && !apis.contains(selectedAPI) {
            selectedAPI = "All"
        }
        if particleFilter != 0 && !particleCounts.contains(particleFilter) {
            particleFilter = 0
        }
        // Particle sizes are not interchangeable: a 2 MB buffer reports cache
        // bandwidth and a 512 MB one reports memory bandwidth. Default to a
        // single size rather than silently charting both against each other.
        if particleFilter == 0, particleCounts.count > 1 {
            particleFilter = particleCounts.last ?? 0
        }
    }

    /// True when "All sizes" is charting genuinely different working sets.
    private var mixesParticleSizes: Bool {
        particleFilter == 0 && particleCounts.count > 1
    }

    private func workloadLabel(_ workload: String) -> String {
        switch workload {
        case "stream":
            return Localization.tr("Particle", "粒子", "パーティクル")
        case "gpu_burn":
            return "GPU Burn"
        case "cinematic_liquid":
            return Localization.tr("Cinematic Liquid", "互动水池", "Cinematic Liquid")
        case "gpu_stress":
            return "GraphicsBurn"
        case "nbody":
            return "N-Body"
        case "synthpeak":
            return "SynthPeak"
        default:
            return workload.isEmpty ? "—" : workload
        }
    }

    private func particleLabel(_ count: UInt32) -> String {
        // Buffer size is part of the identity here: it decides whether the bar
        // is cache bandwidth or memory bandwidth.
        let mb = Double(count) * 32.0 / (1024.0 * 1024.0)
        let size = mb >= 1
            ? String(format: " · %.0f MB", mb)
            : String(format: " · %.1f MB", mb)
        if count >= 1_048_576 {
            let millions = Double(count) / 1_048_576
            let name = millions.rounded() == millions
                ? "\(Int(millions))M"
                : String(format: "%.1fM", millions)
            return name + size
        }
        if count >= 1024 { return "\(count / 1024)K" + size }
        return "\(count)" + size
    }

    private func summaryTile(title: String, value: String, icon: String) -> some View {
        GlassCard(padding: 12) {
            HStack(spacing: 10) {
                Image(systemName: icon)
                    .foregroundStyle(Color.accentColor)
                VStack(alignment: .leading, spacing: 2) {
                    Text(title)
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                    Text(value)
                        .font(.headline)
                        .lineLimit(1)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }
}

@available(macOS 13.0, *)
private struct NativeBenchmarkChart: View {
    let rows: [BenchmarkChartRow]
    let metric: BenchmarkChartMetric

    var body: some View {
        Chart(rows) { row in
            BarMark(
                x: .value(metric.label, row.value),
                y: .value(
                    Localization.tr("Configuration", "配置", "構成"),
                    row.label))
                .foregroundStyle(by: .value("API", row.api))
                .annotation(position: .trailing, alignment: .leading) {
                    Text(metric.formattedValue(row.value, unit: row.unit))
                        .font(.caption2)
                        .monospacedDigit()
                }
        }
        .chartLegend(position: .top, alignment: .leading)
        .chartXAxisLabel(metric.label)
        .frame(height: max(320, CGFloat(rows.count) * 34))
        .padding(.trailing, 84)
    }
}

private struct MontereyBenchmarkBars: View {
    let rows: [BenchmarkChartRow]
    let metric: BenchmarkChartMetric

    private var maximum: Double {
        rows.map(\.value).max() ?? 1
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            ForEach(rows) { row in
                VStack(alignment: .leading, spacing: 5) {
                    HStack(alignment: .firstTextBaseline) {
                        Text(row.label)
                            .font(.caption)
                            .lineLimit(1)
                        Spacer()
                        Text(metric.formattedValue(row.value, unit: row.unit))
                            .font(.caption)
                            .fontWeight(.semibold)
                            .monospacedDigit()
                    }
                    GeometryReader { geometry in
                        ZStack(alignment: .leading) {
                            Capsule().fill(Color.secondary.opacity(0.12))
                            Capsule()
                                .fill(Color.accentColor)
                                .frame(width: max(
                                    2,
                                    geometry.size.width * row.value / max(maximum, 0.000_001)))
                        }
                    }
                    .frame(height: 9)
                }
            }
        }
        .padding(.vertical, 4)
    }
}
