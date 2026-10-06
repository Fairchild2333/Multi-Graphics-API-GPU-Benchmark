// Localization.swift — Tiny i18n layer matching gui/i18n.h.
// Supports English, Simplified Chinese, and Japanese with auto-detection.

import Foundation

enum AppLanguage: String, CaseIterable, Identifiable {
    case auto_ = "auto"
    case en = "en"
    case zh = "zh"
    case ja = "ja"

    var id: String { rawValue }

    var displayName: String {
        switch self {
        case .auto_: return "Auto"
        case .en:    return "English"
        case .zh:    return "简体中文"
        case .ja:    return "日本語"
        }
    }
}

final class Localization {
    nonisolated(unsafe) static var current: AppLanguage = .auto_

    /// Resolve the effective language (handles "auto").
    static var effectiveLang: AppLanguage {
        if current != .auto_ { return current }
        let lang = Locale.current.language.languageCode?.identifier ?? "en"
        if lang.hasPrefix("zh") { return .zh }
        if lang.hasPrefix("ja") { return .ja }
        return .en
    }

    /// Translate: EN / ZH / JA (JA falls back to EN when omitted).
    static func tr(_ en: String, _ zh: String, _ ja: String? = nil) -> String {
        switch effectiveLang {
        case .zh: return zh
        case .ja: return ja ?? en
        default:  return en
        }
    }

    /// The engine prints its summary in English. This translates the label at
    /// the start of a score line, such as "VRAM rate:", and keeps the figures.
    /// The wording matches the Windows app.
    static func scoreLine(_ line: String) -> String {
        // "VRAM rate:" comes before "RAM rate:", which it contains.
        let labels: [(en: String, zh: String, ja: String)] = [
            ("VRAM rate:", "显存速率:", "VRAM レート:"),
            ("RAM rate:", "内存速率:", "RAM レート:"),
            ("Memory rate:", "显存速率:", "VRAM レート:"),
            ("Avg FPS:", "平均帧率:", "平均 FPS:"),
            ("Compute rate:", "计算速率:", "コンピュートレート:"),
            ("Burn rate:", "Burn 速率:", "Burn レート:"),
            ("Stress rate:", "压力速率:", "ストレスレート:"),
            ("Fill rate:", "填充速率:", "フィルレート:"),
            ("Render rate:", "渲染速率:", "レンダーレート:"),
            ("Vol rate:", "体积速率:", "ボリュームレート:"),
            ("Fluid rate:", "流体速率:", "流体レート:"),
            ("Liquid rate:", "流体速率:", "流体レート:"),
        ]
        for label in labels where line.hasPrefix(label.en) {
            let rest = line.dropFirst(label.en.count).trimmingCharacters(in: .whitespaces)
            switch effectiveLang {
            case .zh:
                return label.zh + " " + rest.replacingOccurrences(of: "ms compute", with: "ms 计算")
                    .replacingOccurrences(of: "ms render", with: "ms 渲染")
            case .ja:
                return label.ja + " " + rest.replacingOccurrences(of: "ms compute", with: "ms コンピュート")
                    .replacingOccurrences(of: "ms render", with: "ms レンダー")
            default: return line
            }
        }
        return line
    }

    /// The engine names the paging target and the reason for skipping the
    /// storage test in English. These are all the phrases it uses.
    static func engineText(_ text: String) -> String {
        if text.hasPrefix("page file on ") {
            let drive = String(text.dropFirst("page file on ".count))
            return tr(text, drive + " 盘上的页面文件", drive + " のページファイル")
        }
        if text.hasPrefix("swap on ") {
            let device = String(text.dropFirst("swap on ".count))
            return tr(text, device + " 上的交换分区", device + " のスワップ")
        }
        let phrases: [(en: String, zh: String, ja: String)] = [
            ("no page file",
             "没有页面文件", "ページファイルなし"),
            ("zram (memory is compressed in RAM; no drive is involved)",
             "zram（内存在 RAM 里压缩，不经过硬盘）", "zram（メモリを RAM 内で圧縮、ドライブは使いません）"),
            ("no swap",
             "没有交换分区", "スワップなし"),
            ("none (iOS compresses memory and does not page to storage)",
             "无（iOS 只压缩内存，不向存储换页）", "なし（iOS はメモリを圧縮し、ストレージにページングしません）"),
            ("swap files in /private/var/vm",
             "/private/var/vm 里的交换文件", "/private/var/vm のスワップファイル"),
            ("unknown",
             "未知", "不明"),
            ("no temporary directory",
             "没有可用的临时目录", "一時ディレクトリがありません"),
            ("the directory is on tmpfs, which is memory and not a drive",
             "目录位于 tmpfs，它是内存而不是硬盘", "ディレクトリが tmpfs 上にあります（メモリであり、ドライブではありません）"),
            ("the directory does not exist",
             "目录不存在", "ディレクトリが存在しません"),
            ("not enough free space for the test file",
             "剩余空间不足以写测试文件", "テストファイル用の空き容量が不足しています"),
            ("allocation failed",
             "内存分配失败", "メモリの確保に失敗しました"),
            ("the test file could not be created",
             "无法创建测试文件", "テストファイルを作成できませんでした"),
            ("a read or write on the test file failed",
             "读写测试文件时出错", "テストファイルの読み書きに失敗しました"),
            ("switched off",
             "已关闭", "オフ"),
        ]
        for phrase in phrases where text == phrase.en {
            return tr(phrase.en, phrase.zh, phrase.ja)
        }
        return text
    }
}
