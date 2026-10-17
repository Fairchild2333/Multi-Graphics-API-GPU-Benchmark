#include "benchmark_results.h"
#include "path_service.h"
#include "system_info.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <map>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#if defined(__APPLE__)
#include <TargetConditionals.h>
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <sys/utsname.h>
#endif

namespace gpu_bench {

// ---------------------------------------------------------------------------
// Paths
// ---------------------------------------------------------------------------

std::string ResultsFilePath() {
    const auto destination = paths::ResultsDirectory() / "results.json";

    // One-time compatibility migration for existing source-tree installs.
    // A packaged application never relies on this relative path, but developers
    // keep their historical results when upgrading to the installed layout.
    static const bool migrated = [&] {
        std::error_code ec;
        const auto legacy = std::filesystem::absolute(
            std::filesystem::path("results") / "results.json", ec);
        if (ec || std::filesystem::exists(destination) ||
            !std::filesystem::is_regular_file(legacy)) {
            return false;
        }
        if (legacy.lexically_normal() ==
            std::filesystem::absolute(destination, ec).lexically_normal()) {
            return false;
        }
        std::filesystem::copy_file(legacy, destination,
                                   std::filesystem::copy_options::skip_existing,
                                   ec);
        return !ec;
    }();
    (void)migrated;

    return destination.u8string();
}

// ---------------------------------------------------------------------------
// ID / timestamp generation
// ---------------------------------------------------------------------------

// Local calendar time. POSIX lets localtime_r skip reading the time zone, and
// the C library of Android 7 does: without tzset() the first call in a process
// answered in UTC, so the first result of a run got an id hours away from its
// own timestamp (seen on the API 24 emulator, 2026-10-07).
static std::tm LocalTime(std::time_t time) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    tzset();
    localtime_r(&time, &tm);
#endif
    return tm;
}

std::string GenerateResultId() {
    auto now  = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms   = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()) % 1000;

    const std::tm tm = LocalTime(time);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d-%H%M%S")
        << "-" << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string GenerateTimestamp() {
    auto now  = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    const std::tm tm = LocalTime(time);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string CurrentAppVersion() {
#ifdef GPU_BENCH_APP_VERSION
    return GPU_BENCH_APP_VERSION;
#else
    return "Unknown";
#endif
}

std::string CurrentOsVersion() {
#ifdef _WIN32
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    auto ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        auto fn = reinterpret_cast<RtlGetVersionFn>(
            GetProcAddress(ntdll, "RtlGetVersion"));
        if (fn) {
            RTL_OSVERSIONINFOW vi{};
            vi.dwOSVersionInfoSize = sizeof(vi);
            if (fn(&vi) == 0) {
                const auto maj = vi.dwMajorVersion;
                const auto min = vi.dwMinorVersion;
                const auto bld = vi.dwBuildNumber;
                std::string friendly;
                if (maj == 10 && bld >= 22000) friendly = "Windows 11";
                else if (maj == 10) friendly = "Windows 10";
                else if (maj == 6 && min == 3) friendly = "Windows 8.1";
                else if (maj == 6 && min == 2) friendly = "Windows 8";
                else if (maj == 6 && min == 1) friendly = "Windows 7";
                else if (maj == 6 && min == 0) friendly = "Windows Vista";
                else friendly = "Windows";
                return friendly + " (NT " + std::to_string(maj) + "." +
                       std::to_string(min) + "." + std::to_string(bld) + ")";
            }
        }
    }
#elif defined(__APPLE__)
#if TARGET_OS_WATCH
    if (const char* version = std::getenv("GPU_BENCH_WATCH_OS_VERSION"))
        return std::string("watchOS ") + version;
#endif
    char buf[64]{};
    size_t len = sizeof(buf);
    if (sysctlbyname("kern.osproductversion", buf, &len, nullptr, 0) == 0) {
#if TARGET_OS_WATCH
        return "watchOS " + std::string(buf);
#elif TARGET_OS_IPHONE
        return "iOS " + std::string(buf);
#else
        return "macOS " + std::string(buf);
#endif
    }
#elif defined(__ANDROID__) || defined(__OHOS__)
    // Neither has /etc/os-release, and the kernel version says nothing about
    // the system release; ask the system itself.
    {
        const std::string release = MobileOsVersion();
        if (!release.empty()) return release;
        struct utsname un{};
        if (uname(&un) == 0) return std::string(un.sysname) + " " + un.release;
    }
#elif defined(__linux__)
    std::ifstream osrel("/etc/os-release");
    std::string line;
    while (std::getline(osrel, line)) {
        if (line.rfind("PRETTY_NAME=", 0) == 0) {
            std::string val = line.substr(12);
            if (!val.empty() && val.front() == '"') val.erase(0, 1);
            if (!val.empty() && val.back() == '"') val.pop_back();
            struct utsname un{};
            if (uname(&un) == 0) val += " (kernel " + std::string(un.release) + ")";
            return val;
        }
    }
    struct utsname un{};
    if (uname(&un) == 0) return std::string(un.sysname) + " " + un.release;
#endif
    return "Unknown";
}

std::string CurrentPlatform() {
#if defined(__ANDROID__)
    return "Android";
#elif defined(__OHOS__)
    return "HarmonyOS";
#elif defined(__APPLE__)
#if TARGET_OS_WATCH
    return "watchOS";
#elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    return "iOS";
#else
    return "macOS";
#endif
#elif defined(_WIN32)
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

std::string CurrentProcessArchitecture() {
#if defined(__arm64_32__)
    return "ARM64_32";
#elif defined(_M_ARM64) || defined(__aarch64__) || defined(__arm64__)
    return "ARM64";
#elif defined(_M_X64) || defined(__x86_64__)
    return "x64";
#elif defined(_M_IX86) || defined(__i386__)
    return "x86";
#elif defined(_M_ARM) || defined(__arm__)
    return "ARM32";
#else
    return "Unknown";
#endif
}

std::string CurrentOsArchitecture() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetNativeSystemInfo(&info);
    switch (info.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_ARM64: return "ARM64";
    case PROCESSOR_ARCHITECTURE_AMD64: return "x64";
    case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
    case PROCESSOR_ARCHITECTURE_ARM: return "ARM32";
    default: return "Unknown";
    }
#else
    return CurrentProcessArchitecture();
#endif
}

