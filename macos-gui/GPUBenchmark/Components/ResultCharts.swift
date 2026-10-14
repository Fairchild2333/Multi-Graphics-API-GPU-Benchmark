// ResultCharts.swift — Result tables and charts of the GPU, CPU and Charts pages.
//
// Each view takes parsed engine output (EngineOutput.swift) or saved results
// and shows the same figures as the Windows and Android apps.

import SwiftUI
import UniformTypeIdentifiers
#if os(macOS)
import AppKit
#endif

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
        if score.coreType != "auto" {
            let bound = String(format: "%.2f%%", (score.residencyLowerBound ?? 0) * 100)
            return score.label + "  ·  " + value + Localization.tr("  ·  verified ≥", "  ·  归属 ≥", "  ·  確認 ≥") + bound
        }
        if score.label.isEmpty { return "#\(score.core + 1)  ·  \(value)" }
        return "#\(score.core + 1)  \(score.label)  ·  \(value)"
    }

    private var title: String {
        if output.coreTypeSelection != "auto" {
            return Localization.tr("Verified core-type scores", "已核对归属的大核／小核成绩", "確認済みコア種別スコア")
        }
        return output.schedulerManaged
            ? Localization.tr("One-thread runs", "单线程各次运行的成绩", "1 スレッド実行ごとのスコア")
            : Localization.tr("Score of every logical processor", "各逻辑处理器的成绩", "論理プロセッサ別スコア")
    }

    private func subtitle(unit: String) -> String {
        if output.coreTypeSelection != "auto" {
            if output.efficiencyForeground {
                return unit + Localization.tr(
                    ", one measured thread. Efficiency cores use performance priority while helper threads occupy performance cores. Each round verifies at least 99% efficiency-core CPU time; helper work is excluded from this score.",
                    "，一个计分线程。小核采用性能优先级，大核辅助线程提供调度压力；每轮验证至少 99% 的 CPU 时间属于小核，辅助线程工作量不计入小核成绩。",
                    "、計測は 1 スレッド。効率コアは性能優先度で実行し、補助スレッドが性能コアに負荷をかけます。各ラウンドで効率コア CPU 時間が 99% 以上であることを検証し、補助処理はスコアに含めません。")
            }
            return unit + Localization.tr(
                ", one thread. Every round verifies at least 99% CPU time on the requested core type. The efficiency-core score uses background priority and is not its peak performance. These scores cannot be summed to predict the all-core score.",
                "，单线程。每轮确认至少 99% 的 CPU 时间属于所选核心类型。小核使用后台优先级，成绩不代表其峰值；不能把这些单核成绩相加预测全核成绩。",
                "、1 スレッド。各ラウンドで指定コア種別の CPU 時間が 99% 以上であることを確認。効率コアはバックグラウンド優先度で測定し、最大性能ではありません。合計して全コアスコアを予測することはできません。")
        }
        return output.schedulerManaged
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
        if !scores.isEmpty {
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
            } else if level.cacheBytes > 0 {
                name = "\(level.level)  \(EngineFormat.bytes(level.cacheBytes))"
            } else {
                name = level.level
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
        if output.coreTypeSelection != "auto" {
            let name = cpuClass.name == "Performance"
                ? Localization.tr("Performance core", "大核", "性能コア")
                : output.memoryEfficiencyForeground
                    ? Localization.tr("Efficiency core (performance priority)", "小核（性能优先级）", "効率コア（性能優先度）")
                    : Localization.tr("Efficiency core (background)", "小核（后台策略）", "効率コア（バックグラウンド）")
            return name + " — " + oneThread
        }
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
            if storage.randReadIops > 0 || storage.randWriteIops > 0 {
                Text(requestsText(storage))
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
            }
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
            if let lower = output.memoryResidencyLowerBound {
                Text(Localization.tr("Single-thread core-type verification ≥", "单线程核心类型归属校验 ≥", "1 スレッドのコア種別確認 ≥")
                     + String(format: "%.2f%%", lower * 100)
                     + Localization.tr("; all-core, page mapping and storage use system scheduling.",
                                       "；全核、页面映射和存储使用系统调度。", "。全コア、ページ割り当て、ストレージはシステムがスケジュールします。"))
                    .font(.caption).foregroundStyle(.secondary)
            }
            if output.memoryEfficiencyForeground {
                Text(Localization.tr("Efficiency-core curves use performance priority; performance-core helper work is excluded from bandwidth.",
                                     "小核曲线使用性能优先级；大核辅助负载不计入带宽。",
                                     "効率コアの曲線は性能優先度で測定し、性能コアの補助処理は帯域に含めません。"))
                    .font(.caption).foregroundStyle(.secondary)
            }
            ForEach(groups, id: \.rank) { cpuClass in
                sweepCard(title: classTitle(cpuClass, mixed: mixed), mode: cpuClass.mode,
                          marks: markers(cpuClass: cpuClass.rank, threads: 1))
            }
            // With mixed cores the threads of the all-core run see different caches.
            if output.allThreads > 0 {
                sweepCard(title: allTitle, mode: "all",
                          marks: mixed ? [] : markers(cpuClass: groups[0].rank, threads: max(1, output.allThreads)))
            }
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
        case "gpu_cache":
            return Localization.tr("GPU — Cache latency probe", "GPU —— 缓存延迟探测", "GPU — キャッシュ遅延測定")
        case "cpu_single_core":
            return Localization.tr("CPU — Single-core", "CPU —— 单核", "CPU — コア別")
        case "cpu_single_core_performance":
            return Localization.tr("CPU — Performance core", "CPU —— 大核", "CPU — 性能コア")
        case "cpu_single_core_efficiency":
            return Localization.tr("CPU — Efficiency core", "CPU —— 小核", "CPU — 効率コア")
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
        if result.workload == "cpu_single_core",
           let type = result.workloadConfigValue("coreType"), ["performance", "efficiency"].contains(type) {
            return "cpu_single_core_" + type
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


/// Keep the native Metal preview and Vulkan memory curves separate.
struct GpuMemoryResultsCard: View {
    let output: RunOutput

    var body: some View {
        if !output.gpuMemorySummaries.isEmpty || output.gpuLocalCache != nil {
            vulkanSummary
        }
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
                                ForEach(["read_only", "read_modify_write"], id: \.self) { access in
                                    let points = output.gpuMemoryPoints.filter { $0.series == storage + "/" + access }
                                    if let last = points.max(by: { $0.blockBytes < $1.blockBytes }),
                                       let peak = points.max(by: { $0.gbps < $1.gbps }) {
                                        Text(memoryKernelLabel(access) + ": "
                                            + Localization.tr("largest set ", "最大工作集 ", "最大サイズ ")
                                            + EngineFormat.bytes(last.blockBytes) + " · " + rate(last.gbps)
                                            + Localization.tr("; peak ", "；峰值 ", "、ピーク ") + rate(peak.gbps))
                                            .font(.caption).foregroundStyle(.secondary)
                                    }
                                }
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
        if !output.vulkanMemoryPoints.isEmpty {
            vulkanCard(title: Localization.tr("Vulkan — Memory / Cache", "Vulkan —— 内存 / 缓存", "Vulkan — メモリ / キャッシュ"),
                       kernels: ["read", "write", "copy"])
            vulkanCard(title: Localization.tr("Vulkan — Local cache", "Vulkan —— 核心内缓存", "Vulkan — ローカルキャッシュ"),
                       kernels: ["l1", "l1_single"])
            vulkanCard(title: Localization.tr("Vulkan — Host memory", "Vulkan —— 主机内存", "Vulkan — ホストメモリ"),
                       kernels: ["host_read", "host_write"])
        }
    }

    private func rate(_ value: Double) -> String {
        value > 0 ? String(format: "%.1f GB/s", value) : "—"
    }

    private var vulkanSummary: some View {
        GlassCard {
            VStack(alignment: .leading, spacing: 10) {
                Text(Localization.tr("Vulkan — Cache and memory summary", "Vulkan —— 缓存与内存汇总", "Vulkan — キャッシュ / メモリ概要"))
                    .font(.headline)
                HStack {
                    Text(Localization.tr("Kernel", "访问", "アクセス")).frame(maxWidth: .infinity, alignment: .leading)
                    Text(Localization.tr("Cache hit", "缓存命中段", "キャッシュヒット")).frame(maxWidth: .infinity, alignment: .trailing)
                    Text(Localization.tr("Largest sets", "大工作集", "大サイズ")).frame(maxWidth: .infinity, alignment: .trailing)
                }.font(.caption).foregroundStyle(.secondary)
                ForEach(output.gpuMemorySummaries, id: \.kernel) { item in
                    HStack {
                        Text(memoryKernelLabel(item.kernel)).frame(maxWidth: .infinity, alignment: .leading)
                        Text(rate(item.cache)).frame(maxWidth: .infinity, alignment: .trailing)
                        Text(rate(item.memory)).frame(maxWidth: .infinity, alignment: .trailing)
                    }.font(.caption).monospacedDigit()
                    if item.kernel == "read", item.settled != true {
                        Text(Localization.tr("Memory plateau unverified.", "内存平台尚未验证。", "メモリ帯域は未検証です。"))
                            .font(.caption).foregroundStyle(.secondary)
                    }
                }
                if let local = output.gpuLocalCache {
                    Divider()
                    Text(Localization.tr("Local cache probe (L1-style), full GPU: ", "局部缓存探测（L1 型），全 GPU 合计：", "ローカルキャッシュ測定（L1 型）、GPU 全体: ") + rate(local.fullLoad))
                        .font(.callout)
                    Text(Localization.tr("Single workgroup: ", "单工作组：", "単一ワークグループ: ") + rate(local.singleGroup))
                        .font(.caption)
                    if local.capacityHigh > local.capacityLow, local.capacityLow > 0 {
                        Text(Localization.tr("Working-set transition: ", "工作集转折范围：", "サイズの遷移範囲: ")
                            + EngineFormat.bytes(local.capacityLow) + "–" + EngineFormat.bytes(local.capacityHigh))
                            .font(.caption).foregroundStyle(.secondary)
                    }
                }
                Text(Localization.tr(
                    "Effective kernel rates. The local probe and cache-hit range do not independently identify hardware L1/L2/L3 or the SoC/SLC. The transition range is not a verified cache capacity.",
                    "以上是测试内核的有效吞吐。局部探测和缓存命中段尚未独立确认硬件 L1/L2/L3 或 SoC/SLC；转折范围也不是已验证的缓存容量。",
                    "カーネルの実効速度です。ハードウェアの L1/L2/L3、SoC/SLC と容量は独立に確認されていません。"))
                    .font(.caption).foregroundStyle(.secondary)
            }
        }
    }

    @ViewBuilder private func vulkanCard(title: String, kernels: [String]) -> some View {
        let curves = kernels.enumerated().map { slot, kernel in
            PlotSeries(label: memoryKernelLabel(kernel), slot: slot,
                       points: output.vulkanMemoryPoints.filter { $0.series == kernel }
                        .map { PlotPoint(x: $0.blockBytes, y: $0.gbps) })
        }
        if curves.contains(where: { !$0.points.isEmpty }) {
            GlassCard {
                VStack(alignment: .leading, spacing: 8) {
                    Text(title).font(.headline)
                    LinePlotView(series: curves)
                    Text(Localization.tr(
                        "Median GB/s by working-set size. Small working sets can be cache-assisted. macOS Vulkan uses MoltenVK over Metal; its results are saved separately from native Metal previews.",
                        "工作集大小与 GB/s 中位数的关系。小工作集可能命中缓存。macOS 的 Vulkan 经 MoltenVK 转译到 Metal，成绩与原生 Metal 预览分组保存。",
                        "サイズ別の中央値 GB/s。小さいサイズはキャッシュの影響を受けます。macOS の Vulkan は MoltenVK で Metal に変換され、ネイティブ Metal プレビューとは別に保存します。"))
                        .font(.caption).foregroundStyle(.secondary)
                }
            }
        }
    }

    private func memoryKernelLabel(_ kernel: String) -> String {
        switch kernel {
        case "read", "host_read", "read_only": return Localization.tr("Read", "读取", "読み取り")
        case "read_modify_write": return Localization.tr("Particle update", "粒子更新", "パーティクル更新")
        case "write", "host_write": return Localization.tr("Write", "写入", "書き込み")
        case "copy": return Localization.tr("Copy", "复制", "コピー")
        case "l1": return Localization.tr("Full load", "全 GPU 负载", "全負荷")
        default: return Localization.tr("Single workgroup", "单工作组", "単一ワークグループ")
        }
    }
}

struct GpuCacheResultsCard: View {
    let output: RunOutput
    private func label(_ condition: String) -> String {
        switch condition {
        case "gpu_warm": return Localization.tr("GPU warmed", "GPU 预热", "GPU ウォーム")
        case "cpu_warm": return Localization.tr("CPU warmed", "CPU 预热", "CPU ウォーム")
        case "eviction_pass": return Localization.tr("After eviction pass", "清扫后", "退避処理後")
        default: return Localization.tr("Parallel reads", "并发读取", "並列読み取り")
        }
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            GlassCard {
                VStack(alignment: .leading, spacing: 8) {
                    Text(Localization.tr("GPU dependent-read latency — ns/load", "GPU 随机依赖读取延迟 —— ns/读取", "GPU 依存読み取り — ns/回")).font(.headline)
                    let curves = ["gpu_warm", "eviction_pass", "cpu_warm"].enumerated().map { slot, condition in
                        PlotSeries(label: label(condition), slot: slot, points: output.gpuCachePoints
                            .filter { $0.condition == condition }.map { PlotPoint(x: $0.addressBytes, y: $0.value) })
                    }
                    LinePlotView(series: curves)
                    Text(Localization.tr(
                        "Lower is faster. Each load chooses the next address. The x-axis is address span with one node every 128 bytes. Warming controls show cache-state effects; eviction is not guaranteed and these curves do not identify hardware L2/L3/SLC.",
                        "越低越快。每次读取决定下一地址，横轴是地址跨度（每 128 字节一个节点）。预热对照反映缓存状态影响；清扫不保证完全排空缓存，曲线尚不能独立确认硬件 L2/L3/SLC。",
                        "低いほど高速です。128 バイト間隔の依存読み取りです。退避の完了やハードウェア L2/L3/SLC の識別は未検証です。")).font(.caption).foregroundStyle(.secondary)
                }
            }
            let parallel = output.gpuCachePoints.filter { $0.condition == "parallel_gpu_warm" }
            if !parallel.isEmpty {
                GlassCard {
                    VStack(alignment: .leading, spacing: 8) {
                        Text(Localization.tr("GPU parallel random reads — payload GB/s", "GPU 并发随机读取 —— 逻辑负载 GB/s", "GPU 並列ランダム読み取り — GB/s")).font(.headline)
                        LinePlotView(series: [PlotSeries(label: label("parallel_gpu_warm"), slot: 0,
                            points: parallel.map { PlotPoint(x: $0.addressBytes, y: $0.value) })])
                        Text(Localization.tr(
                            "4096 dependent chains. Traffic counts 4 bytes per load, excluding fetched cache lines; compare only with the same probe. Every result passed the chain checksum check.",
                            "4096 条依赖链。每次读取只计 4 字节逻辑负载，不计额外取回的缓存行；请与相同探测比较。每个结果都通过了读取链校验。",
                            "4096 本の依存チェーンです。読み取り当たり 4 バイトを計上し、全結果をチェックサムで検証します。")).font(.caption).foregroundStyle(.secondary)
                    }
                }
            }
        }
    }
}

/// A read-only browser of curves stored in current or older results files.
/// Opening a file changes only this sheet, never the app's result store.
struct SavedMemoryCurvesView: View {
    let results: [BenchResult]
    var initialSelection: String? = nil
    @Environment(\.dismiss) private var dismiss
    @State private var selectedID = ""
    @State private var openedResults: [BenchResult]?
    @State private var openedFileName = ""
    @State private var showFile = false
    @State private var fileError = ""

    private var source: [BenchResult] { openedResults ?? results }
    private var records: [BenchResult] {
        source.filter { ["membw", "cpu_membw", "gpu_cache"].contains($0.workload) }.sorted { $0.id > $1.id }
    }
    private var selected: BenchResult? {
        records.first { $0.id == selectedID } ?? records.first
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Text(Localization.tr("Cache & memory curves", "缓存与内存曲线", "キャッシュ / メモリ曲線"))
                    .font(.title2).fontWeight(.semibold)
                Spacer()
                Button(Localization.tr("Open results file…", "打开成绩文件…", "結果ファイルを開く…"), action: openFile)
                Button(Localization.tr("Close", "关闭", "閉じる")) { dismiss() }
            }
            if openedResults != nil {
                HStack {
                    Text(openedFileName + Localization.tr(" · read only", " · 只读查看", " · 読み取り専用"))
                        .font(.caption).foregroundStyle(.secondary)
                    Spacer()
                    Button(Localization.tr("Current history", "返回当前历史", "現在の履歴")) {
                        openedResults = nil; openedFileName = ""; selectedID = records.first?.id ?? ""; fileError = ""
                    }
                }
            }
            if !fileError.isEmpty { Text(fileError).font(.caption).foregroundStyle(.red) }
            if records.isEmpty {
                Spacer()
                Text(Localization.tr("No cache or memory sweep is stored in this file.", "这份成绩没有保存缓存或内存扫描。", "このファイルにはキャッシュ / メモリ測定がありません。"))
                    .font(.headline)
                Text(Localization.tr(
                    "Particle scores are individual throughput samples; they cannot reconstruct a working-set sweep. Open a result that contains saved memory curves.",
                    "粒子成绩只有单次吞吐数据，不能还原工作集扫描曲线。请打开包含内存扫描数据的成绩文件。",
                    "パーティクルの個別スコアからサイズ別曲線は復元できません。保存済みのメモリ測定を開いてください。"))
                    .font(.caption).foregroundStyle(.secondary)
                Text("\(source.count) " + Localization.tr("saved results", "条已保存成绩", "件の保存済み結果"))
                    .font(.caption).foregroundStyle(.secondary)
                Spacer()
            } else {
                Picker(Localization.tr("Saved run", "历史运行", "保存済み実行"), selection: $selectedID) {
                    ForEach(records) { row in
                        Text(row.timestamp + " · " + row.graphicsApi + " · " + row.deviceName).tag(row.id)
                    }
                }.pickerStyle(.menu)
                if let result = selected {
                    Text(result.workloadVersion ?? "—").font(.caption).foregroundStyle(.secondary)
                    if result.workload == "gpu_cache" {
                        Text(Localization.tr("Dependent loads: ", "依赖读取次数：", "依存読み取り回数: ")
                            + (result.workloadConfigValue("steps") ?? "—")
                            + Localization.tr(" · median samples: ", " · 中位数样本：", " · 中央値サンプル: ")
                            + (result.workloadConfigValue("repeats") ?? "—"))
                            .font(.caption).foregroundStyle(.secondary)
                    }
                    ScrollView {
                        let output = RunOutput.savedMemory(workload: result.workload, api: result.graphicsApi,
                                                           configuration: result.workloadConfig)
                        VStack(alignment: .leading, spacing: 14) {
                            if result.hidesImplausibleLegacyScore {
                                Text(Localization.tr("This legacy score is invalid; no curve is shown.", "这条旧版成绩异常，不显示曲线。", "この旧スコアは無効のため曲線を表示しません。"))
                            } else if output.hasMemoryCurves {
                                if result.workload == "cpu_membw" { CpuMemoryResultsView(output: output) }
                                else if result.workload == "gpu_cache" { GpuCacheResultsCard(output: output) }
                                else { GpuMemoryResultsCard(output: output) }
                            } else {
                                Text(Localization.tr("This run has no saved curve points.", "这次运行没有保存曲线点。", "この実行には保存済みの曲線がありません。"))
                                if output.hasGpuMemoryResults { GpuMemoryResultsCard(output: output) }
                            }
                            Text(Localization.tr("Loaded from the saved result. No test is running and the file is unchanged.",
                                "曲线读取自已保存成绩，未启动测试，也未修改文件。",
                                "保存済み結果から読み取り、測定やファイル変更は行いません。"))
                                .font(.caption).foregroundStyle(.secondary)
                        }.frame(maxWidth: .infinity, alignment: .leading).padding(.bottom, 8)
                    }
                }
            }
        }
        .padding(20)
#if os(macOS)
        .frame(minWidth: 760, minHeight: 560)
#endif
        .onAppear { selectedID = records.first { $0.id == initialSelection }?.id ?? records.first?.id ?? "" }
#if !os(macOS)
        .fileImporter(isPresented: $showFile, allowedContentTypes: [.json], allowsMultipleSelection: false) { selection in
            do {
                guard let url = try selection.get().first else { return }
                try loadFile(url)
            } catch {
                showError(error)
            }
        }
#endif
    }

    private func openFile() {
#if os(macOS)
        let panel = NSOpenPanel()
        panel.canChooseFiles = true
        panel.canChooseDirectories = false
        panel.allowsMultipleSelection = false
        panel.allowedContentTypes = [.json, .data]
        let parent = NSApp.keyWindow ?? NSApp.mainWindow
        let completion: (NSApplication.ModalResponse) -> Void = { response in
            DispatchQueue.main.async {
                parent?.makeKeyAndOrderFront(nil)
                NSApp.activate(ignoringOtherApps: true)
            }
            guard response == .OK, let url = panel.url else { return }
            do { try loadFile(url) } catch { showError(error) }
        }
        if let parent { panel.beginSheetModal(for: parent, completionHandler: completion) }
        else { panel.begin(completionHandler: completion) }
#else
        showFile = true
#endif
    }

    private func loadFile(_ url: URL) throws {
        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
        let data = try Data(contentsOf: url)
        let decoder = JSONDecoder()
        struct Envelope: Decodable { let results: [BenchResult] }
        let rows: [BenchResult]
        if let array = try? decoder.decode([BenchResult].self, from: data) { rows = array }
        else { rows = try decoder.decode(Envelope.self, from: data).results }
        openedResults = rows; openedFileName = url.lastPathComponent
        selectedID = records.first?.id ?? ""; fileError = ""
    }

    private func showError(_ error: Error) {
        fileError = Localization.tr("Could not open the results file: ", "无法打开成绩文件：", "結果ファイルを開けません: ") + error.localizedDescription
    }
}
