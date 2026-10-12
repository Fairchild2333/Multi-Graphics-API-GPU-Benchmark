// ResultCharts.swift — Result tables and charts of the GPU, CPU and Charts pages.
//
// Each view takes parsed engine output (EngineOutput.swift) or saved results
// and shows the same figures as the Windows and Android apps.

import SwiftUI

// MARK: - GPU page

/// Charts of the GPU runs that just finished: the scores side by side when the
/// jobs report one measure, and the GPU time per frame of every run.
struct GpuRunChartsCard: View {
    let jobs: [GpuJob]

    private struct Timing: Identifiable {
        let id: Int
        let title: String
        let series: [PlotSeries]
    }

    private var timings: [Timing] {
        let computeLabel = Localization.tr("Compute", "计算", "コンピュート")
        let renderLabel = Localization.tr("Render", "渲染", "レンダー")
        return jobs.compactMap { job -> Timing? in
            let measured = job.samples.filter { !$0.warmup }
            let candidates = [
                PlotSeries(label: computeLabel, slot: 0,
                           points: measured.map { PlotPoint(x: $0.seconds, y: $0.computeMs) }),
                PlotSeries(label: renderLabel, slot: 1,
                           points: measured.map { PlotPoint(x: $0.seconds, y: $0.renderMs) }),
            ]
            // A pass that takes no measurable time is left out.
            let series = candidates.filter { item in
                item.points.count >= 2 && (item.points.map { $0.y }.max() ?? 0) > 0.0005
            }
            return series.isEmpty ? nil : Timing(id: job.id, title: job.label, series: series)
        }
    }

    /// The scores of the jobs, when they report one measure in one unit.
    private var scoreBars: [BarItem] {
        var items: [BarItem] = []
        var metric = ""
        var unit = ""
        for job in jobs {
            guard let score = job.score else { continue }
            if items.isEmpty {
                metric = score.name
                unit = score.unit
            } else if score.name != metric || score.unit != unit {
                return []
            }
            items.append(BarItem(id: job.id, label: job.label.isEmpty ? "#\(job.id + 1)" : job.label,
                                 value: score.value))
        }
        return items.count >= 2 ? items : []
    }

    private var scoreUnit: String {
        for job in jobs {
            if let score = job.score { return score.unit.isEmpty ? score.name : score.unit }
        }
        return ""
    }

    private func timingCaption(_ timing: Timing) -> String {
        let base = Localization.tr("GPU time per frame (ms) over the run, after warm-up",
                                   "每帧 GPU 耗时（ms）随运行时间变化，不含预热",
                                   "フレームあたりの GPU 時間（ms）の推移（ウォームアップ後）")
        // One line has no legend, so the caption names it.
        if timing.series.count == 1 { return timing.series[0].label + " · " + base }
        return base
    }

    var body: some View {
        let charts = timings
        let bars = scoreBars
        if !charts.isEmpty || !bars.isEmpty {
            GlassCard(padding: 16) {
                VStack(alignment: .leading, spacing: 12) {
                    Text(Localization.tr("Charts of this run", "本次运行的图表", "この実行のチャート"))
                        .fontWeight(.semibold)
                    if jobs.contains(where: { $0.displayLimited }) {
                        Text(Localization.tr(
                            "The frame rate was held at the refresh rate of the display, so the GPU was not kept "
                                + "busy and the score may be too low. Run a heavier test.",
                            "帧率被屏幕刷新率限制住了，GPU 没有跑满，成绩可能偏低、不准确。请换负载更高的测试。",
                            "フレームレートがディスプレイのリフレッシュレートで頭打ちになり、GPU に十分な負荷がかかっていないため、スコアが低く出ている可能性があります。より負荷の高いテストを実行してください。"))
                            .font(.caption)
                            .foregroundStyle(Color.orange)
                    }
                    if !bars.isEmpty {
                        VStack(alignment: .leading, spacing: 6) {
                            Text(scoreUnit + Localization.tr(", score of every job, higher is better",
                                                             "，各项的成绩，越高越好",
                                                             "、各ジョブのスコア、高いほど良い"))
                                .font(.caption)
                                .foregroundStyle(.secondary)
                            BarRowsView(items: bars, unit: scoreUnit)
                        }
                    }
                    LazyVGrid(columns: [GridItem(.adaptive(minimum: 300), spacing: 20, alignment: .top)],
                              alignment: .leading, spacing: 16) {
                        ForEach(charts) { chart in
                            VStack(alignment: .leading, spacing: 4) {
                                if !chart.title.isEmpty {
                                    Text(chart.title)
                                        .font(.subheadline)
                                        .fontWeight(.medium)
                                }
                                Text(timingCaption(chart))
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                                LinePlotView(series: chart.series, logBytes: false)
                            }
                        }
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading)
            }
        }
    }
}

// MARK: - CPU page, compute test

/// The score of every logical processor as one bar each, with the list below.
struct CpuComputeResultsCard: View {
    let output: RunOutput