// ---------------------------------------------------------------------------
// The results file
// ---------------------------------------------------------------------------
//
// results.json is a JSON array of result objects with one member per line.
// Builds of different ages share it, in both directions:
//
//   * An older file read by this build: a member that is missing takes the
//     value ApplyLegacyDefaults gives it, otherwise empty, 0 or false.
//   * A newer file read by this build: members it does not know are kept in
//     BenchmarkResult::unknownFields and written back.
//   * A file of this build read by an older build: builds up to 0.2.6 find
//     the objects by counting braces, look a member up by its quoted name and
//     expect one member per line. The writer keeps that layout and keeps the
//     text those builds stumble over out of strings (EscapeText). Such a
//     build ignores the members it does not know, and drops them when it
//     rewrites the file.
//
// kFields is the one list of members. The reader, the writer and the order of
// the members in the file all come from it.

namespace {

struct Field {
    const char* key;
    std::string BenchmarkResult::* text = nullptr;
    std::uint32_t BenchmarkResult::* count = nullptr;
    bool BenchmarkResult::* flag = nullptr;
    double BenchmarkResult::* real = nullptr;
    int decimals = 3;
};

constexpr Field Text(const char* key, std::string BenchmarkResult::* member) {
    return {key, member};
}
constexpr Field Count(const char* key, std::uint32_t BenchmarkResult::* member) {
    return {key, nullptr, member};
}
constexpr Field Flag(const char* key, bool BenchmarkResult::* member) {
    return {key, nullptr, nullptr, member};
}
constexpr Field Real(const char* key, double BenchmarkResult::* member, int decimals = 3) {
    return {key, nullptr, nullptr, nullptr, member, decimals};
}

// In the order they are written. A new member goes where it reads best; the
// position has no meaning to any reader.
const Field kFields[] = {
    Text ("id",                  &BenchmarkResult::id),
    Text ("timestamp",           &BenchmarkResult::timestamp),
    Count("resultSchemaVersion", &BenchmarkResult::resultSchemaVersion),
    Text ("appVersion",          &BenchmarkResult::appVersion),
    Text ("workload",            &BenchmarkResult::workload),
    Text ("workloadVersion",     &BenchmarkResult::workloadVersion),
    Text ("workloadConfig",      &BenchmarkResult::workloadConfig),
    Text ("graphicsApi",         &BenchmarkResult::graphicsApi),
    Text ("deviceName",          &BenchmarkResult::deviceName),
    Text ("driverVersion",       &BenchmarkResult::driverVersion),
    Text ("cpuName",             &BenchmarkResult::cpuName),
    Text ("osVersion",           &BenchmarkResult::osVersion),
    Text ("platform",            &BenchmarkResult::platform),
    Text ("osArchitecture",      &BenchmarkResult::osArchitecture),
    Text ("processArchitecture", &BenchmarkResult::processArchitecture),
    Text ("deviceModel",         &BenchmarkResult::deviceModel),
    Count("systemMemoryMB",      &BenchmarkResult::systemMemoryMB),
    Text ("memory",              &BenchmarkResult::memory),
    Count("vramMB",              &BenchmarkResult::vramMB),
    Count("resWidth",            &BenchmarkResult::resWidth),
    Count("resHeight",           &BenchmarkResult::resHeight),
    Count("particleCount",       &BenchmarkResult::particleCount),
    Text ("difficulty",          &BenchmarkResult::difficulty),
    Flag ("vsync",               &BenchmarkResult::vsync),
    Flag ("isSoftware",          &BenchmarkResult::isSoftware),
    Flag ("headless",            &BenchmarkResult::headless),
    Count("framesInFlight",      &BenchmarkResult::framesInFlight),

    Real ("durationSec",         &BenchmarkResult::durationSec, 1),
    Real ("warmupSec",           &BenchmarkResult::warmupSec, 1),
    Count("measuredFrames",      &BenchmarkResult::measuredFrames),
    Count("timingSamples",       &BenchmarkResult::timingSamples),

    Real ("avgComputeMs",        &BenchmarkResult::avgComputeMs),
    Real ("minComputeMs",        &BenchmarkResult::minComputeMs),
    Real ("maxComputeMs",        &BenchmarkResult::maxComputeMs),
    Real ("avgRenderMs",         &BenchmarkResult::avgRenderMs),
    Real ("minRenderMs",         &BenchmarkResult::minRenderMs),
    Real ("maxRenderMs",         &BenchmarkResult::maxRenderMs),
    Real ("avgTotalGpuMs",       &BenchmarkResult::avgTotalGpuMs),
    Real ("minTotalGpuMs",       &BenchmarkResult::minTotalGpuMs),
    Real ("maxTotalGpuMs",       &BenchmarkResult::maxTotalGpuMs),

    Real ("avgFps",              &BenchmarkResult::avgFps, 1),
    Real ("avgFrameTimeMs",      &BenchmarkResult::avgFrameTimeMs),
    Real ("gpuUtilisation",      &BenchmarkResult::gpuUtilisation, 1),

    Real ("score",               &BenchmarkResult::score),
    Text ("scoreUnit",           &BenchmarkResult::scoreUnit),
    Text ("precision",           &BenchmarkResult::precision),
    Text ("bottleneck",          &BenchmarkResult::bottleneck),
    Real ("stableScore",         &BenchmarkResult::stableScore),
    Real ("stableVariancePct",   &BenchmarkResult::stableVariancePct, 2),
    Real ("throttlePct",         &BenchmarkResult::throttlePct, 2),
};
constexpr std::size_t kFieldCount = sizeof(kFields) / sizeof(kFields[0]);

bool IsFieldName(const std::string& text) {
    for (const Field& field : kFields) {
        if (text == field.key) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Writing
// ---------------------------------------------------------------------------

void AppendUnicodeEscape(std::string& out, unsigned char c) {
    static const char* const kHex = "0123456789abcdef";
    out += "\\u00";
    out += kHex[c >> 4];
    out += kHex[c & 15];
}

// A string as JSON text. Three things are written as \u escapes although JSON
// allows them plain, because builds up to 0.2.6 misread the file otherwise:
// braces (they split the file by counting them), a backslash at the end of
// the string (they take the closing quote for an escaped one), and the first
// letter of a value that is itself the name of a member (they look a member
// up by searching for its quoted name).
std::string EscapeText(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    const bool looksLikeMember = IsFieldName(s);
    for (std::size_t i = 0; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (i == 0 && looksLikeMember) {
            AppendUnicodeEscape(out, c);
            continue;
        }
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        case '\\':
            if (i + 1 == s.size()) AppendUnicodeEscape(out, c);
            else out += "\\\\";
            break;
        case '{':
        case '}':
            AppendUnicodeEscape(out, c);
            break;
        default:
            // The other control characters are not allowed plain in JSON.
            if (c < 0x20 || c == 0x7f) AppendUnicodeEscape(out, c);
            else out += static_cast<char>(c);
        }
    }
    return out;
}

std::string ResultToJson(const BenchmarkResult& r) {
    const std::string pad = "      ";
    std::ostringstream o;
    o.imbue(std::locale::classic());   // "1234.5" whatever the process locale is
    o << std::fixed;

    o << "    {";
    const char* separator = "\n";
    for (const Field& field : kFields) {
        o << separator << pad << "\"" << field.key << "\": ";
        if (field.text) {
            o << "\"" << EscapeText(r.*field.text) << "\"";
        } else if (field.count) {
            o << r.*field.count;
        } else if (field.flag) {
            o << (r.*field.flag ? "true" : "false");
        } else {
            // "nan" and "inf" are not JSON and stop other readers of the file.
            const double value = r.*field.real;
            o << std::setprecision(field.decimals) << (std::isfinite(value) ? value : 0.0);
        }
        separator = ",\n";
    }
    for (const auto& unknown : r.unknownFields) {
        o << separator << pad << "\"" << EscapeText(unknown.first) << "\": " << unknown.second;
    }
    o << "\n    }";
    return o.str();
}

// ---------------------------------------------------------------------------
// Reading
// ---------------------------------------------------------------------------

constexpr int kMaxNesting = 64;

bool IsJsonSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

void SkipSpace(const std::string& s, std::size_t& i) {
    while (i < s.size() && IsJsonSpace(s[i])) ++i;
}

bool ReadHex4(const std::string& s, std::size_t& i, unsigned long& value) {
    if (i + 4 > s.size()) return false;
    value = 0;
    for (std::size_t k = 0; k < 4; ++k) {
        const char c = s[i + k];
        unsigned long digit = 0;
        if (c >= '0' && c <= '9') digit = static_cast<unsigned long>(c - '0');
        else if (c >= 'a' && c <= 'f') digit = static_cast<unsigned long>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') digit = static_cast<unsigned long>(c - 'A' + 10);
        else return false;
        value = value * 16 + digit;
    }
    i += 4;
    return true;
}

void AppendUtf8(std::string& out, unsigned long code) {
    if (code >= 0xD800 && code <= 0xDFFF) code = 0xFFFD;   // half of a pair on its own
    if (code < 0x80) {
        out += static_cast<char>(code);
    } else if (code < 0x800) {
        out += static_cast<char>(0xC0 | (code >> 6));
        out += static_cast<char>(0x80 | (code & 0x3F));
    } else if (code < 0x10000) {
        out += static_cast<char>(0xE0 | (code >> 12));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (code >> 18));
        out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code & 0x3F));
    }
}

