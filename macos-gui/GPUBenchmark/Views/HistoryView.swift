// HistoryView.swift — Saved benchmark results with sorting, filtering, deletion.
// Uses SwiftUI Table for native multi-column display with sorting support.

import SwiftUI

struct HistoryView: View {
    @EnvironmentObject var engine: BenchEngine

    @State private var selection = Set<String>()
    @State private var sortOrder = [KeyPathComparator(\BenchResult.id, order: .reverse)]

    // Filters
    @State private var selectedSort: SortOption = .timeNewest
    @State private var gpuFilter: String = "All"
    @State private var timeRange: TimeRange = .all

    enum SortOption: String, CaseIterable {
        case timeNewest  = "Time (newest)"
        case scoreHigh   = "Score (high→low)"
        case api         = "Graphics API"
        case device      = "GPU / Renderer"
        case workload    = "Workload"
    }

    enum TimeRange: String, CaseIterable {
        case all     = "All"
        case today   = "Today"
        case week    = "Last 7 days"
        case month   = "Last 30 days"
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            // Header
            HStack(spacing: 12) {
                Text(Localization.tr("History", "历史"))
                    .font(.largeTitle)
                    .fontWeight(.bold)

                Button(action: refresh) {
                    Label(Localization.tr("Refresh", "刷新"), systemImage: "arrow.clockwise")
                }

                Button(role: .destructive, action: deleteSelected) {
                    Label(Localization.tr("Delete selected", "删除选中"), systemImage: "trash")
                }
                .disabled(selection.isEmpty)

                Spacer()
            }

            // Filter bar
            HStack(spacing: 16) {
                Picker(Localization.tr("Sort by", "排序"), selection: $selectedSort) {
                    ForEach(SortOption.allCases, id: \.self) { opt in
                        Text(opt.rawValue).tag(opt)
                    }
                }
                .frame(width: 200)

                Picker(Localization.tr("GPU", "GPU"), selection: $gpuFilter) {
                    Text(Localization.tr("All GPUs", "所有 GPU")).tag("All")
                    ForEach(uniqueDevices, id: \.self) { dev in
                        Text(dev).tag(dev)
                    }
                }
                .frame(width: 250)

                Picker(Localization.tr("Time range", "时间范围"), selection: $timeRange) {
                    ForEach(TimeRange.allCases, id: \.self) { r in
                        Text(r.rawValue).tag(r)
                    }
                }
                .frame(width: 160)
            }

            // Results table
            GlassCard(padding: 0) {
                Table(filteredResults, selection: $selection, sortOrder: $sortOrder) {
                    TableColumn("#", value: \.id) { r in
                        Text(String(engine.results.firstIndex(where: { $0.id == r.id }).map { $0 + 1 } ?? 0))
                            .font(.caption).monospacedDigit()
                    }
                    .width(min: 30, ideal: 40, max: 50)

                    TableColumn(Localization.tr("API", "API"), value: \.graphicsApi)
                        .width(min: 50, ideal: 65, max: 80)

                    TableColumn(Localization.tr("Device", "设备"), value: \.deviceName)
                        .width(min: 120, ideal: 200)

                    TableColumn(Localization.tr("Workload", "负载"), value: \.workload)
                        .width(min: 60, ideal: 80, max: 100)

                    TableColumn(Localization.tr("Difficulty", "难度"), value: \.difficulty)
                        .width(min: 60, ideal: 80, max: 100)

                    TableColumn("FPS") { r in
                        Text(String(format: "%.0f", r.avgFps))
                            .monospacedDigit()
                    }
                    .width(min: 50, ideal: 70, max: 90)

                    TableColumn("GPU ms") { r in
                        Text(r.avgTotalGpuMs > 0
                             ? String(format: "%.3f", r.avgTotalGpuMs)
                             : "N/A")
                            .monospacedDigit()
                    }
                    .width(min: 60, ideal: 80, max: 100)

                    TableColumn(Localization.tr("Score", "分数")) { r in
                        Text(r.score > 0
                             ? String(format: "%.1f %@", r.score, r.scoreUnit)
                             : "—")
                            .monospacedDigit()
                    }
                    .width(min: 80, ideal: 120)

                    TableColumn(Localization.tr("Time", "时间"), value: \.timestamp) { r in
                        Text(formatTimestamp(r.id))
                            .font(.caption)
                    }
                    .width(min: 100, ideal: 140)
                }
                .tableStyle(.inset(alternatesRowBackgrounds: true))
            }
        }
        .padding(28)
        .onAppear { refresh() }
    }

    // MARK: - Helpers

    private var uniqueDevices: [String] {
        Array(Set(engine.results.map(\.deviceName))).sorted()
    }

    private var filteredResults: [BenchResult] {
        var r = engine.results

        // GPU filter
        if gpuFilter != "All" {
            r = r.filter { $0.deviceName == gpuFilter }
        }

        // Time range filter
        let now = Date()
        switch timeRange {
        case .today:
            r = r.filter { $0.date.map { Calendar.current.isDateInToday($0) } ?? false }
        case .week:
            r = r.filter { $0.date.map { now.timeIntervalSince($0) < 7 * 86400 } ?? false }
        case .month:
            r = r.filter { $0.date.map { now.timeIntervalSince($0) < 30 * 86400 } ?? false }
        case .all:
            break
        }

        // Sort
        switch selectedSort {
        case .timeNewest: r.sort { $0.id > $1.id }
        case .scoreHigh:  r.sort { $0.avgFps > $1.avgFps }
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
        for id in selection {
            _ = engine.deleteResult(id: id)
        }
        selection.removeAll()
    }

    private func formatTimestamp(_ id: String) -> String {
        guard id.count >= 15 else { return id }
        let idx = id.index(id.startIndex, offsetBy: 15)
        let s = String(id[..<idx])
        // "20260627-101245" → "2026-06-27 10:12:45"
        guard s.count == 15 else { return id }
        let y = s.prefix(4), m = s.dropFirst(4).prefix(2),
            d = s.dropFirst(6).prefix(2), H = s.dropFirst(9).prefix(2),
            M = s.dropFirst(11).prefix(2), S = s.dropFirst(13).prefix(2)
        return "\(y)-\(m)-\(d) \(H):\(M):\(S)"
    }
}