    private func rowText(_ score: CoreScore) -> String {
        let value = "\(EngineFormat.number(score.score)) \(score.unit)"
        if score.label.isEmpty { return "#\(score.core + 1)  ·  \(value)" }
        return "#\(score.core + 1)  \(score.label)  ·  \(value)"
    }

    private var title: String {
        output.schedulerManaged
            ? Localization.tr("One-thread runs", "单线程各次运行的成绩", "1 スレッド実行ごとのスコア")
            : Localization.tr("Score of every logical processor", "各逻辑处理器的成绩", "論理プロセッサ別スコア")
    }

    private func subtitle(unit: String) -> String {
        output.schedulerManaged
            ? unit + Localization.tr(
                ", one run per logical processor, higher is better. The system chooses the core for each run, so a bar is not the score of one particular core.",
                "，每个逻辑处理器对应一次运行，越高越好。每次运行跑在哪个核心由系统决定，所以一根柱子不代表某一个特定核心的成绩。",
                "、論理プロセッサの数だけ実行、高いほど良い。各実行のコアはシステムが選ぶため、1 本のバーは特定のコアのスコアではありません。")
            : unit + Localization.tr(", one thread at a time, higher is better",
                                    "，每次只跑一个线程，越高越好",
                                    "、1 スレッドずつ、高いほど良い")
    }

    var body: some View {
        let scores = output.coreScores.sorted { $0.core < $1.core }
        if scores.count >= 2 {
            GlassCard(padding: 16) {
                VStack(alignment: .leading, spacing: 8) {
                    Text(title).fontWeight(.semibold)
                    Text(subtitle(unit: scores[0].unit))
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                    ColumnPlotView(values: scores.map { $0.score }, labels: scores.map { String($0.core + 1) })
                    Text(Localization.tr("The number under a bar is the row number in the list below.",
                                         "条形下方的数字是下面列表里的行号。",
                                         "バーの下の数字は下のリストの行番号です。"))
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    LazyVGrid(columns: [GridItem(.adaptive(minimum: 280), spacing: 16, alignment: .leading)],
                              alignment: .leading, spacing: 3) {
                        ForEach(scores) { score in
                            Text(rowText(score))
                                .font(.system(.caption, design: .monospaced))
                                .lineLimit(1)
                                .frame(maxWidth: .infinity, alignment: .leading)
                        }
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading)
            }
        }
    }
}

// MARK: - CPU page, cache and memory sweep

/// Read, write and copy rates of every cache level and of memory.
private struct LevelTable: View {
    let levels: [MemLevel]

    private struct Row: Identifiable {
        let id: Int
        var name: String
        var cells: [String]
        var header = false
        var note = false
    }

