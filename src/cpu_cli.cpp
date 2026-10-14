#include "cpu_cli.h"
#include "cpu_cancel.h"
#include "benchmark_results.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace gpu_bench {

static gpu_bench::BenchmarkResult MakeStoredCpuResult(
    const gpu_bench::CpuBenchmarkReport& report,
    const gpu_bench::CpuBenchmarkConfig& config,
    bool multiCore) {
    gpu_bench::BenchmarkResult stored;
    stored.id = gpu_bench::GenerateResultId() +
                (multiCore ? "-cpu-multi" : "-cpu-single");
    stored.timestamp = gpu_bench::GenerateTimestamp();
    stored.resultSchemaVersion = 4;
    stored.appVersion = gpu_bench::CurrentAppVersion();
    stored.workload = multiCore ? "cpu_multi_core" : "cpu_single_core";
    const auto measureMs = static_cast<long long>(std::llround(config.measureSeconds * 1000.0));
    const auto warmupMs = static_cast<long long>(std::llround(config.warmupSeconds * 1000.0));
    const bool formalContract = measureMs == 15000 && warmupMs == 200 &&
                                config.roundCount == 3;
    const bool beforePerCore = multiCore && config.mode == gpu_bench::CpuBenchmarkMode::All;
    std::ostringstream storedVersion;
    storedVersion << report.workloadVersion
                  << (formalContract ? "_formal" : "_preview")
                  << "_r" << config.roundCount
                  << "_t" << measureMs << "ms"
                  << "_w" << warmupMs << "ms"
                  << "_sequence_" << (beforePerCore ? "before_percore" : "standalone");
    stored.workloadVersion = storedVersion.str();
    const auto coreType = !multiCore && !report.perCore.empty()
        ? report.perCore.front().coreType : CpuCoreType::Automatic;
    if (coreType != CpuCoreType::Automatic) {
        stored.workloadVersion += std::string("_qos_bound_v1_") + CpuCoreTypeName(coreType);
        stored.id += std::string("-") + CpuCoreTypeName(coreType);
    }
#if defined(__APPLE__) && TARGET_OS_IPHONE
    stored.workloadVersion += "_ios_preview";
#endif
    stored.graphicsApi = "CPU";
    stored.deviceName = report.cpuName;
    stored.cpuName = report.cpuName;
    stored.osVersion = gpu_bench::CurrentOsVersion();
    stored.platform = gpu_bench::CurrentPlatform();
    stored.osArchitecture = gpu_bench::CurrentOsArchitecture();
    stored.processArchitecture = gpu_bench::CurrentProcessArchitecture();
    stored.difficulty = multiCore ? "Multi-core" : coreType == CpuCoreType::Performance
        ? "Performance single-thread" : coreType == CpuCoreType::Efficiency
        ? "Efficiency single-thread" : "Per-core";
    stored.headless = true;
    stored.framesInFlight = 0;
    stored.scoreUnit = "MWork/s";
    stored.precision = "Mixed";
    stored.bottleneck = "Native mixed CPU kernel";

    std::uint32_t physicalCount = 0;
    for (const auto& cpu : report.processors)
        physicalCount = (std::max)(physicalCount, cpu.physicalCore + 1u);

    std::ostringstream contract;
    contract << "kernel=" << gpu_bench::kCpuBenchmarkWorkloadVersion
             << ";rounds=" << config.roundCount
             << ";totalMeasureSeconds=" << config.measureSeconds
             << ";roundSeconds="
             << (config.measureSeconds / static_cast<double>(config.roundCount))
             << ";warmupSeconds=" << config.warmupSeconds
             << ";topologySource=" << report.topologySource
             << ";affinityCapability=" << report.affinityCapability
             << ";logicalProcessors=" << report.processors.size()
             << ";physicalCores=" << physicalCount
             << ";coreClassLabels=" << (coreType == CpuCoreType::Automatic
                 ? "inferred_rank_not_architectural_identity" : "verified_type_not_numbered_core")
             << ";checksumRole=dce_sink";
    contract << ";scoreContract=" << (formalContract ? "formal" : "preview")
             << ";sequence=" << (beforePerCore ? "before_percore" : "standalone");

    if (multiCore) {
        stored.score = report.multiCore.scoreMWorkPerSec;
        stored.durationSec = config.measureSeconds;
        stored.warmupSec = config.warmupSeconds;
        stored.timingSamples = report.multiCore.roundCount;
        contract << ";aggregation=median"
                 << ";affinityMode=" << report.multiCore.affinityMode
                 << ";threads=" << report.multiCore.threadCount
                 << ";pinnedThreads=" << report.multiCore.pinnedThreadCount;
    } else {
        double sum = 0.0;
        std::vector<std::string> affinityModes;
        for (const auto& item : report.perCore) {
            sum += item.scoreMWorkPerSec;
            if (std::find(affinityModes.begin(), affinityModes.end(), item.affinityMode) ==
                affinityModes.end()) {
                affinityModes.push_back(item.affinityMode);
            }
        }
        stored.score = report.perCore.empty()
            ? 0.0 : sum / static_cast<double>(report.perCore.size());
        stored.durationSec = config.measureSeconds * report.perCore.size();
        stored.warmupSec = config.warmupSeconds * report.perCore.size();
        stored.timingSamples = static_cast<std::uint32_t>(
            report.perCore.size() * config.roundCount);
        contract << ";aggregation=arithmetic_mean_of_per_core_medians"
                 << ";affinityModes=";
        if (affinityModes.empty()) {
            contract << "unavailable";
        } else {
            for (std::size_t i = 0; i < affinityModes.size(); ++i) {
                if (i != 0) contract << ',';
                contract << affinityModes[i];
            }
        }
    }
    if (coreType != CpuCoreType::Automatic) {
        const auto& item = report.perCore.front();
        contract << ";coreType=" << CpuCoreTypeName(coreType)
                 << ";scheduling=" << (coreType == CpuCoreType::Performance ? "user_interactive" : "background")
                 << ";residencyMethod=process_accounting_thread_clock_bound"
                 << ";residencyThreshold=0.99;residencyVerified=1"
                 << ";residencyRetries=" << item.residencyRetries;
        for (std::size_t i = 0; i < item.residencyRounds.size(); ++i) {
            const auto& round = item.residencyRounds[i];
            const std::string prefix = ";round" + std::to_string(i + 1);
            contract << std::setprecision(9)
                     << prefix << "PFraction=" << round.performanceFraction
                     << prefix << "RequestedLowerBound=" << round.requestedLowerBound
                     << prefix << "EffectiveGHz=" << round.effectiveGHz;
        }
    }
    stored.workloadConfig = contract.str();
    return stored;
}

