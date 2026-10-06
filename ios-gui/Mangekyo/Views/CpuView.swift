// CpuView.swift — iOS CPU benchmark page.

import SwiftUI

struct CpuView: View {
    @EnvironmentObject var engine: BenchEngine

    @State private var mode: String = "all"          // per-core | multi | all | memory
    @State private var durationPreset: String = "1"  // 1 | 15
    @State private var secondsPerTest: String = "1"
    @State private var warmupSec: String = "0.2"
    @State private var logScrollID = 0

    private var memoryMode: Bool { mode == "memory" }

    var body: some View {
        VStack(spacing: 0) {
            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    Text(Localization.tr(
                        "Measures CPU compute throughput with dense math loops on logical processors (single-core and multi-core), or the bandwidth of every cache level and of memory.",
                        "用密集数学循环测量逻辑处理器上的 CPU 计算吞吐（单核与多核），或测量各级缓存与内存的带宽。",
                        "論理プロセッサ上の密集数学ループで CPU スループットを測定（単一/複数コア）、または各キャッシュ階層とメモリの帯域を測定します。"))
                        .font(.callout)
                        .foregroundStyle(.secondary)

                    optionsCard
                    progressCard
                    summaryCard
                    resultsSection
                    outputCard
                }
                .padding(16)
            }