    private var rows: [Row] {
        var rows: [Row] = []
        func add(_ name: String, _ read: String, _ write: String, _ copy: String, header: Bool = false) {
            rows.append(Row(id: rows.count, name: name, cells: [read, write, copy], header: header))
        }
        func note(_ text: String) {
            rows.append(Row(id: rows.count, name: text, cells: [], note: true))
        }
        func rate(_ value: Double) -> String {
            value > 0 ? EngineFormat.number(value) : "-"
        }

        add(Localization.tr("Level and size", "层级与容量", "階層と容量"),
            Localization.tr("read", "读", "読込"),
            Localization.tr("write", "写", "書込"),
            Localization.tr("copy", "复制", "コピー"), header: true)
        var anyFromCurve = false
        for level in levels {
            let memory = level.level == "memory"
            let name: String
            if memory {
                name = Localization.tr("Memory", "内存", "メモリ")
            } else if level.fromCurve {
                anyFromCurve = true
                if level.capacityHigh > 0 {
                    name = "\(level.level)  \(EngineFormat.bytes(level.capacityLow))–\(EngineFormat.bytes(level.capacityHigh)) *"
                } else {
                    name = "\(level.level)  > \(EngineFormat.bytes(level.capacityLow)) *"
                }
            } else {
                name = "\(level.level)  \(EngineFormat.bytes(level.cacheBytes))"
            }
            if level.measured {
                add(name, rate(level.read), rate(level.write), rate(level.copy))
            } else {
                add(name, "-", "-", "-")
            }
            if memory, level.writeStream > 0 {
                add(Localization.tr("  streaming stores", "  流式写入", "  ストリーミング書込"),
                    "-", rate(level.writeStream), rate(level.copyStream))
            }
            if memory, level.zeroFill > 0 {
                add(Localization.tr("  CLZERO zero fill", "  CLZERO 清零", "  CLZERO ゼロ埋め"),
                    "-", rate(level.zeroFill), "-")
            }
            if memory, level.measured, !level.settled {
                note(Localization.tr(
                    "Not a settled memory figure: the sweep ended before the curve went flat.",
                    "这不是稳定的内存数字：扫描在曲线变平之前就结束了。",
                    "メモリの値は収束していません：曲線が平らになる前にスイープが終わりました。"))
            }
        }
        if anyFromCurve {
            note(Localization.tr(
                "* The system reports no size for this cache; its capacity was read off the curve and lies in this range.",
                "* 系统没有报告这级缓存的容量；容量是从曲线上读出来的，落在这个范围内。",
                "* このキャッシュの容量はシステムから報告されないため、曲線から読み取った範囲です。"))
        }
        return rows
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            ForEach(rows) { row in
                if row.note {
                    Text(row.name)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                } else {
                    HStack(spacing: 8) {
                        Text(row.name)
                            .font(.caption)
                            .lineLimit(1)
                            .minimumScaleFactor(0.75)
                            .frame(maxWidth: .infinity, alignment: .leading)
                        ForEach(row.cells.indices, id: \.self) { index in
                            Text(row.cells[index])
                                .font(.system(.caption, design: .monospaced))
                                .lineLimit(1)
                                .minimumScaleFactor(0.75)
                                .frame(width: 54, alignment: .trailing)
                        }
                    }
                    .foregroundColor(row.header ? Color.secondary : Color.primary)
                }
            }
        }
        .frame(maxWidth: 520, alignment: .leading)
    }
}

/// Tables and curves of a cache and memory sweep: one card per core class, one
/// for all cores, and one for virtual memory and storage.
struct CpuMemoryResultsView: View {
    let output: RunOutput

    private var classes: [MemClass] {
        output.classes.isEmpty ? [MemClass(rank: 0, mode: "single", name: "", cores: 0)] : output.classes
    }

    private func curves(mode: String) -> [PlotSeries] {
        let kernels = [
            ("read", Localization.tr("Read", "读", "読み取り")),
            ("write", Localization.tr("Write", "写", "書き込み")),
            ("copy", Localization.tr("Copy", "复制", "コピー")),
        ]
        return kernels.enumerated().map { index, kernel -> PlotSeries in
            let key = mode + "/" + kernel.0
            return PlotSeries(label: kernel.1, slot: index,
                              points: output.points.filter { $0.series == key }
                                  .map { PlotPoint(x: $0.blockBytes, y: $0.gbps) })
        }
    }

    private func markers(cpuClass: Int, threads: Int) -> [PlotMarker] {
        output.caches.filter { $0.cpuClass == cpuClass }.map { cache -> PlotMarker in
            // A cache shared by several of the running threads counts per thread.
            let sharers = max(1, min(threads, cache.sharedCores))
            return PlotMarker(x: cache.sizeBytes / Double(sharers),
                              label: sharers > 1 ? "L\(cache.level)÷\(sharers)" : "L\(cache.level)")
        }
    }

    private func classTitle(_ cpuClass: MemClass, mixed: Bool) -> String {
        let oneThread = Localization.tr("One thread, GB/s", "单线程，GB/s", "1 スレッド、GB/s")
        guard mixed else { return oneThread }
        return Localization.tr("Core class ", "核心类别 ", "コアクラス ") + "\(cpuClass.rank) · \(cpuClass.name) · "
            + "\(cpuClass.cores)" + Localization.tr(" cores", " 个核心", " コア") + " — " + oneThread
    }

    private var allTitle: String {
        Localization.tr("All cores — ", "全核 —— ", "全コア — ") + "\(max(1, output.allThreads))"
            + Localization.tr(" threads, GB/s (sizes are per thread)", " 个线程，GB/s（数据量按每个线程计）",
                              " スレッド、GB/s（サイズはスレッドあたり）")
    }