// Reads the string that starts with the quote at s[i] and leaves i after the
// closing quote. out may be null to only skip it. False when the text ends
// inside the string.
bool ReadString(const std::string& s, std::size_t& i, std::string* out) {
    ++i;
    while (i < s.size()) {
        const char c = s[i++];
        if (c == '"') return true;
        if (c != '\\') {
            if (out) out->push_back(c);
            continue;
        }
        if (i >= s.size()) return false;
        const char escaped = s[i++];
        if (!out) {
            continue;   // whatever follows a backslash is part of the string
        }
        switch (escaped) {
        case 'n': out->push_back('\n'); break;
        case 'r': out->push_back('\r'); break;
        case 't': out->push_back('\t'); break;
        case 'b': out->push_back('\b'); break;
        case 'f': out->push_back('\f'); break;
        case '"':
        case '\\':
        case '/':
            out->push_back(escaped);
            break;
        case 'u': {
            unsigned long code = 0;
            if (!ReadHex4(s, i, code)) {
                out->append("\\u");   // not an escape after all; keep the text
                break;
            }
            // Characters beyond U+FFFF come as two escapes.
            if (code >= 0xD800 && code <= 0xDBFF && i + 1 < s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                std::size_t next = i + 2;
                unsigned long low = 0;
                if (ReadHex4(s, next, low) && low >= 0xDC00 && low <= 0xDFFF) {
                    code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                    i = next;
                }
            }
            AppendUtf8(*out, code);
            break;
        }
        default:
            // Not a JSON escape. Older builds kept both characters; so does this one.
            out->push_back('\\');
            out->push_back(escaped);
        }
    }
    return false;
}

