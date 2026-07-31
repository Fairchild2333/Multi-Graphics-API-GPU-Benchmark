// AboutView.swift — Version info, description, and GitHub link.

import AppKit
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
                            // The app artwork rather than an SF Symbol. This is
                            // a normal image set, not the app icon set, because
                            // only image sets honour a dark appearance — macOS
                            // itself has no dark Dock icon before 26.
                            Image("AppIconArt")
                                .resizable()
                                .interpolation(.high)
                                .frame(width: 64, height: 64)
                                .accessibilityHidden(true)
                            VStack(alignment: .leading, spacing: 4) {
                                Text("Mangekyo")
                                    .font(.title2)
                                    .fontWeight(.bold)
                                Text(Localization.tr(
                                    "Cross-API CPU & GPU Benchmark Suite",
                                    "跨 API CPU 与 GPU 测试套件"))
                                    .font(.subheadline)
                                    .foregroundStyle(.secondary)
                                Text("v\(appVersion) — macOS (\(archLabel))")
                                    .font(.callout)
                                    .foregroundStyle(.secondary)
                            }
                        }

                        Divider()

                        Text(Localization.tr(
                            """
                            Mangekyo measures how fast your Mac's CPU and GPU are. It runs a set \
                            of workloads — particles, GPU burn, and an interactive fluid pool — \
                            and turns each run into a score you can compare.

                            Every run is saved, so you can look back at past results in History \
                            and compare them side by side in Charts.
                            """,
                            """
                            Mangekyo 用来测量 Mac 的 CPU 与 GPU 性能。它会运行一组负载——粒子、\
                            GPU Burn、互动水池——并把每次运行换算成可以互相比较的成绩。

                            每次运行都会自动保存，可以在「历史」里回看，在「图表」里对比。
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

    private var appVersion: String {
        Bundle.main.object(
            forInfoDictionaryKey: "CFBundleShortVersionString"
        ) as? String ?? "Unknown"
    }

    private var archLabel: String {
        #if arch(arm64)
        return "arm64"
        #elseif arch(x86_64)
        return "x86_64"
        #else
        return "unknown"
        #endif
    }

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
        guard size > 1 else {
            return architectureString()
        }
        var buf = [CChar](repeating: 0, count: size)
        sysctlbyname("machdep.cpu.brand_string", &buf, &size, nil, 0)
        let bytes = buf.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }
        let name = String(decoding: bytes, as: UTF8.self)
        return name.isEmpty ? architectureString() : name
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
