// AboutView.swift — iOS Version info, description, and system diagnostics.

import SwiftUI

struct AboutView: View {
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
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
                                Text("v1.0 — iOS Edition")
                                    .font(.callout)
                                    .foregroundStyle(.secondary)
                            }
                        }

                        Divider()

                        Text(Localization.tr(
                            """
                            Native iOS GPU frontend for Mangekyo, measuring compute and rendering \
                            performance across Metal. Features multiple workloads (particle stream, \
                            N-body simulation, stress fractal, synthetic peak throughput, 3D rendering) \
                            with real-time GPU timing.
                            
                            This is the native iOS SwiftUI front-end driving the C++ engine \
                            in-process — no subprocess or shell invocation required.
                            """,
                            """
                            Mangekyo 的原生 iOS GPU 前端，用于测量 Metal 的计算与渲染性能。\
                            包含多种负载（粒子流、N 体模拟、压力分形、合成峰值吞吐、3D 渲染），\
                            配有实时 GPU 计时。
                            
                            这是原生 iOS SwiftUI 前端，直接在进程内调用 C++ 引擎 —— \
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
                                systemInfoRow("iOS", UIDevice.current.systemVersion)
                                systemInfoRow("Device", cpuName())
                                systemInfoRow("Memory", memoryString())
                                systemInfoRow("Architecture", architectureString())
                            }
                        }
                    }
                }
            }
            .padding(16)
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
        var systemInfo = utsname()
        uname(&systemInfo)
        let machineMirror = Mirror(reflecting: systemInfo.machine)
        let identifier = machineMirror.children.reduce("") { identifier, element in
            guard let value = element.value as? Int8, value != 0 else { return identifier }
            return identifier + String(UnicodeScalar(UInt8(value)))
        }
        return identifier.isEmpty ? UIDevice.current.model : identifier
    }

    private func memoryString() -> String {
        let bytes = ProcessInfo.processInfo.physicalMemory
        return String(format: "%.0f GB", Double(bytes) / 1_073_741_824)
    }

    private func architectureString() -> String {
        #if arch(arm64)
        return "ARM64"
        #else
        return "Unknown / Simulator"
        #endif
    }
}