// Moves i past one JSON value of any kind. A word without quotes may be
// anything up to the next comma, bracket or space: older builds wrote "nan",
// "inf" or "-nan(ind)" for a number that was not finite.
bool SkipValue(const std::string& s, std::size_t& i, int nesting = 0) {
    SkipSpace(s, i);
    if (i >= s.size() || nesting > kMaxNesting) return false;
    const char first = s[i];
    if (first == '"') return ReadString(s, i, nullptr);
    if (first == '{' || first == '[') {
        const char closing = first == '{' ? '}' : ']';
        ++i;
        for (;;) {
            SkipSpace(s, i);
            if (i >= s.size()) return false;
            if (s[i] == closing) {
                ++i;
                return true;
            }
            if (s[i] == ',') {
                ++i;
                continue;
            }
            if (first == '{') {
                if (s[i] != '"' || !ReadString(s, i, nullptr)) return false;
                SkipSpace(s, i);
                if (i >= s.size() || s[i] != ':') return false;
                ++i;
            }
            if (!SkipValue(s, i, nesting + 1)) return false;
        }
    }
    const std::size_t start = i;
    while (i < s.size() && !IsJsonSpace(s[i]) && s[i] != ',' && s[i] != '}' && s[i] != ']' &&
           s[i] != '"' && s[i] != ':') {
        ++i;
    }
    return i > start;
}

// A value of a member this build knows, before it is turned into the type of
// the member. A member of another type than expected gives the nearest
// sensible value, never an error: one odd value must not hide a whole history.
struct MemberValue {
    bool isString = false;
    bool isWord = false;    // a number, true, false, null, or a word older builds wrote
    std::string text;       // the decoded string, or the word as written
};

// Number text to a double, whatever the process locale is. Text that is not a
// finite number gives 0.
double ToNumber(const std::string& text) {
    std::istringstream in(text);
    in.imbue(std::locale::classic());
    double value = 0.0;
    in >> value;
    return in.fail() || !std::isfinite(value) ? 0.0 : value;
}

std::string AsText(const MemberValue& value) {
    if (value.isString) return value.text;
    if (value.isWord && value.text != "null") return value.text;
    return {};
}

double AsNumber(const MemberValue& value) {
    if (!value.isString && !value.isWord) return 0.0;
    if (value.isWord && value.text == "true") return 1.0;
    return ToNumber(value.text);
}

std::uint32_t AsCount(const MemberValue& value) {
    const double number = AsNumber(value);
    if (number <= 0.0) return 0;
    if (number >= static_cast<double>(std::numeric_limits<std::uint32_t>::max())) {
        return std::numeric_limits<std::uint32_t>::max();
    }
    return static_cast<std::uint32_t>(number);
}

bool AsFlag(const MemberValue& value) {
    if (!value.isString && !value.isWord) return false;
    return value.text == "true" || ToNumber(value.text) != 0.0;
}

// What a result is before its members are read: everything empty, 0 or false.
// The defaults of BenchmarkResult describe a new result, not a missing member.
BenchmarkResult BlankResult() {
    BenchmarkResult r;
    for (const Field& field : kFields) {
        if (field.text) (r.*field.text).clear();
        else if (field.count) r.*field.count = 0;
        else if (field.flag) r.*field.flag = false;
        else r.*field.real = 0.0;
    }
    return r;
}

// What members that older builds did not write are taken to be.
void ApplyLegacyDefaults(BenchmarkResult& r, bool hasFramesInFlight) {
    if (r.resultSchemaVersion == 0) r.resultSchemaVersion = 1;
    if (r.appVersion.empty()) r.appVersion = "Unknown (legacy)";
    if (r.workload.empty()) r.workload = "stream";
    if (r.platform.empty()) r.platform = "Unknown (legacy)";
    if (r.osArchitecture.empty()) r.osArchitecture = "Unknown";
    if (r.processArchitecture.empty()) r.processArchitecture = "Unknown";
    if (!hasFramesInFlight) r.framesInFlight = 2;
}

// Reads the object that starts with the brace at s[i] and leaves i after its
// closing brace. False when the object is cut off or not well formed.
bool ReadResult(const std::string& s, std::size_t& i, BenchmarkResult& r) {
    r = BlankResult();
    bool seen[kFieldCount] = {};
    bool hasFramesInFlight = false;
    std::size_t expected = 0;   // members usually come in the order of kFields

    ++i;
    for (;;) {
        SkipSpace(s, i);
        if (i >= s.size()) return false;
        if (s[i] == '}') {
            ++i;
            break;
        }
        if (s[i] == ',') {
            ++i;
            continue;
        }
        if (s[i] != '"') return false;
        std::string key;
        if (!ReadString(s, i, &key)) return false;
        SkipSpace(s, i);
        if (i >= s.size() || s[i] != ':') return false;
        ++i;
        SkipSpace(s, i);
        const std::size_t valueStart = i;
        if (!SkipValue(s, i)) return false;

        std::size_t index = kFieldCount;
        for (std::size_t step = 0; step < kFieldCount; ++step) {
            const std::size_t candidate = (expected + step) % kFieldCount;
            if (key == kFields[candidate].key) {
                index = candidate;
                break;
            }
        }
        if (index == kFieldCount) {
            std::string raw = s.substr(valueStart, i - valueStart);
            raw.erase(std::remove(raw.begin(), raw.end(), '\r'), raw.end());
            r.unknownFields.emplace_back(std::move(key), std::move(raw));
            continue;
        }
        expected = index + 1;
        if (seen[index]) continue;   // the first one counts, as in older builds
        seen[index] = true;
        if (key == "framesInFlight") hasFramesInFlight = true;

        MemberValue value;
        if (s[valueStart] == '"') {
            value.isString = true;
            std::size_t at = valueStart;
            ReadString(s, at, &value.text);
        } else if (s[valueStart] != '{' && s[valueStart] != '[') {
            value.isWord = true;
            value.text = s.substr(valueStart, i - valueStart);
        }

        const Field& field = kFields[index];
        if (field.text) r.*field.text = AsText(value);
        else if (field.count) r.*field.count = AsCount(value);
        else if (field.flag) r.*field.flag = AsFlag(value);
        else r.*field.real = AsNumber(value);
    }
    ApplyLegacyDefaults(r, hasFramesInFlight);
    return true;
}

