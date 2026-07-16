// RunView.swift — Benchmark run configuration and execution page.
// Feature-parity with WinUI 3 Run page: presets, GPU/API/workload selection,
// advanced toggles, real-time log output, and score display.

import SwiftUI

/// Preset configurations matching the WinUI 3 GUI presets.
enum BenchPreset: String, CaseIterable, Identifiable {
    case quick       = "Quick run (best API / GPU, Medium)"
    case custom      = "Custom run (choose API / GPU / workload)"
    case fullOne     = "Full analysis — one GPU (all APIs + charts)"
    case fullAll     = "Full analysis — all GPUs × APIs (+ charts)"
    case flights     = "Flights test — one GPU (all APIs, custom flights)"
    case particles   = "Particle test — one GPU (all APIs, custom particles)"
    case headless    = "Headless compute — one GPU (all APIs, pure compute)"

    var id: String { rawValue }
}

struct RunView: View {
    @EnvironmentObject var engine: BenchEngine

    // --- Run configuration state ---
    @State private var preset: BenchPreset = .custom
    @State private var selectedGpuIndex: Int = -1   // -1 = auto
    @State private var selectedAPI: String = "Auto"
    @State private var selectedWorkload: String = "stream"
    @State private var selectedPrecision: String = "fp32"
    @State private var frames: String = "600"
    @State private var extra: String = ""

    // Advanced toggles
    @State private var headless: Bool = false
    @State private var vsync: Bool = false
    @State private var hostMemory: Bool = false

    // UI state
    @State private var logScrollID: Int = 0

    private let workloads = ["stream", "nbody", "stress", "synthpeak", "render3d"]
    private let precisions = ["fp32", "fp16", "fp64", "int32"]

