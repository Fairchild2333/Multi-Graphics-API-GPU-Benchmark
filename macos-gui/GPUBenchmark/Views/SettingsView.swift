// SettingsView.swift — Theme, language, and working directory settings.

import SwiftUI

struct SettingsView: View {
    @EnvironmentObject var engine: BenchEngine
    @AppStorage("appTheme") private var appTheme: String = "system"
    @AppStorage("appLanguage") private var appLanguage: String = "auto"
    // Force view refresh when language changes by tracking a revision counter.
    @State private var languageRevision: Int = 0

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                Text(Localization.tr("Settings", "设置", "設定"))
                    .font(.largeTitle)
                    .fontWeight(.bold)
                    .frame(maxWidth: .infinity, alignment: .leading)

                GlassCard {
                    VStack(alignment: .leading, spacing: 20) {
                        // Theme
                        VStack(alignment: .leading, spacing: 4) {
                            Text(Localization.tr("Theme", "主题", "テーマ"))
                                .font(.headline)
                            Picker(selection: $appTheme) {
                                Text(Localization.tr("Use system setting", "跟随系统", "システムに合わせる")).tag("system")
                                Text(Localization.tr("Light", "浅色", "ライト")).tag("light")
                                Text(Localization.tr("Dark", "深色", "ダーク")).tag("dark")
                            } label: {
                                EmptyView()
                            }
                            .labelsHidden()
                            .pickerStyle(.segmented)
                            .fixedSize()
                        }

                        Divider()

                        // Language
                        VStack(alignment: .leading, spacing: 4) {
                            Text(Localization.tr("Language", "语言", "言語"))
                                .font(.headline)
                            Picker(selection: $appLanguage) {
                                ForEach(AppLanguage.allCases) { lang in
                                    Text(lang.displayName).tag(lang.rawValue)
                                }
                            } label: {
                                EmptyView()
                            }
                            .labelsHidden()
                            // A bare `.frame(width:)` centres the pop-up
                            // button and pushes it away from its label.
                            .frame(width: 160, alignment: .leading)
                            .onChange(of: appLanguage) { newValue in
                                Localization.current = AppLanguage(rawValue: newValue) ?? .auto_
                                engine.refreshLocalizedStatus()
                                // Bump revision to force an immediate full view re-render
                                languageRevision += 1
                            }
                        }

                        Divider()

                        // Runtime / optional development directory
                        VStack(alignment: .leading, spacing: 8) {
                            Text(Localization.tr(
                                "Runtime Directory",
                                "运行目录",
                                "ランタイムディレクトリ"))
                                .font(.headline)
                            Text(Localization.tr(
                                "Standalone builds use the bundled Helpers directory automatically. Choose a repository/build directory only to override the runtime during development. Results and captures always use Application Support.",
                                "独立应用会自动使用内置 Helpers 目录。仅在开发时需要覆盖 Runtime，才选择仓库或构建目录。成绩与抓帧始终使用 Application Support。",
                                "単体アプリは同梱 Helpers を自動使用します。開発時にランタイムを差し替える場合だけリポジトリ/ビルドを選択してください。結果とキャプチャは常に Application Support に保存されます。"))
                                .font(.caption)
                                .foregroundStyle(.secondary)

                            HStack {
                                TextField("", text: .constant(engine.workingDirectory))
                                    .textFieldStyle(.roundedBorder)
                                    .disabled(true)
                                Button(Localization.tr("Choose…", "选择…", "選択…")) {
                                    chooseDirectory()
                                }
                            }

                            if let cli = engine.resolveCliExecutable() {
                                Label(
                                    Localization.tr(
                                        "Isolated benchmark worker ready: \(URL(fileURLWithPath: cli).lastPathComponent)",
                                        "隔离测试 worker 已就绪：\(URL(fileURLWithPath: cli).lastPathComponent)",
                                        "分離ベンチマークワーカー準備完了：\(URL(fileURLWithPath: cli).lastPathComponent)"),
                                    systemImage: "checkmark.circle.fill")
                                    .foregroundStyle(.green)
                                    .font(.caption)
                            } else {
                                Label(
                                    Localization.tr(
                                        "External worker not found; the development in-process bridge remains available.",
                                        "未找到外部 worker；仍可使用开发用进程内桥接。",
                                        "外部ワーカーが見つかりません。開発用のプロセス内ブリッジを使用できます。"),
                                    systemImage: "info.circle")
                                    .foregroundStyle(.secondary)
                                    .font(.caption)
                            }
                        }
                    }
                }
                .frame(maxWidth: 520)
            }
            .padding(28)
            .id(languageRevision) // Force complete re-layout on language change
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
            "Select a Mangekyo repository or runtime directory",
            "选择 Mangekyo 仓库或运行目录",
            "Mangekyo リポジトリまたはランタイムディレクトリを選択")

        if panel.runModal() == .OK, let url = panel.url {
            engine.setWorkingDirectory(url.path)
        }
    }
}
