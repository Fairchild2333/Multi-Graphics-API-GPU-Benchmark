// Checks that result files of older and newer builds are read, kept and
// rewritten correctly, and that a file written by this build is still read by
// the reader of builds up to 0.2.6. Needs no GPU. Run it after every change of
// BenchmarkResult or of the reader and writer in benchmark_results.cpp:
//
//     ctest --test-dir build -C Release
//
// It works in a temporary data folder and never touches saved results.

#include "benchmark_results.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

using gpu_bench::BenchmarkResult;

namespace {

int g_checks = 0;
int g_failures = 0;

void Check(bool condition, const char* what, int line) {
    ++g_checks;
    if (condition) return;
    ++g_failures;
    std::cerr << "FAILED line " << line << ": " << what << "\n";
}
#define CHECK(condition) Check((condition), #condition, __LINE__)

bool Contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

// ---------------------------------------------------------------------------
// The reader of builds up to 0.2.6, copied from their benchmark_results.cpp
// (only the members the checks below look at). It is here to prove that those
// builds still read what this build writes; do not "fix" it.
// ---------------------------------------------------------------------------

struct OldResult {
    std::string id, workload, deviceName, scoreUnit, cpuName, memory;
    double score = 0.0;
    double particleCount = 0.0;
    bool vsync = false;
    bool headless = false;
};

std::string OldUnescape(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
            case '"':  out += '"';  ++i; break;
            case '\\': out += '\\'; ++i; break;
            case 'n':  out += '\n'; ++i; break;
            case 'r':  out += '\r'; ++i; break;
            case 't':  out += '\t'; ++i; break;
            default:   out += s[i];
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

OldResult OldJsonToResult(const std::string& json) {
    auto findStr = [&](const char* key) -> std::string {
        std::string needle = std::string("\"") + key + "\"";
        auto pos = json.find(needle);
        if (pos == std::string::npos) return "";
        pos = json.find(':', pos);
        if (pos == std::string::npos) return "";
        auto q1 = json.find('"', pos + 1);
        if (q1 == std::string::npos) return "";
        auto q2 = q1 + 1;
        while (q2 < json.size() && !(json[q2] == '"' && json[q2 - 1] != '\\')) ++q2;
        return OldUnescape(json.substr(q1 + 1, q2 - q1 - 1));
    };
    auto findNum = [&](const char* key) -> double {
        std::string needle = std::string("\"") + key + "\"";
        auto pos = json.find(needle);
        if (pos == std::string::npos) return 0.0;
        pos = json.find(':', pos);
        if (pos == std::string::npos) return 0.0;
        ++pos;
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
        return std::stod(json.substr(pos));   // throws on text that is no number
    };
    auto findBool = [&](const char* key) -> bool {
        std::string needle = std::string("\"") + key + "\"";
        auto pos = json.find(needle);
        if (pos == std::string::npos) return false;
        pos = json.find(':', pos);
        if (pos == std::string::npos) return false;
        return json.find("true", pos) < json.find('\n', pos);
    };

    OldResult r;
    r.id = findStr("id");
    r.workload = findStr("workload");
    r.deviceName = findStr("deviceName");
    r.cpuName = findStr("cpuName");
    r.memory = findStr("memory");
    r.particleCount = findNum("particleCount");
    r.vsync = findBool("vsync");
    r.headless = findBool("headless");
    r.score = findNum("score");
    r.scoreUnit = findStr("scoreUnit");
    return r;
}

std::vector<OldResult> OldLoad(const std::string& all) {
    std::vector<OldResult> results;
    size_t pos = 0;
    while (pos < all.size()) {
        auto start = all.find('{', pos);
        if (start == std::string::npos) break;
        int depth = 1;
        auto end = start + 1;
        while (end < all.size() && depth > 0) {
            if (all[end] == '{') ++depth;
            if (all[end] == '}') --depth;
            ++end;
        }
        if (depth == 0) results.push_back(OldJsonToResult(all.substr(start, end - start)));
        pos = end;
    }
    return results;
}

// ---------------------------------------------------------------------------
// Sample files
// ---------------------------------------------------------------------------

// Two results as the first builds wrote them: no resultSchemaVersion, no
// appVersion, no workloadVersion, no platform, no stable-score members.
const char* const kOldestFile = R"json([
    {
      "id": "20260627-101245-760",
      "timestamp": "2026-06-27 10:12:45",
      "workload": "stream",
      "graphicsApi": "Vulkan",
      "deviceName": "NVIDIA GeForce RTX 5090",
      "driverVersion": "595.79",
      "cpuName": "AMD Ryzen 7 9800X3D 8-Core Processor",
      "osVersion": "Windows 11 (NT 10.0.26200)",
      "memory": "96 GB",
      "vramMB": 32607,
      "resWidth": 1920,
      "resHeight": 1080,
      "particleCount": 1000000,
      "difficulty": "Medium (1M)",
      "vsync": false,
      "isSoftware": false,
      "headless": true,
      "durationSec": 15.0,
      "warmupSec": 2.0,
      "measuredFrames": 9012,
      "timingSamples": 9012,
      "avgComputeMs": 0.412,
      "minComputeMs": 0.380,
      "maxComputeMs": 0.901,
      "avgRenderMs": 0.233,
      "minRenderMs": 0.200,
      "maxRenderMs": 0.640,
      "avgTotalGpuMs": 0.645,
      "minTotalGpuMs": 0.580,
      "maxTotalGpuMs": 1.541,
      "avgFps": 693.2,
      "avgFrameTimeMs": 1.443,
      "gpuUtilisation": 44.7,
      "score": 97.087,
      "scoreUnit": "GB/s",
      "precision": "",
      "bottleneck": "CPU-bound"
    },
    {
      "id": "20260627-101502-113",
      "timestamp": "2026-06-27 10:15:02",
      "graphicsApi": "DX12",
      "deviceName": "AMD Radeon(TM) Graphics",
      "vsync": true,
      "avgFps": 60.0
    }
]
)json";