    @ViewBuilder
    private func sweepCard(title: String, mode: String, marks: [PlotMarker]) -> some View {
        let levels = output.levels.filter { $0.mode == mode }
        let series = curves(mode: mode)
        let hasCurve = series.contains { $0.points.count >= 2 }
        if !levels.isEmpty || hasCurve {
            GlassCard(padding: 16) {
                VStack(alignment: .leading, spacing: 10) {
                    Text(title).fontWeight(.semibold)
                    if !levels.isEmpty {
                        LevelTable(levels: levels)
                    }
                    if hasCurve {
                        Text(Localization.tr(
                            "GB/s by data-set size per thread, log scales. Dashed lines mark the cache sizes the system reports.",
                            "带宽（GB/s）随每个线程的数据量变化，双对数坐标。虚线标出系统报告的各级缓存容量。",
                            "スレッドあたりのデータセットサイズ別の GB/s（両対数）。破線はシステムが報告するキャッシュ容量です。"))
                            .font(.caption)
                            .foregroundStyle(.secondary)
                            .fixedSize(horizontal: false, vertical: true)
                        LinePlotView(series: series, markers: marks, logBytes: true)
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading)
            }
        }
    }

    private func pageMappingText(_ page: PageMapping) -> String {
        Localization.tr("Mapping new pages: ", "映射新页面：", "新規ページの割り当て: ")
            + "\(EngineFormat.number(page.gbps)) GB/s · \(EngineFormat.grouped(page.nsPerPage)) ns / "
            + EngineFormat.bytes(page.pageBytes) + Localization.tr(" page", " 页", " ページ")
    }

    private func requestsText(_ storage: StorageResult) -> String {
        var text = EngineFormat.number(storage.randReadIops)
            + Localization.tr(" read and ", " 次读、", " 回の読込と ")
            + EngineFormat.number(storage.randWriteIops)
            + Localization.tr(" write requests per second", " 次写请求每秒", " 回の書込（毎秒）")
        if !storage.directIo {
            text += Localization.tr(". Buffered I/O was used, so the file cache may inflate these.",
                                    "。走了系统文件缓存，数字可能偏高。",
                                    "。バッファ付き I/O のため、実際より高い可能性があります。")
        }
        return text
    }

    @ViewBuilder
    private func storageSection(_ storage: StorageResult) -> some View {
        if !storage.backing.isEmpty {
            Text(Localization.tr("The system pages to: ", "系统换页到：", "ページング先: ")
                + Localization.engineText(storage.backing))
                .font(.caption)
                .foregroundStyle(.secondary)
        }
        if storage.measured {
            Text(Localization.tr("Storage, sequential 1 MiB (MB/s)", "存储，顺序 1 MiB（MB/s）",
                                 "ストレージ、シーケンシャル 1 MiB（MB/s）"))
                .font(.caption)
                .foregroundStyle(.secondary)
            BarRowsView(items: [
                BarItem(id: 0, label: Localization.tr("Read", "读", "読み取り"), value: storage.seqRead),
                BarItem(id: 1, label: Localization.tr("Write", "写", "書き込み"), value: storage.seqWrite),
            ], unit: "MB/s")
            Text(Localization.tr("Storage, random 4 KiB, one request at a time (MB/s)",
                                 "存储，随机 4 KiB，每次一个请求（MB/s）",
                                 "ストレージ、ランダム 4 KiB、1 要求ずつ（MB/s）"))
                .font(.caption)
                .foregroundStyle(.secondary)
            BarRowsView(items: [
                BarItem(id: 0, label: Localization.tr("Read", "读", "読み取り"), value: storage.randRead),
                BarItem(id: 1, label: Localization.tr("Write", "写", "書き込み"), value: storage.randWrite),
            ], unit: "MB/s")
            Text(requestsText(storage))
                .font(.caption)
                .foregroundStyle(.secondary)
                .fixedSize(horizontal: false, vertical: true)
        } else {
            Text(Localization.tr("Storage not measured", "存储未测量", "ストレージは未測定")
                + (storage.skipped.isEmpty ? "" : Localization.tr(": ", "：", ": ") + Localization.engineText(storage.skipped)))
                .font(.caption)
                .foregroundStyle(.secondary)
        }
    }

    var body: some View {
        let groups = classes
        let mixed = groups.count > 1
        VStack(alignment: .leading, spacing: 14) {
            ForEach(groups, id: \.rank) { cpuClass in
                sweepCard(title: classTitle(cpuClass, mixed: mixed), mode: cpuClass.mode,
                          marks: markers(cpuClass: cpuClass.rank, threads: 1))
            }
            // With mixed cores the threads of the all-core run see different caches.
            sweepCard(title: allTitle, mode: "all",
                      marks: mixed ? [] : markers(cpuClass: groups[0].rank, threads: max(1, output.allThreads)))
            if output.pageMapping != nil || output.storage != nil {
                GlassCard(padding: 16) {
                    VStack(alignment: .leading, spacing: 8) {
                        Text(Localization.tr("Virtual memory and storage", "虚拟内存与存储", "仮想メモリとストレージ"))
                            .fontWeight(.semibold)
                        if let page = output.pageMapping {
                            Text(pageMappingText(page)).font(.callout)
                        }
                        if let storage = output.storage {
                            storageSection(storage)
                        }
                    }
                    .frame(maxWidth: 520, alignment: .leading)
                    .frame(maxWidth: .infinity, alignment: .leading)
                }
            }
        }
    }
}

// MARK: - Charts page

/// One chart per test: the best saved score of every device and API under the
/// newest version of the test. Scores of different versions are not comparable.
struct SavedResultCharts: View {
    let results: [BenchResult]