#ifdef HAVE_CPU_MEMBW
static gpu_bench::BenchmarkResult MakeStoredCpuMemBwResult(
    const gpu_bench::CpuMemBwReport& report,
    const gpu_bench::CpuMemBwConfig& config) {
    const auto findLevel = [](const gpu_bench::CpuMemBwModeSummary& mode, const char* name)
        -> const gpu_bench::CpuMemBwLevelSummary* {
        for (const auto& level : mode.levels)
            if (level.name == name && level.measured) return &level;
        return nullptr;
    };
    const auto* allMemory = findLevel(report.all, "memory");
    const auto* singleMemory = findLevel(report.single, "memory");
    const auto* headline = allMemory ? allMemory : singleMemory;

    gpu_bench::BenchmarkResult stored;
    stored.id = gpu_bench::GenerateResultId() + "-cpu-membw";
    stored.timestamp = gpu_bench::GenerateTimestamp();
    stored.resultSchemaVersion = 4;
    stored.appVersion = gpu_bench::CurrentAppVersion();
    stored.workload = gpu_bench::kCpuMemBwWorkloadId;
    // The formal contract is the default sweep with the widest vectors and a
    // settled memory plateau; anything else is kept in a preview group.
    const gpu_bench::CpuMemBwConfig defaults;
    const bool formal = headline && headline->settled &&
                        !report.datasetLimitedByMemory &&
                        config.maxDatasetBytes == defaults.maxDatasetBytes &&
                        config.vectorBytes == defaults.vectorBytes &&
                        config.levelsFromCurve == defaults.levelsFromCurve &&
                        config.debugSplitClasses == 0 && !config.debugHideLastCache &&
                        config.warmupRuns == defaults.warmupRuns &&
                        config.measuredRuns == defaults.measuredRuns &&
                        config.sampleMs == defaults.sampleMs;
    stored.workloadVersion = std::string(gpu_bench::kCpuMemBwWorkloadVersion) +
                             (formal ? "" : "_preview");
    // The phone default ends the sweep at 256 MiB, so phone results never
    // share a group with desktop ones, as with the other mobile workloads.
#if defined(__ANDROID__)
    stored.workloadVersion =
        std::string(gpu_bench::kCpuMemBwWorkloadVersion) + "_android_preview";
#elif defined(__OHOS__)
    stored.workloadVersion = std::string(gpu_bench::kCpuMemBwWorkloadVersion) + "_ohos_preview";
#elif defined(__APPLE__) && TARGET_OS_IPHONE
    stored.workloadVersion = std::string(gpu_bench::kCpuMemBwWorkloadVersion) + "_ios_preview";
#endif
    stored.graphicsApi = "CPU";
    stored.deviceName = report.cpuName;
    stored.cpuName = report.cpuName;
    stored.osVersion = gpu_bench::CurrentOsVersion();
    stored.platform = gpu_bench::CurrentPlatform();
    stored.osArchitecture = gpu_bench::CurrentOsArchitecture();
    stored.processArchitecture = gpu_bench::CurrentProcessArchitecture();
    if (config.coreType != CpuCoreType::Automatic)
        stored.workloadVersion += std::string("_qos_bound_v1_") + CpuCoreTypeName(config.coreType);
    stored.difficulty = "Sweep";
    stored.headless = true;
    stored.framesInFlight = 0;
    stored.durationSec = report.durationSec;
    stored.timingSamples = static_cast<std::uint32_t>(
        report.points.size() * report.measuredRuns);
    stored.score = headline ? headline->readGBps : 0.0;
    stored.scoreUnit = "GB/s";
    stored.precision = report.vectorName;
    stored.bottleneck = "Memory bandwidth";

    std::ostringstream contract;
    contract << std::fixed << std::setprecision(1)
             << "score=" << (allMemory ? "all_core_memory_read" : "single_thread_memory_read")
             << ";vector=" << report.vectorName
             << ";vectorBytes=" << report.vectorBytes
             << ";physicalCores=" << report.physicalCores
             << ";logicalProcessors=" << report.logicalProcessors
             << ";allCoreThreads=" << report.all.threads
             << ";affinity=" << report.affinity
             << ";cacheSource=" << report.cacheSource;
    for (const auto& cache : report.caches) {
        contract << ";cacheL" << cache.level << "KiB=" << cache.sizeBytes / 1024
                 << ";cacheL" << cache.level << "SharedCores=" << cache.sharedByCores;
    }
    // Levels of every core class ("single", "single_1", ...) and of all cores.
    std::vector<std::pair<std::string, const gpu_bench::CpuMemBwModeSummary*>> modes;
    contract << ";coreClasses=" << report.classes.size();
    for (const auto& cpuClass : report.classes) {
        modes.emplace_back(cpuClass.summary.mode, &cpuClass.summary);
        contract << ";class" << cpuClass.rank << "Name=" << cpuClass.name
                 << ";class" << cpuClass.rank << "Cores=" << cpuClass.coreCount
                 << ";class" << cpuClass.rank << "CacheSource=" << cpuClass.cacheSource;
        for (const auto& cache : cpuClass.caches) {
            contract << ";class" << cpuClass.rank << "CacheL" << cache.level << "KiB=" << cache.sizeBytes / 1024
                     << ";class" << cpuClass.rank << "CacheL" << cache.level << "SharedCores=" << cache.sharedByCores;
        }
    }
    modes.emplace_back("all", &report.all);
    for (const auto& entry : modes) {
        for (const auto& level : entry.second->levels) {
            if (!level.measured) continue;
            const std::string key = entry.first + "_" + level.name;
            if (level.fromCurve) {
                contract << ';' << key << "_capacityLowKiB=" << level.capacityLowBytes / 1024
                         << ';' << key << "_capacityHighKiB=" << level.capacityHighBytes / 1024;
            }
            contract << ';' << key << "_read=" << level.readGBps
                     << ';' << key << "_write=" << level.writeGBps
                     << ';' << key << "_copy=" << level.copyGBps
                     << ';' << key << "_fromKiB=" << level.fromBytes / 1024
                     << ';' << key << "_toKiB=" << level.toBytes / 1024;
            if (level.name == "memory") {
                contract << ';' << key << "_writeStream=" << level.writeStreamGBps
                         << ';' << key << "_copyStream=" << level.copyStreamGBps
                         << ';' << key << "_zeroFill=" << level.zeroFillGBps
                         << ';' << key << "_settled=" << (level.settled ? 1 : 0);
            }
        }
    }
    contract << ";pageFaultMeasured=" << (report.pageFault.measured ? 1 : 0)
             << ";pageFaultGBps=" << std::setprecision(2) << report.pageFault.gbps
             << ";pageFaultNsPerPage=" << std::setprecision(0) << report.pageFault.nsPerPage
             << ";pageBytes=" << report.pageFault.pageBytes
             << ";storageMeasured=" << (report.storage.measured ? 1 : 0)
             << ";storageBacking=" << report.storage.backing
             << ";storageOnPagingDrive=" << (report.storage.onPagingDrive ? 1 : 0)
             << ";storageDirectIo=" << (report.storage.directIo ? 1 : 0)
             << ";storageFileMiB=" << report.storage.fileBytes / (1024 * 1024)
             << ";storageSeqReadMBps=" << report.storage.seqReadMBps
             << ";storageSeqWriteMBps=" << report.storage.seqWriteMBps
             << ";storageRandReadMBps=" << report.storage.randReadMBps
             << ";storageRandWriteMBps=" << report.storage.randWriteMBps
             << ";maxDatasetMiB=" << report.single.maxBlockBytes / (1024 * 1024)
             << ";warmupRuns=" << report.warmupRuns
             << ";measuredRuns=" << report.measuredRuns
             << ";sampleMs=" << report.sampleMs
             << ";remeasuredPoints=" << report.remeasuredPoints
             << ";aggregation=median;timing=wall_clock";
    if (config.coreType != CpuCoreType::Automatic) {
        contract << ";coreType=" << CpuCoreTypeName(config.coreType)
                 << ";residencyMethod=process_accounting_thread_clock_bound"
                 << ";residencyThreshold=0.99;residencyVerified=1"
                 << ";schedulingScope=single_thread_curves_only";
        for (const auto& entry : modes) {
            if (entry.first == "all") continue;
            contract << std::setprecision(9) << ";residency_" << entry.first << "=";
            bool first = true;
            for (const auto& point : report.points) {
                if (point.mode != entry.first) continue;
                if (!first) contract << ',';
                first = false;
                contract << point.blockBytes / 1024 << ':' << point.kernel << ':'
                         << point.residency.requestedLowerBound << ':' << point.residencyRetries << ':'
                         << point.residency.effectiveGHz << ':' << point.residency.performanceFraction
                         << ':' << point.maxSampleMs;
            }
        }
    }
    // Full curves as "<data set KiB per thread>:<median GB/s>" pairs.
    contract << std::setprecision(1);
    for (const auto& entry : modes) {
        const std::string& mode = entry.first;
        for (const char* kernel : {"read", "write", "copy", "write_nt", "copy_nt", "zero"}) {
            contract << ";curve_" << mode << '_' << kernel << '=';
            bool first = true;
            for (const auto& point : report.points) {
                if (point.mode != mode || point.kernel != kernel) continue;
                if (!first) contract << ',';
                first = false;
                contract << point.blockBytes / 1024 << ':' << point.medianGBps;
            }
        }
    }
    stored.workloadConfig = contract.str();
    return stored;
}
#endif