// One result of a build newer than this one: a higher schema version and
// members this build has never heard of, of every JSON type.
const char* const kNewerFile = R"json([
    {
      "id": "20270301-090000-001",
      "timestamp": "2027-03-01 09:00:00",
      "resultSchemaVersion": 9,
      "appVersion": "1.4.0",
      "workload": "raytrace",
      "workloadVersion": "raytrace_v1",
      "powerWatts": 412.5,
      "thermalLimited": true,
      "fanCurve": null,
      "tags": ["night", "undervolt {test}"],
      "display": {
        "name": "A \"quoted\" panel } with a brace",
        "hz": 240
      },
      "graphicsApi": "Vulkan",
      "deviceName": "Some Future GPU",
      "score": 1234.5,
      "scoreUnit": "Mray/s",
      "note": "caf\u00e9 \ud83d\ude00"
    }
]
)json";

BenchmarkResult SampleResult() {
    BenchmarkResult r;
    r.id = "20261006-183000-123";
    r.timestamp = "2026-10-06 18:30:00";
    r.resultSchemaVersion = 4;
    r.appVersion = "0.2.6";
    r.workload = "cpu_membw";
    r.workloadVersion = "cpu_membw_v1";
    r.workloadConfig = "threads=8 vector=512";
    r.graphicsApi = "CPU";
    r.deviceName = "AMD Ryzen 7 9800X3D 8-Core Processor";
    r.cpuName = r.deviceName;
    r.osVersion = "Windows 11 (NT 10.0.26300)";
    r.platform = "Windows";
    r.osArchitecture = "x64";
    r.processArchitecture = "x64";
    r.deviceModel = "ASUSTeK COMPUTER INC. ROG CROSSHAIR X670E HERO";
    r.systemMemoryMB = 98304;
    r.memory = "96 GB";
    r.particleCount = 1000000;
    r.headless = true;
    r.durationSec = 61.4;
    r.score = 55.123;
    r.scoreUnit = "GB/s";
    r.stableVariancePct = -1.0;
    return r;
}

