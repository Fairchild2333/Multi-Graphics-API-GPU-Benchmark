// ChartsView.swift — Benchmark comparison charts for iOS.
//
// Mirrors the macOS page: filter to one workload, API, execution mode, metric
// and particle size, keep the best run per configuration, and chart those. The
// previous version plotted every stored result in one figure regardless of
// workload or working-set size, which compares quantities that are not
// comparable — a 2 MB particle buffer reports cache bandwidth and a 512 MB one
// reports memory bandwidth.

import SwiftUI
import Charts

private enum BenchmarkChartMetric: String, CaseIterable, Identifiable {
    case score, fps, gpuTime
    var id: String { rawValue }

    var label: String {
        switch self {
        case .score:   return Localization.tr("Score", "分数", "スコア")
        case .fps:     return "FPS"
        case .gpuTime: return Localization.tr("GPU time", "GPU 时间", "GPU 時間")
        }
    }

    var prefersLowerValue: Bool { self == .gpuTime }

    func value(for r: BenchResult) -> Double {
        switch self {
        case .score:   return r.normalizedScore
        case .fps:     return r.avgFps
        case .gpuTime: return r.avgTotalGpuMs
        }
    }

    func formatted(_ v: Double, unit: String) -> String {
        switch self {
        case .score:   return String(format: "%.1f", v) + (unit.isEmpty ? "" : " \(unit)")
        case .fps:     return String(format: "%.0f FPS", v)
        case .gpuTime: return String(format: "%.3f ms", v)
        }
    }
}

private enum ExecutionModeFilter: String, CaseIterable, Identifiable {
    case all, single, headless
    var id: String { rawValue }

    var label: String {
        switch self {
        case .all:      return Localization.tr("All modes", "所有模式", "すべてのモード")
        case .single:   return Localization.tr("Single GPU", "单 GPU", "単一 GPU")
        case .headless: return "Headless"
        }
    }

    func includes(_ r: BenchResult) -> Bool {
        switch self {
        case .all:      return true
        case .single:   return !r.headless
        case .headless: return r.headless
        }
    }
}

private struct ChartRow: Identifiable {
    let id: String
    let label: String
    let api: String
    let value: Double
    let unit: String
}

struct ChartsView: View {
    @EnvironmentObject var engine: BenchEngine