    var body: some View {
        VStack(spacing: 0) {
            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    Text(Localization.tr("GPU Benchmark", "GPU 跑分"))
                        .font(.largeTitle)
                        .fontWeight(.bold)
                        .padding(.bottom, 4)

                    // --- Options card ---
                    GlassCard {
                        VStack(alignment: .leading, spacing: 16) {
                            // Preset picker
                            VStack(alignment: .leading, spacing: 4) {
                                Text("Preset")
                                    .font(.headline)
                                Picker("", selection: $preset) {
                                    ForEach(BenchPreset.allCases) { p in
                                        Text(p.rawValue).tag(p)
                                    }
                                }
                                .labelsHidden()
                                .pickerStyle(.menu)
                                .frame(maxWidth: .infinity, alignment: .leading)
                            }

                            // GPU / API / Workload / Precision row
                            HStack(spacing: 16) {
                                VStack(alignment: .leading, spacing: 4) {
                                    Text("GPU")
                                        .font(.subheadline).foregroundStyle(.secondary)
                                    Picker("", selection: $selectedGpuIndex) {
                                        Text(Localization.tr("(auto)", "（自动）")).tag(-1)
                                        ForEach(engine.gpus) { gpu in
                                            Text("\(gpu.index): \(gpu.name)").tag(gpu.index)
                                        }
                                    }
                                    .labelsHidden()
                                    .frame(minWidth: 200)
                                }

                                VStack(alignment: .leading, spacing: 4) {
                                    Text(Localization.tr("Graphics API", "图形 API"))
                                        .font(.subheadline).foregroundStyle(.secondary)
                                    Picker("", selection: $selectedAPI) {
                                        ForEach(macOSAPIs, id: \.self) { api in
                                            Text(api).tag(api)
                                        }
                                    }
                                    .labelsHidden()
                                    .frame(width: 130)
                                }

                                VStack(alignment: .leading, spacing: 4) {
                                    Text("Workload")
                                        .font(.subheadline).foregroundStyle(.secondary)
                                    Picker("", selection: $selectedWorkload) {
                                        ForEach(workloads, id: \.self) { w in
                                            Text(w).tag(w)
                                        }
                                    }
                                    .labelsHidden()
                                    .frame(width: 130)
                                }

                                if selectedWorkload == "synthpeak" {
                                    VStack(alignment: .leading, spacing: 4) {
                                        Text("Precision")
                                            .font(.subheadline).foregroundStyle(.secondary)
                                        Picker("", selection: $selectedPrecision) {
                                            ForEach(precisions, id: \.self) { p in
                                                Text(p).tag(p)
                                            }
                                        }
                                        .labelsHidden()
                                        .frame(width: 100)
                                    }
                                }
                            }

                            // Frames / Extra row
                            HStack(spacing: 16) {
                                VStack(alignment: .leading, spacing: 4) {
                                    Text("Frames")
                                        .font(.subheadline).foregroundStyle(.secondary)
                                    TextField("600", text: $frames)
                                        .textFieldStyle(.roundedBorder)
                                        .frame(width: 100)
                                }
                                VStack(alignment: .leading, spacing: 4) {
                                    Text(extraLabel)
                                        .font(.subheadline).foregroundStyle(.secondary)
                                    TextField(Localization.tr("optional", "可选"), text: $extra)
                                        .textFieldStyle(.roundedBorder)
                                        .frame(width: 260)
                                }
                            }
                        }
                    }

                    // --- Advanced card ---
                    GlassCard(padding: 16) {
                        VStack(alignment: .leading, spacing: 8) {
                            Text(Localization.tr("Advanced", "高级选项"))
                                .font(.headline)
                            HStack(spacing: 24) {
                                Toggle("Headless", isOn: $headless)
                                Toggle("V-Sync", isOn: $vsync)
                                Toggle(Localization.tr("Host memory", "主机内存"), isOn: $hostMemory)
                            }
                            .toggleStyle(.checkbox)
                        }
                    }

                    // --- Score / Result card ---
                    AccentGlassCard {
                        HStack {
                            Text(engine.lastScore.isEmpty ? "—" : engine.lastScore)
                                .font(.title2)
                                .fontWeight(.semibold)
                                .foregroundStyle(engine.lastScore.isEmpty ? .secondary : .primary)
                            Spacer()
                        }
                    }

                    // --- Log output ---
                    GlassCard(padding: 12) {
                        VStack(alignment: .leading, spacing: 4) {
                            Text(Localization.tr("Output", "输出"))
                                .font(.subheadline)
                                .foregroundStyle(.secondary)
                            ScrollViewReader { proxy in
                                ScrollView {
                                    Text(engine.logOutput.isEmpty
                                         ? Localization.tr("Benchmark output will appear here…",
                                                          "基准测试输出将显示在此处…")
                                         : engine.logOutput)
                                        .font(.system(.caption, design: .monospaced))
                                        .frame(maxWidth: .infinity, alignment: .leading)
                                        .textSelection(.enabled)
                                        .id(logScrollID)
                                }
                                .frame(height: 200)
                                .onChange(of: engine.logOutput) { _ in
                                    logScrollID += 1
                                    withAnimation {
                                        proxy.scrollTo(logScrollID, anchor: .bottom)
                                    }
                                }
                            }
                        }
                    }
                }
                .padding(28)
            }

