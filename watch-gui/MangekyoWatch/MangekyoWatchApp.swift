import SwiftUI
import Charts

@main
struct MangekyoWatchApp: App {
    @StateObject private var engine = WatchEngine()
    @Environment(\.scenePhase) private var scenePhase
    var body: some Scene {
        WindowGroup { WatchHome().environmentObject(engine) }
            .onChange(of: scenePhase) { _, phase in
                if phase != .active { engine.cancel() }
            }
    }
}

struct WatchHome: View {
    @EnvironmentObject var engine: WatchEngine
    var body: some View {
        NavigationStack {
            List {
                Section {
                    if engine.running {
                        Button(watchText("停止测试", "Stop test"), role: .destructive) { engine.cancel() }
                            .accessibilityIdentifier("watch-stop")
                        ProgressView(value: engine.progress)
                        Text(engine.stage).font(.caption2)
                    } else {
                        Button { engine.startAll() } label: {
                            Label(watchText("一键测试 CPU 全部项目", "Test all CPU items"), systemImage: "play.fill")
                        }.accessibilityIdentifier("watch-run-all")
                        if let suite = engine.suite {
                            Text("\(engine.finishedJobs)/2 · \(suite.statusText)")
                                .accessibilityIdentifier("watch-suite-status")
                                .accessibilityValue(suite.status)
                        }
                    }
                    if !engine.error.isEmpty { Text(engine.error).font(.caption2).foregroundStyle(.red) }
                }
                NavigationLink { WatchHistory() } label: {
                    Label(watchText("已保存成绩", "Saved results") + " (\(engine.results.count))", systemImage: "chart.xyaxis.line")
                }.accessibilityIdentifier("watch-history")
                Section {
                    if engine.simulator {
                        Text(watchText("模拟器：仅验证流程，分数来自 Mac", "Simulator: flow only; scores use the Mac CPU"))
                            .font(.caption2).foregroundStyle(.orange)
                    }
                    if engine.smoke { Text("UI smoke profile").font(.caption2).foregroundStyle(.orange) }
                    Text(watchText("自动调度，不绑定编号核心。离开前台即停止；抬腕保持前台。", "Scheduler managed; no numbered-core pinning. Keep the app active; leaving the foreground stops the test."))
                        .font(.caption2)
                    Text(watchText("JSON 与日志保存在手表 App 的 Documents/Mangekyo；页面测试测首次访问，存储测试不代表交换内存。", "JSON and logs: app Documents/Mangekyo. Page tests measure first touch; file I/O is not swap performance."))
                        .font(.caption2).foregroundStyle(.secondary)
                }
            }.navigationTitle("Mangekyo CPU")
        }
    }
}

struct WatchHistory: View {
    @EnvironmentObject var engine: WatchEngine
    var body: some View {
        List {
            if engine.results.isEmpty { Text(watchText("暂无成绩", "No results yet")).font(.caption2) }
            ForEach(engine.results.reversed()) { result in
                NavigationLink { WatchResultDetail(result: result) } label: {
                    VStack(alignment: .leading, spacing: 3) {
                        Text(result.title).font(.headline)
                        if result.workload == "cpu_membw", !result.memoryVerified {
                            Text(watchText("查看缓存和内存曲线", "View cache and memory curves")).font(.caption2)
                        } else {
                            Text("\(result.score, specifier: "%.3f") \(result.scoreUnit)").font(.caption)
                        }
                        Text(result.timestamp).font(.caption2).foregroundStyle(.secondary)
                    }
                }.accessibilityIdentifier("watch-result-\(result.workload)")
            }
        }.navigationTitle(watchText("已保存成绩", "Saved results"))
    }
}

struct WatchResultDetail: View {
    let result: WatchResult
    var body: some View {
        List {
            if !result.curves.isEmpty {
                NavigationLink { WatchCurveView(result: result) } label: {
                    Label(watchText("查看曲线", "View curves") + " (\(result.curves.count))", systemImage: "chart.xyaxis.line")
                }.accessibilityIdentifier("watch-open-curves")
            }
            VStack(alignment: .leading, spacing: 4) {
                Text(result.title).font(.headline)
                if result.workload == "cpu_membw", !result.memoryVerified {
                    Text(watchText("读取参考值", "Read reference")).font(.caption2).foregroundStyle(.orange)
                }
                Text("\(result.score, specifier: "%.3f") \(result.scoreUnit)")
                Text(result.isSoftware ? watchText("模拟器：Mac 宿主成绩", "Simulator: Mac host result") : result.deviceModel)
                    .font(.caption2).foregroundStyle(result.isSoftware ? .orange : .secondary)
            }
            ForEach(result.reviewReasons, id: \.self) { Text($0).font(.caption2).foregroundStyle(.orange) }
            ForEach(details, id: \.0) { key, value in
                VStack(alignment: .leading) {
                    Text(key).font(.caption2).foregroundStyle(.secondary)
                    Text(value).font(.caption)
                }
            }
        }.navigationTitle(result.title)
    }
    private var details: [(String, String)] {
        let c = result.config
        var rows = [(watchText("调度", "Scheduling"), c["corePlacement"] ?? "scheduler_managed"),
                    (watchText("全核线程", "All-core threads"), c["threads"] ?? c["allCoreThreads"] ?? "—")]
        for prefix in ["single_L1", "single_L2", "single_L3", "single_L4", "single_memory", "all_memory"] {
            if let read = c[prefix + "_read"] { rows.append((prefix + " read", read + " GB/s")) }
        }
        if result.workload == "cpu_membw" {
            rows += [(watchText("页面首次访问", "First-page touch"), (c["pageFaultNsPerPage"] ?? "—") + " ns/page"),
                     (watchText("页面大小", "Page size"), (c["pageBytes"] ?? "—") + " bytes"),
                     (watchText("顺序读取", "Sequential read"), (c["storageSeqReadMBps"] ?? "—") + " MB/s"),
                     (watchText("顺序写入", "Sequential write"), (c["storageSeqWriteMBps"] ?? "—") + " MB/s"),
                     (watchText("随机 4 KiB 读取", "Random 4 KiB read"), (c["storageRandReadMBps"] ?? "—") + " MB/s"),
                     (watchText("随机 4 KiB 写入", "Random 4 KiB write"), (c["storageRandWriteMBps"] ?? "—") + " MB/s"),
                     (watchText("存储访问", "Storage I/O"), c["storageDirectIo"] == "1" ? "uncached" : "buffered / cache may contribute")]
        }
        return rows
    }
}

struct WatchCurveView: View {
    let result: WatchResult
    var body: some View {
        VStack(alignment: .leading, spacing: 5) {
            Text(watchText("曲线点：", "Curve points: ") + "\(result.curves.count)")
                .font(.caption2).accessibilityIdentifier("watch-curve-count")
            Chart(result.curves) { p in
                LineMark(x: .value("KiB", p.kib), y: .value("GB/s", p.gbps))
                    .foregroundStyle(by: .value("Series", p.series))
            }.chartXScale(type: .log)
            Text(watchText("每线程 KiB / GB/s", "KiB per thread / GB/s")).font(.caption2)
        }.padding(.horizontal, 8)
            .navigationTitle(watchText("缓存曲线", "Cache curves"))
    }
}