bool SameKnownMembers(const BenchmarkResult& a, const BenchmarkResult& b) {
    return a.id == b.id && a.timestamp == b.timestamp && a.resultSchemaVersion == b.resultSchemaVersion &&
           a.appVersion == b.appVersion && a.workload == b.workload && a.workloadVersion == b.workloadVersion &&
           a.workloadConfig == b.workloadConfig && a.graphicsApi == b.graphicsApi &&
           a.deviceName == b.deviceName && a.driverVersion == b.driverVersion && a.cpuName == b.cpuName &&
           a.osVersion == b.osVersion && a.platform == b.platform && a.osArchitecture == b.osArchitecture &&
           a.processArchitecture == b.processArchitecture && a.deviceModel == b.deviceModel &&
           a.systemMemoryMB == b.systemMemoryMB && a.memory == b.memory && a.vramMB == b.vramMB &&
           a.resWidth == b.resWidth && a.resHeight == b.resHeight && a.particleCount == b.particleCount &&
           a.difficulty == b.difficulty && a.vsync == b.vsync && a.isSoftware == b.isSoftware &&
           a.headless == b.headless && a.framesInFlight == b.framesInFlight &&
           a.measuredFrames == b.measuredFrames && a.timingSamples == b.timingSamples &&
           a.scoreUnit == b.scoreUnit && a.precision == b.precision && a.bottleneck == b.bottleneck &&
           std::fabs(a.score - b.score) < 0.0006 && std::fabs(a.durationSec - b.durationSec) < 0.06 &&
           std::fabs(a.avgFps - b.avgFps) < 0.06 && std::fabs(a.stableVariancePct - b.stableVariancePct) < 0.006;
}

// ---------------------------------------------------------------------------
// Checks
// ---------------------------------------------------------------------------

void OlderFilesAreRead() {
    bool intact = false;
    const auto results = gpu_bench::ParseResultsJson(kOldestFile, &intact);
    CHECK(intact);
    CHECK(results.size() == 2);
    if (results.size() != 2) return;

    const BenchmarkResult& first = results[0];
    CHECK(first.id == "20260627-101245-760");
    CHECK(first.deviceName == "NVIDIA GeForce RTX 5090");
    CHECK(first.vramMB == 32607);
    CHECK(first.particleCount == 1000000);
    CHECK(first.headless && !first.vsync);
    CHECK(std::fabs(first.score - 97.087) < 1e-9);
    CHECK(std::fabs(first.avgFps - 693.2) < 1e-9);
    CHECK(first.unknownFields.empty());
    // What the members those builds did not write are taken to be.
    CHECK(first.resultSchemaVersion == 1);
    CHECK(first.appVersion == "Unknown (legacy)");
    CHECK(first.platform == "Unknown (legacy)");
    CHECK(first.osArchitecture == "Unknown");
    CHECK(first.framesInFlight == 2);
    CHECK(first.workloadVersion.empty());
    CHECK(first.deviceModel.empty() && first.systemMemoryMB == 0);
    CHECK(first.stableScore == 0.0 && first.stableVariancePct == 0.0);

    const BenchmarkResult& second = results[1];
    CHECK(second.workload == "stream");   // the only test there was
    CHECK(second.vsync);
    CHECK(second.score == 0.0 && second.scoreUnit.empty());

    // Rewriting the file keeps every value.
    const auto again = gpu_bench::ParseResultsJson(gpu_bench::SerializeResultsJson(results), &intact);
    CHECK(intact);
    CHECK(again.size() == 2 && SameKnownMembers(again[0], first) && SameKnownMembers(again[1], second));
}

void OwnFilesRoundTrip() {
    std::vector<BenchmarkResult> results = {SampleResult(), SampleResult()};
    results[1].id = "20261006-183100-456";
    results[1].vsync = true;

    const std::string text = gpu_bench::SerializeResultsJson(results);
    bool intact = false;
    const auto read = gpu_bench::ParseResultsJson(text, &intact);
    CHECK(intact);
    CHECK(read.size() == 2);
    if (read.size() != 2) return;
    CHECK(SameKnownMembers(read[0], results[0]));
    CHECK(SameKnownMembers(read[1], results[1]));
    CHECK(read[0].unknownFields.empty());
    // Reading and writing again changes nothing, byte for byte.
    CHECK(gpu_bench::SerializeResultsJson(read) == text);
    // The layout older builds rely on: one member per line.
    CHECK(Contains(text, "      \"vsync\": true,\n"));
    CHECK(Contains(text, "      \"systemMemoryMB\": 98304,\n"));
    CHECK(Contains(text, "      \"throttlePct\": 0.00\n    }"));
}

