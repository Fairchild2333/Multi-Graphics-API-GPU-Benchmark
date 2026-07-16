// SettingsView.swift — Theme, language, and working directory settings.

import SwiftUI

struct SettingsView: View {
    @EnvironmentObject var engine: BenchEngine
    @AppStorage("appTheme") private var appTheme: String = "system"
    @AppStorage("appLanguage") private var appLanguage: String = "auto"

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                Text(Localization.tr("Settings", "设置"))
                    .font(.largeTitle)
                    .fontWeight(.bold)

                GlassCard {
                    VStack(alignment: .leading, spacing: 20) {
                        // Theme
                        VStack(alignment: .leading, spacing: 4) {
                            Text(Localization.tr("Theme", "主题"))
                                .font(.headline)
                            Picker("", selection: $appTheme) {
                                Text(Localization.tr("Use system setting", "跟随系统")).tag("system")
                                Text(Localization.tr("Light", "浅色")).tag("light")
                                Text(Localization.tr("Dark", "深色")).tag("dark")
                            }
                            .labelsHidden()
                            .pickerStyle(.segmented)
                            .frame(width: 360)
                        }

                        Divider()

                        // Language
                        VStack(alignment: .leading, spacing: 4) {
                            Text(Localization.tr("Language", "语言"))
                                .font(.headline)
                            Picker("", selection: $appLanguage) {
                                ForEach(AppLanguage.allCases) { lang in
                                    Text(lang.displayName).tag(lang.rawValue)
                                }
                            }
                            .labelsHidden()
                            .frame(width: 220)
                            .onChange(of: appLanguage) { newValue in
                                Localization.current = AppLanguage(rawValue: newValue) ?? .auto_
                            }
                        }

                        Divider()

                        // Working directory
                        VStack(alignment: .leading, spacing: 8) {
                            Text(Localization.tr("Working Directory", "工作目录"))
                                .font(.headline)
                            Text(Localization.tr(
                                "Path to the repository root. Required for results, shaders, and charts.",
                                "仓库根目录路径。结果、着色器和图表需要此路径。"))
                                .font(.caption)
                                .foregroundStyle(.secondary)

                            HStack {
                                TextField("", text: .constant(engine.workingDirectory))
                                    .textFieldStyle(.roundedBorder)
                                    .disabled(true)
                                Button(Localization.tr("Choose…", "选择…")) {
                                    chooseDirectory()
                                }
                            }

                            if engine.workingDirectory.isEmpty {
                                Label(Localization.tr(
                                    "No working directory set. Please select the repository root.",
                                    "未设置工作目录。请选择仓库根目录。"),
                                      systemImage: "exclamationmark.triangle.fill")
                                    .foregroundStyle(.orange)
                                    .font(.caption)
                            } else {
                                Label(Localization.tr("Directory set.", "目录已设置。"),
                                      systemImage: "checkmark.circle.fill")
                                    .foregroundStyle(.green)
                                    .font(.caption)
                            }
                        }
                    }
                }
                .frame(maxWidth: 520)
            }
            .padding(28)
        }
        .onAppear {
            Localization.current = AppLanguage(rawValue: appLanguage) ?? .auto_
        }
    }

    private func chooseDirectory() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true
        panel.canChooseFiles = false
        panel.allowsMultipleSelection = false
        panel.message = Localization.tr(
            "Select the Mangekyo repository root directory",
            "选择 Mangekyo 仓库根目录")

        if panel.runModal() == .OK, let url = panel.url {
            engine.setWorkingDirectory(url.path)
        }
    }
}
