// HistoryView.swift — Saved benchmark results (WinUI-aligned filters + folders).

import SwiftUI

struct HistoryView: View {
    @EnvironmentObject var engine: BenchEngine

    @State private var selection = Set<String>()
    @State private var sortOrder = [KeyPathComparator(\BenchResult.id, order: .reverse)]
    @State private var selectedSort: SortOption = .timeNewest
    @State private var gpuFilter: String = "All"
    @State private var apiFilter: String = "All"
    @State private var workloadFilter: String = "All"
    @State private var timeRange: TimeRange = .all
    @State private var confirmClear = false

    enum SortOption: String, CaseIterable {
        case timeNewest, scoreHigh, api, device, workload, mode
        var label: String {
            switch self {
            case .timeNewest: return Localization.tr("Time (newest)", "时间（最新）", "時間（新しい順）")
            case .scoreHigh:  return Localization.tr("Score (high→low)", "分数（高→低）", "スコア（高→低）")
            case .api:        return Localization.tr("Graphics API", "图形 API", "グラフィックス API")
            case .device:     return Localization.tr("GPU / Renderer", "GPU / 渲染器", "GPU / レンダラ")
            case .workload:   return Localization.tr("Workload", "负载", "ワークロード")
            case .mode:       return Localization.tr("Execution mode", "运行模式", "実行モード")
            }
        }
    }

    enum TimeRange: String, CaseIterable {
        case all, today, week, month
        var label: String {
            switch self {
            case .all:   return Localization.tr("All", "全部", "すべて")
            case .today: return Localization.tr("Today", "今天", "今日")
            case .week:  return Localization.tr("Last 7 days", "近 7 天", "過去 7 日")
            case .month: return Localization.tr("Last 30 days", "近 30 天", "過去 30 日")
            }
        }
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text(Localization.tr("History", "历史", "履歴"))
                .font(.largeTitle).fontWeight(.bold)

            // Scrolls sideways rather than reporting a minimum width, so this
            // page does not force the window wider than any other page.
            horizontalStrip {
                HStack(spacing: 12) {
                    Button(action: refresh) {
                        Label(Localization.tr("Refresh", "刷新", "更新"), systemImage: "arrow.clockwise")
                    }
                    Button(role: .destructive, action: deleteSelected) {
                        Label(Localization.tr("Delete selected", "删除选中", "選択を削除"), systemImage: "trash")
                    }
                    .disabled(selection.isEmpty)

                    Button(role: .destructive) { confirmClear = true } label: {
                        Label(Localization.tr("Clear all", "清空全部", "すべて消去"), systemImage: "trash.fill")
                    }
                    .disabled(engine.results.isEmpty)

                    Divider().frame(height: 16)

                    Button(Localization.tr("Open results folder", "打开结果文件夹", "結果フォルダを開く")) {
                        engine.openResultsFolder()
                    }
                    Button(Localization.tr("Open captures folder", "打开抓帧文件夹", "キャプチャフォルダを開く")) {
                        engine.openCapturesFolder()
                    }
                }
            }

            FormRow(spacing: 14) {
                FormField(
                    title: Localization.tr("Sort by", "排序", "並べ替え"),
                    width: 180
                ) {
                    Picker("", selection: $selectedSort) {
                        ForEach(SortOption.allCases, id: \.self) { opt in
                            Text(opt.label).tag(opt)
                        }
                    }
                }

                FormField(title: "GPU", width: 210) {
                    Picker("", selection: $gpuFilter) {
                        Text(Localization.tr("All GPUs", "所有 GPU", "すべての GPU")).tag("All")
                        ForEach(uniqueDevices, id: \.self) { Text($0).tag($0) }
                    }
                }

                FormField(title: "API", width: 130) {
                    Picker("", selection: $apiFilter) {
                        Text(Localization.tr("All APIs", "所有 API", "すべての API")).tag("All")
                        ForEach(uniqueApis, id: \.self) { Text($0).tag($0) }
                    }
                }

                FormField(
                    title: Localization.tr("Workload", "负载", "ワークロード"),
                    width: 175
                ) {
                    Picker("", selection: $workloadFilter) {
                        Text(Localization.tr("All workloads", "所有负载", "すべてのワークロード")).tag("All")
                        ForEach(uniqueWorkloads, id: \.self) { Text($0).tag($0) }
                    }
                }

                FormField(
                    title: Localization.tr("Time range", "时间范围", "期間"),
                    width: 150
                ) {
                    Picker("", selection: $timeRange) {
                        ForEach(TimeRange.allCases, id: \.self) { Text($0.label).tag($0) }
                    }
                }
            }