void NewerFilesKeepWhatThisBuildDoesNotKnow() {
    bool intact = false;
    const auto results = gpu_bench::ParseResultsJson(kNewerFile, &intact);
    CHECK(intact);
    CHECK(results.size() == 1);
    if (results.size() != 1) return;

    const BenchmarkResult& r = results[0];
    CHECK(r.resultSchemaVersion == 9);
    CHECK(r.workload == "raytrace");
    CHECK(r.deviceName == "Some Future GPU");   // read although it follows unknown members
    CHECK(std::fabs(r.score - 1234.5) < 1e-9);
    CHECK(r.unknownFields.size() == 6);
    if (r.unknownFields.size() != 6) return;
    CHECK(r.unknownFields[0].first == "powerWatts" && r.unknownFields[0].second == "412.5");
    CHECK(r.unknownFields[1].first == "thermalLimited" && r.unknownFields[1].second == "true");
    CHECK(r.unknownFields[2].first == "fanCurve" && r.unknownFields[2].second == "null");
    CHECK(r.unknownFields[3].first == "tags");
    CHECK(r.unknownFields[4].first == "display" && Contains(r.unknownFields[4].second, "\"hz\": 240"));
    CHECK(r.unknownFields[5].first == "note");

    // Appending a result of this build next to it keeps all of it.
    std::vector<BenchmarkResult> all = results;
    all.push_back(SampleResult());
    const std::string rewritten = gpu_bench::SerializeResultsJson(all);
    CHECK(Contains(rewritten, "\"resultSchemaVersion\": 9"));
    CHECK(Contains(rewritten, "\"powerWatts\": 412.5"));
    CHECK(Contains(rewritten, "\"tags\": [\"night\", \"undervolt {test}\"]"));
    CHECK(Contains(rewritten, "\"name\": \"A \\\"quoted\\\" panel } with a brace\""));
    CHECK(Contains(rewritten, "\"note\": \"caf\\u00e9 \\ud83d\\ude00\""));

    const auto again = gpu_bench::ParseResultsJson(rewritten, &intact);
    CHECK(intact);
    CHECK(again.size() == 2);
    if (again.size() != 2) return;
    CHECK(again[0].unknownFields == r.unknownFields);
    CHECK(SameKnownMembers(again[0], r));
    CHECK(again[1].unknownFields.empty());
    // And a second rewrite is stable.
    CHECK(gpu_bench::SerializeResultsJson(again) == rewritten);
}

void EscapesAreRead() {
    const auto results = gpu_bench::ParseResultsJson(
        R"json([{"id": "a", "deviceName": "caf\u00e9 \u4e2d\u6587 \ud83d\ude00 \/ \u007bx\u007d", "cpuName": "tab\there"}])json");
    CHECK(results.size() == 1);
    if (results.size() != 1) return;
    CHECK(results[0].deviceName == "caf\xC3\xA9 \xE4\xB8\xAD\xE6\x96\x87 \xF0\x9F\x98\x80 / {x}");
    CHECK(results[0].cpuName == "tab\there");
}