            // --- Bottom action bar ---
            HStack {
                Spacer()
                Text(engine.statusText)
                    .foregroundStyle(.secondary)
                    .font(.callout)
                if engine.isRunning {
                    ProgressView()
                        .controlSize(.small)
                }
                Button(action: runBenchmark) {
                    Text(Localization.tr("Run GPU Benchmark", "开始 GPU 跑分"))
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.large)
                .disabled(engine.isRunning || engine.workingDirectory.isEmpty)
            }
            .padding(.horizontal, 28)
            .padding(.vertical, 12)
        }
    }

    // MARK: - Helpers

    /// Available APIs on macOS (filter based on detected GPU capabilities).
    private var macOSAPIs: [String] {
        var apis = ["Auto"]
        if engine.gpus.contains(where: { $0.supportsMetal })  { apis.append("Metal") }
        if engine.gpus.contains(where: { $0.supportsVulkan }) { apis.append("Vulkan") }
        if engine.gpus.contains(where: { $0.supportsOpenGL }) { apis.append("OpenGL") }
        if apis.count == 1 { apis = BenchEngine.availableAPIs }  // fallback before enumeration
        return apis
    }

    /// Dynamic label for the "Extra" text field based on workload/preset.
    private var extraLabel: String {
        switch preset {
        case .flights:   return "Flights (--flights)"
        case .particles: return "Particles (--particles)"
        default:
            switch selectedWorkload {
            case "nbody":               return "Bodies (--bodies)"
            case "stress", "synthpeak": return "Iterations (--iter)"
            default:                    return "Extra"
            }
        }
    }

    /// Build argv and run.
    private func runBenchmark() {
        var needCharts = false
        let jobs = buildPresetJobs(needCharts: &needCharts)
        guard !jobs.isEmpty else { return }
        engine.run(jobs: jobs, needCharts: needCharts)
    }

    /// Map the selected API display name to the engine token.
    private func backendValue() -> String {
        switch selectedAPI {
        case "Metal":  return "metal"
        case "Vulkan": return "vulkan"
        case "OpenGL": return "opengl"
        default:       return ""  // Auto
        }
    }

    /// Build the argv for a single custom run.
    private func buildArgs(runAll: Bool = false) -> [String] {
        var args = ["gpu_benchmark", "--benchmark", frames.isEmpty ? "600" : frames]

        if runAll {
            args.append("--run-all")
        } else {
            let b = backendValue()
            if !b.isEmpty { args += ["--backend", b] }
            if selectedGpuIndex >= 0 { args += ["--gpu", "\(selectedGpuIndex)"] }
        }

        if selectedWorkload != "stream" {
            args += ["--workload", selectedWorkload]
        }
        if selectedWorkload == "synthpeak" {
            args += ["--precision", selectedPrecision]
        }
        if !extra.isEmpty {
            let flag: String
            switch selectedWorkload {
            case "nbody":               flag = "--bodies"
            case "stress", "synthpeak": flag = "--iter"
            default:                    flag = "--particles"
            }
            args += [flag, extra]
        }

        if headless    { args.append("--headless") }
        if vsync       { args.append("--vsync") }
        if hostMemory  { args.append("--host-memory") }

        return args
    }

    /// Build job(s) based on the selected preset (mirrors WinUI buildPresetJobs).
    private func buildPresetJobs(needCharts: inout Bool) -> [[String]] {
        needCharts = false
        let f = frames.isEmpty ? "600" : frames

        // Helper: build one job per API for the selected GPU.
        let multiApi: ([String]) -> [[String]] = { extraArgs in
            var jobs: [[String]] = []
            for api in ["metal", "vulkan", "opengl"] {
                var a = ["gpu_benchmark", "--benchmark", f, "--backend", api]
                if self.selectedGpuIndex >= 0 {
                    a += ["--gpu", "\(self.selectedGpuIndex)"]
                }
                if self.selectedWorkload != "stream" {
                    a += ["--workload", self.selectedWorkload]
                }
                if self.selectedWorkload == "synthpeak" {
                    a += ["--precision", self.selectedPrecision]
                }
                a += extraArgs
                jobs.append(a)
            }
            return jobs
        }

        switch preset {
        case .quick:
            return [["gpu_benchmark", "--benchmark", f]]
        case .custom:
            return [buildArgs()]
        case .fullOne:
            needCharts = true
            return multiApi(["--capture", "5"])
        case .fullAll:
            needCharts = true
            var a = ["gpu_benchmark", "--benchmark", f, "--run-all", "--capture", "5"]
            if selectedWorkload != "stream" { a += ["--workload", selectedWorkload] }
            if selectedWorkload == "synthpeak" { a += ["--precision", selectedPrecision] }
            return [a]
        case .flights:
            return multiApi(extra.isEmpty ? [] : ["--flights", extra])
        case .particles:
            return multiApi(extra.isEmpty ? [] : ["--particles", extra])
        case .headless:
            return multiApi(["--headless"])
        }
    }
}