// Runs `--cpu-membw-reread <file>`: reads the cache levels of every CPU cache
// result in a results file again, off the curves the result keeps, with the
// rules of this build. Nothing is measured and the file is not changed.
int RereadCpuMemoryBandwidthCli(const std::string& path) {
    std::ifstream file(std::filesystem::u8path(path), std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open " << path << "\n";
        return 2;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string text = buffer.str();
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3);

    bool intact = true;
    const auto results = gpu_bench::ParseResultsJson(text, &intact);
    if (!intact)
        std::cerr << "[warn] The file is damaged or cut off; reading what can be read.\n";

    std::uint32_t found = 0;
    for (const auto& result : results) {
        if (result.workload != gpu_bench::kCpuMemBwWorkloadId) continue;
        ++found;

        // The settings of a result: "key=value" pairs separated by ';'.
        std::map<std::string, std::string> fields;
        std::istringstream pairs(result.workloadConfig);
        for (std::string pair; std::getline(pairs, pair, ';');) {
            const std::size_t equals = pair.find('=');
            if (equals != std::string::npos) fields[pair.substr(0, equals)] = pair.substr(equals + 1);
        }
        const auto field = [&](const std::string& key) {
            const auto it = fields.find(key);
            return it == fields.end() ? std::string() : it->second;
        };

        // The curves: "curve_<mode>_<kernel>=<KiB>:<GB/s>,...".
        std::vector<gpu_bench::CpuMemBwPoint> points;
        std::vector<std::string> modes;
        for (const auto& entry : fields) {
            const std::string& key = entry.first;
            if (key.compare(0, 6, "curve_") != 0) continue;
            std::string mode;
            std::string kernel;
            for (const char* name : {"write_nt", "copy_nt", "read", "write", "copy", "zero"}) {
                const std::string suffix = std::string("_") + name;
                if (key.size() > 6 + suffix.size() &&
                    key.compare(key.size() - suffix.size(), suffix.size(), suffix) == 0) {
                    mode = key.substr(6, key.size() - 6 - suffix.size());
                    kernel = name;
                    break;
                }
            }
            if (mode.empty()) continue;
            if (kernel == "read") modes.push_back(mode);
            std::istringstream samples(entry.second);
            for (std::string sample; std::getline(samples, sample, ',');) {
                const std::size_t colon = sample.find(':');
                if (colon == std::string::npos) continue;
                gpu_bench::CpuMemBwPoint point;
                point.mode = mode;
                point.kernel = kernel;
                point.blockBytes = std::strtoull(sample.c_str(), nullptr, 10) * 1024ull;
                point.medianGBps = std::strtod(sample.c_str() + colon + 1, nullptr);
                point.bestGBps = point.medianGBps;
                points.push_back(point);
            }
        }

        std::cout << "\n========== " << result.id << "  " << result.timestamp << "  "
                  << (result.deviceModel.empty() ? result.deviceName
                                                 : result.deviceModel + " / " + result.deviceName)
                  << " ==========\n";
        if (modes.empty()) {
            std::cout << "This result keeps no curves; its levels cannot be read again.\n";
            continue;
        }
        const bool arm = result.processArchitecture.find("ARM") != std::string::npos ||
                         field("vector") == "NEON";
        const std::uint32_t classCount =
            static_cast<std::uint32_t>(std::strtoul(field("coreClasses").c_str(), nullptr, 10));
        const std::uint32_t allThreads =
            static_cast<std::uint32_t>(std::strtoul(field("allCoreThreads").c_str(), nullptr, 10));

        // The fastest class first, all cores last.
        std::sort(modes.begin(), modes.end(), [](const std::string& a, const std::string& b) {
            const auto rank = [](const std::string& mode) {
                if (mode == "single") return 0ul;
                if (mode == "all") return 1000ul;
                return std::strtoul(mode.c_str() + mode.rfind('_') + 1, nullptr, 10);
            };
            return rank(a) < rank(b);
        });
        for (const std::string& mode : modes) {
            const bool all = mode == "all";
            const std::string classRank =
                all ? std::string() : (mode == "single" ? "0" : mode.substr(mode.rfind('_') + 1));
            std::string source = all ? field("cacheSource") : field("class" + classRank + "CacheSource");
            if (source.empty()) source = field("cacheSource");
            const std::string title = all
                ? "all cores (" + std::to_string(allThreads) + " threads, data set sizes are per thread)"
                : "class " + classRank + ", " + field("class" + classRank + "Name") + ", one thread";
            if (source != "curve") {
                std::cout << "\n--- " << title << " ---\n"
                          << "Its levels come from the cache sizes the system reported; nothing to read again.\n";
                continue;
            }
            if (all && classCount > 1) {
                std::cout << "\n--- " << title << " ---\n"
                          << "A CPU with several core types reports memory only for all cores: "
                          << field("all_memory_read") << " / " << field("all_memory_write") << " / "
                          << field("all_memory_copy") << " GB/s (read / write / copy), unchanged.\n";
                continue;
            }
            gpu_bench::RereadCpuMemBwLevels(points, mode, all ? (std::max)(1u, allThreads) : 1u, arm,
                                            title, std::cout);
        }
    }
    if (found == 0) {
        std::cout << "No CPU cache result (cpu_membw) in " << path << "\n";
        return 1;
    }
    std::cout << "\nRead from the saved curves, which keep one decimal: a figure may differ from the"
              << "\nsaved summary in its last digit. The file was not changed.\n";
    return 0;
}

