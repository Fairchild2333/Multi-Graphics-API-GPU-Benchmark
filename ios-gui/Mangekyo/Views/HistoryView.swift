// HistoryView.swift — iOS adaptive saved benchmark results list.

import SwiftUI

struct HistoryView: View {
    @EnvironmentObject var engine: BenchEngine

    @State private var selection = Set<String>()
    @State private var editMode: EditMode = .inactive
    @State private var selectedSort: SortOption = .timeNewest
    @State private var gpuFilter: String = "All"
    @State private var apiFilter: String = "All"
    @State private var workloadFilter: String = "All"
    @State private var timeRange: TimeRange = .all
    @State private var confirmClear = false

    enum SortOption: String, CaseIterable {
        case timeNewest, scoreHigh, api, device, workload
        var label: String {
            switch self {
            case .timeNewest: return Localization.tr("Time (newest)", "时间（最新）", "時間（新しい順）")
            case .scoreHigh:  return Localization.tr("Score (high→low)", "分数（高→低）", "スコア（高→低）")
            case .api:        return Localization.tr("Graphics API", "图形 API", "グラフィックス API")
            case .device:     return Localization.tr("GPU / Renderer", "GPU / 渲染器", "GPU / レンダラ")
            case .workload:   return Localization.tr("Workload", "负载", "ワークロード")
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
            HStack(spacing: 8) {
                Button(action: refresh) {
                    Label(Localization.tr("Refresh", "刷新", "更新"), systemImage: "arrow.clockwise")
                }
                
                if editMode == .active {
                    Button(role: .destructive, action: deleteSelected) {
                        Label(Localization.tr("Delete", "删除", "削除"), systemImage: "trash")
                    }
                    .disabled(selection.isEmpty)
                } else {
                    Button(role: .destructive) { confirmClear = true } label: {
                        Label(Localization.tr("Clear all", "清空全部", "すべて消去"), systemImage: "trash.fill")
                    }
                    .disabled(engine.results.isEmpty)
                }

                Spacer()
                
                EditButton()
            }
            .padding(.horizontal, 16)
            .padding(.top, 8)

            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 8) {
                    Picker(Localization.tr("Sort by", "排序", "並べ替え"), selection: $selectedSort) {
                        ForEach(SortOption.allCases, id: \.self) { opt in
                            Text(opt.label).tag(opt)
                        }
                    }
                    .pickerStyle(.menu)

                    Picker("GPU", selection: $gpuFilter) {
                        Text(Localization.tr("All GPUs", "所有 GPU", "すべての GPU")).tag("All")
                        ForEach(uniqueDevices, id: \.self) { Text($0).tag($0) }
                    }
                    .pickerStyle(.menu)

                    Picker("API", selection: $apiFilter) {
                        Text(Localization.tr("All APIs", "所有 API", "すべての API")).tag("All")
                        ForEach(uniqueApis, id: \.self) { Text($0).tag($0) }
                    }
                    .pickerStyle(.menu)

                    Picker(Localization.tr("Workload", "负载", "ワークロード"), selection: $workloadFilter) {
                        Text(Localization.tr("All workloads", "所有负载", "すべてのワークロード")).tag("All")
                        ForEach(uniqueWorkloads, id: \.self) { Text($0).tag($0) }
                    }
                    .pickerStyle(.menu)

                    Picker(Localization.tr("Time range", "时间范围", "期間"), selection: $timeRange) {
                        ForEach(TimeRange.allCases, id: \.self) { Text($0.label).tag($0) }
                    }
                    .pickerStyle(.menu)
                }
                .padding(.horizontal, 16)
            }

            if filteredResults.isEmpty {
                VStack(spacing: 12) {
                    Spacer()
                    Image(systemName: "clock.badge.exclamationmark")
                        .font(.largeTitle)
                        .foregroundStyle(.secondary)
                    Text(Localization.tr("No history found", "未找到历史记录", "履歴がありません"))
                        .foregroundStyle(.secondary)
                    Spacer()
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else {
                List(filteredResults, selection: $selection) { result in
                    HistoryRow(result: result, index: engine.results.firstIndex(where: { $0.id == result.id }).map { $0 + 1 } ?? 0)
                        .swipeActions(edge: .trailing, allowsFullSwipe: true) {
                            Button(role: .destructive) {
                                _ = engine.deleteResult(id: result.id)
                            } label: {
                                Label(Localization.tr("Delete", "删除", "削除"), systemImage: "trash")
                            }
                        }
                }
                .listStyle(.insetGrouped)
                .environment(\.editMode, $editMode)
            }
        }
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

    private var uniqueDevices: [String] {
        Array(Set(engine.results.map(\.deviceName))).sorted()
    }
    private var uniqueApis: [String] {
        Array(Set(engine.results.map(\.graphicsApi))).sorted()
    }
    private var uniqueWorkloads: [String] {
        Array(Set(engine.results.map(\.workload))).sorted()
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
        case .scoreHigh:  r.sort { $0.score > $1.score }
        case .api:        r.sort { $0.graphicsApi < $1.graphicsApi }
        case .device:     r.sort { $0.deviceName < $1.deviceName }
        case .workload:   r.sort { $0.workload < $1.workload }
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
        editMode = .inactive
    }
}

struct HistoryRow: View {
    let result: BenchResult
    let index: Int

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text("#\(index)")
                    .font(.caption).bold().monospacedDigit()
                    .foregroundStyle(.secondary)
                
                Text(result.workload.capitalized)
                    .font(.headline)
                
                Spacer()
                
                Text(result.graphicsApi.uppercased())
                    .font(.caption).bold()
                    .padding(.horizontal, 6)
                    .padding(.vertical, 2)
                    .background(Color.accentColor.opacity(0.12))
                    .cornerRadius(4)
            }
            
            HStack {
                Text(result.deviceName)
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
                
                Spacer()
                
                if result.score > 0 {
                    Text(String(format: "%.1f %@", result.score, result.scoreUnit))
                        .font(.headline)
                        .foregroundStyle(Color.accentColor)
                } else {
                    Text(String(format: "%.0f FPS", result.avgFps))
                        .font(.subheadline)
                }
            }
            
            HStack {
                Text(formatTimestamp(result.id))
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                
                Spacer()
                
                Text("GPU: \(String(format: "%.2f", result.avgTotalGpuMs))ms | Diff: \(result.difficulty)")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
            }
        }
        .padding(.vertical, 4)
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