    private struct Chart: Identifiable {
        let id: String
        let title: String
        let subtitle: String
        let unit: String
        let bars: [BarItem]
    }

    static func workloadTitle(_ id: String) -> String {
        switch id {
        case "stream":
            return Localization.tr("Particle — Memory Throughput", "粒子 —— 内存吞吐", "パーティクル — メモリスループット")
        case "headless_compute":
            return Localization.tr("Headless Compute — Particle", "Headless 计算 —— 粒子", "Headless コンピュート — パーティクル")
        case "gpu_burn":
            return Localization.tr("Plasma x Kaleidoscope — GPU Burn", "等离子晶核 × 万花镜 —— GPU Burn",
                                   "プラズマ核 × カレイドスコープ — GPU Burn")
        case "cinematic_liquid":
            return Localization.tr("Fluid — Interactive Pool", "流体 —— 互动水池", "流体 — インタラクティブプール")
        case "cinematic_liquid_v1":
            return Localization.tr("Legacy Cinematic Liquid v1", "旧版电影化液体 v1", "旧版シネマティック液体 v1")
        case "membw":
            return Localization.tr("Memory Bandwidth — VRAM / Cache", "显存带宽 —— 显存 / 缓存", "メモリ帯域 — VRAM / キャッシュ")
        case "cpu_single_core":
            return Localization.tr("CPU — Single-core", "CPU —— 单核", "CPU — コア別")
        case "cpu_multi_core":
            return Localization.tr("CPU — All-core", "CPU —— 全核", "CPU — 全コア")
        case "cpu_membw":
            return Localization.tr("CPU — Cache & memory bandwidth", "CPU —— 缓存与内存带宽", "CPU — キャッシュ / メモリ帯域")
        default:
            return id
        }
    }

    private static func workloadKey(_ result: BenchResult) -> String {
        // Liquid v1 and v2 rows share one workload id and differ in the version.
        if result.workload == "cinematic_liquid", result.workloadVersion == "cinematic_liquid_v1" {
            return "cinematic_liquid_v1"
        }
        return result.workload
    }

    private static func isCpu(_ result: BenchResult) -> Bool {
        result.graphicsApi == "CPU" || result.workload.hasPrefix("cpu_")
    }

    private var charts: [Chart] {
        // The legacy 2D fluid score was never verified.
        let scored = results.filter { $0.score.isFinite && $0.score > 0 && $0.workload != "fluid" }
        let groups = Dictionary(grouping: scored) { Self.workloadKey($0) }
        let known = ["stream", "headless_compute", "gpu_burn", "cinematic_liquid", "membw",
                     "cpu_single_core", "cpu_multi_core", "cpu_membw"]
        let order = known + groups.keys.filter { !known.contains($0) }.sorted()
        return order.compactMap { key -> Chart? in
            guard let rows = groups[key], let newest = rows.max(by: { $0.timestamp < $1.timestamp }) else {
                return nil
            }
            let cpu = Self.isCpu(newest)
            var best: [String: Double] = [:]
            for row in rows where row.workloadVersion == newest.workloadVersion {
                let label = cpu ? (row.cpuName.isEmpty ? row.deviceName : row.cpuName)
                                : row.deviceName + " — " + row.graphicsApi
                best[label] = max(best[label] ?? 0, row.score)
            }
            let bars = best.keys.sorted().enumerated().map { index, label in
                BarItem(id: index, label: label, value: best[label] ?? 0)
            }
            guard !bars.isEmpty else { return nil }
            var subtitle = newest.scoreUnit + (cpu
                ? Localization.tr(", best saved score per device", "，每个设备保存过的最好成绩", "、デバイスごとの最高スコア")
                : Localization.tr(", best saved score per device and API", "，每个设备和 API 保存过的最好成绩",
                                  "、デバイス・API ごとの最高スコア"))
            if let version = newest.workloadVersion, !version.isEmpty { subtitle += "  ·  " + version }
            return Chart(id: key, title: Self.workloadTitle(key), subtitle: subtitle, unit: newest.scoreUnit,
                         bars: bars)
        }
    }