    @State private var selectedWorkload = "stream"
    @State private var selectedAPI = "All"
    @State private var selectedVersion = ""
    @State private var selectedMode: ExecutionModeFilter = .single
    @State private var selectedMetric: BenchmarkChartMetric = .score
    @State private var particleFilter: UInt32 = 0

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                if engine.results.isEmpty {
                    emptyState
                } else {
                    filterCard
                    if mixesParticleSizes { mixedSizeWarning }
                    if chartRows.isEmpty {
                        noMatchState
                    } else {
                        summaryStrip
                        chartCard
                    }
                }
                SavedResultCharts(results: engine.results.filter { $0.graphicsApi == "CPU" })
            }
            .padding(16)
        }
        .onAppear { engine.refreshResults(); repairSelections() }
        .onChange(of: engine.results) { _ in repairSelections() }
        .onChange(of: selectedVersion) { _ in particleFilter = 0; repairSelections() }
        .onChange(of: selectedMode) { _ in particleFilter = 0; repairSelections() }
        .onChange(of: selectedAPI) { _ in particleFilter = 0; repairSelections() }
        .onChange(of: selectedWorkload) { _ in particleFilter = 0; repairSelections() }
    }

    // MARK: - Cards

    private var filterCard: some View {
        GlassCard {
            VStack(spacing: 10) {
                filterRow(Localization.tr("Workload", "负载", "ワークロード")) {
                    Picker("", selection: $selectedWorkload) {
                        ForEach(workloads, id: \.self) { Text(workloadLabel($0)).tag($0) }
                    }
                }
                filterRow(Localization.tr("Version", "版本", "バージョン")) {
                    Picker("", selection: $selectedVersion) {
                        ForEach(versions, id: \.self) { Text($0.isEmpty ? "—" : $0).tag($0) }
                    }
                }
                filterRow("API") {
                    Picker("", selection: $selectedAPI) {
                        Text(Localization.tr("All APIs", "所有 API", "すべての API")).tag("All")
                        ForEach(apis, id: \.self) { Text($0).tag($0) }
                    }
                }
                filterRow(Localization.tr("Mode", "模式", "モード")) {
                    Picker("", selection: $selectedMode) {
                        ForEach(ExecutionModeFilter.allCases) { Text($0.label).tag($0) }
                    }
                }
                filterRow(Localization.tr("Metric", "指标", "指標")) {
                    Picker("", selection: $selectedMetric) {
                        ForEach(BenchmarkChartMetric.allCases) { Text($0.label).tag($0) }
                    }
                }
                if !particleCounts.isEmpty {
                    filterRow(Localization.tr("Particles", "粒子数", "パーティクル数")) {
                        Picker("", selection: $particleFilter) {
                            Text(Localization.tr("All sizes", "所有档位", "すべてのサイズ"))
                                .tag(UInt32(0))
                            ForEach(particleCounts, id: \.self) {
                                Text(particleLabel($0)).tag($0)
                            }
                        }
                    }
                }
            }
        }
    }

    private func filterRow<C: View>(_ title: String,
                                    @ViewBuilder content: () -> C) -> some View {
        HStack {
            Text(title).foregroundStyle(.secondary)
            Spacer(minLength: 12)
            content().labelsHidden().pickerStyle(.menu)
        }
    }

    private var mixedSizeWarning: some View {
        Label {
            Text(Localization.tr(
                "\"All sizes\" charts different working sets together. A small particle buffer sits in cache and reads far above memory bandwidth, so those bars are not comparable with the large ones.",
                "「所有档位」会把不同大小的工作集画在一起。小粒子缓冲装在缓存里，读数远高于内存带宽，与大档位的柱子不可比。",
                "「すべてのサイズ」は異なるワーキングセットを同時に描画します。小さいバッファはキャッシュに収まり比較できません。"))
                .fixedSize(horizontal: false, vertical: true)
        } icon: {
            Image(systemName: "exclamationmark.triangle.fill").foregroundStyle(.orange)
        }
        .font(.caption)
        .foregroundStyle(.secondary)
    }

    private var summaryStrip: some View {
        HStack(spacing: 10) {
            tile(Localization.tr("Results", "成绩", "結果"), "\(chartRows.count)", "list.number")
            tile(Localization.tr("Devices", "设备", "デバイス"),
                 "\(Set(chartRows.map(\.label)).count)", "iphone")
            tile(Localization.tr("Best", "最佳", "ベスト"), bestValueText,
                 selectedMetric.prefersLowerValue ? "arrow.down.circle.fill"
                                                  : "arrow.up.circle.fill")
        }
    }

    private func tile(_ title: String, _ value: String, _ icon: String) -> some View {
        GlassCard(padding: 12) {
            VStack(alignment: .leading, spacing: 3) {
                Label(title, systemImage: icon)
                    .font(.caption2).foregroundStyle(.secondary).labelStyle(.titleAndIcon)
                Text(value).font(.subheadline.weight(.semibold)).lineLimit(1)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    private var chartCard: some View {
        GlassCard {
            VStack(alignment: .leading, spacing: 10) {
                Text("\(workloadLabel(selectedWorkload)) · \(selectedMetric.label)")
                    .font(.headline)
                Chart(chartRows) { row in
                    BarMark(x: .value(selectedMetric.label, row.value),
                            y: .value(Localization.tr("Configuration", "配置", "構成"),
                                      row.label))
                        .foregroundStyle(by: .value("API", row.api))
                        .annotation(position: .trailing, alignment: .leading) {
                            Text(selectedMetric.formatted(row.value, unit: row.unit))
                                .font(.caption2).monospacedDigit()
                        }
                }
                .chartLegend(position: .top, alignment: .leading)
                .frame(height: max(220, CGFloat(chartRows.count) * 40))
                .padding(.trailing, 60)
                Text(Localization.tr(
                    "Best result per configuration · up to 24 rows",
                    "每种配置的最佳成绩 · 最多 24 行",
                    "構成ごとのベスト · 最大 24 行"))
                    .font(.caption2).foregroundStyle(.secondary)
            }
        }
    }

    private var emptyState: some View { placeholder(
        Localization.tr("No benchmark results yet. Run some tests to see comparison charts.",
                        "尚无测试结果。运行一些测试以查看对比图表。",
                        "ベンチマーク結果がありません。テストを実行してください。")) }

    private var noMatchState: some View { placeholder(
        Localization.tr("No comparable results match these filters.",
                        "没有符合当前筛选条件的可比较成绩。",
                        "このフィルタに一致する比較可能な結果はありません。")) }

    private func placeholder(_ text: String) -> some View {
        GlassCard {
            VStack(spacing: 12) {
                Image(systemName: "chart.bar.xaxis")
                    .font(.system(size: 44)).foregroundStyle(.secondary)
                Text(text).multilineTextAlignment(.center).foregroundStyle(.secondary)
            }
            .frame(maxWidth: .infinity)
            .padding(32)
        }
    }

    // MARK: - Data

    private var workloads: [String] {
        Array(Set(engine.results.map(\.workload))).sorted()
    }
    private var versions: [String] {
        Array(Set(engine.results.filter { $0.workload == selectedWorkload }.map { $0.workloadVersion ?? "" })).sorted()
    }
    private var apis: [String] {
        Array(Set(engine.results
            .filter { $0.workload == selectedWorkload }
            .map(\.graphicsApi))).sorted()
    }
    private var particleCounts: [UInt32] {
        Array(Set(engine.results
             .filter { $0.workload == selectedWorkload && $0.particleCount > 0 &&
                ($0.workloadVersion ?? "") == selectedVersion && selectedMode.includes($0) &&
                (selectedAPI == "All" || $0.graphicsApi == selectedAPI) }
            .map(\.particleCount))).sorted()
    }
    private var mixesParticleSizes: Bool {
        particleFilter == 0 && particleCounts.count > 1
    }

    private var chartRows: [ChartRow] {
        let candidates = engine.results.filter { r in
            r.workload == selectedWorkload &&
                (r.workloadVersion ?? "") == selectedVersion &&
                (selectedAPI == "All" || r.graphicsApi == selectedAPI) &&
                selectedMode.includes(r) &&
                (particleFilter == 0 || r.particleCount == particleFilter) &&
                selectedMetric.value(for: r).isFinite &&
                selectedMetric.value(for: r) > 0
        }
        var best: [String: BenchResult] = [:]
        for r in candidates {
            let key = configurationKey(r)
            guard let cur = best[key] else { best[key] = r; continue }
            let isBetter = selectedMetric.prefersLowerValue
                ? selectedMetric.value(for: r) < selectedMetric.value(for: cur)
                : selectedMetric.value(for: r) > selectedMetric.value(for: cur)
            if isBetter { best[key] = r }
        }
        return best.values
            .map { ChartRow(id: $0.id, label: configurationLabel($0),
                            api: $0.graphicsApi,
                            value: selectedMetric.value(for: $0),
                            unit: selectedMetric == .score ? $0.scoreUnit : "") }
            .sorted {
                if $0.value == $1.value { return $0.label < $1.label }
                return selectedMetric.prefersLowerValue
                    ? $0.value < $1.value : $0.value > $1.value
            }
            .prefix(24)
            .map { $0 }
    }

    private var bestValueText: String {
        guard let first = chartRows.first else { return "—" }
        return selectedMetric.formatted(first.value, unit: first.unit)
    }

    private func configurationKey(_ r: BenchResult) -> String {
        [r.graphicsApi, r.deviceName, r.executionMode, r.workloadVersion ?? "",
         r.particleCount > 0 ? "\(r.particleCount)" : "", r.precision]
            .joined(separator: "\u{1f}")
    }

    private func configurationLabel(_ r: BenchResult) -> String {
        var parts = [r.deviceName, r.executionMode]
        if particleFilter == 0, r.particleCount > 0 {
            parts.append(particleLabel(r.particleCount))
        }
        if !r.precision.isEmpty { parts.append(r.precision) }
        return parts.joined(separator: " · ")
    }

    private func repairSelections() {
        if !workloads.contains(selectedWorkload) {
            selectedWorkload = workloads.contains("stream") ? "stream"
                                                           : workloads.first ?? ""
        }
        if !versions.contains(selectedVersion) {
            selectedVersion = engine.results.filter { $0.workload == selectedWorkload }
                .max(by: { $0.timestamp < $1.timestamp })?.workloadVersion ?? ""
        }
        if selectedAPI != "All" && !apis.contains(selectedAPI) { selectedAPI = "All" }
        if particleFilter != 0 && !particleCounts.contains(particleFilter) {
            particleFilter = 0
        }
        // Sizes are not interchangeable, so default to one rather than silently
        // charting several against each other.
        if particleFilter == 0, particleCounts.count > 1 {
            particleFilter = particleCounts.last ?? 0
        }
    }

    private func workloadLabel(_ w: String) -> String {
        switch w {
        case "stream":           return Localization.tr("Particle", "粒子", "パーティクル")
        case "gpu_burn":         return "GPU Burn"
        case "cinematic_liquid": return Localization.tr("Cinematic Liquid", "互动水池",
                                                        "Cinematic Liquid")
        case "cpu_multi_core":   return Localization.tr("CPU all-core", "CPU 全核",
                                                        "CPU 全コア")
        case "cpu_single_core":  return Localization.tr("CPU single-core", "CPU 单核",
                                                        "CPU 単一コア")
        default:                 return w.isEmpty ? "—" : w
        }
    }

    /// Buffer size is part of the identity: it decides whether the bar is cache
    /// bandwidth or memory bandwidth.
    private func particleLabel(_ count: UInt32) -> String {
        let mb = Double(count) * 32.0 / (1024.0 * 1024.0)
        let size = mb >= 1 ? String(format: " · %.0f MB", mb)
                           : String(format: " · %.1f MB", mb)
        if count >= 1_048_576 {
            let m = Double(count) / 1_048_576
            return (m.rounded() == m ? "\(Int(m))M" : String(format: "%.1fM", m)) + size
        }
        if count >= 1024 { return "\(count / 1024)K" + size }
        return "\(count)" + size
    }
}