// ---------------------------------------------------------------------------
// The file on disk
// ---------------------------------------------------------------------------

std::filesystem::path ResultsPath() {
    return std::filesystem::u8path(ResultsFilePath());
}

bool ReadWholeFile(const std::filesystem::path& path, std::string& text) {
    // Binary: in text mode Windows ends the file at a Ctrl-Z byte.
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;
    text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

// Saving rewrites the whole file from what was read of it. When the file on
// disk cannot be read in full (cut off by a crash, damaged, or in a layout of
// a build far newer than this one), that would discard the rest for good, so
// the file is first copied aside. False when it cannot be kept safe.
bool KeepCopyOfUnreadableFile(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return true;
    std::string text;
    if (!ReadWholeFile(path, text)) return false;
    bool intact = true;
    ParseResultsJson(text, &intact);
    if (intact) return true;

    std::filesystem::path copy = path;
    copy += ".bak-unreadable-" + GenerateResultId();
    std::filesystem::copy_file(path, copy, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "[results] " << path.filename().u8string()
                  << " cannot be read in full and could not be copied aside; it is left as it is.\n";
        return false;
    }
    std::cerr << "[results] " << path.filename().u8string()
              << " could not be read in full. A copy was kept as "
              << copy.filename().u8string() << ".\n";
    return true;
}

bool WriteText(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return false;
    out << text;
    out.flush();
    return out.good();
}

// Puts `from` in the place of `to` in one step, so that no reader ever sees a
// half-written file and a crash leaves the old file whole.
bool MoveOver(const std::filesystem::path& from, const std::filesystem::path& to) {
#ifdef _WIN32
    // Another program may have the file open for a moment (a second instance
    // reading its history, a virus scanner); wait for it instead of giving up.
    for (int attempt = 0; attempt < 40; ++attempt) {
        if (MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            return true;
        }
        Sleep(25);
    }
    return false;
#else
    std::error_code ec;
    std::filesystem::rename(from, to, ec);
    return !ec;
#endif
}

bool ReplaceFileText(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    std::error_code ec;
    bool created = false;
    {
        std::ofstream probe(temporary, std::ios::trunc);
        created = probe.is_open();
    }
    if (created) {
        if (!WriteText(temporary, text)) {
            // No room for the new file. The old one is still whole; keep it.
            std::filesystem::remove(temporary, ec);
            return false;
        }
        if (MoveOver(temporary, path)) return true;
        std::filesystem::remove(temporary, ec);
    }
    // The folder takes no second file, or the file system cannot swap files:
    // write in place, as builds up to 0.2.6 always did.
    return WriteText(path, text);
}

}  // namespace

std::vector<BenchmarkResult> ParseResultsJson(const std::string& text, bool* intact) {
    std::vector<BenchmarkResult> results;
    bool complete = true;

    std::size_t i = text.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0;
    SkipSpace(text, i);
    if (i < text.size()) {          // an empty file is a file with no results
        if (text[i] != '[') {
            complete = false;
        } else {
            ++i;
            for (;;) {
                SkipSpace(text, i);
                if (i >= text.size()) {
                    complete = false;   // cut off
                    break;
                }
                if (text[i] == ']') {
                    ++i;
                    SkipSpace(text, i);
                    if (i < text.size()) complete = false;   // something follows the array
                    break;
                }
                if (text[i] == ',') {
                    ++i;
                    continue;
                }
                if (text[i] == '{') {
                    BenchmarkResult result;
                    if (!ReadResult(text, i, result)) {
                        complete = false;
                        break;
                    }
                    results.push_back(std::move(result));
                } else {
                    // Not a result object; this build has no way to keep it.
                    complete = false;
                    if (!SkipValue(text, i)) break;
                }
            }
        }
    }
    if (intact) *intact = complete;
    return results;
}

std::string SerializeResultsJson(const std::vector<BenchmarkResult>& results) {
    std::string text = "[\n";
    for (std::size_t i = 0; i < results.size(); ++i) {
        text += ResultToJson(results[i]);
        if (i + 1 < results.size()) text += ",";
        text += "\n";
    }
    text += "]\n";
    return text;
}

std::vector<BenchmarkResult> LoadResults() {
    std::string text;
    if (!ReadWholeFile(ResultsPath(), text)) return {};
    return ParseResultsJson(text);
}

bool SaveResults(const std::vector<BenchmarkResult>& results) {
    const std::filesystem::path path = ResultsPath();
    if (!KeepCopyOfUnreadableFile(path)) return false;
    return ReplaceFileText(path, SerializeResultsJson(results));
}

