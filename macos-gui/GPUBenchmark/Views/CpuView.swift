// CpuView.swift — WinUI-aligned CPU benchmark page.

import SwiftUI

struct CpuView: View {
    @EnvironmentObject var engine: BenchEngine

    @State private var mode: String = "all"          // per-core | multi | all | memory
    @State private var durationPreset: String = "15"  // 1 | 15
    @State private var secondsPerTest: String = "15"
    @State private var warmupSec: String = "0.2"
    @State private var logScrollID = 0

    private var memoryMode: Bool { mode == "memory" }
    /// This page only ever reads and writes the CPU channel.
    private var run: BenchChannelState { engine.cpuState }

    var body: some View {
        VStack(spacing: 0) {
            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    VStack(alignment: .leading, spacing: 4) {
                        Text(Localization.tr("CPU Benchmark", "CPU 测试", "CPU ベンチマーク"))
                            .font(.largeTitle).fontWeight(.bold)
                        Text(Localization.tr(
                            "Measures CPU compute throughput with dense math loops on logical processors (single-core and multi-core), or the bandwidth of every cache level and of memory.",
                            "用密集数学循环测量逻辑处理器上的 CPU 计算吞吐（单核与多核），或测量各级缓存与内存的带宽。",
                            "論理プロセッサ上の密集数学ループで CPU スループットを測定（単一/複数コア）、または各キャッシュ階層とメモリの帯域を測定します。"))
                            .font(.callout)
                            .foregroundStyle(.secondary)
                            .fixedSize(horizontal: false, vertical: true)
                    }

                    optionsCard
                    progressCard
                    summaryCard
                    resultsSection
                    outputCard
                }
                .padding(28)
            }

            HStack {
                Spacer()
                Circle()
                    .fill(run.isRunning ? Color.orange : Color.green)
                    .frame(width: 10, height: 10)
                Text(run.statusText)
                    .foregroundStyle(.secondary)
                    .font(.callout)
                    .lineLimit(1)
                if run.isRunning {
                    ProgressView().controlSize(.small)
                }
                Button(Localization.tr("Cancel", "取消", "キャンセル")) {
                    engine.cancel(channel: .cpu)
                }
                .disabled(!run.isRunning)
                Button {
                    engine.run(jobs: [["gpu_benchmark", "--cpu-suite"]], needCharts: false, channel: .cpu)
                } label: {
                    Text(Localization.tr("Run all CPU tests", "一键测试 CPU 全套", "CPU 全項目を実行"))
                }
                .buttonStyle(.borderedProminent)
                .disabled(engine.isRunning || engine.workingDirectory.isEmpty)
                Button(action: runCpu) {
                    Text(memoryMode
                         ? Localization.tr("Run Cache & Memory Test", "开始缓存与内存测试", "キャッシュ / メモリテストを実行")
                         : Localization.tr("Run CPU Benchmark", "开始 CPU 测试", "CPU ベンチマークを実行"))
                }
                .buttonStyle(.bordered)
                .controlSize(.large)
                .disabled(engine.isRunning || engine.workingDirectory.isEmpty)
            }
            .padding(.horizontal, 28)
            .padding(.vertical, 12)
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

                HStack(spacing: 16) {
                    VStack(alignment: .leading, spacing: 4) {
                        Text(Localization.tr("Test mode", "测试模式", "テストモード"))
                            .font(.subheadline).foregroundStyle(.secondary)
                        Picker("", selection: $mode) {
                            Text(Localization.tr("Single thread", "单线程", "1 スレッド")).tag("per-core")
                            Text(Localization.tr("All-core", "全核", "全コア")).tag("multi")
                            Text(Localization.tr("Single thread + All-core", "单线程 + 全核", "1 スレッド + 全コア")).tag("all")
                            Text(Localization.tr("Cache & memory bandwidth", "缓存与内存带宽", "キャッシュ / メモリ帯域")).tag("memory")
                        }
                        .labelsHidden()
                        .frame(minWidth: 200)
                    }

                    if !memoryMode { durationFields }
                }