    var body: some View {
        let items = charts
        VStack(alignment: .leading, spacing: 14) {
            if items.isEmpty {
                GlassCard {
                    VStack(spacing: 12) {
                        Image(systemName: "chart.bar.fill")
                            .font(.system(size: 48))
                            .foregroundStyle(.secondary)
                        Text(Localization.tr("No saved results yet. Run a test first.",
                                             "还没有保存的成绩，请先运行一项测试。",
                                             "保存された結果がありません。先にテストを実行してください。"))
                            .multilineTextAlignment(.center)
                            .foregroundStyle(.secondary)
                    }
                    .frame(maxWidth: .infinity)
                    .padding(40)
                }
            } else {
                ForEach(items) { chart in
                    GlassCard(padding: 16) {
                        VStack(alignment: .leading, spacing: 6) {
                            Text(chart.title).fontWeight(.semibold)
                            Text(chart.subtitle)
                                .font(.caption)
                                .foregroundStyle(.secondary)
                            BarRowsView(items: chart.bars, unit: chart.unit)
                                .frame(maxWidth: 900, alignment: .leading)
                        }
                        .frame(maxWidth: .infinity, alignment: .leading)
                    }
                }
                Text(Localization.tr(
                    "Each chart uses only the results of the newest version of its test. Every run is listed on the History page.",
                    "每张图只使用该测试最新版本的成绩；每一次运行都列在历史页里。",
                    "各チャートはそのテストの最新バージョンの結果だけを使います。すべての実行は履歴ページにあります。"))
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
            }
        }
    }
}


/// Native Metal diagnostic; its preview contract is separate from Vulkan membw_v1.
struct GpuMemoryResultsCard: View {
    let output: RunOutput

    var body: some View {
        if !output.gpuMemoryPoints.isEmpty {
            VStack(alignment: .leading, spacing: 12) {
                ForEach(["device_local", "host_visible"], id: \.self) { storage in
                    let curves = ["read_only", "read_modify_write"].enumerated().map { slot, access in
                        PlotSeries(label: access == "read_only"
                            ? Localization.tr("Read", "读取", "読み取り")
                            : Localization.tr("Particle update", "粒子更新", "パーティクル更新"),
                            slot: slot,
                            points: output.gpuMemoryPoints.filter { $0.series == storage + "/" + access }
                                .map { PlotPoint(x: $0.blockBytes, y: $0.gbps) })
                    }
                    if curves.contains(where: { !$0.points.isEmpty }) {
                        GlassCard {
                            VStack(alignment: .leading, spacing: 8) {
                                Text(storage == "device_local"
                                    ? Localization.tr("GPU private allocation", "GPU 私有内存", "GPU プライベートメモリ")
                                    : Localization.tr("CPU visible allocation", "CPU 可见内存", "CPU 可視メモリ"))
                                    .font(.headline)
                                LinePlotView(series: curves)
                                Text(Localization.tr(
                                    "GB/s by working-set size. This particle-derived probe is a preview; the final point may still be served by cache. On Apple silicon both allocation modes use unified memory.",
                                    "工作集大小与 GB/s 的关系。这是基于粒子的预览探测，最后一个点仍可能命中缓存。Apple 芯片的两种内存分配都使用统一内存。",
                                    "ワーキングセットサイズ別の GB/s。プレビュー測定で、最後の点もキャッシュの影響を受ける場合があります。Apple silicon は両方ともユニファイドメモリです。"))
                                    .font(.caption).foregroundStyle(.secondary)
                            }
                        }
                    }
                }
            }
        }
    }
}