// Text that the reader of builds up to 0.2.6 stumbles over when it is written
// plainly. This build must write it so that both readers get it right.
void OlderBuildsReadWhatThisBuildWrites() {
    std::vector<BenchmarkResult> results = {SampleResult(), SampleResult(), SampleResult()};
    results[0].deviceName = "Odd {GPU} name";           // braces: they split the file by counting them
    results[0].workloadConfig = "dir=C:\\Temp\\";       // ends in a backslash: the closing quote looked escaped
    results[0].memory = "score";                        // a value that is the name of a member
    results[0].cpuName = "He said \"hi\"\n\x01";        // quote, line break, control character
    results[0].score = 12.345;
    // Explicit zero denotes a non-frame CPU/cache measurement. Repeated
    // append operations must not replace it with the legacy default of two.
    results[0].framesInFlight = 0;
    results[0].unknownFields.emplace_back("futureCount", "7");
    results[1].id = "second";
    results[1].deviceName = "\xE4\xB8\xAD\xE6\x96\x87 GPU";   // UTF-8 passes through
    results[1].vsync = true;
    results[1].score = std::nan("");                    // must not become "nan" in the file
    results[2].id = "third";
    results[2].particleCount = 16000000;

    const std::string text = gpu_bench::SerializeResultsJson(results);
    CHECK(!Contains(text, ": nan") && !Contains(text, ": -nan"));
    CHECK(!Contains(text, "\x01"));

    // This build.
    bool intact = false;
    const auto read = gpu_bench::ParseResultsJson(text, &intact);
    CHECK(intact);
    CHECK(read.size() == 3);
    if (read.size() == 3) {
        CHECK(read[0].deviceName == results[0].deviceName);
        CHECK(read[0].workloadConfig == results[0].workloadConfig);
        CHECK(read[0].memory == "score");
        CHECK(read[0].cpuName == results[0].cpuName);
        CHECK(read[0].framesInFlight == 0);
        CHECK(read[0].unknownFields.size() == 1);
        CHECK(read[1].deviceName == results[1].deviceName);
        CHECK(read[1].score == 0.0);
        CHECK(read[2].particleCount == 16000000);
    }

    // The reader of builds up to 0.2.6: the same three results, with the right
    // values in the members after the awkward ones.
    std::vector<OldResult> old;
    bool threw = false;
    try {
        old = OldLoad(text);
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(!threw);
    CHECK(old.size() == 3);
    if (old.size() == 3) {
        CHECK(old[0].id == results[0].id);
        CHECK(old[0].workload == "cpu_membw");
        CHECK(std::fabs(old[0].score - 12.345) < 1e-9);
        CHECK(old[0].scoreUnit == "GB/s");
        CHECK(old[0].particleCount == 1000000);
        CHECK(old[0].headless && !old[0].vsync);
        CHECK(old[1].id == "second");
        CHECK(old[1].deviceName == results[1].deviceName);
        CHECK(old[1].vsync);
        CHECK(old[2].id == "third");
        CHECK(old[2].particleCount == 16000000);
    }

    // The same strings written plainly do break that reader, which is why the
    // writer takes the trouble. If this stops failing, the copy above no
    // longer matches the old builds.
    const std::string plain =
        "[\n    {\n      \"id\": \"a\",\n      \"deviceName\": \"Odd {GPU\",\n      \"score\": 1.000\n    },\n"
        "    {\n      \"id\": \"b\",\n      \"score\": 2.000\n    }\n]\n";
    CHECK(OldLoad(plain).size() != 2);
}

void OddValuesDoNotStopTheReader() {
    bool intact = false;
    const auto results = gpu_bench::ParseResultsJson(R"json([
        {"id": 42, "score": "12.5", "vsync": 1, "headless": "true", "particleCount": -5,
         "vramMB": 99999999999, "driverVersion": 31.0, "avgFps": nan, "avgFrameTimeMs": -nan(ind),
         "gpuUtilisation": inf, "stableScore": null, "memory": {"total": 16}, "difficulty": ["a"],
         "score": 99, "resWidth": 1920.9, "isSoftware": "no"},
        {}
    ])json", &intact);
    CHECK(intact);
    CHECK(results.size() == 2);
    if (results.size() != 2) return;
    const BenchmarkResult& r = results[0];
    CHECK(r.id == "42");
    CHECK(std::fabs(r.score - 12.5) < 1e-9);   // the first of two members with one name counts
    CHECK(r.vsync && r.headless && !r.isSoftware);
    CHECK(r.particleCount == 0);
    CHECK(r.vramMB == 4294967295u);
    CHECK(r.driverVersion == "31.0");
    CHECK(r.avgFps == 0.0 && r.avgFrameTimeMs == 0.0 && r.gpuUtilisation == 0.0 && r.stableScore == 0.0);
    CHECK(r.memory.empty() && r.difficulty.empty());
    CHECK(r.resWidth == 1920);
    CHECK(results[1].workload == "stream" && results[1].resultSchemaVersion == 1);
}

