// -----------------------------------------------------------------------------
// Tiny i18n layer for the WinUI frontend (and any shared call sites).
//
// Design:
//   - Single global `g_lang` picked once at startup (auto-detect + override).
//   - `tr(en, zh, ja)` picks the right literal at the call site; languages live
//     next to each other in source so diff-reviewing translations is trivial.
//   - If `ja` is null/empty and the active language is Japanese, English is used
//     as a safe fallback so partial migrations still compile and run.
//
// Auto-detection (Windows):
//   - LANG_JAPANESE -> ja
//   - LANG_CHINESE  -> zh
//   - otherwise     -> en
//   - Env SDR2HDR_LANG / MANGEKYO_LANG = "en"|"zh"|"ja"|"auto"
// -----------------------------------------------------------------------------
#pragma once

#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#endif

namespace i18n {

enum class Lang { En, Zh, Ja };

inline Lang& langRef()
{
    static Lang g = Lang::En;
    return g;
}

inline Lang currentLang() { return langRef(); }

inline const char* tr(const char* en, const char* zh, const char* ja = nullptr)
{
    switch (langRef())
    {
    case Lang::Zh: return zh ? zh : en;
    case Lang::Ja: return (ja && *ja) ? ja : en;
    default:       return en;
    }
}

inline std::string trs(const char* en, const char* zh, const char* ja = nullptr)
{
    return tr(en, zh, ja);
}

// Prefer for dynamic (assembled) strings.
inline std::string trDyn(std::string const& en,
                         std::string const& zh,
                         std::string const& ja)
{
    switch (langRef())
    {
    case Lang::Zh: return zh;
    case Lang::Ja: return ja;
    default:       return en;
    }
}

inline bool usesYmdDate()
{
    return langRef() == Lang::Zh || langRef() == Lang::Ja;
}

inline bool parseLangTag(const char* s, Lang& out)
{
    if (!s || !*s) return false;
    if (!::strcmp(s, "en") || !::strcmp(s, "EN") || !::strcmp(s, "english"))
    {
        out = Lang::En;
        return true;
    }
    if (!::strcmp(s, "zh") || !::strcmp(s, "ZH") || !::strcmp(s, "chinese") ||
        !::strcmp(s, "zh-CN") || !::strcmp(s, "zh_CN"))
    {
        out = Lang::Zh;
        return true;
    }
    if (!::strcmp(s, "ja") || !::strcmp(s, "JA") || !::strcmp(s, "jp") ||
        !::strcmp(s, "japanese") || !::strcmp(s, "ja-JP") || !::strcmp(s, "ja_JP"))
    {
        out = Lang::Ja;
        return true;
    }
    return false;
}

inline Lang detectOsLang()
{
#ifdef _WIN32
    LANGID id = GetUserDefaultUILanguage();
    const auto primary = PRIMARYLANGID(id);
    if (primary == LANG_JAPANESE) return Lang::Ja;
    if (primary == LANG_CHINESE) return Lang::Zh;
#endif
    return Lang::En;
}

inline const wchar_t* detectOsLangLabel()
{
    switch (detectOsLang())
    {
    case Lang::Zh: return L"中文";
    case Lang::Ja: return L"日本語";
    default:       return L"English";
    }
}

// cliOverride: explicit tag, or nullptr/"auto" to use env then OS.
inline void initLang(const char* cliOverride = nullptr)
{
    Lang chosen = detectOsLang();

    if (const char* env = std::getenv("MANGEKYO_LANG"))
    {
        Lang fromEnv;
        if (parseLangTag(env, fromEnv))
            chosen = fromEnv;
    }
    else if (const char* env = std::getenv("SDR2HDR_LANG"))
    {
        Lang fromEnv;
        if (parseLangTag(env, fromEnv))
            chosen = fromEnv;
    }

    if (cliOverride && *cliOverride && ::strcmp(cliOverride, "auto") != 0)
    {
        Lang fromCli;
        if (parseLangTag(cliOverride, fromCli))
            chosen = fromCli;
    }

    langRef() = chosen;
}

} // namespace i18n
