"""Extract the production kernel for isolated branch/tuning diagnostics.

No result from this program is a production score. Never import it into the
device histories. Run only after the independent Geekbench process finishes.
"""
from pathlib import Path
import hashlib
import json

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
original = (REPO / "src/cpu_benchmark.cpp").read_text()
fragment = original[original.index("struct KernelState {"):original.index("static std::string SanitizeField")]
assert fragment.count("static void RunKernelBatch(") == 1
alternate = fragment[fragment.index("static void RunKernelBatch("):fragment.index("static KernelState MakeKernelState")]
alternate = alternate.replace("RunKernelBatch(", "RunAlternatingBranchBatch(").replace("if ((a ^ b) & 1ull)", "if (i & 1ull)")
assert alternate != fragment and "if (i & 1ull)" in alternate
headers = r'''
#include "cpu_scheduling.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <TargetConditionals.h>
constexpr std::uint64_t kKernelBatch = 4096;
'''
driver = r'''
std::atomic<std::uint64_t> sink{0};
int main(int argc, char** argv) {
    using Clock = std::chrono::steady_clock;
    using namespace gpu_bench;
    if (argc != 2) return 2;
    const std::string mode = argv[1];
    if (mode != "random" && mode != "alternating") return 2;
    const auto batch = mode == "random" ? RunKernelBatch : RunAlternatingBranchBatch;
    int result = 0;
    std::thread worker([&] {
        CpuQosScope qos(CpuCoreType::Performance);
        if (!qos.applied()) { result = 3; return; }
        auto fixed = MakeKernelState(0x4350555f53494e47ull);
        for (int i = 0; i < 256; ++i) batch(fixed);
        sink.fetch_xor(fixed.checksum);
        std::cout << "FIXED mode=" << mode << " batches=256 checksum=" << std::hex
                  << fixed.checksum << std::dec << '\n' << std::flush;
        for (int round = 1; round <= 3; ++round) {
            auto state = MakeKernelState(0x4350555f53494e47ull);
            auto warmEnd = Clock::now() + std::chrono::milliseconds(200);
            do { batch(state); } while (Clock::now() < warmEnd);
            const auto before = CaptureCpuThreadCounters();
            const auto start = Clock::now();
            const auto end = start + std::chrono::seconds(2);
            std::uint64_t units = 0;
            do { batch(state); units += kKernelBatch; } while (Clock::now() < end);
            const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
            const auto after = CaptureCpuThreadCounters();
            const auto residency = EvaluateCpuThreadCounters(before, after, CpuCoreType::Performance);
            sink.fetch_xor(state.checksum);
            const double score = units / elapsed / 1e6;
            const double busy = residency.workerCpuNs / 1e9 / elapsed;
            const bool valid = residency.verified() && busy >= .90;
            if (!valid) result = 4;
            std::cout << std::fixed << std::setprecision(9)
                      << "ROUND mode=" << mode << " round=" << round
                      << " score=" << score << " units=" << units << " seconds=" << elapsed
                      << " p_lower_bound=" << residency.requestedLowerBound
                      << " busy=" << busy << " effective_ghz=" << residency.effectiveGHz
                      << " cycles_per_work=" << residency.effectiveGHz * 1000 / score
                      << " valid=" << valid << " checksum=" << std::hex << state.checksum
                      << std::dec << '\n' << std::flush;
        }
    });
    worker.join();
    return result;
}
'''
source = headers + fragment + alternate + driver
(ROOT / "diagnostic.cpp.txt").write_text(source)
commands = []
for target in ["default", "apple-m4"]:
    name = "diagnostic-" + target
    cmd = ["xcrun", "clang++", "-O3", "-DNDEBUG", "-std=c++17", "-arch", "arm64",
           "-mmacosx-version-min=12.0", "-I", str(REPO / "src"), "-x", "c++",
           str(ROOT / "diagnostic.cpp.txt"), "-o", str(ROOT / name)]
    if target != "default":
        cmd[2:2] = ["-mcpu=" + target]
    commands.append({"target": target, "binary": name, "command": cmd})
(ROOT / "diagnostic-build-commands.json").write_text(json.dumps(commands, indent=2) + "\n")
(ROOT / "diagnostic-source.json").write_text(json.dumps({
    "productionSourceSha256": hashlib.sha256(original.encode()).hexdigest(),
    "sourceSha256": hashlib.sha256(source.encode()).hexdigest(),
    "randomKernelExtractedVerbatim": True,
    "alternatingVariantChange": "Only branch predicate: ((a ^ b) & 1ull) -> (i & 1ull)",
    "timing": "Three 2-second rounds, .2-second warmup each, P QoS and verified per-thread P residency",
    "scope": "CONTROLLED DIAGNOSTIC; not production score, not cross-device general ranking",
}, indent=2) + "\n")
print("Prepared source and compile commands; no compilation or performance run yet")
