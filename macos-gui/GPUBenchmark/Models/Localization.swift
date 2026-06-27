// Localization.swift — Tiny i18n layer matching gui/i18n.h.
// Supports English and Simplified Chinese with auto-detection.

import Foundation

enum AppLanguage: String, CaseIterable, Identifiable {
    case auto_ = "auto"
    case en = "en"
    case zh = "zh"

    var id: String { rawValue }

    var displayName: String {
        switch self {
        case .auto_: return "Auto"
        case .en:    return "English"
        case .zh:    return "简体中文"
        }
    }
}

final class Localization {
    nonisolated(unsafe) static var current: AppLanguage = .auto_

    /// Resolve the effective language (handles "auto").
    static var effectiveLang: AppLanguage {
        if current != .auto_ { return current }
        // Auto-detect from system locale
        let lang = Locale.current.language.languageCode?.identifier ?? "en"
        return lang.hasPrefix("zh") ? .zh : .en
    }

    /// Translate: returns the appropriate string for the current language.
    static func tr(_ en: String, _ zh: String) -> String {
        effectiveLang == .zh ? zh : en
    }
}