            if hiddenLegacyScoreCount > 0 {
                Label(
                    Localization.tr(
                        "\(hiddenLegacyScoreCount) implausible legacy bandwidth score(s) are shown as — and excluded from score sorting.",
                        "\(hiddenLegacyScoreCount) 条异常旧版带宽成绩显示为 —，且不参与分数排序。",
                        "\(hiddenLegacyScoreCount) 件の不自然な旧帯域スコアは — と表示し、スコア順から除外しています。"),
                    systemImage: "exclamationmark.triangle")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            GlassCard(padding: 0) {
                Table(filteredResults, selection: $selection, sortOrder: $sortOrder) {
                    TableColumn("#", value: \.id) { r in
                        Text(String(engine.results.firstIndex(where: { $0.id == r.id }).map { $0 + 1 } ?? 0))
                            .font(.caption).monospacedDigit()
                    }
                    .width(min: 28, ideal: 40, max: 50)

                    TableColumn(Localization.tr("API", "API", "API"), value: \.graphicsApi)
                        .width(min: 44, ideal: 65, max: 80)

                    TableColumn(Localization.tr("Device", "设备", "デバイス"), value: \.deviceName)
                        .width(min: 90, ideal: 180)

                    TableColumn(Localization.tr("Workload", "负载", "ワークロード"), value: \.workload)
                        .width(min: 60, ideal: 90, max: 110)

                    TableColumn(Localization.tr("Mode", "模式", "モード")) { r in
                        Text(r.executionMode)
                    }
                    .width(min: 60, ideal: 100, max: 150)

                    TableColumn(Localization.tr("Version", "版本", "バージョン")) { r in
                        Text(r.workloadVersion ?? "—")
                            .font(.caption2)
                            .lineLimit(1)
                    }
                    .width(min: 80, ideal: 160)

                    TableColumn("FPS") { r in
                        Text(String(format: "%.0f", r.avgFps)).monospacedDigit()
                    }
                    .width(min: 44, ideal: 70, max: 90)

                    TableColumn("GPU ms") { r in
                        Text(r.avgTotalGpuMs > 0
                             ? String(format: "%.3f", r.avgTotalGpuMs) : "N/A")
                            .monospacedDigit()
                    }
                    .width(min: 56, ideal: 80, max: 100)

                    TableColumn(Localization.tr("Score", "分数", "スコア")) { r in
                        Text(r.scoreDisplay)
                            .monospacedDigit()
                    }
                    .width(min: 70, ideal: 120)

                    TableColumn(
                        Localization.tr("Time", "时间", "時間"),
                        value: \.timestamp)
                    .width(min: 90, ideal: 140)
                }
                .tableStyle(.inset(alternatesRowBackgrounds: true))
            }
        }
        .padding(24)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .onAppear { refresh() }
        .confirmationDialog(
            Localization.tr("Clear all results?", "清空全部结果？", "すべての結果を消去しますか？"),
            isPresented: $confirmClear
        ) {
            Button(Localization.tr("Clear all", "清空全部", "すべて消去"), role: .destructive) {
                _ = engine.clearAllResults()
            }
            Button(Localization.tr("Cancel", "取消", "キャンセル"), role: .cancel) {}
        }
    }

    /// Keeps a wide control strip from forcing a minimum width on the pane.
    @ViewBuilder
    private func horizontalStrip<Content: View>(
        @ViewBuilder content: () -> Content
    ) -> some View {
        ScrollView(.horizontal, showsIndicators: false) {
            content()
                .padding(.vertical, 2)
                .padding(.trailing, 4)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }

    private var uniqueDevices: [String] {
        Array(Set(engine.results.map(\.deviceName))).sorted()
    }
    private var uniqueApis: [String] {
        Array(Set(engine.results.map(\.graphicsApi))).sorted()
    }
    private var uniqueWorkloads: [String] {
        Array(Set(engine.results.map(\.workload))).sorted()
    }
    private var hiddenLegacyScoreCount: Int {
        engine.results.lazy.filter(\.hidesImplausibleLegacyScore).count
    }

    private var filteredResults: [BenchResult] {
        var r = engine.results
        if gpuFilter != "All" { r = r.filter { $0.deviceName == gpuFilter } }
        if apiFilter != "All" { r = r.filter { $0.graphicsApi == apiFilter } }
        if workloadFilter != "All" { r = r.filter { $0.workload == workloadFilter } }

        let now = Date()
        switch timeRange {
        case .today:
            r = r.filter { $0.date.map { Calendar.current.isDateInToday($0) } ?? false }
        case .week:
            r = r.filter { $0.date.map { now.timeIntervalSince($0) < 7 * 86400 } ?? false }
        case .month:
            r = r.filter { $0.date.map { now.timeIntervalSince($0) < 30 * 86400 } ?? false }
        case .all: break
        }

        switch selectedSort {
        case .timeNewest: r.sort { $0.id > $1.id }
        case .scoreHigh:  r.sort {
            if $0.normalizedScore == $1.normalizedScore { return $0.id > $1.id }
            return $0.normalizedScore > $1.normalizedScore
        }
        case .api:        r.sort { $0.graphicsApi < $1.graphicsApi }
        case .device:     r.sort { $0.deviceName < $1.deviceName }
        case .workload:   r.sort { $0.workload < $1.workload }
        case .mode:       r.sort {
            if $0.executionMode == $1.executionMode { return $0.id > $1.id }
            return $0.executionMode < $1.executionMode
        }
        }
        return r
    }

    private func refresh() {
        engine.refreshResults()
        selection.removeAll()
    }

    private func deleteSelected() {
        for id in selection { _ = engine.deleteResult(id: id) }
        selection.removeAll()
    }

    private func formatTimestamp(_ id: String) -> String {
        guard id.count >= 15 else { return id }
        let idx = id.index(id.startIndex, offsetBy: 15)
        let s = String(id[..<idx])
        guard s.count == 15 else { return id }
        let y = s.prefix(4), m = s.dropFirst(4).prefix(2),
            d = s.dropFirst(6).prefix(2), H = s.dropFirst(9).prefix(2),
            M = s.dropFirst(11).prefix(2), S = s.dropFirst(13).prefix(2)
        return "\(y)-\(m)-\(d) \(H):\(M):\(S)"
    }
}