void DamagedFilesGiveWhatCanBeRead() {
    const std::string whole = gpu_bench::SerializeResultsJson({SampleResult(), SampleResult(), SampleResult()});
    bool intact = true;

    // Cut off in the middle of the third result, as after a crash while writing.
    const std::string cut = whole.substr(0, whole.size() - 400);
    auto results = gpu_bench::ParseResultsJson(cut, &intact);
    CHECK(!intact);
    CHECK(results.size() == 2);

    intact = true;
    results = gpu_bench::ParseResultsJson("{\"formatVersion\": 2, \"results\": []}", &intact);
    CHECK(!intact && results.empty());      // a layout this build does not read

    intact = true;
    results = gpu_bench::ParseResultsJson("[1, \"text\", {\"id\": \"x\"}, null]", &intact);
    CHECK(!intact && results.size() == 1);  // entries that are no results cannot be kept

    intact = true;
    results = gpu_bench::ParseResultsJson(whole + "trailing", &intact);
    CHECK(!intact && results.size() == 3);

    intact = false;
    results = gpu_bench::ParseResultsJson("", &intact);
    CHECK(intact && results.empty());       // nothing saved yet

    intact = false;
    results = gpu_bench::ParseResultsJson("\xEF\xBB\xBF[]\r\n", &intact);
    CHECK(intact && results.empty());       // a byte order mark and Windows line ends

    intact = false;
    std::string windows = whole;
    for (std::size_t at = 0; (at = windows.find('\n', at)) != std::string::npos; at += 2) {
        windows.replace(at, 1, "\r\n");
    }
    results = gpu_bench::ParseResultsJson(windows, &intact);
    CHECK(intact && results.size() == 3);

    // Garbage of any kind must end in a list, not in a crash.
    const char* const junk[] = {"[", "[{", "[{\"id\"", "[{\"id\":", "[{\"id\":\"x", "]", "nonsense",
                                "[{\"id\":\"x\"}", "[[[[[[[[", "{{{{", "[{\"a\":{\"b\":[{\"c\":\"", "\"", "[,,,]"};
    for (const char* text : junk) {
        results = gpu_bench::ParseResultsJson(text, &intact);
        CHECK(results.size() <= 1);
    }
    std::string deep(5000, '[');
    results = gpu_bench::ParseResultsJson("[{\"id\": \"x\", \"deep\": " + deep + "}]", &intact);
    CHECK(!intact);
}

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
}

int CountFiles(const std::filesystem::path& folder, const std::string& namePart) {
    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        if (Contains(entry.path().filename().u8string(), namePart)) ++count;
    }
    return count;
}