bool AppendResult(const BenchmarkResult& r) {
    auto results = LoadResults();
    results.push_back(r);
    // Every new result names the machine it was measured on. Done here, so no
    // test has to remember it.
    BenchmarkResult& saved = results.back();
    if (saved.deviceModel.empty()) saved.deviceModel = SystemDeviceModel();
    if (saved.systemMemoryMB == 0) saved.systemMemoryMB = SystemMemoryMB();
#if defined(__APPLE__) && TARGET_OS_WATCH
    saved.workloadConfig += ";watchProfile=cpu_watch_v1;corePlacement=scheduler_managed_unclassified";
    const char* watchProfile = std::getenv("GPU_BENCH_WATCH_PROFILE");
    const bool watchSmoke = watchProfile && std::string(watchProfile) == "smoke";
    saved.workloadConfig += watchSmoke ? ";watchRunProfile=smoke" : ";watchRunProfile=standard";
    if (watchSmoke) saved.workloadVersion += "_ui_smoke";
#if TARGET_OS_SIMULATOR
    saved.isSoftware = true;
    saved.workloadVersion += "_simulator";
    saved.workloadConfig += ";executionEnvironment=simulator;performanceScope=host_cpu_not_watch";
#else
    saved.workloadConfig += ";executionEnvironment=device";
#endif
#endif
    return SaveResults(results);
}

bool DeleteResult(const std::string& id) {
    auto results = LoadResults();
    auto it = std::remove_if(results.begin(), results.end(),
        [&](const BenchmarkResult& r) { return r.id == id; });
    if (it == results.end()) return false;
    results.erase(it, results.end());
    return SaveResults(results);
}

