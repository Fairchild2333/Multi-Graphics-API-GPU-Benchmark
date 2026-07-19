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
}