void SavingOnDisk() {
    const std::filesystem::path file = std::filesystem::u8path(gpu_bench::ResultsFilePath());
    const std::filesystem::path folder = file.parent_path();

    // A fresh folder: the first result creates the file.
    std::filesystem::remove(file);
    CHECK(gpu_bench::LoadResults().empty());
    BenchmarkResult first = SampleResult();
    first.deviceModel.clear();      // AppendResult fills these in from the machine
    first.systemMemoryMB = 0;
    CHECK(gpu_bench::AppendResult(first));
    auto results = gpu_bench::LoadResults();
    CHECK(results.size() == 1);
    CHECK(!results.empty() && results[0].systemMemoryMB > 0);
    CHECK(CountFiles(folder, ".tmp") == 0);
    CHECK(CountFiles(folder, ".bak-unreadable") == 0);

    // A file of a newer build: appending keeps what this build does not know.
    WriteFile(file, kNewerFile);
    CHECK(gpu_bench::AppendResult(SampleResult()));
    results = gpu_bench::LoadResults();
    CHECK(results.size() == 2);
    CHECK(Contains(ReadFile(file), "\"powerWatts\": 412.5"));
    CHECK(Contains(ReadFile(file), "\"resultSchemaVersion\": 9"));
    CHECK(CountFiles(folder, ".bak-unreadable") == 0);   // it was read in full: no copy needed

    // Deleting one result keeps the other whole.
    CHECK(gpu_bench::DeleteResult(SampleResult().id));
    CHECK(!gpu_bench::DeleteResult("no such id"));
    results = gpu_bench::LoadResults();
    CHECK(results.size() == 1 && results[0].unknownFields.size() == 6);

    // A file cut off by a crash: what can be read is kept, and the damaged
    // file is copied aside before it is rewritten.
    const std::string whole = gpu_bench::SerializeResultsJson({SampleResult(), SampleResult(), SampleResult()});
    const std::string cut = whole.substr(0, whole.size() - 400);
    WriteFile(file, cut);
    CHECK(gpu_bench::LoadResults().size() == 2);
    CHECK(CountFiles(folder, ".bak-unreadable") == 0);   // reading alone changes nothing
    CHECK(ReadFile(file) == cut);
    CHECK(gpu_bench::AppendResult(SampleResult()));
    CHECK(gpu_bench::LoadResults().size() == 3);
    CHECK(CountFiles(folder, ".bak-unreadable") == 1);
    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        if (Contains(entry.path().filename().u8string(), ".bak-unreadable")) {
            CHECK(ReadFile(entry.path()) == cut);
            std::filesystem::remove(entry.path());
            break;
        }
    }

    // A layout this build does not read at all is not overwritten without a copy.
    const std::string foreign = "{\"formatVersion\": 2, \"results\": [{\"id\": \"kept\"}]}";
    WriteFile(file, foreign);
    CHECK(gpu_bench::LoadResults().empty());
    CHECK(gpu_bench::AppendResult(SampleResult()));
    CHECK(gpu_bench::LoadResults().size() == 1);
    CHECK(CountFiles(folder, ".bak-unreadable") == 1);
    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        if (Contains(entry.path().filename().u8string(), ".bak-unreadable")) {
            CHECK(ReadFile(entry.path()) == foreign);
        }
    }

    // Clearing leaves an empty, valid file.
    CHECK(gpu_bench::ClearResults());
    CHECK(gpu_bench::LoadResults().empty());
    bool intact = false;
    gpu_bench::ParseResultsJson(ReadFile(file), &intact);
    CHECK(intact);
    CHECK(CountFiles(folder, ".tmp") == 0);
}

void CsvExport() {
    BenchmarkResult r = SampleResult();
    r.deviceName = "A \"quoted\" name";
    const std::filesystem::path file =
        std::filesystem::u8path(gpu_bench::ResultsFilePath()).parent_path() / "export.csv";
    CHECK(gpu_bench::ExportResultsCsv(file.u8string(), {r}));
    const std::string text = ReadFile(file);
    std::string header = text.substr(0, text.find('\n'));
    if (!header.empty() && header.back() == '\r') header.pop_back();
    // Columns are only ever added at the end.
    CHECK(header.rfind("id,timestamp,resultSchemaVersion,appVersion,workload,", 0) == 0);
    CHECK(header.size() > 28 && header.compare(header.size() - 27, 27, ",deviceModel,systemMemoryMB") == 0);
    CHECK(Contains(text, "\"A \"\"quoted\"\" name\""));
}

}  // namespace

int main() {
    // A data folder of its own. Set before the first call into the engine,
    // which reads the variable once.
    std::error_code ec;
    const std::filesystem::path data =
        std::filesystem::temp_directory_path() / ("mangekyo-results-test-" + gpu_bench::GenerateResultId());
    std::filesystem::create_directories(data, ec);
#ifdef _WIN32
    _putenv_s("GPU_BENCH_DATA_DIR", data.u8string().c_str());
#else
    setenv("GPU_BENCH_DATA_DIR", data.u8string().c_str(), 1);
#endif

    OlderFilesAreRead();
    OwnFilesRoundTrip();
    NewerFilesKeepWhatThisBuildDoesNotKnow();
    EscapesAreRead();
    OlderBuildsReadWhatThisBuildWrites();
    OddValuesDoNotStopTheReader();
    DamagedFilesGiveWhatCanBeRead();
    SavingOnDisk();
    CsvExport();

    std::filesystem::remove_all(data, ec);
    std::cout << (g_checks - g_failures) << " of " << g_checks << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