bool ClearResults() {
    return SaveResults({});
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

void PrintResultsTable(const std::vector<BenchmarkResult>& results) {
    if (results.empty()) {
        std::cout << "No benchmark results saved.\n"
                  << "Results file: " << ResultsFilePath() << "\n";
        return;
    }

    std::cout << "\n"
        "==========================================================\n"
        "                   Saved Benchmark Results\n"
        "==========================================================\n\n";

    std::cout << std::left
              << std::setw(4)  << "#"
              << std::setw(8)  << "API"
              << std::setw(28) << "Device"
              << std::setw(22) << "System"
              << std::setw(10) << "Difficulty"
              << std::setw(10) << "Avg FPS"
              << std::setw(12) << "GPU Avg(ms)"
              << "\n";
    std::cout << std::string(94, '-') << "\n";

    for (std::size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        std::string dev = r.deviceName;
        if (dev.size() > 26) dev = dev.substr(0, 23) + "...";
        std::string system = r.platform + " " + r.osArchitecture;
        if (r.processArchitecture != "Unknown" &&
            r.processArchitecture != r.osArchitecture)
            system += " (" + r.processArchitecture + ")";
        if (system.size() > 20) system = system.substr(0, 17) + "...";

        std::cout << std::left
                  << std::setw(4)  << (i + 1)
                  << std::setw(8)  << r.graphicsApi
                  << std::setw(28) << dev
                  << std::setw(22) << system
                  << std::setw(10) << r.difficulty
                  << std::setw(10) << static_cast<int>(r.avgFps);

        if (r.timingSamples > 0)
            std::cout << std::fixed << std::setprecision(3)
                      << std::setw(12) << r.avgTotalGpuMs;
        else
            std::cout << std::setw(12) << "N/A";

        std::cout << "\n";
    }

    std::cout << "\nTotal: " << results.size() << " result(s)\n"
              << "File:  " << ResultsFilePath() << "\n"
              << "==========================================================\n\n";
}

// ---------------------------------------------------------------------------
// Comparison
// ---------------------------------------------------------------------------

static std::string DeltaStr(double baseline, double value, bool higherIsBetter) {
    if (baseline == 0.0) return "--";
    double pct = (value - baseline) / baseline * 100.0;
    std::ostringstream o;
    o << std::fixed << std::setprecision(1);
    if (pct > 0.0) o << (higherIsBetter ? "+" : "+") << pct << "%";
    else if (pct < 0.0) o << pct << "%";
    else o << "0.0%";
    return o.str();
}

void PrintComparisonTable(const std::vector<BenchmarkResult>& results) {
    if (results.empty()) {
        std::cout << "No benchmark results to compare.\n";
        return;
    }
    if (results.size() < 2) {
        std::cout << "Need at least 2 results to compare. Currently saved: "
                  << results.size() << "\n";
        return;
    }

    std::cout << "\n"
        "==========================================================\n"
        "                  Benchmark Comparison\n"
        "==========================================================\n\n";

    // Scores from different workloads, algorithm versions, units or
    // precisions are not comparable. GPU Burn tiers use different fixed step
    // counts, so an all-workload FPS ranking is misleading.
    std::map<std::string, std::vector<BenchmarkResult>> groups;
    for (const auto& r : results) {
        const std::string workload = r.workload.empty() ? "stream" : r.workload;
        const std::string version = r.workloadVersion.empty() ? "legacy" : r.workloadVersion;
        const std::string unit = r.scoreUnit.empty() ? "Avg FPS" : r.scoreUnit;
        const std::string precision = r.precision.empty() ? "default" : r.precision;
        const std::string execution = r.headless ? "headless" : "windowed";
        groups[workload + "\x1f" + version + "\x1f" + unit + "\x1f" + precision +
               "\x1f" + execution]
            .push_back(r);
    }

    for (auto& [key, group] : groups) {
        const auto& first = group.front();
        const std::string workload = first.workload.empty() ? "stream" : first.workload;
        const std::string version = first.workloadVersion.empty() ? "legacy" : first.workloadVersion;
        const bool useScore = !first.scoreUnit.empty() &&
            std::all_of(group.begin(), group.end(), [](const BenchmarkResult& r) {
                return r.score > 0.0;
            });
        const bool useStable = useScore &&
            std::all_of(group.begin(), group.end(), [](const BenchmarkResult& r) {
                return r.stableScore > 0.0;
            });
        auto metric = [useScore, useStable](const BenchmarkResult& r) {
            if (useStable) return r.stableScore;
            if (useScore) return r.score;
            return r.avgFps;
        };

        std::sort(group.begin(), group.end(), [&](const BenchmarkResult& a,
                                                   const BenchmarkResult& b) {
            return metric(a) > metric(b);
        });
        const double baseline = metric(group.front());
        const std::string metricLabel = useStable ? "Stable score" :
            (useScore ? "Score" : "Avg FPS");
        const std::string metricUnit = useScore ? first.scoreUnit : "FPS";

        std::cout << "Workload: " << workload
                  << "  |  Version: " << version
                  << "  |  Mode: " << (first.headless ? "Headless" : "Windowed")
                  << "  |  Metric: " << metricLabel << " (" << metricUnit << ")";
        if (!first.precision.empty()) std::cout << "  |  Precision: " << first.precision;
        std::cout << "\n";
        std::cout << std::left
                  << std::setw(4)  << "#"
                  << std::setw(8)  << "API"
                  << std::setw(26) << "GPU"
                  << std::setw(10) << "Diff."
                  << std::right
                  << std::setw(15) << metricLabel
                  << std::setw(12) << "GPU ms"
                  << std::setw(10) << "Delta"
                  << "\n";
        std::cout << std::string(85, '-') << "\n";

        for (size_t i = 0; i < group.size(); ++i) {
            const auto& r = group[i];
            std::string dev = r.deviceName;
            if (dev.size() > 24) dev = dev.substr(0, 21) + "...";

            std::cout << std::left
                      << std::setw(4)  << (i + 1)
                      << std::setw(8)  << r.graphicsApi
                      << std::setw(26) << dev
                      << std::setw(10) << r.difficulty
                      << std::right << std::fixed << std::setprecision(useScore ? 3 : 0)
                      << std::setw(15) << metric(r);

            if (r.timingSamples > 0)
                std::cout << std::fixed << std::setprecision(3)
                          << std::setw(12) << r.avgTotalGpuMs;
            else
                std::cout << std::setw(12) << "N/A";

            std::cout << std::setw(10)
                      << (i == 0 ? "baseline" : DeltaStr(baseline, metric(r), true))
                      << "\n";
        }
        std::cout << "\n";
    }

    std::cout << "Total: " << results.size() << " result(s) in " << groups.size()
              << " comparable group(s). Rows are never ranked across workload/version/unit/precision.\n"
              << "==========================================================\n\n";
}

void PrintDetailedComparison(const BenchmarkResult& a, const BenchmarkResult& b) {
    const int w1 = 16, w2 = 24, w3 = 24, w4 = 10;

    std::cout << "\n"
        "==========================================================\n"
        "              Benchmark Comparison (A vs B)\n"
        "==========================================================\n";

    std::cout << std::left
              << std::setw(w1) << ""
              << std::setw(w2) << "A"
              << std::setw(w3) << "B"
              << std::setw(w4) << "Delta"
              << "\n";
    std::cout << std::string(w1 + w2 + w3 + w4, '-') << "\n";

    auto rowStr = [&](const char* label, const std::string& va, const std::string& vb) {
        std::cout << std::left
                  << std::setw(w1) << label
                  << std::setw(w2) << va
                  << std::setw(w3) << vb
                  << "\n";
    };

    auto rowFps = [&](const char* label, double va, double vb) {
        std::cout << std::left << std::setw(w1) << label
                  << std::setw(w2) << static_cast<int>(va)
                  << std::setw(w3) << static_cast<int>(vb)
                  << std::setw(w4) << DeltaStr(va, vb, true)
                  << "\n";
    };

    auto rowMs = [&](const char* label, double va, double vb) {
        std::ostringstream oa, ob;
        oa << std::fixed << std::setprecision(3) << va << " ms";
        ob << std::fixed << std::setprecision(3) << vb << " ms";
        std::cout << std::left << std::setw(w1) << label
                  << std::setw(w2) << oa.str()
                  << std::setw(w3) << ob.str()
                  << std::setw(w4) << DeltaStr(va, vb, false)
                  << "\n";
    };

    auto rowPct = [&](const char* label, double va, double vb) {
        std::ostringstream oa, ob;
        oa << std::fixed << std::setprecision(1) << (va * 100.0) << "%";
        ob << std::fixed << std::setprecision(1) << (vb * 100.0) << "%";
        std::cout << std::left << std::setw(w1) << label
                  << std::setw(w2) << oa.str()
                  << std::setw(w3) << ob.str()
                  << "\n";
    };

    auto rowMetric = [&](const char* label, double va, double vb,
                         const std::string& unit) {
        std::ostringstream oa, ob;
        oa << std::fixed << std::setprecision(3) << va << " " << unit;
        ob << std::fixed << std::setprecision(3) << vb << " " << unit;
        std::cout << std::left << std::setw(w1) << label
                  << std::setw(w2) << oa.str()
                  << std::setw(w3) << ob.str()
                  << std::setw(w4) << DeltaStr(va, vb, true)
                  << "\n";
    };

    rowStr("ID:",          a.id,          b.id);
    rowStr("Timestamp:",   a.timestamp,   b.timestamp);
    std::cout << "\n";

    rowStr("Graphics API:", a.graphicsApi, b.graphicsApi);
    rowStr("GPU:",          a.deviceName,  b.deviceName);
    rowStr("Driver:",       a.driverVersion, b.driverVersion);
    rowStr("CPU:",          a.cpuName,     b.cpuName);
    rowStr("OS:",           a.osVersion,   b.osVersion);
    rowStr("Platform:",     a.platform, b.platform);
    rowStr("OS architecture:", a.osArchitecture, b.osArchitecture);
    rowStr("Process architecture:", a.processArchitecture, b.processArchitecture);
    rowStr("Memory:",       a.memory,      b.memory);
    rowStr("Workload:",     a.workload,    b.workload);
    rowStr("Version:",      a.workloadVersion, b.workloadVersion);
    rowStr("App version:",  a.appVersion, b.appVersion);
    rowStr("Config:",       a.workloadConfig, b.workloadConfig);

    std::string resA = std::to_string(a.resWidth) + "x" + std::to_string(a.resHeight);
    std::string resB = std::to_string(b.resWidth) + "x" + std::to_string(b.resHeight);
    rowStr("Resolution:",   resA, resB);

    std::string partA = std::to_string(a.particleCount) + " (" + a.difficulty + ")";
    std::string partB = std::to_string(b.particleCount) + " (" + b.difficulty + ")";
    rowStr("Particles:",    partA, partB);

    rowStr("V-Sync:",       a.vsync ? "ON" : "OFF", b.vsync ? "ON" : "OFF");
    rowStr("Execution:",    a.headless ? "Headless" : "Windowed",
                             b.headless ? "Headless" : "Windowed");
    std::cout << "\n";

    const bool comparableScore = a.score > 0.0 && b.score > 0.0 &&
        a.workload == b.workload &&
        a.workloadVersion == b.workloadVersion &&
        a.scoreUnit == b.scoreUnit &&
        a.precision == b.precision &&
        a.headless == b.headless;
    if (comparableScore) {
        rowMetric("Score:", a.score, b.score, a.scoreUnit);
        if (a.stableScore > 0.0 && b.stableScore > 0.0)
            rowMetric("Stable score:", a.stableScore, b.stableScore, a.scoreUnit);
    } else {
        std::cout << "Score delta omitted: workload/version/unit/precision/execution mode differ "
                     "or a result has no axis score.\n";
    }

    rowFps("Avg FPS:",      a.avgFps, b.avgFps);
    rowMs ("Frame Time:",   a.avgFrameTimeMs, b.avgFrameTimeMs);
    std::cout << "\n";

    if (a.timingSamples > 0 || b.timingSamples > 0) {
        rowMs("Avg Compute:",  a.avgComputeMs,  b.avgComputeMs);
        rowMs("Avg Render:",   a.avgRenderMs,   b.avgRenderMs);
        rowMs("Avg GPU Total:",a.avgTotalGpuMs, b.avgTotalGpuMs);
        rowPct("GPU Util:",    a.gpuUtilisation, b.gpuUtilisation);
        std::cout << "\n";
    }

    rowStr("Bottleneck:",   a.bottleneck, b.bottleneck);

    std::cout << "==========================================================\n\n";
}

// ---------------------------------------------------------------------------
// Export
// ---------------------------------------------------------------------------

bool ExportResultsCsv(const std::string& path,
                      const std::vector<BenchmarkResult>& results) {
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << "id,timestamp,resultSchemaVersion,appVersion,workload,workloadVersion,workloadConfig,"
           "graphicsApi,deviceName,driverVersion,cpuName,osVersion,platform,osArchitecture,processArchitecture,memory,vramMB,"
           "resolution,particleCount,difficulty,vsync,isSoftware,headless,framesInFlight,"
           "durationSec,warmupSec,measuredFrames,timingSamples,"
           "avgComputeMs,minComputeMs,maxComputeMs,"
           "avgRenderMs,minRenderMs,maxRenderMs,"
           "avgTotalGpuMs,minTotalGpuMs,maxTotalGpuMs,"
           "avgFps,avgFrameTimeMs,gpuUtilisation,bottleneck,"
           "score,scoreUnit,precision,stableScore,stableVariancePct,throttlePct,"
           "deviceModel,systemMemoryMB\n";

    for (const auto& r : results) {
        // A quote inside a quoted CSV field is written twice.
        auto q = [](const std::string& s) {
            std::string quoted = "\"";
            for (char c : s) {
                if (c == '"') quoted += '"';
                quoted += c;
            }
            return quoted + "\"";
        };
        out << std::fixed << std::setprecision(3)
            << q(r.id) << ","
            << q(r.timestamp) << ","
            << r.resultSchemaVersion << ","
            << q(r.appVersion) << ","
            << q(r.workload) << ","
            << q(r.workloadVersion) << ","
            << q(r.workloadConfig) << ","
            << q(r.graphicsApi) << ","
            << q(r.deviceName) << ","
            << q(r.driverVersion) << ","
            << q(r.cpuName) << ","
            << q(r.osVersion) << ","
            << q(r.platform) << ","
            << q(r.osArchitecture) << ","
            << q(r.processArchitecture) << ","
            << q(r.memory) << ","
            << r.vramMB << ","
            << r.resWidth << "x" << r.resHeight << ","
            << r.particleCount << ","
            << q(r.difficulty) << ","
            << (r.vsync ? "true" : "false") << ","
            << (r.isSoftware ? "true" : "false") << ","
            << (r.headless ? "true" : "false") << ","
            << r.framesInFlight << ","
            << std::setprecision(1) << r.durationSec << ","
            << r.warmupSec << ","
            << r.measuredFrames << ","
            << r.timingSamples << ","
            << std::setprecision(3)
            << r.avgComputeMs << ","
            << r.minComputeMs << ","
            << r.maxComputeMs << ","
            << r.avgRenderMs << ","
            << r.minRenderMs << ","
            << r.maxRenderMs << ","
            << r.avgTotalGpuMs << ","
            << r.minTotalGpuMs << ","
            << r.maxTotalGpuMs << ","
            << std::setprecision(1) << r.avgFps << ","
            << std::setprecision(3) << r.avgFrameTimeMs << ","
            << std::setprecision(1) << r.gpuUtilisation << ","
            << q(r.bottleneck) << ","
            << std::setprecision(3) << r.score << ","
            << q(r.scoreUnit) << ","
            << q(r.precision) << ","
            << r.stableScore << ","
            << std::setprecision(2) << r.stableVariancePct << ","
            << r.throttlePct << ","
            << q(r.deviceModel) << ","
            << r.systemMemoryMB << "\n";
    }

    out.close();
    return true;
}

}  // namespace gpu_bench