// Runs `--cpu-membw`. Returns the process exit code.
int RunCpuMemoryBandwidthCli(gpu_bench::CpuMemBwConfig config, bool saveResult) {
#ifdef HAVE_CPU_MEMBW
#ifndef _WIN32
    // The working directory may be on tmpfs; the results directory is on a drive.
    if (config.storageTest && config.storageDir.empty()) {
        const auto resultsDir = std::filesystem::path(gpu_bench::ResultsFilePath()).parent_path();
        std::error_code ec;
        std::filesystem::create_directories(resultsDir, ec);
        if (!ec) config.storageDir = resultsDir.string();
    }
#endif
    const auto report = gpu_bench::RunCpuMemoryBandwidth(config, std::cout);
    if (gpu_bench::CpuStopRequested()) { std::cout << "CPUMEM_CANCELLED\n" << std::flush; return 130; }
    if (!report.valid) {
        std::cerr << "CPU memory bandwidth sweep failed: "
                  << (report.error.empty() ? "no valid result" : report.error) << "\n";
        return 1;
    }
    if (saveResult) {
        try {
            const auto stored = MakeStoredCpuMemBwResult(report, config);
            if (gpu_bench::AppendResult(stored)) {
                std::cout << "[Results] Saved as " << stored.id << " -> "
                          << gpu_bench::ResultsFilePath() << '\n';
            } else {
                std::cerr << "[Results] Warning: failed to append the CPU memory result.\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "[Results] Warning: CPU memory result storage failed: "
                      << e.what() << '\n';
        }
    }
    return 0;
#else
    (void)config; (void)saveResult;
    std::cerr << "The CPU memory bandwidth sweep is only built into the desktop CLI.\n";
    return 2;
#endif
}

int RunCpuBenchmarkCli(const CpuBenchmarkConfig& cpuCfg, bool saveCpuResults) {
        const auto report = gpu_bench::RunCpuBenchmark(cpuCfg, std::cout);
        if (gpu_bench::CpuStopRequested()) { std::cout << "CPU_CANCELLED\n" << std::flush; return 130; }
        const bool perCoreRequested = cpuCfg.mode == gpu_bench::CpuBenchmarkMode::PerCore ||
                                      cpuCfg.mode == gpu_bench::CpuBenchmarkMode::All;
        const bool multiRequested = cpuCfg.mode == gpu_bench::CpuBenchmarkMode::MultiCore ||
                                    cpuCfg.mode == gpu_bench::CpuBenchmarkMode::All;
        const bool perCoreValid = !perCoreRequested ||
            (report.perCore.size() == (cpuCfg.coreType == CpuCoreType::Automatic ? report.processors.size()
                : cpuCfg.coreType == CpuCoreType::Both ? 2u : 1u) &&
             !report.perCore.empty() &&
             std::all_of(report.perCore.begin(), report.perCore.end(),
                         [](const gpu_bench::CpuCoreResult& item) { return item.valid; }));
        const bool multiValid = !multiRequested || report.multiCore.valid;
        if (!perCoreValid || !multiValid) {
            std::cerr << "CPU benchmark did not meet its affinity/completion contract; "
                         "see CPU_ERROR and valid=0 records above.\n";
        }
        // Each part is saved on its own merit: an all-core result is not thrown
        // away because a processor of the per-processor part could not be held.
        if (saveCpuResults) {
            bool saved = true;
            bool storageExceptionReported = false;
            try {
                if (perCoreRequested) {
                    std::vector<CpuBenchmarkReport> summaries;
                    if (cpuCfg.coreType == CpuCoreType::Automatic && perCoreValid) summaries.push_back(report);
                    else if (cpuCfg.coreType != CpuCoreType::Automatic) {
                        for (const auto& item : report.perCore) {
                            if (!item.valid) continue;
                            auto singleReport = report;
                            singleReport.perCore = {item};
                            summaries.push_back(std::move(singleReport));
                        }
                    }
                    for (const auto& summary : summaries) {
                        const auto stored = MakeStoredCpuResult(summary, cpuCfg, false);
                        const bool thisSaved = gpu_bench::AppendResult(stored);
                        saved = thisSaved && saved;
                        if (thisSaved)
                            std::cout << "[Results] Saved CPU single-thread summary as " << stored.id
                                      << " -> " << gpu_bench::ResultsFilePath() << '\n';
                    }
                }
                if (multiRequested && multiValid) {
                    const auto stored = MakeStoredCpuResult(report, cpuCfg, true);
                    const bool thisSaved = gpu_bench::AppendResult(stored);
                    saved = thisSaved && saved;
                    if (thisSaved)
                        std::cout << "[Results] Saved CPU multi-core summary as " << stored.id
                                  << " -> " << gpu_bench::ResultsFilePath() << '\n';
                }
            } catch (const std::exception& e) {
                saved = false;
                storageExceptionReported = true;
                std::cerr << "[Results] Warning: CPU result storage failed: "
                          << e.what() << '\n';
            }
            if (!saved && !storageExceptionReported)
                std::cerr << "[Results] Warning: failed to append one or more CPU summaries.\n";
        }
        return perCoreValid && multiValid ? 0 : 3;
    }

int CpuCliMain(int argc, const char* const* argv) {
    CpuBenchmarkConfig cpu;
    CpuMemBwConfig memory;
    bool memoryRequested = false;
    bool cpuRequested = false;
    bool saveCpu = true;
    bool saveMemory = true;
    std::string reread;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            const auto value = [&]() -> std::string {
                if (i + 1 >= argc) throw std::invalid_argument(arg + " requires a value");
                return argv[++i];
            };
            const auto number = [&](double low, double high) {
                const std::string text = value();
                std::size_t used = 0;
                const double n = std::stod(text, &used);
                if (used != text.size() || !std::isfinite(n) || n < low || n > high)
                    throw std::invalid_argument("invalid value for " + arg);
                return n;
            };
            const auto whole = [&](int low, int high) {
                const double n = number(low, high);
                if (std::floor(n) != n) throw std::invalid_argument(arg + " needs a whole number");
                return static_cast<std::uint64_t>(n);
            };
            const auto mode = [&](const std::string& text) {
                if (text == "all") cpu.mode = CpuBenchmarkMode::All;
                else if (text == "multi" || text == "multi-core" || text == "multi_core")
                    cpu.mode = CpuBenchmarkMode::MultiCore;
                else if (text == "per-core" || text == "per_core" || text == "percore" || text == "single")
                    cpu.mode = CpuBenchmarkMode::PerCore;
                else throw std::invalid_argument("unknown CPU mode: " + text);
            };
            if (arg == "--cpu-benchmark") {
                cpuRequested = true;
                if (i + 1 < argc && argv[i + 1][0] != '-') mode(value());
            } else if (arg == "--cpu-mode") { cpuRequested = true; mode(value()); }
            else if (arg == "--cpu-time") { cpuRequested = true; cpu.measureSeconds = number(0.03, 3600); }
            else if (arg == "--cpu-warmup") { cpuRequested = true; cpu.warmupSeconds = number(0, 60); }
            else if (arg == "--cpu-no-save") { cpuRequested = true; saveCpu = false; }
            else if (arg == "--cpu-membw") memoryRequested = true;
            else if (arg == "--cpu-membw-max-mib") { memoryRequested = true; memory.maxDatasetBytes = whole(1, 65536) << 20; }
            else if (arg == "--cpu-membw-vector") {
                memoryRequested = true;
                const std::string bits = value();
                if (bits == "auto") memory.vectorBytes = 0;
                else if (bits == "128") memory.vectorBytes = 16;
                else if (bits == "256") memory.vectorBytes = 32;
                else if (bits == "512") memory.vectorBytes = 64;
                else throw std::invalid_argument("--cpu-membw-vector must be auto, 128, 256 or 512");
            } else if (arg == "--cpu-membw-no-storage") { memoryRequested = true; memory.storageTest = false; }
            else if (arg == "--cpu-membw-storage-dir") { memoryRequested = true; memory.storageDir = value(); }
            else if (arg == "--cpu-membw-storage-mib") { memoryRequested = true; memory.storageFileBytes = whole(32, 65536) << 20; }
            else if (arg == "--cpu-membw-curve-levels") { memoryRequested = true; memory.levelsFromCurve = true; }
            else if (arg == "--cpu-membw-debug-classes") { memoryRequested = true; memory.debugSplitClasses = static_cast<std::uint32_t>(whole(2, 8)); }
            else if (arg == "--cpu-membw-debug-hide-llc") { memoryRequested = true; memory.debugHideLastCache = true; }
            else if (arg == "--cpu-membw-reread") reread = value();
            else if (arg == "--cpu-membw-no-save") { memoryRequested = true; saveMemory = false; }
            else throw std::invalid_argument("unsupported CPU option: " + arg);
        }
        if (!reread.empty()) return RereadCpuMemoryBandwidthCli(reread);
        if (memoryRequested) return RunCpuMemoryBandwidthCli(memory, saveMemory);
        if (cpuRequested) return RunCpuBenchmarkCli(cpu, saveCpu);
        throw std::invalid_argument("expected --cpu-benchmark or --cpu-membw");
    } catch (const std::exception& e) {
        std::cerr << "CPU_ERROR\tmessage=" << e.what() << '\n';
        return 2;
    }
}

}  // namespace gpu_bench