            HStack {
                Spacer()
                Button(Localization.tr("Cancel", "取消", "キャンセル")) {
                    engine.cancel()
                }
                .disabled(!engine.isRunning)
                Button(action: runCpu) {
                    Text(memoryMode
                         ? Localization.tr("Run Cache & Memory Test", "开始缓存与内存测试", "キャッシュ / メモリテストを実行")
                         : Localization.tr("Run CPU Benchmark", "开始 CPU 跑分", "CPU ベンチマークを実行"))
                }
                .buttonStyle(.borderedProminent)
                .disabled(engine.isRunning || engine.workingDirectory.isEmpty)
            }
            .padding(.horizontal, 16)
            .padding(.vertical, 12)
            .background(.ultraThinMaterial)
        }
    }

    // MARK: - Cards

    private var optionsCard: some View {
        GlassCard {
            VStack(alignment: .leading, spacing: 14) {
                Text(infoText)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .padding(8)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .background(RoundedRectangle(cornerRadius: 8).fill(Color.accentColor.opacity(0.08)))

                VStack(alignment: .leading, spacing: 12) {
                    VStack(alignment: .leading, spacing: 4) {
                        Text(Localization.tr("Test mode", "测试模式", "テストモード"))
                            .font(.subheadline).foregroundStyle(.secondary)
                        Picker("", selection: $mode) {
                            Text(Localization.tr("Single-core", "单核", "コア別")).tag("per-core")
                            Text(Localization.tr("All-core", "全核", "全コア")).tag("multi")
                            Text(Localization.tr("Single-core + All-core", "单核 + 全核", "コア別 + 全コア")).tag("all")
                            Text(Localization.tr("Cache & memory bandwidth", "缓存与内存带宽", "キャッシュ / メモリ帯域")).tag("memory")
                        }
                        .labelsHidden()
                    }

                    if !memoryMode {
                        durationFields
                    }
                }

                Text(memoryMode
                     ? Localization.tr(
                        "Takes about a minute and warms the device. The storage test writes a temporary file of up to 1 GiB in the app's documents and removes it afterwards. Close other apps first: the sweep uses a large part of the free memory.",
                        "大约需要一分钟，设备会发热。存储测试会在 App 文档目录里写一个最大 1 GiB 的临时文件，测完即删除。请先关闭其他 App：扫描会占用大部分空闲内存。",
                        "約 1 分かかり、端末が温かくなります。ストレージテストはアプリの書類フォルダに最大 1 GiB の一時ファイルを書き込み、終了後に削除します。空きメモリの大部分を使うため、先に他のアプリを閉じてください。")
                     : Localization.tr(
                        "Single-core duration is applied once to each logical processor; the formal preset can take a long time.",
                        "单核测试会依次测试每个逻辑处理器；正式测试可能耗时较长。",
                        "単一コア時間は論理プロセッサごとに適用。正式プリセットは長時間かかる場合があります。"))
                    .font(.caption2)
                    .foregroundStyle(.secondary)
            }
        }
    }

    @ViewBuilder
    private var durationFields: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(Localization.tr("Duration preset", "时长预设", "時間プリセット"))
                .font(.subheadline).foregroundStyle(.secondary)
            Picker("", selection: $durationPreset) {
                Text(Localization.tr("Quick (1 s)", "快速（1 秒）", "クイック（1 秒）")).tag("1")
                Text(Localization.tr("Formal (15 s)", "正式（15 秒）", "正式（15 秒）")).tag("15")
            }
            .labelsHidden()
            .onChange(of: durationPreset) { v in
                secondsPerTest = v
            }
        }

        VStack(alignment: .leading, spacing: 4) {
            Text(Localization.tr("Seconds per test", "每项测试秒数", "テストごとの秒数"))
                .font(.subheadline).foregroundStyle(.secondary)
            TextField("1", text: $secondsPerTest)
                .textFieldStyle(.roundedBorder)
                .keyboardType(.decimalPad)
        }

        VStack(alignment: .leading, spacing: 4) {
            Text(Localization.tr("Warm-up seconds", "预热秒数", "ウォームアップ秒数"))
                .font(.subheadline).foregroundStyle(.secondary)
            TextField("0.2", text: $warmupSec)
                .textFieldStyle(.roundedBorder)
                .keyboardType(.decimalPad)
        }
    }

    private var progressCard: some View {
        GlassCard(padding: 16) {
            VStack(alignment: .leading, spacing: 8) {
                HStack {
                    Text(engine.statusText).fontWeight(.semibold)
                    Spacer()
                    Text(engine.progressLabel).foregroundStyle(.secondary)
                }
                ProgressView(value: engine.progressFraction)
            }
        }
    }

    private var summaryCard: some View {
        AccentGlassCard {
            VStack(alignment: .leading, spacing: 8) {
                Text(Localization.tr("Summary", "摘要", "要約")).fontWeight(.semibold)
                summaryValues
                if !cpuLabel.isEmpty {
                    Text(cpuLabel)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    @ViewBuilder
    private var summaryValues: some View {
        let output = engine.runOutput
        if output.hasMemoryResults {
            summaryValue(Localization.tr("Memory read, one thread", "内存读取，单线程", "メモリ読込、1 スレッド"),
                         memoryHeadline(mode: output.classes.first?.mode ?? "single", threads: 1))
            summaryValue(Localization.tr("Memory read, all cores", "内存读取，全核", "メモリ読込、全コア"),
                         memoryHeadline(mode: "all", threads: output.allThreads))
        } else if output.hasComputeResults {
            summaryValue(Localization.tr("Single-core average", "单核平均", "コア別平均"),
                         scoreText(output.singleAverage))
            summaryValue(Localization.tr("All-core", "全核", "全コア"), scoreText(output.multiScore))
        } else {
            Text(engine.lastScore.isEmpty ? "—" : engine.lastScore)
                .font(.title2).fontWeight(.semibold)
        }
    }

    private func summaryValue(_ title: String, _ value: String) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(title)
                .font(.caption)
                .foregroundStyle(.secondary)
            Text(value)
                .font(.title2).fontWeight(.semibold)
        }
    }

    @ViewBuilder
    private var resultsSection: some View {
        if engine.runOutput.hasMemoryResults {
            CpuMemoryResultsView(output: engine.runOutput)
        } else {
            CpuComputeResultsCard(output: engine.runOutput)
        }
    }

    private var outputCard: some View {
        GlassCard(padding: 12) {
            VStack(alignment: .leading, spacing: 4) {
                Text(Localization.tr("Output", "输出", "出力"))
                    .font(.subheadline).foregroundStyle(.secondary)
                ScrollViewReader { proxy in
                    ScrollView {
                        Text(engine.logOutput.isEmpty
                             ? Localization.tr("CPU output will appear here…",
                                              "CPU 输出将显示在此处…",
                                              "CPU 出力はここに表示されます…")
                             : engine.logOutput)
                            .font(.system(.caption, design: .monospaced))
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .textSelection(.enabled)
                            .id(logScrollID)
                    }
                    .frame(height: 180)
                    .onChange(of: engine.logOutput) { _ in
                        logScrollID += 1
                        proxy.scrollTo(logScrollID, anchor: .bottom)
                    }
                }
            }
        }
    }

    // MARK: - Text

    private var infoText: String {
        memoryMode
            ? Localization.tr(
                "Sweeps read, write and copy over data sets from 4 KiB to 256 MiB, on one thread and on all cores, then measures page mapping and storage. iOS does not let an app bind a thread to a core, so the one-thread sweep runs on the core the system picks, normally a performance core; efficiency cores are not measured separately. Result: cpu_membw_v1_ios_preview.",
                "在 4 KiB 到 256 MiB 的数据量上扫描读、写、复制，分别用单线程和全部核心，然后测量页面映射和存储。iOS 不允许 App 把线程绑定到某个核心，所以单线程扫描跑在系统挑选的核心上（通常是性能核），能效核不会单独测量。结果：cpu_membw_v1_ios_preview。",
                "4 KiB〜256 MiB のデータセットで読込・書込・コピーをスイープ（1 スレッドと全コア）、続いてページ割り当てとストレージを測定します。iOS ではスレッドをコアに固定できないため、1 スレッドのスイープはシステムが選んだコア（通常は高性能コア）で実行され、高効率コアは個別に測定されません。結果: cpu_membw_v1_ios_preview。")
            : Localization.tr(
                "Native mixed CPU kernel (per-core and/or all-core). Results use graphicsApi=CPU.",
                "原生混合 CPU 内核（单核和/或全核）。结果使用 graphicsApi=CPU。",
                "ネイティブ混合 CPU カーネル。結果は graphicsApi=CPU。")
    }

    /// "Apple A17 Pro · 6 logical · 6 physical · NEON".
    private var cpuLabel: String {
        let output = engine.runOutput
        var parts: [String] = []
        if !output.cpuName.isEmpty { parts.append(output.cpuName) }
        if output.logicalCount > 0 {
            parts.append("\(output.logicalCount)" + Localization.tr(" logical", " 个逻辑处理器", " 論理"))
        }
        if output.physicalCount > 0 {
            parts.append("\(output.physicalCount)" + Localization.tr(" physical", " 个物理核心", " 物理"))
        }
        if !output.vectorName.isEmpty { parts.append(output.vectorName) }
        return parts.joined(separator: " · ")
    }

    private func scoreText(_ score: ScoreValue?) -> String {
        guard let score = score, score.value > 0 else { return "—" }
        var text = EngineFormat.number(score.value) + " " + score.unit
        if score.threads > 1 {
            text += "  (\(score.threads)" + Localization.tr(" threads", " 线程", " スレッド") + ")"
        }
        return text
    }

    /// The memory read rate of one mode of the sweep, the headline figure.
    private func memoryHeadline(mode: String, threads: Int) -> String {
        guard let row = engine.runOutput.levels.first(where: { $0.mode == mode && $0.level == "memory" }),
              row.measured else { return "—" }
        var text = EngineFormat.number(row.read) + " GB/s"
        if threads > 1 {
            text += "  (\(threads)" + Localization.tr(" threads", " 线程", " スレッド") + ")"
        }
        if !row.settled {
            text += Localization.tr("  [not settled]", "  [未稳定]", "  [未収束]")
        }
        return text
    }

    private func runCpu() {
        if memoryMode {
            engine.run(jobs: [["gpu_benchmark", "--cpu-membw"]], needCharts: false)
            return
        }
        let sec = secondsPerTest.trimmingCharacters(in: .whitespaces)
        let warm = warmupSec.trimmingCharacters(in: .whitespaces)
        let args = [
            "gpu_benchmark",
            "--cpu-benchmark", mode,
            "--cpu-time", sec.isEmpty ? "1" : sec,
            "--cpu-warmup", warm.isEmpty ? "0.2" : warm,
        ]
        engine.run(jobs: [args], needCharts: false)
    }
}