                Text(memoryMode
                     ? Localization.tr(
                        "Takes about a minute. The storage test writes a temporary file of up to 1 GiB in the results folder and removes it afterwards.",
                        "大核与小核完整扫描需要几分钟。存储测试会写一个最大 1 GiB 的临时文件，测完即删除。",
                        "約 1 分かかります。ストレージテストは結果フォルダに最大 1 GiB の一時ファイルを書き込み、終了後に削除します。")
                     : Localization.tr(
                        "Single-thread tests automatically cover performance and efficiency cores where supported. All-core always uses every core.",
                        "单线程默认分别测大核和小核；全核始终使用全部核心，无需选择核心类型。",
                        "対応環境では性能・効率コアを自動測定します。全コアでは全てのコアを使用します。"))
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
            .frame(width: 160)
            .onChange(of: durationPreset) { v in
                secondsPerTest = v
            }
        }

        VStack(alignment: .leading, spacing: 4) {
            Text(Localization.tr("Seconds per test", "每项测试秒数", "テストごとの秒数"))
                .font(.subheadline).foregroundStyle(.secondary)
            TextField("1", text: $secondsPerTest)
                .textFieldStyle(.roundedBorder)
                .frame(width: 100)
        }

        VStack(alignment: .leading, spacing: 4) {
            Text(Localization.tr("Warm-up seconds", "预热秒数", "ウォームアップ秒数"))
                .font(.subheadline).foregroundStyle(.secondary)
            TextField("0.2", text: $warmupSec)
                .textFieldStyle(.roundedBorder)
                .frame(width: 100)
        }
    }

    private var progressCard: some View {
        GlassCard(padding: 16) {
            VStack(alignment: .leading, spacing: 8) {
                HStack {
                    Text(run.statusText).fontWeight(.semibold)
                    Spacer()
                    Text(run.progressLabel).foregroundStyle(.secondary)
                }
                ProgressView(value: run.progressFraction)
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
                Button(Localization.tr("Open results folder", "打开结果文件夹", "結果フォルダを開く")) {
                    engine.openResultsFolder()
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    @ViewBuilder
    private var summaryValues: some View {
        let output = run.runOutput
        if output.hasMemoryResults {
            summaryValue(Localization.tr("Memory read, one thread", "内存读取，单线程", "メモリ読込、1 スレッド"),
                         memoryHeadline(mode: output.classes.first?.mode ?? "single", threads: 1))
            summaryValue(Localization.tr("Memory read, all cores", "内存读取，全核", "メモリ読込、全コア"),
                         memoryHeadline(mode: "all", threads: output.allThreads))
        }
        if output.hasComputeResults {
            ForEach(output.coreScores.filter { $0.coreType != "auto" }) { score in
                summaryValue(score.label, EngineFormat.number(score.score) + " " + score.unit)
            }
            if output.coreTypeSelection == "auto" { summaryValue(Localization.tr("Single-thread sample average", "单线程采样平均", "1 スレッド実行の平均"),
                         scoreText(output.singleAverage)) }
            summaryValue(Localization.tr("All-core", "全核", "全コア"), scoreText(output.multiScore))
        } else if !output.hasMemoryResults {
            Text(run.lastScore.isEmpty ? "—" : run.lastScore)
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
        if run.runOutput.hasMemoryResults {
            CpuMemoryResultsView(output: run.runOutput)
        }
        if run.runOutput.hasComputeResults {
            CpuComputeResultsCard(output: run.runOutput)
        }
    }

    private var outputCard: some View {
        GlassCard(padding: 12) {
            VStack(alignment: .leading, spacing: 4) {
                Text(Localization.tr("Output", "输出", "出力"))
                    .font(.subheadline).foregroundStyle(.secondary)
                ScrollViewReader { proxy in
                    ScrollView {
                        Text(run.logOutput.isEmpty
                             ? Localization.tr("CPU output will appear here…",
                                              "CPU 输出将显示在此处…",
                                              "CPU 出力はここに表示されます…")
                             : run.logOutput)
                            .font(.system(.caption, design: .monospaced))
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .textSelection(.enabled)
                            .id(logScrollID)
                    }
                    .frame(height: 180)
                    .onChange(of: run.logOutput) { _ in
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
                "Automatically sweeps both core types and all-core memory, then measures page mapping and storage. Every classified sample verifies actual core-type CPU time.",
                "默认扫描大核、小核缓存与内存，以及全核内存，再测页面首次映射和换页磁盘速度。每轮核对核心类型归属，未通过就重试。",
                "4 KiB〜1 GiB のデータセットで読込・書込・コピーをスイープ（1 スレッドと全コア）、続いてページ割り当てと仮想メモリのストレージを測定します。macOS ではスレッドをコアに固定できないため、1 スレッドのスイープはシステムが選んだコアで実行されます。結果: cpu_membw_v1。")
            : Localization.tr(
                "Uses the same mixed compute workload for single-thread and all-core scores. Selected core types are saved separately with verified CPU time and observed clock.",
                "使用相同的混合计算负载测量单线程和全核速度。大核／小核成绩分别保存，并记录实际归属与时钟。",
                "ネイティブ混合 CPU カーネル。結果は graphicsApi=CPU。")
    }

    /// "Apple M2 · 8 logical · 8 physical · NEON".
    private var cpuLabel: String {
        let output = run.runOutput
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
        guard let row = run.runOutput.levels.first(where: { $0.mode == mode && $0.level == "memory" }),
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
            engine.run(jobs: [["gpu_benchmark", "--cpu-membw"]], needCharts: false, channel: .cpu)
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
        // Prefer direct mode; engine CLI accepts --cpu-benchmark <mode>
        engine.run(jobs: [args], needCharts: false, channel: .cpu)
    }
}
