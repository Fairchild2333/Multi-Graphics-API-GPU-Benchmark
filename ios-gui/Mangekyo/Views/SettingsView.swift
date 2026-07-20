// SettingsView.swift — iOS Theme and Language settings.

import SwiftUI

struct SettingsView: View {
    @EnvironmentObject var engine: BenchEngine
    @AppStorage("appTheme") private var appTheme: String = "system"
    @AppStorage("appLanguage") private var appLanguage: String = "auto"

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                GlassCard {
                    VStack(alignment: .leading, spacing: 20) {
                        // Theme
                        VStack(alignment: .leading, spacing: 4) {
                            Text(Localization.tr("Theme", "主题", "テーマ"))
                                .font(.headline)
                            Picker("", selection: $appTheme) {
                                Text(Localization.tr("Use system setting", "跟随系统", "システムに合わせる")).tag("system")
                                Text(Localization.tr("Light", "浅色", "ライト")).tag("light")
                                Text(Localization.tr("Dark", "深色", "ダーク")).tag("dark")
                            }
                            .labelsHidden()
                            .pickerStyle(.segmented)
                        }

                        Divider()

                        // Language
                        VStack(alignment: .leading, spacing: 4) {
                            Text(Localization.tr("Language", "语言", "言語"))
                                .font(.headline)
                            Picker("", selection: $appLanguage) {
                                ForEach(AppLanguage.allCases) { lang in
                                    Text(lang.displayName).tag(lang.rawValue)
                                }
                            }
                            .labelsHidden()
                            .onChange(of: appLanguage) { newValue in
                                Localization.current = AppLanguage(rawValue: newValue) ?? .auto_
                            }
                        }

                        Divider()

                        // Sandboxed Data Root
                        VStack(alignment: .leading, spacing: 8) {
                            Text(Localization.tr("Sandboxed Storage", "沙盒存储目录", "サンドボックスストレージ"))
                                .font(.headline)
                            Text(Localization.tr(
                                "Mangekyo runs inside a secure sandbox on iOS. Results and captures are stored here.",
                                "iOS 上 Mangekyo 运行在安全的沙盒中。结果和抓帧数据保存在此目录下。",
                                "iOSのサンドボックス内で実行されます。結果とキャプチャデータはここに保存されます。"))
                                .font(.caption)
                                .foregroundStyle(.secondary)

                            Text(engine.workingDirectory)
                                .font(.system(.caption2, design: .monospaced))
                                .padding(8)
                                .background(Color.primary.opacity(0.05))
                                .cornerRadius(6)
                                .textSelection(.enabled)
                        }
                    }
                }
            }
            .padding(16)
        }
        .onAppear {
            Localization.current = AppLanguage(rawValue: appLanguage) ?? .auto_
        }
    }
}
