// AboutView.swift — Version info, description, and GitHub link.

import SwiftUI

struct AboutView: View {
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                Text(Localization.tr("About", "关于"))
                    .font(.largeTitle)
                    .fontWeight(.bold)

                GlassCard {
                    VStack(alignment: .leading, spacing: 14) {
                        HStack(spacing: 16) {
                            Image(systemName: "cpu.fill")
                                .font(.system(size: 48))
                                .foregroundStyle(.tint)
                            VStack(alignment: .leading, spacing: 4) {
                                Text("Mangekyo")
                                    .font(.title2)
                                    .fontWeight(.bold)
                                Text(Localization.tr(
                                    "Cross-API CPU & GPU Benchmark Suite",
                                    "跨 API CPU 与 GPU 跑分套件"))
                                    .font(.subheadline)
                                    .foregroundStyle(.secondary)
                                Text("v1.0 — macOS Edition")
                                    .font(.callout)
                                    .foregroundStyle(.secondary)
                            }
                        }

                        Divider()

                        Text(Localization.tr(
                            """
                            Native macOS GPU frontend for Mangekyo, measuring compute and rendering \
                            performance across Metal, Vulkan, and OpenGL. Features multiple \
                            workloads (particle stream, N-body simulation, stress fractal, \
                            synthetic peak throughput, 3D rendering) with real-time GPU timing.
                            
                            This is the native macOS SwiftUI front-end driving the C++ engine \
                            in-process — no subprocess or shell invocation required.
                            """,
                            """
                            Mangekyo 的原生 macOS GPU 前端，用于测量 Metal、Vulkan 和 OpenGL 的计算与渲染\
                            性能。包含多种负载（粒子流、N 体模拟、压力分形、合成峰值吞吐、3D 渲染），\
                            配有实时 GPU 计时。
                            
                            这是原生 macOS SwiftUI 前端，直接在进程内调用 C++ 引擎 —— \
                            无需子进程或 Shell 调用。
                            """))
                            .font(.body)
                            .foregroundStyle(.secondary)

                        Divider()

                        HStack(spacing: 20) {
                            Link(destination: URL(string: "https://github.com/Fairchild2333/Vulkan-GPU-Compute-Microbenchmark")!) {
                                Label("GitHub", systemImage: "link")
                            }

                            Text("MIT License")
                                .font(.caption)
                                .foregroundStyle(.tertiary)
                        }

                        // System info
                        GlassCard(padding: 12, cornerRadius: 8) {
                            VStack(alignment: .leading, spacing: 6) {
                                Text(Localization.tr("System Information", "系统信息"))
                                    .font(.subheadline)
                                    .fontWeight(.medium)
                                systemInfoRow("macOS", ProcessInfo.processInfo.operatingSystemVersionString)
                                systemInfoRow("CPU", cpuName())
                                systemInfoRow("Memory", memoryString())
                                systemInfoRow("Architecture", architectureString())
                            }
                        }
                    }
                }
                .frame(maxWidth: 560)
            }
            .padding(28)
        }
    }

    // MARK: - Helpers

    @ViewBuilder
    private func systemInfoRow(_ label: String, _ value: String) -> some View {
        HStack {
            Text(label)
                .font(.caption)
                .foregroundStyle(.secondary)
                .frame(width: 90, alignment: .leading)
            Text(value)
                .font(.system(.caption, design: .monospaced))
                .textSelection(.enabled)
        }
    }

    private func cpuName() -> String {
        var size: Int = 0
        sysctlbyname("machdep.cpu.brand_string", nil, &size, nil, 0)
        var buf = [CChar](repeating: 0, count: size)
        sysctlbyname("machdep.cpu.brand_string", &buf, &size, nil, 0)
        return String(cString: buf)
    }

    private func memoryString() -> String {
        let bytes = ProcessInfo.processInfo.physicalMemory
        return String(format: "%.0f GB", Double(bytes) / 1_073_741_824)
    }

    private func architectureString() -> String {
        #if arch(arm64)
        return "Apple Silicon (arm64)"
        #elseif arch(x86_64)
        return "Intel (x86_64)"
        #else
        return "Unknown"
        #endif
    }
}
