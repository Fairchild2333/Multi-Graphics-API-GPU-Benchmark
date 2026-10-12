#include "cpu_membw.h"

#include "cpu_benchmark.h"
#include "cpu_parked_cores.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <mutex>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <intrin.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#include <sys/vfs.h>
#endif
#if defined(__APPLE__)
#include <sys/sysctl.h>
#if TARGET_OS_IPHONE
#include <os/proc.h>
#else
#include <mach/mach.h>
#endif
#endif
#endif

// 64-bit x86, and 32-bit x86 as Android builds it (SSE2 is part of that ABI):
// without the second a 32-bit build ran the scalar kernels (an Atom tablet on
// Android 6.0, 2026-10-08), as a 32-bit ARM build did before it.
#if (defined(_M_X64) && !defined(_M_ARM64EC)) || defined(__x86_64__) || \
    (defined(__i386__) && defined(__SSE2__))
#define CPUMEM_X86 1
#include <immintrin.h>
#if !defined(_MSC_VER)
#include <cpuid.h>
#include <x86intrin.h>
#endif
// 64-bit ARM, and 32-bit ARM built with NEON: the kernels use nothing that
// ARMv7 NEON lacks. Without this a 32-bit build ran the scalar kernels (a
// Tegra K1 tablet, 2026-10-08) and its figures compared with nothing else.
#elif defined(_M_ARM64) || defined(__aarch64__) || (defined(__arm__) && defined(__ARM_NEON))
#define CPUMEM_ARM_NEON 1
#include <arm_neon.h>
#endif

// GCC and Clang only accept wide-vector intrinsics inside functions that
// enable the instruction set; MSVC accepts them anywhere.
#if defined(__GNUC__) || defined(__clang__)
#define CPUMEM_TARGET(features) __attribute__((target(features)))
#else
#define CPUMEM_TARGET(features)
#endif

namespace gpu_bench {
namespace {

using Clock = std::chrono::steady_clock;

constexpr std::uint64_t kKiB = 1024;
constexpr std::uint64_t kMiB = 1024 * kKiB;
constexpr std::uint64_t kGiB = 1024 * kMiB;
constexpr std::uint64_t kMinBlockBytes = 4 * kKiB;
// A worker checks the stop flag once per slice. A block smaller than a slice
// is walked often enough to fill one. Every kernel length is a multiple of
// 256 bytes, which is four vectors at the widest vector size.
constexpr std::size_t kSliceBytes = 256 * kKiB;
constexpr std::uint64_t kAllCoreMaxTotalBytes = 8 * kGiB;
// A data set counts as memory-bound from four times the last-level cache
// share of one thread. Without cache sizes from the OS this size is used.
constexpr std::uint64_t kMemoryFallbackBytes = 256 * kMiB;
constexpr double kSettledSpread = 1.25;
constexpr std::uint64_t kPageFaultRegionBytes = 256 * kMiB;
constexpr std::uint32_t kPageFaultRuns = 9;
// A round whose median is this far below its best sample was disturbed by
// other activity on the machine and is measured again, at most twice.
constexpr double kDisturbedRatio = 0.85;
constexpr std::uint32_t kMaxRemeasure = 2;
// Reading levels off the curve: a plateau is at least this many consecutive
// data sets within kPlateauSpread of each other, and a new level is at least
// kLevelDrop times slower than the one before it.
constexpr double kPlateauSpread = 1.15;
constexpr std::size_t kPlateauMinPoints = 3;
constexpr double kLevelDrop = 1.15;
// The system cache behind the last cache of the CPU can be so small that only
// two data sets of the sweep fall into it (8 and 12 MiB on a Snapdragon 8 Gen
// 2, whose 5 MiB system cache follows an 8 MiB L3). Two data sets directly
// before the memory tail are a level when they agree this closely and stand
// clear of both neighbours by kLevelDrop.
constexpr double kShortPlateauSpread = 1.05;
// On an ARM system a cache the OS does not list, or a fourth level on the
// curve, is the system cache of the SoC, which the CPU shares with the GPU and
// other parts. Elsewhere such a level keeps a number.
#if defined(CPUMEM_ARM_NEON)
constexpr bool kSystemCacheNaming = true;
#else
constexpr bool kSystemCacheNaming = false;
#endif
// Storage test: 1 MiB sequential requests and 4 KiB random requests on a file
// that is written once beforehand.
constexpr std::size_t kStorageBlockBytes = 1024 * 1024;
constexpr std::size_t kStoragePageBytes = 4096;
constexpr std::uint64_t kStorageMinFileBytes = 32 * kMiB;
constexpr double kStoragePrepareSeconds = 4.0;
constexpr double kStorageSettleSeconds = 1.0;
constexpr double kStorageSequentialSeconds = 2.0;
constexpr double kStorageRandomSeconds = 2.0;

enum class Kernel { Read, Write, Copy, WriteStream, CopyStream, ZeroFill };
constexpr std::size_t kKernelCount = 6;

const char* KernelName(Kernel kernel) {
    switch (kernel) {
        case Kernel::Read:        return "read";
        case Kernel::Write:       return "write";
        case Kernel::Copy:        return "copy";
        case Kernel::WriteStream: return "write_nt";
        case Kernel::CopyStream:  return "copy_nt";
        case Kernel::ZeroFill:    return "zero";
    }
    return "";
}

// `bytes` is a multiple of 256 and every pointer is 64-byte aligned.
using ReadFn = std::uint64_t (*)(const std::uint8_t* data, std::size_t bytes, std::size_t passes);
using WriteFn = void (*)(std::uint8_t* data, std::size_t bytes, std::size_t passes,
                         std::uint64_t seed);
using CopyFn = void (*)(const std::uint8_t* source, std::uint8_t* destination,
                        std::size_t bytes, std::size_t passes);

struct KernelSet {
    const char* name = "";
    std::uint32_t vectorBytes = 0;
    ReadFn read = nullptr;
    WriteFn write = nullptr;
    CopyFn copy = nullptr;
    WriteFn writeStream = nullptr;
    CopyFn copyStream = nullptr;
    WriteFn zeroFill = nullptr;
};

// ---------------------------------------------------------------------------
// Kernels. Each loop body handles four vectors with independent accumulators,
// so the load ports rather than one add chain set the pace, and no statement
// is a contiguous copy or fill that a compiler could replace with memcpy or
// memset.
// ---------------------------------------------------------------------------

#if defined(CPUMEM_X86)

std::uint64_t ReadSse2(const std::uint8_t* data, std::size_t bytes, std::size_t passes) {
    __m128i a0 = _mm_setzero_si128(), a1 = a0, a2 = a0, a3 = a0;
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::size_t i = 0; i < bytes; i += 64) {
            a0 = _mm_add_epi64(a0, _mm_load_si128(reinterpret_cast<const __m128i*>(data + i)));
            a1 = _mm_add_epi64(a1, _mm_load_si128(reinterpret_cast<const __m128i*>(data + i + 16)));
            a2 = _mm_add_epi64(a2, _mm_load_si128(reinterpret_cast<const __m128i*>(data + i + 32)));
            a3 = _mm_add_epi64(a3, _mm_load_si128(reinterpret_cast<const __m128i*>(data + i + 48)));
        }
    }
    a0 = _mm_add_epi64(_mm_add_epi64(a0, a1), _mm_add_epi64(a2, a3));
    alignas(16) std::uint64_t lanes[2];
    _mm_store_si128(reinterpret_cast<__m128i*>(lanes), a0);
    return lanes[0] + lanes[1];
}

void WriteSse2(std::uint8_t* data, std::size_t bytes, std::size_t passes, std::uint64_t seed) {
    __m128i value = _mm_set1_epi64x(static_cast<long long>(seed));
    const __m128i one = _mm_set1_epi64x(1);
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::size_t i = 0; i < bytes; i += 64) {
            _mm_store_si128(reinterpret_cast<__m128i*>(data + i), value);
            _mm_store_si128(reinterpret_cast<__m128i*>(data + i + 16), value);
            _mm_store_si128(reinterpret_cast<__m128i*>(data + i + 32), value);
            _mm_store_si128(reinterpret_cast<__m128i*>(data + i + 48), value);
        }
        value = _mm_add_epi64(value, one);
    }
}

void WriteStreamSse2(std::uint8_t* data, std::size_t bytes, std::size_t passes,
                     std::uint64_t seed) {
    __m128i value = _mm_set1_epi64x(static_cast<long long>(seed));
    const __m128i one = _mm_set1_epi64x(1);
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::size_t i = 0; i < bytes; i += 64) {
            _mm_stream_si128(reinterpret_cast<__m128i*>(data + i), value);
            _mm_stream_si128(reinterpret_cast<__m128i*>(data + i + 16), value);
            _mm_stream_si128(reinterpret_cast<__m128i*>(data + i + 32), value);
            _mm_stream_si128(reinterpret_cast<__m128i*>(data + i + 48), value);
        }
        value = _mm_add_epi64(value, one);
    }
    _mm_sfence();
}

void CopySse2(const std::uint8_t* source, std::uint8_t* destination, std::size_t bytes,
              std::size_t passes) {
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::size_t i = 0; i < bytes; i += 64) {
            const __m128i v0 = _mm_load_si128(reinterpret_cast<const __m128i*>(source + i));
            const __m128i v1 = _mm_load_si128(reinterpret_cast<const __m128i*>(source + i + 16));
            const __m128i v2 = _mm_load_si128(reinterpret_cast<const __m128i*>(source + i + 32));
            const __m128i v3 = _mm_load_si128(reinterpret_cast<const __m128i*>(source + i + 48));
            _mm_store_si128(reinterpret_cast<__m128i*>(destination + i), v0);
            _mm_store_si128(reinterpret_cast<__m128i*>(destination + i + 16), v1);
            _mm_store_si128(reinterpret_cast<__m128i*>(destination + i + 32), v2);
            _mm_store_si128(reinterpret_cast<__m128i*>(destination + i + 48), v3);
        }
    }
}

void CopyStreamSse2(const std::uint8_t* source, std::uint8_t* destination, std::size_t bytes,
                    std::size_t passes) {
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::size_t i = 0; i < bytes; i += 64) {
            const __m128i v0 = _mm_load_si128(reinterpret_cast<const __m128i*>(source + i));
            const __m128i v1 = _mm_load_si128(reinterpret_cast<const __m128i*>(source + i + 16));
            const __m128i v2 = _mm_load_si128(reinterpret_cast<const __m128i*>(source + i + 32));
            const __m128i v3 = _mm_load_si128(reinterpret_cast<const __m128i*>(source + i + 48));
            _mm_stream_si128(reinterpret_cast<__m128i*>(destination + i), v0);
            _mm_stream_si128(reinterpret_cast<__m128i*>(destination + i + 16), v1);
            _mm_stream_si128(reinterpret_cast<__m128i*>(destination + i + 32), v2);
            _mm_stream_si128(reinterpret_cast<__m128i*>(destination + i + 48), v3);
        }
    }
    _mm_sfence();
}

CPUMEM_TARGET("avx2")
std::uint64_t ReadAvx2(const std::uint8_t* data, std::size_t bytes, std::size_t passes) {
    __m256i a0 = _mm256_setzero_si256(), a1 = a0, a2 = a0, a3 = a0;
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::size_t i = 0; i < bytes; i += 128) {
            a0 = _mm256_add_epi64(a0, _mm256_load_si256(reinterpret_cast<const __m256i*>(data + i)));
            a1 = _mm256_add_epi64(a1, _mm256_load_si256(reinterpret_cast<const __m256i*>(data + i + 32)));
            a2 = _mm256_add_epi64(a2, _mm256_load_si256(reinterpret_cast<const __m256i*>(data + i + 64)));
            a3 = _mm256_add_epi64(a3, _mm256_load_si256(reinterpret_cast<const __m256i*>(data + i + 96)));
        }
    }
    a0 = _mm256_add_epi64(_mm256_add_epi64(a0, a1), _mm256_add_epi64(a2, a3));
    alignas(32) std::uint64_t lanes[4];
    _mm256_store_si256(reinterpret_cast<__m256i*>(lanes), a0);
    _mm256_zeroupper();
    return lanes[0] + lanes[1] + lanes[2] + lanes[3];
}

CPUMEM_TARGET("avx2")
void WriteAvx2(std::uint8_t* data, std::size_t bytes, std::size_t passes, std::uint64_t seed) {
    __m256i value = _mm256_set1_epi64x(static_cast<long long>(seed));
    const __m256i one = _mm256_set1_epi64x(1);
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::size_t i = 0; i < bytes; i += 128) {
            _mm256_store_si256(reinterpret_cast<__m256i*>(data + i), value);
            _mm256_store_si256(reinterpret_cast<__m256i*>(data + i + 32), value);
            _mm256_store_si256(reinterpret_cast<__m256i*>(data + i + 64), value);
            _mm256_store_si256(reinterpret_cast<__m256i*>(data + i + 96), value);
        }
        value = _mm256_add_epi64(value, one);
    }
    _mm256_zeroupper();
}

CPUMEM_TARGET("avx2")
void WriteStreamAvx2(std::uint8_t* data, std::size_t bytes, std::size_t passes,
                     std::uint64_t seed) {
    __m256i value = _mm256_set1_epi64x(static_cast<long long>(seed));
    const __m256i one = _mm256_set1_epi64x(1);
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::size_t i = 0; i < bytes; i += 128) {
            _mm256_stream_si256(reinterpret_cast<__m256i*>(data + i), value);
            _mm256_stream_si256(reinterpret_cast<__m256i*>(data + i + 32), value);
            _mm256_stream_si256(reinterpret_cast<__m256i*>(data + i + 64), value);
            _mm256_stream_si256(reinterpret_cast<__m256i*>(data + i + 96), value);
        }
        value = _mm256_add_epi64(value, one);
    }
    _mm_sfence();
    _mm256_zeroupper();
}

CPUMEM_TARGET("avx2")
void CopyAvx2(const std::uint8_t* source, std::uint8_t* destination, std::size_t bytes,
              std::size_t passes) {
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::size_t i = 0; i < bytes; i += 128) {
            const __m256i v0 = _mm256_load_si256(reinterpret_cast<const __m256i*>(source + i));
            const __m256i v1 = _mm256_load_si256(reinterpret_cast<const __m256i*>(source + i + 32));
            const __m256i v2 = _mm256_load_si256(reinterpret_cast<const __m256i*>(source + i + 64));
            const __m256i v3 = _mm256_load_si256(reinterpret_cast<const __m256i*>(source + i + 96));
            _mm256_store_si256(reinterpret_cast<__m256i*>(destination + i), v0);
            _mm256_store_si256(reinterpret_cast<__m256i*>(destination + i + 32), v1);
            _mm256_store_si256(reinterpret_cast<__m256i*>(destination + i + 64), v2);
            _mm256_store_si256(reinterpret_cast<__m256i*>(destination + i + 96), v3);
        }
    }
    _mm256_zeroupper();
}

CPUMEM_TARGET("avx2")
void CopyStreamAvx2(const std::uint8_t* source, std::uint8_t* destination, std::size_t bytes,
                    std::size_t passes) {
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::size_t i = 0; i < bytes; i += 128) {
            const __m256i v0 = _mm256_load_si256(reinterpret_cast<const __m256i*>(source + i));
            const __m256i v1 = _mm256_load_si256(reinterpret_cast<const __m256i*>(source + i + 32));
            const __m256i v2 = _mm256_load_si256(reinterpret_cast<const __m256i*>(source + i + 64));
            const __m256i v3 = _mm256_load_si256(reinterpret_cast<const __m256i*>(source + i + 96));
            _mm256_stream_si256(reinterpret_cast<__m256i*>(destination + i), v0);
            _mm256_stream_si256(reinterpret_cast<__m256i*>(destination + i + 32), v1);
            _mm256_stream_si256(reinterpret_cast<__m256i*>(destination + i + 64), v2);
            _mm256_stream_si256(reinterpret_cast<__m256i*>(destination + i + 96), v3);
        }
    }
    _mm_sfence();
    _mm256_zeroupper();
}

CPUMEM_TARGET("avx512f")
std::uint64_t ReadAvx512(const std::uint8_t* data, std::size_t bytes, std::size_t passes) {
    __m512i a0 = _mm512_setzero_si512(), a1 = a0, a2 = a0, a3 = a0;
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::size_t i = 0; i < bytes; i += 256) {
            a0 = _mm512_add_epi64(a0, _mm512_load_si512(data + i));
            a1 = _mm512_add_epi64(a1, _mm512_load_si512(data + i + 64));
            a2 = _mm512_add_epi64(a2, _mm512_load_si512(data + i + 128));
            a3 = _mm512_add_epi64(a3, _mm512_load_si512(data + i + 192));
        }
    }
    a0 = _mm512_add_epi64(_mm512_add_epi64(a0, a1), _mm512_add_epi64(a2, a3));
    alignas(64) std::uint64_t lanes[8];
    _mm512_store_si512(lanes, a0);
    _mm256_zeroupper();
    std::uint64_t sum = 0;
    for (const std::uint64_t lane : lanes) sum += lane;
    return sum;
}

CPUMEM_TARGET("avx512f")
void WriteAvx512(std::uint8_t* data, std::size_t bytes, std::size_t passes, std::uint64_t seed) {
    __m512i value = _mm512_set1_epi64(static_cast<long long>(seed));
    const __m512i one = _mm512_set1_epi64(1);
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::size_t i = 0; i < bytes; i += 256) {
            _mm512_store_si512(data + i, value);
            _mm512_store_si512(data + i + 64, value);
            _mm512_store_si512(data + i + 128, value);
            _mm512_store_si512(data + i + 192, value);
        }
        value = _mm512_add_epi64(value, one);
    }
    _mm256_zeroupper();
}

CPUMEM_TARGET("avx512f")
void WriteStreamAvx512(std::uint8_t* data, std::size_t bytes, std::size_t passes,
                       std::uint64_t seed) {
    __m512i value = _mm512_set1_epi64(static_cast<long long>(seed));
    const __m512i one = _mm512_set1_epi64(1);
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::size_t i = 0; i < bytes; i += 256) {
            _mm512_stream_si512(reinterpret_cast<__m512i*>(data + i), value);
            _mm512_stream_si512(reinterpret_cast<__m512i*>(data + i + 64), value);
            _mm512_stream_si512(reinterpret_cast<__m512i*>(data + i + 128), value);
            _mm512_stream_si512(reinterpret_cast<__m512i*>(data + i + 192), value);
        }
        value = _mm512_add_epi64(value, one);
    }
    _mm_sfence();
    _mm256_zeroupper();
}

CPUMEM_TARGET("avx512f")
void CopyAvx512(const std::uint8_t* source, std::uint8_t* destination, std::size_t bytes,
                std::size_t passes) {
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::size_t i = 0; i < bytes; i += 256) {
            const __m512i v0 = _mm512_load_si512(source + i);
            const __m512i v1 = _mm512_load_si512(source + i + 64);
            const __m512i v2 = _mm512_load_si512(source + i + 128);
            const __m512i v3 = _mm512_load_si512(source + i + 192);
            _mm512_store_si512(destination + i, v0);
            _mm512_store_si512(destination + i + 64, v1);
            _mm512_store_si512(destination + i + 128, v2);
            _mm512_store_si512(destination + i + 192, v3);
        }
    }
    _mm256_zeroupper();
}

CPUMEM_TARGET("avx512f")
void CopyStreamAvx512(const std::uint8_t* source, std::uint8_t* destination, std::size_t bytes,
                      std::size_t passes) {
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::size_t i = 0; i < bytes; i += 256) {
            const __m512i v0 = _mm512_load_si512(source + i);
            const __m512i v1 = _mm512_load_si512(source + i + 64);
            const __m512i v2 = _mm512_load_si512(source + i + 128);
            const __m512i v3 = _mm512_load_si512(source + i + 192);
            _mm512_stream_si512(reinterpret_cast<__m512i*>(destination + i), v0);
            _mm512_stream_si512(reinterpret_cast<__m512i*>(destination + i + 64), v1);
            _mm512_stream_si512(reinterpret_cast<__m512i*>(destination + i + 128), v2);
            _mm512_stream_si512(reinterpret_cast<__m512i*>(destination + i + 192), v3);
        }
    }
    _mm_sfence();
    _mm256_zeroupper();
}

// AMD's CLZERO zeroes a whole cache line without reading it first. It is the
// one way to write memory that neither fetches the old line nor goes through
// the write-combining buffers of a streaming store.
CPUMEM_TARGET("clzero")
void ZeroFillClzero(std::uint8_t* data, std::size_t bytes, std::size_t passes, std::uint64_t) {
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::size_t i = 0; i < bytes; i += 256) {
            _mm_clzero(data + i);
            _mm_clzero(data + i + 64);
            _mm_clzero(data + i + 128);
            _mm_clzero(data + i + 192);
        }
    }
    _mm_sfence();
}

struct X64Features {
    bool avx2 = false;
    bool avx512f = false;
    bool clzero = false;
};

X64Features DetectX64Features() {
    X64Features features;
#if defined(_MSC_VER)
    int regs[4]{};
    __cpuid(regs, static_cast<int>(0x80000000u));
    if (static_cast<unsigned>(regs[0]) >= 0x80000008u) {
        __cpuid(regs, static_cast<int>(0x80000008u));
        features.clzero = (regs[1] & 1) != 0;
    }
    __cpuid(regs, 0);
    const int maxLeaf = regs[0];
    __cpuid(regs, 1);
    const bool osxsave = (regs[2] & (1 << 27)) != 0;
    const bool avx = (regs[2] & (1 << 28)) != 0;
    if (!osxsave || !avx || maxLeaf < 7) return features;
    // The OS must save the wide registers across context switches.
    const unsigned long long xcr0 = _xgetbv(0);
    const bool ymmState = (xcr0 & 0x6) == 0x6;
    const bool zmmState = (xcr0 & 0xE6) == 0xE6;
    __cpuidex(regs, 7, 0);
    features.avx2 = ymmState && (regs[1] & (1 << 5)) != 0;
    features.avx512f = zmmState && (regs[1] & (1 << 16)) != 0;
#else
    __builtin_cpu_init();
    features.avx2 = __builtin_cpu_supports("avx2") != 0;
    features.avx512f = __builtin_cpu_supports("avx512f") != 0;
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (__get_cpuid(0x80000008u, &a, &b, &c, &d)) features.clzero = (b & 1u) != 0;
#endif
    return features;
}

#elif defined(CPUMEM_ARM_NEON)

std::uint64_t ReadNeon(const std::uint8_t* data, std::size_t bytes, std::size_t passes) {
    uint64x2_t a0 = vdupq_n_u64(0), a1 = a0, a2 = a0, a3 = a0;
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::size_t i = 0; i < bytes; i += 64) {
            a0 = vaddq_u64(a0, vreinterpretq_u64_u8(vld1q_u8(data + i)));
            a1 = vaddq_u64(a1, vreinterpretq_u64_u8(vld1q_u8(data + i + 16)));
            a2 = vaddq_u64(a2, vreinterpretq_u64_u8(vld1q_u8(data + i + 32)));
            a3 = vaddq_u64(a3, vreinterpretq_u64_u8(vld1q_u8(data + i + 48)));
        }
    }
    a0 = vaddq_u64(vaddq_u64(a0, a1), vaddq_u64(a2, a3));
    return vgetq_lane_u64(a0, 0) + vgetq_lane_u64(a0, 1);
}

void WriteNeon(std::uint8_t* data, std::size_t bytes, std::size_t passes, std::uint64_t seed) {
    uint64x2_t value = vdupq_n_u64(seed);
    const uint64x2_t one = vdupq_n_u64(1);
    for (std::size_t pass = 0; pass < passes; ++pass) {
        const uint8x16_t packed = vreinterpretq_u8_u64(value);
        for (std::size_t i = 0; i < bytes; i += 64) {
            vst1q_u8(data + i, packed);
            vst1q_u8(data + i + 16, packed);
            vst1q_u8(data + i + 32, packed);
            vst1q_u8(data + i + 48, packed);
        }
        value = vaddq_u64(value, one);
    }
}

void CopyNeon(const std::uint8_t* source, std::uint8_t* destination, std::size_t bytes,
              std::size_t passes) {
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::size_t i = 0; i < bytes; i += 64) {
            const uint8x16_t v0 = vld1q_u8(source + i);
            const uint8x16_t v1 = vld1q_u8(source + i + 16);
            const uint8x16_t v2 = vld1q_u8(source + i + 32);
            const uint8x16_t v3 = vld1q_u8(source + i + 48);
            vst1q_u8(destination + i, v0);
            vst1q_u8(destination + i + 16, v1);
            vst1q_u8(destination + i + 32, v2);
            vst1q_u8(destination + i + 48, v3);
        }
    }
}

#else

std::uint64_t ReadScalar(const std::uint8_t* data, std::size_t bytes, std::size_t passes) {
    std::uint64_t a0 = 0, a1 = 0, a2 = 0, a3 = 0;
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::size_t i = 0; i < bytes; i += 32) {
            std::uint64_t words[4];
            std::memcpy(words, data + i, sizeof(words));
            a0 += words[0];
            a1 += words[1];
            a2 += words[2];
            a3 += words[3];
        }
    }
    return a0 + a1 + a2 + a3;
}

void WriteScalar(std::uint8_t* data, std::size_t bytes, std::size_t passes, std::uint64_t seed) {
    for (std::size_t pass = 0; pass < passes; ++pass) {
        const std::uint64_t words[4] = {seed + pass, seed + pass, seed + pass, seed + pass};
        for (std::size_t i = 0; i < bytes; i += 32) std::memcpy(data + i, words, sizeof(words));
    }
}

void CopyScalar(const std::uint8_t* source, std::uint8_t* destination, std::size_t bytes,
                std::size_t passes) {
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::size_t i = 0; i < bytes; i += 32) {
            std::uint64_t words[4];
            std::memcpy(words, source + i, sizeof(words));
            std::memcpy(destination + i, words, sizeof(words));
        }
    }
}

#endif

// Picks the kernels for the requested vector size, or the widest supported
// one when `vectorBytes` is 0. Throws when the CPU lacks the requested size.
KernelSet SelectKernels(std::uint32_t vectorBytes) {
#if defined(CPUMEM_X86)
    const X64Features features = DetectX64Features();
    const WriteFn zeroFill = features.clzero ? ZeroFillClzero : nullptr;
    if (vectorBytes == 0) vectorBytes = features.avx512f ? 64u : features.avx2 ? 32u : 16u;
    if (vectorBytes == 64 && features.avx512f) {
        return {"AVX-512", 64, ReadAvx512, WriteAvx512, CopyAvx512, WriteStreamAvx512,
                CopyStreamAvx512, zeroFill};
    }
    if (vectorBytes == 32 && features.avx2) {
        return {"AVX2", 32, ReadAvx2, WriteAvx2, CopyAvx2, WriteStreamAvx2, CopyStreamAvx2,
                zeroFill};
    }
    if (vectorBytes == 16) {
        return {"SSE2", 16, ReadSse2, WriteSse2, CopySse2, WriteStreamSse2, CopyStreamSse2,
                zeroFill};
    }
#elif defined(CPUMEM_ARM_NEON)
    if (vectorBytes == 0 || vectorBytes == 16) {
        return {"NEON", 16, ReadNeon, WriteNeon, CopyNeon, nullptr, nullptr, nullptr};
    }
#else
    if (vectorBytes == 0) {
        return {"scalar", 8, ReadScalar, WriteScalar, CopyScalar, nullptr, nullptr, nullptr};
    }
#endif
    throw std::runtime_error("this CPU or build does not support " +
                             std::to_string(vectorBytes * 8u) + "-bit vectors");
}

// ---------------------------------------------------------------------------
// Platform helpers
// ---------------------------------------------------------------------------

void CpuRelax() {
#if defined(CPUMEM_X86)
    _mm_pause();
#elif defined(CPUMEM_ARM_NEON) && defined(_MSC_VER)
    __yield();
#elif defined(CPUMEM_ARM_NEON)
    __asm__ __volatile__("yield");
#endif
}

bool PinCurrentThread(const CpuLogicalProcessor& cpu) {
#if defined(_WIN32)
    if (cpu.logicalIndex >= sizeof(KAFFINITY) * 8u) return false;
    GROUP_AFFINITY requested{};
    requested.Group = cpu.group;
    requested.Mask = static_cast<KAFFINITY>(1) << cpu.logicalIndex;
    return SetThreadGroupAffinity(GetCurrentThread(), &requested, nullptr) != FALSE;
#elif defined(__linux__)
    if (cpu.logicalIndex >= CPU_SETSIZE) return false;
    cpu_set_t requested;
    CPU_ZERO(&requested);
    CPU_SET(cpu.logicalIndex, &requested);
    // A phone parks cores it has no work for and refuses a thread that asks
    // for one of them; the cores are woken first (cpu_parked_cores.h). Every
    // caller pins before it measures.
    return parked_cores::SetAffinityWaking([&requested] {
#if defined(__ANDROID__) || defined(__OHOS__)
        return sched_setaffinity(0, sizeof(requested), &requested) == 0 ? 0 : errno;
#else
        return pthread_setaffinity_np(pthread_self(), sizeof(requested), &requested);
#endif
    });
#else
    (void)cpu;
    return false;
#endif
}

bool PlatformCanPin() {
#if defined(_WIN32) || defined(__linux__)
    return true;
#else
    return false;
#endif
}

// Fresh, page-aligned memory straight from the OS.
void* AllocatePages(std::size_t bytes) {
#if defined(_WIN32)
    return VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* base = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    return base == MAP_FAILED ? nullptr : base;
#endif
}

void FreePages(void* base, std::size_t bytes) {
    if (!base) return;
#if defined(_WIN32)
    (void)bytes;
    VirtualFree(base, 0, MEM_RELEASE);
#else
    munmap(base, bytes);
#endif
}

std::uint64_t SystemPageBytes() {
#if defined(_WIN32)
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return info.dwPageSize;
#else
    const long size = sysconf(_SC_PAGESIZE);
    return size > 0 ? static_cast<std::uint64_t>(size) : 4096u;
#endif
}

// The sample length is short, so the default 15.6 ms Windows timer tick would
// stretch it. A high-resolution waitable timer needs no global timer change.
void SleepFor(double seconds) {
#if defined(_WIN32)
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
    static const HANDLE timer = CreateWaitableTimerExW(
        nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (timer) {
        LARGE_INTEGER due;
        due.QuadPart = -static_cast<LONGLONG>(seconds * 1.0e7);
        if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
            WaitForSingleObject(timer, INFINITE);
            return;
        }
    }
#endif
    std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
}

struct MemoryInfo {
    std::uint64_t totalBytes = 0;
    std::uint64_t availableBytes = 0;
};

MemoryInfo QueryMemory() {
    MemoryInfo info;
#if defined(_WIN32)
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        info.totalBytes = status.ullTotalPhys;
        info.availableBytes = status.ullAvailPhys;
    }
#elif defined(__linux__)
    std::ifstream file("/proc/meminfo");
    std::string key;
    std::uint64_t value = 0;
    std::string unit;
    while (file >> key >> value) {
        std::getline(file, unit);
        if (key == "MemTotal:") info.totalBytes = value * kKiB;
        else if (key == "MemAvailable:") info.availableBytes = value * kKiB;
    }
#elif defined(__APPLE__)
    std::uint64_t total = 0;
    std::size_t size = sizeof(total);
    if (sysctlbyname("hw.memsize", &total, &size, nullptr, 0) == 0) info.totalBytes = total;
#if TARGET_OS_IPHONE
    // What this app may still allocate before the system ends it. Zero where
    // the system gives no figure; the sweep then uses its fixed ceilings.
    info.availableBytes = os_proc_available_memory();
#else
    // Free pages and pages the system can reclaim without paging out.
    vm_statistics64_data_t vm{};
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    const long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize > 0 &&
        host_statistics64(mach_host_self(), HOST_VM_INFO64,
                          reinterpret_cast<host_info64_t>(&vm), &count) == KERN_SUCCESS) {
        info.availableBytes = (static_cast<std::uint64_t>(vm.free_count) + vm.inactive_count) *
                              static_cast<std::uint64_t>(pageSize);
    }
#endif
#endif
    return info;
}

#if defined(__linux__)
std::string ReadTextFile(const std::string& path) {
    std::ifstream file(path);
    std::string text;
    std::getline(file, text);
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' '))
        text.pop_back();
    return text;
}

// "48K", "1024K", "96M".
std::uint64_t ParseCacheSize(const std::string& text) {
    if (text.empty()) return 0;
    std::uint64_t value = 0;
    std::size_t i = 0;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9')
        value = value * 10 + static_cast<std::uint64_t>(text[i++] - '0');
    if (i < text.size()) {
        if (text[i] == 'K' || text[i] == 'k') value *= kKiB;
        else if (text[i] == 'M' || text[i] == 'm') value *= kMiB;
        else if (text[i] == 'G' || text[i] == 'g') value *= kGiB;
    }
    return value;
}

// "0-7,16-23".
std::set<std::uint32_t> ParseCpuList(const std::string& text) {
    std::set<std::uint32_t> cpus;
    std::stringstream stream(text);
    std::string part;
    while (std::getline(stream, part, ',')) {
        if (part.empty()) continue;
        const auto dash = part.find('-');
        try {
            const unsigned long first = std::stoul(part.substr(0, dash));
            const unsigned long last =
                dash == std::string::npos ? first : std::stoul(part.substr(dash + 1));
            for (unsigned long cpu = first; cpu <= last && cpu < 4096; ++cpu)
                cpus.insert(static_cast<std::uint32_t>(cpu));
        } catch (...) {
            // Malformed entry: leave it out.
        }
    }
    return cpus;
}
#endif

#if defined(__APPLE__)
std::uint64_t SysctlU64(const char* name) {
    std::uint64_t value = 0;
    std::size_t size = sizeof(value);
    if (sysctlbyname(name, &value, &size, nullptr, 0) != 0) return 0;
    // Some keys are 32-bit; the unused upper half stays zero on little-endian.
    return value;
}
#endif

// Data and unified caches that serve `reference`, smallest level first.
std::vector<CpuCacheLevel> ProbeCaches(const CpuLogicalProcessor& reference,
                                       const std::vector<CpuLogicalProcessor>& processors,
                                       std::string& source) {
    std::vector<CpuCacheLevel> caches;
#if defined(_WIN32)
    DWORD bytes = 0;
    GetLogicalProcessorInformationEx(RelationCache, nullptr, &bytes);
    std::vector<std::uint8_t> buffer(bytes);
    if (bytes == 0 ||
        !GetLogicalProcessorInformationEx(
            RelationCache,
            reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()), &bytes)) {
        return caches;
    }
    source = "GetLogicalProcessorInformationEx";
    for (DWORD offset = 0; offset < bytes;) {
        const auto* info =
            reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data() + offset);
        if (info->Size == 0) break;
        offset += info->Size;
        if (info->Relationship != RelationCache) continue;
        const CACHE_RELATIONSHIP& cache = info->Cache;
        if (cache.Type != CacheData && cache.Type != CacheUnified) continue;
        const GROUP_AFFINITY& mask = cache.GroupMask;
        if (mask.Group != reference.group || reference.logicalIndex >= sizeof(KAFFINITY) * 8u ||
            (mask.Mask & (static_cast<KAFFINITY>(1) << reference.logicalIndex)) == 0) {
            continue;
        }
        CpuCacheLevel level;
        level.level = cache.Level;
        level.sizeBytes = cache.CacheSize;
        level.lineBytes = cache.LineSize;
        std::set<std::uint32_t> cores;
        std::uint32_t threads = 0;
        for (const auto& cpu : processors) {
            if (cpu.group != mask.Group || cpu.logicalIndex >= sizeof(KAFFINITY) * 8u) continue;
            if ((mask.Mask & (static_cast<KAFFINITY>(1) << cpu.logicalIndex)) == 0) continue;
            ++threads;
            cores.insert(cpu.physicalCore);
        }
        level.sharedByThreads = (std::max)(1u, threads);
        level.sharedByCores = (std::max<std::uint32_t>)(1u, static_cast<std::uint32_t>(cores.size()));
        caches.push_back(level);
    }
#elif defined(__linux__)
    const std::string base =
        "/sys/devices/system/cpu/cpu" + std::to_string(reference.logicalIndex) + "/cache/index";
    for (int index = 0; index < 10; ++index) {
        const std::string dir = base + std::to_string(index) + "/";
        const std::string type = ReadTextFile(dir + "type");
        if (type.empty()) break;
        if (type != "Data" && type != "Unified") continue;
        CpuCacheLevel level;
        level.level = static_cast<std::uint32_t>(ParseCacheSize(ReadTextFile(dir + "level")));
        level.sizeBytes = ParseCacheSize(ReadTextFile(dir + "size"));
        level.lineBytes =
            static_cast<std::uint32_t>(ParseCacheSize(ReadTextFile(dir + "coherency_line_size")));
        if (level.level == 0 || level.sizeBytes == 0) continue;
        const std::set<std::uint32_t> shared = ParseCpuList(ReadTextFile(dir + "shared_cpu_list"));
        std::set<std::uint32_t> cores;
        std::uint32_t threads = 0;
        for (const auto& cpu : processors) {
            if (shared.count(cpu.logicalIndex) == 0) continue;
            ++threads;
            cores.insert(cpu.physicalCore);
        }
        level.sharedByThreads = (std::max)(1u, threads);
        level.sharedByCores = (std::max<std::uint32_t>)(1u, static_cast<std::uint32_t>(cores.size()));
        caches.push_back(level);
    }
    if (!caches.empty()) source = "sysfs";
#elif defined(__APPLE__)
    (void)reference;
    (void)processors;
    const struct {
        std::uint32_t level;
        const char* perfLevelSize;
        const char* size;
        const char* perfLevelShare;
    } keys[] = {
        {1, "hw.perflevel0.l1dcachesize", "hw.l1dcachesize", nullptr},
        {2, "hw.perflevel0.l2cachesize", "hw.l2cachesize", "hw.perflevel0.cpusperl2"},
        {3, "hw.perflevel0.l3cachesize", "hw.l3cachesize", "hw.perflevel0.cpusperl3"},
    };
    const std::uint32_t line = static_cast<std::uint32_t>(SysctlU64("hw.cachelinesize"));
    for (const auto& key : keys) {
        std::uint64_t size = SysctlU64(key.perfLevelSize);
        if (size == 0) size = SysctlU64(key.size);
        if (size == 0) continue;
        CpuCacheLevel level;
        level.level = key.level;
        level.sizeBytes = size;
        level.lineBytes = line;
        const std::uint64_t share = key.perfLevelShare ? SysctlU64(key.perfLevelShare) : 0;
        level.sharedByCores = share > 0 ? static_cast<std::uint32_t>(share) : 1u;
        level.sharedByThreads = level.sharedByCores;
        caches.push_back(level);
    }
    if (!caches.empty()) source = "sysctl";
#else
    (void)reference;
    (void)processors;
#endif
    std::sort(caches.begin(), caches.end(),
              [](const CpuCacheLevel& a, const CpuCacheLevel& b) { return a.level < b.level; });
    caches.erase(std::unique(caches.begin(), caches.end(),
                             [](const CpuCacheLevel& a, const CpuCacheLevel& b) {
                                 return a.level == b.level;
                             }),
                 caches.end());
    if (caches.empty()) source = "unavailable";
    return caches;
}

// ---------------------------------------------------------------------------
// Formatting and statistics
// ---------------------------------------------------------------------------

double Median(std::vector<double> values) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const std::size_t mid = values.size() / 2;
    return values.size() % 2 == 0 ? 0.5 * (values[mid - 1] + values[mid]) : values[mid];
}

std::string FormatBytes(std::uint64_t bytes) {
    std::ostringstream text;
    const auto scaled = [&](std::uint64_t unit, const char* suffix) {
        if (bytes % unit == 0) {
            text << (bytes / unit);
        } else {
            text << std::fixed << std::setprecision(1)
                 << static_cast<double>(bytes) / static_cast<double>(unit);
        }
        text << ' ' << suffix;
    };
    if (bytes >= kGiB && bytes % kGiB == 0) scaled(kGiB, "GiB");
    else if (bytes >= kMiB) scaled(kMiB, "MiB");
    else scaled(kKiB, "KiB");
    return text.str();
}

std::string SanitizeField(std::string value) {
    for (char& c : value) {
        if (c == '\t' || c == '\r' || c == '\n' || c == '=') c = '_';
    }
    return value;
}

// 4 KiB, 8 KiB, then doubling from 16 KiB with a x1.5 step in between, ending
// on `maxBytes`.
std::vector<std::uint64_t> SweepBlocks(std::uint64_t maxBytes) {
    std::vector<std::uint64_t> blocks;
    for (std::uint64_t base = kMinBlockBytes; base <= maxBytes; base *= 2) {
        blocks.push_back(base);
        const std::uint64_t mid = base + base / 2;
        if (base >= 16 * kKiB && mid <= maxBytes) blocks.push_back(mid);
    }
    if (blocks.empty() || blocks.back() != maxBytes) blocks.push_back(maxBytes);
    return blocks;
}

// Share of one cache instance that one benchmark thread can count on.
std::uint64_t CacheShareBytes(const CpuCacheLevel& cache, std::uint32_t threads) {
    const std::uint32_t sharers = (std::max)(1u, (std::min)(threads, cache.sharedByCores));
    return cache.sizeBytes / sharers;
}

// Smallest per-thread data set that is treated as memory-bound.
std::uint64_t MemoryBandStart(const std::vector<CpuCacheLevel>& caches, std::uint32_t threads) {
    if (caches.empty()) return kMemoryFallbackBytes / (std::max)(1u, threads);
    return 4 * CacheShareBytes(caches.back(), threads);
}

// ---------------------------------------------------------------------------
// Worker threads for one data-set size
// ---------------------------------------------------------------------------

void FillBlock(std::uint8_t* data, std::size_t bytes, std::uint64_t seed) {
    std::uint64_t state = seed;
    for (std::size_t i = 0; i < bytes; i += 8) {
        state += 0x9E3779B97F4A7C15ull;
        std::uint64_t z = state;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z ^= z >> 31;
        std::memcpy(data + i, &z, sizeof(z));
    }
}

// Owns one pinned thread per processor, each with a private block that the
// thread itself allocates and fills. Sample() runs one kernel on all of them
// for a fixed time and returns the summed rate.
class BlockRunner {
public:
    BlockRunner(const std::vector<CpuLogicalProcessor>& cpus, std::size_t blockBytes,
                const KernelSet& kernels)
        : blockBytes_(blockBytes), kernels_(kernels), workers_(cpus.size()) {
        for (std::size_t i = 0; i < cpus.size(); ++i) {
            workers_[i].cpu = cpus[i];
            workers_[i].seed = 0x4d42u + i;
            workers_[i].thread = std::thread(&BlockRunner::WorkerMain, this, i);
        }
        std::unique_lock<std::mutex> lock(mutex_);
        done_.wait(lock, [&] { return initialised_ == workers_.size(); });
    }

    ~BlockRunner() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            quit_ = true;
        }
        wake_.notify_all();
        for (auto& worker : workers_)
            if (worker.thread.joinable()) worker.thread.join();
    }

    BlockRunner(const BlockRunner&) = delete;
    BlockRunner& operator=(const BlockRunner&) = delete;

    bool allocated() const {
        return std::all_of(workers_.begin(), workers_.end(),
                           [](const Worker& worker) { return worker.block != nullptr; });
    }

    std::uint32_t pinned() const {
        return static_cast<std::uint32_t>(std::count_if(
            workers_.begin(), workers_.end(), [](const Worker& worker) { return worker.pinned; }));
    }

    // Bytes per second, summed over the threads.
    double Sample(Kernel kernel, double seconds) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            kernel_ = kernel;
            finished_ = 0;
            armed_.store(0, std::memory_order_relaxed);
            go_.store(false, std::memory_order_relaxed);
            stop_.store(false, std::memory_order_relaxed);
            ++generation_;
        }
        wake_.notify_all();
        // Start every thread within the same microsecond: they spin on `go_`
        // once awake instead of starting as the scheduler wakes them.
        while (armed_.load(std::memory_order_acquire) < workers_.size())
            std::this_thread::yield();
        go_.store(true, std::memory_order_release);
        SleepFor(seconds);
        stop_.store(true, std::memory_order_release);

        std::unique_lock<std::mutex> lock(mutex_);
        done_.wait(lock, [&] { return finished_ == workers_.size(); });
        double rate = 0.0;
        for (const auto& worker : workers_) {
            if (worker.seconds > 0.0) rate += static_cast<double>(worker.bytes) / worker.seconds;
        }
        return rate;
    }

private:
    struct Worker {
        std::thread thread;
        CpuLogicalProcessor cpu;
        std::uint8_t* block = nullptr;
        bool pinned = false;
        std::uint64_t seed = 0;
        std::uint64_t checksum = 0;
        std::size_t cursor = 0;
        std::uint64_t bytes = 0;
        double seconds = 0.0;
    };

    void WorkerMain(std::size_t index) {
        Worker& worker = workers_[index];
        worker.pinned = PinCurrentThread(worker.cpu);
        // Allocating and filling here places the pages on this thread's node
        // and makes every page real before a read touches it.
        worker.block = static_cast<std::uint8_t*>(AllocatePages(blockBytes_));
        if (worker.block) FillBlock(worker.block, blockBytes_, worker.seed);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++initialised_;
        }
        done_.notify_all();

        std::uint64_t seen = 0;
        for (;;) {
            Kernel kernel;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                wake_.wait(lock, [&] { return quit_ || generation_ != seen; });
                if (quit_) break;
                seen = generation_;
                kernel = kernel_;
            }
            armed_.fetch_add(1, std::memory_order_acq_rel);
            while (!go_.load(std::memory_order_acquire)) CpuRelax();
            const auto start = Clock::now();
            RunKernel(worker, kernel);
            worker.seconds = std::chrono::duration<double>(Clock::now() - start).count();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                ++finished_;
            }
            done_.notify_all();
        }
        FreePages(worker.block, blockBytes_);
        worker.block = nullptr;
    }

    // Walks the block slice by slice until the stop flag is set. The position
    // carries over to the next sample, so a block that one sample cannot
    // cover is still visited end to end.
    void RunKernel(Worker& worker, Kernel kernel) {
        const bool copy = kernel == Kernel::Copy || kernel == Kernel::CopyStream;
        // A copy reads the first half of the block and writes the second.
        const std::size_t span = copy ? blockBytes_ / 2 : blockBytes_;
        const std::size_t slice = copy ? kSliceBytes / 2 : kSliceBytes;
        const std::size_t passes = span < slice ? slice / span : 1;
        std::uint8_t* const block = worker.block;
        std::size_t cursor = worker.cursor < span ? worker.cursor : 0;
        std::uint64_t bytes = 0;
        do {
            const std::size_t length = (std::min)(slice, span - cursor);
            switch (kernel) {
                case Kernel::Read:
                    worker.checksum += kernels_.read(block + cursor, length, passes);
                    break;
                case Kernel::Write:
                    kernels_.write(block + cursor, length, passes, worker.seed++);
                    break;
                case Kernel::WriteStream:
                    kernels_.writeStream(block + cursor, length, passes, worker.seed++);
                    break;
                case Kernel::Copy:
                    kernels_.copy(block + cursor, block + span + cursor, length, passes);
                    break;
                case Kernel::CopyStream:
                    kernels_.copyStream(block + cursor, block + span + cursor, length, passes);
                    break;
                case Kernel::ZeroFill:
                    kernels_.zeroFill(block + cursor, length, passes, 0);
                    break;
            }
            bytes += static_cast<std::uint64_t>(length) * passes * (copy ? 2u : 1u);
            cursor += length;
            if (cursor == span) cursor = 0;
        } while (!stop_.load(std::memory_order_relaxed));
        worker.cursor = cursor;
        worker.bytes = bytes;
    }

    const std::size_t blockBytes_;
    const KernelSet kernels_;
    std::vector<Worker> workers_;

    std::mutex mutex_;
    std::condition_variable wake_;
    std::condition_variable done_;
    std::size_t initialised_ = 0;
    std::size_t finished_ = 0;
    std::uint64_t generation_ = 0;
    bool quit_ = false;
    Kernel kernel_ = Kernel::Read;
    std::atomic<std::size_t> armed_{0};
    std::atomic<bool> go_{false};
    std::atomic<bool> stop_{false};
};

// ---------------------------------------------------------------------------
// Page mapping cost
// ---------------------------------------------------------------------------

CpuMemBwPageFault MeasurePageFaults(const CpuLogicalProcessor& cpu, std::uint64_t regionBytes) {
    CpuMemBwPageFault result;
    result.pageBytes = SystemPageBytes();
    result.regionBytes = regionBytes;
#if defined(__linux__)
    result.transparentHugePages =
        ReadTextFile("/sys/kernel/mm/transparent_hugepage/enabled").find("[always]") !=
        std::string::npos;
#endif
    if (regionBytes < result.pageBytes) return result;

    std::vector<double> seconds;
    std::thread worker([&] {
        PinCurrentThread(cpu);
        // The first run is a warm-up.
        for (std::uint32_t run = 0; run <= kPageFaultRuns; ++run) {
            void* base = AllocatePages(static_cast<std::size_t>(regionBytes));
            if (!base) return;
            volatile std::uint8_t* bytes = static_cast<std::uint8_t*>(base);
            const auto start = Clock::now();
            for (std::uint64_t offset = 0; offset < regionBytes; offset += result.pageBytes)
                bytes[offset] = 1;
            const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
            FreePages(base, static_cast<std::size_t>(regionBytes));
            if (run > 0) seconds.push_back(elapsed);
        }
    });
    worker.join();
    if (seconds.empty()) return result;

    const double median = Median(seconds);
    if (median <= 0.0) return result;
    result.measured = true;
    result.gbps = static_cast<double>(regionBytes) / median / 1.0e9;
    result.nsPerPage = median * 1.0e9 /
                       (static_cast<double>(regionBytes) / static_cast<double>(result.pageBytes));
    return result;
}

// ---------------------------------------------------------------------------
// Summary
// ---------------------------------------------------------------------------

struct Curve {
    std::vector<std::uint64_t> blocks;
    std::vector<double> rates;
};

Curve CurveOf(const std::vector<CpuMemBwPoint>& points, const std::string& mode,
              const char* kernel) {
    Curve curve;
    for (const auto& point : points) {
        if (point.mode != mode || point.kernel != kernel) continue;
        curve.blocks.push_back(point.blockBytes);
        curve.rates.push_back(point.medianGBps);
    }
    return curve;
}

// Median rate over the data sets in [from, to], or 0 without any.
double BandRate(const Curve& curve, std::uint64_t from, std::uint64_t to) {
    std::vector<double> values;
    for (std::size_t i = 0; i < curve.blocks.size(); ++i) {
        if (curve.blocks[i] >= from && curve.blocks[i] <= to) values.push_back(curve.rates[i]);
    }
    return Median(values);
}

struct ModeCurves {
    Curve read, write, copy, writeStream, copyStream, zeroFill;
};

ModeCurves CurvesOf(const std::vector<CpuMemBwPoint>& points, const std::string& mode) {
    return {CurveOf(points, mode, "read"),     CurveOf(points, mode, "write"),
            CurveOf(points, mode, "copy"),     CurveOf(points, mode, "write_nt"),
            CurveOf(points, mode, "copy_nt"),  CurveOf(points, mode, "zero")};
}

// Fills `level` from the data sets in [from, to]. False without any.
bool FillLevel(const ModeCurves& curves, CpuMemBwLevelSummary& level, std::uint64_t from,
               std::uint64_t to) {
    const Curve& read = curves.read;
    std::uint64_t low = 0, high = 0;
    for (const std::uint64_t block : read.blocks) {
        if (block < from || block > to) continue;
        if (low == 0) low = block;
        high = block;
    }
    if (low == 0) return false;
    level.fromBytes = low;
    level.toBytes = high;
    level.readGBps = BandRate(read, low, high);
    level.writeGBps = BandRate(curves.write, low, high);
    level.copyGBps = BandRate(curves.copy, low, high);
    level.readLowGBps = 0.0;
    level.readHighGBps = 0.0;
    for (std::size_t i = 0; i < read.blocks.size(); ++i) {
        if (read.blocks[i] < low || read.blocks[i] > high) continue;
        level.readLowGBps = level.readLowGBps == 0.0
            ? read.rates[i] : (std::min)(level.readLowGBps, read.rates[i]);
        level.readHighGBps = (std::max)(level.readHighGBps, read.rates[i]);
    }
    level.measured = level.readGBps > 0.0;
    return level.measured;
}

void FillMemoryExtras(const ModeCurves& curves, CpuMemBwLevelSummary& memory) {
    memory.writeStreamGBps = BandRate(curves.writeStream, memory.fromBytes, memory.toBytes);
    memory.copyStreamGBps = BandRate(curves.copyStream, memory.fromBytes, memory.toBytes);
    memory.zeroFillGBps = BandRate(curves.zeroFill, memory.fromBytes, memory.toBytes);
}

// A flat stretch of the read curve: indices of its first and last data set.
struct Plateau {
    std::size_t first = 0;
    std::size_t last = 0;
    double level = 0.0;
};

// Splits the read curve into the flat stretches of a descending staircase.
// A three-point running median removes single outliers first.
// - The memory tail is the longest run of data sets at the end of the curve
//   that agree within kSettledSpread; `flatTail` says whether there is one,
//   and it is then the last plateau returned.
// - Before the tail, a plateau is a run within kPlateauSpread. A run of fewer
//   than kPlateauMinPoints points is a transition, and a run that is not
//   clearly slower than the plateau before it belongs to the same level.
std::vector<Plateau> DetectPlateaus(const Curve& curve, bool& flatTail) {
    const std::size_t count = curve.rates.size();
    std::vector<double> smooth(count);
    for (std::size_t i = 0; i < count; ++i) {
        smooth[i] = (i == 0 || i + 1 == count)
            ? curve.rates[i]
            : Median({curve.rates[i - 1], curve.rates[i], curve.rates[i + 1]});
    }
    const auto levelOf = [&](std::size_t first, std::size_t last) {
        return Median(std::vector<double>(curve.rates.begin() + static_cast<std::ptrdiff_t>(first),
                                          curve.rates.begin() + static_cast<std::ptrdiff_t>(last) + 1));
    };

    std::size_t tailStart = count;
    if (count > 0) {
        double low = smooth[count - 1];
        double high = low;
        tailStart = count - 1;
        while (tailStart > 0) {
            const double newLow = (std::min)(low, smooth[tailStart - 1]);
            const double newHigh = (std::max)(high, smooth[tailStart - 1]);
            if (newLow <= 0.0 || newHigh / newLow > kSettledSpread) break;
            low = newLow;
            high = newHigh;
            --tailStart;
        }
    }
    flatTail = count - tailStart >= kPlateauMinPoints;
    const std::size_t cacheEnd = flatTail ? tailStart : count;

    std::vector<Plateau> plateaus;
    std::size_t start = 0;
    while (start < cacheEnd) {
        double low = smooth[start];
        double high = low;
        std::size_t end = start;
        while (end + 1 < cacheEnd) {
            const double newLow = (std::min)(low, smooth[end + 1]);
            const double newHigh = (std::max)(high, smooth[end + 1]);
            if (newLow <= 0.0 || newHigh / newLow > kPlateauSpread) break;
            low = newLow;
            high = newHigh;
            ++end;
        }
        if (end - start + 1 >= kPlateauMinPoints) {
            const double level = levelOf(start, end);
            if (!plateaus.empty() && level * kLevelDrop > plateaus.back().level) {
                plateaus.back().last = end;
                plateaus.back().level = levelOf(plateaus.back().first, end);
            } else {
                plateaus.push_back({start, end, level});
            }
        } else if (flatTail && end + 1 == cacheEnd && end - start + 1 == 2 && !plateaus.empty()) {
            // A short level directly before memory: see kShortPlateauSpread.
            const double slower = (std::min)(curve.rates[start], curve.rates[end]);
            const double faster = (std::max)(curve.rates[start], curve.rates[end]);
            const double level = levelOf(start, end);
            const double tail = levelOf(tailStart, count - 1);
            if (slower > 0.0 && faster / slower <= kShortPlateauSpread &&
                plateaus.back().level >= level * kLevelDrop && level >= tail * kLevelDrop) {
                plateaus.push_back({start, end, level});
            }
        }
        start = end + 1;
    }
    if (flatTail) plateaus.push_back({tailStart, count - 1, levelOf(tailStart, count - 1)});
    return plateaus;
}

// A cache level read off a plateau. Its capacity lies between the last data
// set of the plateau and the first data set of the next one.
CpuMemBwLevelSummary CurveLevel(const std::string& name, const ModeCurves& curves,
                                const Plateau& plateau, const Plateau* next) {
    CpuMemBwLevelSummary level;
    level.name = name;
    level.fromCurve = true;
    FillLevel(curves, level, curves.read.blocks[plateau.first], curves.read.blocks[plateau.last]);
    level.capacityLowBytes = curves.read.blocks[plateau.last];
    level.capacityHighBytes = next ? curves.read.blocks[next->first] : 0;
    return level;
}

// The name of the n-th cache the OS does not list (0-based): "SoC" on an ARM
// system, the next level number elsewhere.
std::string UnlistedLevelName(std::size_t index, std::uint32_t lastListedLevel, bool systemCache) {
    if (!systemCache) return "L" + std::to_string(lastListedLevel + 1 + index);
    return index == 0 ? "SoC" : "SoC" + std::to_string(index + 1);
}

// Levels from the cache sizes the OS reports.
CpuMemBwModeSummary SummariseMode(const std::vector<CpuMemBwPoint>& points,
                                  const std::string& mode, std::uint32_t threads,
                                  const std::vector<CpuCacheLevel>& caches,
                                  bool systemCacheNaming = kSystemCacheNaming) {
    CpuMemBwModeSummary summary;
    summary.mode = mode;
    summary.threads = threads;
    const ModeCurves curves = CurvesOf(points, mode);
    const Curve& read = curves.read;
    if (read.blocks.empty()) return summary;
    summary.maxBlockBytes = read.blocks.back();

    // A cache level is read off the data sets that are at least twice the
    // next smaller level and at most half of this one, so neither neighbour
    // contributes. The first level has no smaller neighbour.
    std::uint64_t previous = 0;
    for (const auto& cache : caches) {
        CpuMemBwLevelSummary level;
        level.name = "L" + std::to_string(cache.level);
        level.cacheBytes = cache.sizeBytes;
        const std::uint64_t share = CacheShareBytes(cache, threads);
        const std::uint64_t from = previous == 0 ? share / 4 : 2 * previous;
        if (!FillLevel(curves, level, from, share / 2)) {
            // Levels less than four times apart: accept the narrower range
            // between 1.5x the smaller level and 0.75x this one.
            const std::uint64_t looseFrom = previous == 0 ? 0 : previous + previous / 2 + 1;
            FillLevel(curves, level, looseFrom, share / 4 * 3);
        }
        summary.levels.push_back(level);
        previous = (std::max)(previous, share);
    }

    CpuMemBwLevelSummary memory;
    memory.name = "memory";
    const std::uint64_t memoryFrom = MemoryBandStart(caches, threads);
    const std::uint64_t unlimited = ~0ull;
    std::size_t count = 0;
    for (const std::uint64_t block : read.blocks) count += block >= memoryFrom ? 1u : 0u;
    if (count >= 2) {
        FillLevel(curves, memory, memoryFrom, unlimited);
        memory.settled = memory.readLowGBps > 0.0 &&
                         memory.readHighGBps / memory.readLowGBps <= kSettledSpread;
    } else {
        // The sweep ends inside the caches: report its largest data set and
        // say that it is not a memory figure.
        FillLevel(curves, memory, read.blocks.back(), unlimited);
        memory.settled = false;
    }

    // An unsettled memory band can hide a cache the OS did not report, such
    // as a system-level cache of an SoC. If the curve has a flat tail, that
    // tail is memory and the plateaus between the last reported cache and the
    // tail are further cache levels.
    if (!memory.settled && !caches.empty()) {
        bool flatTail = false;
        std::vector<Plateau> above;
        for (const Plateau& plateau : DetectPlateaus(read, flatTail)) {
            if (read.blocks[plateau.last] > 4 * previous) above.push_back(plateau);
        }
        if (flatTail && !above.empty()) {
            for (std::size_t i = 0; i + 1 < above.size(); ++i) {
                summary.levels.push_back(CurveLevel(
                    UnlistedLevelName(i, caches.back().level, systemCacheNaming), curves, above[i],
                    &above[i + 1]));
            }
            memory = CpuMemBwLevelSummary();
            memory.name = "memory";
            memory.fromCurve = true;
            FillLevel(curves, memory, read.blocks[above.back().first], unlimited);
        }
    }
    FillMemoryExtras(curves, memory);
    summary.levels.push_back(memory);
    return summary;
}

// Levels from the shape of the curve alone, for systems that report no cache
// sizes: every plateau but the last is a cache level, the flat tail is memory.
// A CPU has three levels of its own at most; with `systemCacheNaming` a level
// after the third is the system cache of the SoC.
CpuMemBwModeSummary SummariseFromCurve(const std::vector<CpuMemBwPoint>& points,
                                       const std::string& mode, std::uint32_t threads,
                                       bool systemCacheNaming = kSystemCacheNaming) {
    CpuMemBwModeSummary summary;
    summary.mode = mode;
    summary.threads = threads;
    const ModeCurves curves = CurvesOf(points, mode);
    const Curve& read = curves.read;
    if (read.blocks.empty()) return summary;
    summary.maxBlockBytes = read.blocks.back();

    bool flatTail = false;
    const std::vector<Plateau> plateaus = DetectPlateaus(read, flatTail);
    const std::size_t cacheCount = flatTail ? plateaus.size() - 1 : plateaus.size();
    for (std::size_t i = 0; i < cacheCount; ++i) {
        const std::string name = i < 3 ? "L" + std::to_string(i + 1)
                                       : UnlistedLevelName(i - 3, 3, systemCacheNaming);
        summary.levels.push_back(CurveLevel(name, curves, plateaus[i],
                                            i + 1 < plateaus.size() ? &plateaus[i + 1] : nullptr));
    }
    CpuMemBwLevelSummary memory;
    memory.name = "memory";
    memory.fromCurve = true;
    if (flatTail) {
        FillLevel(curves, memory, read.blocks[plateaus.back().first], ~0ull);
    } else {
        FillLevel(curves, memory, read.blocks.back(), ~0ull);
        memory.settled = false;
    }
    FillMemoryExtras(curves, memory);
    summary.levels.push_back(memory);
    return summary;
}

// `protocol`: also print the CPUMEM_LEVEL lines that front ends read.
void PrintSummary(const CpuMemBwModeSummary& summary, const std::string& title, std::ostream& out,
                  bool protocol = true) {
    out << "\n--- Summary: " << title << " ---\n"
        << "Level              Cache size          Data sets used       Read      Write       Copy"
           "   GB/s\n";
    bool anyFromCurve = false;
    bool anySystemCache = false;
    for (const auto& level : summary.levels) {
        const bool memory = level.name == "memory";
        anySystemCache = anySystemCache || level.name.rfind("SoC", 0) == 0;
        std::string size = "-";
        if (!memory && level.fromCurve) {
            anyFromCurve = true;
            size = level.capacityHighBytes > 0
                ? FormatBytes(level.capacityLowBytes) + " - " + FormatBytes(level.capacityHighBytes) + " *"
                : "> " + FormatBytes(level.capacityLowBytes) + " *";
        } else if (!memory) {
            size = FormatBytes(level.cacheBytes);
        }
        out << std::left << std::setw(8) << (memory ? "Memory" : level.name) << std::right
            << std::setw(21) << size;
        if (!level.measured) {
            out << "   not separable: no data set lies between twice the smaller cache and\n"
                << std::setw(32) << "" << "half of this one's share per thread\n";
        } else {
            const std::string range = level.fromBytes == level.toBytes
                ? FormatBytes(level.fromBytes)
                : FormatBytes(level.fromBytes) + " - " + FormatBytes(level.toBytes);
            out << std::setw(24) << range << std::fixed << std::setprecision(1)
                << std::setw(11) << level.readGBps << std::setw(11) << level.writeGBps
                << std::setw(11) << level.copyGBps << "\n";
            if (memory && level.writeStreamGBps > 0.0) {
                out << std::setw(53) << "with streaming stores" << std::setw(11) << "-"
                    << std::setw(11) << level.writeStreamGBps << std::setw(11)
                    << level.copyStreamGBps << "\n";
            }
            if (memory && level.zeroFillGBps > 0.0) {
                out << std::setw(53) << "zero fill with CLZERO" << std::setw(11) << "-"
                    << std::setw(11) << level.zeroFillGBps << std::setw(11) << "-" << "\n";
            }
            if (memory && !level.settled) {
                out << "         not a settled memory plateau: the sweep ends inside the caches or"
                       " is still descending\n";
            } else if (!memory && level.readLowGBps > 0.0 &&
                       level.readHighGBps / level.readLowGBps > kSettledSpread) {
                out << "         not flat: read ranges from " << level.readLowGBps << " to "
                    << level.readHighGBps << " GB/s across these data sets\n";
            }
        }
        if (!protocol) continue;
        out << "CPUMEM_LEVEL\tmode=" << summary.mode << "\tlevel=" << level.name
            << "\tcache_bytes=" << level.cacheBytes
            << "\tfrom_bytes=" << level.fromBytes << "\tto_bytes=" << level.toBytes
            << std::fixed << std::setprecision(3)
            << "\tread_gbps=" << level.readGBps << "\twrite_gbps=" << level.writeGBps
            << "\tcopy_gbps=" << level.copyGBps
            << "\twrite_nt_gbps=" << level.writeStreamGBps
            << "\tcopy_nt_gbps=" << level.copyStreamGBps
            << "\tzero_gbps=" << level.zeroFillGBps
            << "\tread_low_gbps=" << level.readLowGBps
            << "\tread_high_gbps=" << level.readHighGBps
            << "\tfrom_curve=" << (level.fromCurve ? 1 : 0)
            << "\tcapacity_low_bytes=" << level.capacityLowBytes
            << "\tcapacity_high_bytes=" << level.capacityHighBytes
            << "\tmeasured=" << (level.measured ? 1 : 0)
            << "\tsettled=" << (level.settled ? 1 : 0) << "\n";
    }
    if (anyFromCurve) {
        out << "         * read off the curve, because the OS reports no size for this cache: its\n"
            << "           capacity lies between the two data sets shown\n";
    }
    if (anySystemCache) {
        out << "         SoC: a cache behind the CPU's own levels that the OS does not list. On a\n"
            << "           phone chip it is the system cache, which the CPU shares with the GPU and\n"
            << "           other parts; its capacity above includes the caches before it\n";
    }
}

// ---------------------------------------------------------------------------
// Storage behind virtual memory
// ---------------------------------------------------------------------------

// A temporary file opened for unbuffered I/O where the platform allows it.
// The file is removed when it is closed, also if the process dies.
class TestFile {
public:
    ~TestFile() { Close(); }

    bool Open(const std::string& path, std::uint64_t reserveBytes, bool& direct) {
#if defined(_WIN32)
        const int length = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
        std::wstring wide(static_cast<std::size_t>(length > 0 ? length : 1), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wide.data(), length);
        handle_ = CreateFileW(wide.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_FLAG_NO_BUFFERING | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) return false;
        direct = true;
        LARGE_INTEGER size;
        size.QuadPart = static_cast<LONGLONG>(reserveBytes);
        if (SetFilePointerEx(handle_, size, nullptr, FILE_BEGIN)) SetEndOfFile(handle_);
        return true;
#else
        const int flags = O_RDWR | O_CREAT | O_TRUNC;
        direct = false;
#if defined(O_DIRECT)
        fd_ = open(path.c_str(), flags | O_DIRECT, 0600);
        direct = fd_ >= 0;
#endif
        if (fd_ < 0) fd_ = open(path.c_str(), flags, 0600);
        if (fd_ < 0) return false;
#if defined(__APPLE__)
        direct = fcntl(fd_, F_NOCACHE, 1) == 0;
#endif
        unlink(path.c_str());
        if (ftruncate(fd_, static_cast<off_t>(reserveBytes)) != 0) {
            // Not fatal: the file grows as it is written.
        }
        return true;
#endif
    }

    bool Write(std::uint64_t offset, const std::uint8_t* data, std::size_t bytes) {
#if defined(_WIN32)
        OVERLAPPED at{};
        at.Offset = static_cast<DWORD>(offset);
        at.OffsetHigh = static_cast<DWORD>(offset >> 32);
        DWORD done = 0;
        return WriteFile(handle_, data, static_cast<DWORD>(bytes), &done, &at) && done == bytes;
#else
        std::size_t done = 0;
        while (done < bytes) {
            const ssize_t n = pwrite(fd_, data + done, bytes - done, static_cast<off_t>(offset + done));
            if (n <= 0) return false;
            done += static_cast<std::size_t>(n);
        }
        return true;
#endif
    }

    bool Read(std::uint64_t offset, std::uint8_t* data, std::size_t bytes) {
#if defined(_WIN32)
        OVERLAPPED at{};
        at.Offset = static_cast<DWORD>(offset);
        at.OffsetHigh = static_cast<DWORD>(offset >> 32);
        DWORD done = 0;
        return ReadFile(handle_, data, static_cast<DWORD>(bytes), &done, &at) && done == bytes;
#else
        std::size_t done = 0;
        while (done < bytes) {
            const ssize_t n = pread(fd_, data + done, bytes - done, static_cast<off_t>(offset + done));
            if (n <= 0) return false;
            done += static_cast<std::size_t>(n);
        }
        return true;
#endif
    }

    // Waits until written data has reached the drive.
    void Flush() {
#if defined(_WIN32)
        FlushFileBuffers(handle_);
#else
        fsync(fd_);
#endif
    }

    // Asks the OS to forget the cached pages of the file, so the next reads
    // come from the drive. Nothing to forget where unbuffered I/O works.
    void DropCache() {
#if !defined(_WIN32) && defined(POSIX_FADV_DONTNEED)
        posix_fadvise(fd_, 0, 0, POSIX_FADV_DONTNEED);
#endif
    }

    void Close() {
#if defined(_WIN32)
        if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
#else
        if (fd_ >= 0) close(fd_);
        fd_ = -1;
#endif
    }

private:
#if defined(_WIN32)
    HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
    int fd_ = -1;
#endif
};

#if defined(_WIN32)
// Drive letter of the first page file, or 0 without one.
char PageFileDriveLetter() {
    wchar_t value[1024]{};
    DWORD bytes = sizeof(value) - sizeof(wchar_t);
    if (RegGetValueW(HKEY_LOCAL_MACHINE,
                     L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management",
                     L"ExistingPageFiles", RRF_RT_REG_MULTI_SZ, nullptr, value,
                     &bytes) != ERROR_SUCCESS) {
        return 0;
    }
    // "\??\C:\pagefile.sys"
    for (std::size_t i = 1; i < 1023 && value[i] != L'\0'; ++i) {
        if (value[i] == L':' && value[i - 1] < 128) {
            return static_cast<char>(std::toupper(static_cast<int>(value[i - 1])));
        }
    }
    return 0;
}

std::string Utf8FromWide(const std::wstring& text) {
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(length > 0 ? length : 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), length, nullptr, nullptr);
    result.resize(std::strlen(result.c_str()));
    return result;
}
#endif

// Describes where the OS pages to, picks the directory for the test file and
// reports its free space. False when no usable directory was found.
bool PlanStorage(const CpuMemBwConfig& config, CpuMemBwStorage& storage,
                 std::uint64_t& freeBytes) {
    freeBytes = 0;
#if defined(_WIN32)
    const char pagingDrive = PageFileDriveLetter();
    storage.backing = pagingDrive ? std::string("page file on ") + pagingDrive + ":" : "no page file";
    std::string directory = config.storageDir;
    if (directory.empty()) {
        wchar_t temp[MAX_PATH + 2]{};
        GetTempPathW(MAX_PATH + 1, temp);
        directory = Utf8FromWide(temp);
    }
    if (directory.empty()) {
        storage.skippedReason = "no temporary directory";
        return false;
    }
    if (directory.back() != '\\' && directory.back() != '/') directory += '\\';
    storage.directory = directory;
    storage.onPagingDrive = pagingDrive != 0 && directory.size() >= 2 && directory[1] == ':' &&
                            std::toupper(static_cast<unsigned char>(directory[0])) == pagingDrive;
    const int length = MultiByteToWideChar(CP_UTF8, 0, directory.c_str(), -1, nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(length > 0 ? length : 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, directory.c_str(), -1, wide.data(), length);
    ULARGE_INTEGER available{};
    if (GetDiskFreeSpaceExW(wide.c_str(), &available, nullptr, nullptr))
        freeBytes = available.QuadPart;
    return true;
#else
    std::string directory = config.storageDir.empty() ? std::string(".") : config.storageDir;
    if (directory.back() != '/') directory += '/';
    storage.directory = directory;
    struct stat directoryStat {};
    const bool haveDirectory = stat(directory.c_str(), &directoryStat) == 0;
#if defined(__linux__)
    // /proc/swaps: a header line, then one line per swap area.
    std::ifstream swaps("/proc/swaps");
    std::string line;
    std::getline(swaps, line);
    std::string first;
    bool zram = false;
    while (std::getline(swaps, line)) {
        const std::string name = line.substr(0, line.find_first_of(" \t"));
        if (name.empty()) continue;
        if (first.empty()) first = name;
        zram = zram || name.find("zram") != std::string::npos;
        struct stat swapStat {};
        if (haveDirectory && stat(name.c_str(), &swapStat) == 0 && S_ISREG(swapStat.st_mode) &&
            swapStat.st_dev == directoryStat.st_dev) {
            storage.onPagingDrive = true;
        }
    }
    if (zram) storage.backing = "zram (memory is compressed in RAM; no drive is involved)";
    else if (!first.empty()) storage.backing = "swap on " + first;
    else {
        // An Android app may not read /proc/swaps, and every phone tested on
        // 2026-10-07 was reported as having no swap while it had 8 GiB of it.
        // The total in /proc/meminfo is open to everyone.
        std::uint64_t swapKiB = 0;
        std::ifstream meminfo("/proc/meminfo");
        while (std::getline(meminfo, line)) {
            if (line.rfind("SwapTotal:", 0) == 0) {
                swapKiB = std::strtoull(line.c_str() + 10, nullptr, 10);
                break;
            }
        }
        if (swapKiB == 0) {
            storage.backing = "no swap";
        } else {
            storage.backing = "swap of " + std::to_string(swapKiB / 1024) +
                              " MiB, held by something this process may not see";
#if defined(__ANDROID__)
            storage.backing += " (on Android normally zram: memory is compressed in RAM, no drive is involved)";
#endif
        }
    }
    struct statfs filesystem {};
    if (statfs(directory.c_str(), &filesystem) == 0 &&
        static_cast<unsigned long>(filesystem.f_type) == 0x01021994ul) {
        storage.skippedReason = "the directory is on tmpfs, which is memory and not a drive";
        return false;
    }
#elif defined(__APPLE__)
#if TARGET_OS_IPHONE
    storage.backing = "none (iOS compresses memory and does not page to storage)";
#else
    storage.backing = "swap files in /private/var/vm";
    struct stat swapStat {};
    storage.onPagingDrive = haveDirectory && stat("/private/var/vm", &swapStat) == 0 &&
                            swapStat.st_dev == directoryStat.st_dev;
#endif
#else
    storage.backing = "unknown";
#endif
    if (!haveDirectory) {
        storage.skippedReason = "the directory does not exist";
        return false;
    }
    struct statvfs volume {};
    if (statvfs(directory.c_str(), &volume) == 0) {
        freeBytes = static_cast<std::uint64_t>(volume.f_bavail) *
                    static_cast<std::uint64_t>(volume.f_frsize);
    }
    return true;
#endif
}

// Sequential 1 MiB and random 4 KiB read and write on a temporary file, one
// request at a time. Random 4 KiB requests are what paging issues.
CpuMemBwStorage MeasureStorage(const CpuMemBwConfig& config) {
    CpuMemBwStorage storage;
    std::uint64_t freeBytes = 0;
    if (!PlanStorage(config, storage, freeBytes)) return storage;

    // Leave three quarters of the free space alone.
    std::uint64_t target = config.storageFileBytes / kMiB * kMiB;
    if (freeBytes > 0) target = (std::min)(target, freeBytes / 4 / kMiB * kMiB);
    if (target < kStorageMinFileBytes) {
        storage.skippedReason = "not enough free space for the test file";
        return storage;
    }

    auto* block = static_cast<std::uint8_t*>(AllocatePages(kStorageBlockBytes));
    if (!block) {
        storage.skippedReason = "allocation failed";
        return storage;
    }
    FillBlock(block, kStorageBlockBytes, 0x5354u);

    const auto secondsSince = [](Clock::time_point start) {
        return std::chrono::duration<double>(Clock::now() - start).count();
    };
#if defined(_WIN32)
    const unsigned long processId = GetCurrentProcessId();
#else
    const unsigned long processId = static_cast<unsigned long>(getpid());
#endif
    TestFile file;
    if (!file.Open(storage.directory + "mangekyo-storage-" + std::to_string(processId) + ".tmp",
                   target, storage.directIo)) {
        storage.skippedReason = "the test file could not be created";
        FreePages(block, kStorageBlockBytes);
        return storage;
    }

    bool ok = true;
    std::uint64_t stamp = 0;
    // Every 4 KiB page gets a unique stamp, so no drive or file system can
    // deduplicate or compress what is written.
    const auto stampBlock = [&]() {
        for (std::size_t page = 0; page < kStorageBlockBytes; page += kStoragePageBytes) {
            ++stamp;
            std::memcpy(block + page, &stamp, sizeof(stamp));
        }
    };

    // Lay the file out with one sequential write. A slow drive is cut off
    // after kStoragePrepareSeconds and the tests use what was written. The
    // tests below then read and overwrite existing data, as paging does, and
    // do not include the cost of growing a file.
    std::uint64_t written = 0;
    auto start = Clock::now();
    while (ok && written < target) {
        stampBlock();
        ok = file.Write(written, block, kStorageBlockBytes);
        written += kStorageBlockBytes;
        if (written >= kStorageMinFileBytes && secondsSince(start) > kStoragePrepareSeconds) break;
    }
    file.Flush();
    storage.fileBytes = written;
    // Give the drive a moment to finish its own housekeeping after the fill.
    SleepFor(kStorageSettleSeconds);

    // Sequential 1 MiB requests in whole passes over the file; the fastest
    // pass counts, so a pass that another program disturbed does not. Reads
    // repeat until the time is up. Writes stop once two passes agree within
    // 10%, and after three passes at most, to limit wear.
    const auto sequential = [&](bool write) {
        double best = 0.0;
        const auto begin = Clock::now();
        for (std::uint32_t pass = 0; ok && pass < (write ? 3u : 8u); ++pass) {
            // Also for a file opened unbuffered: a file system may accept the
            // flag and buffer all the same. A TCL television (Android 11,
            // 2026-10-08) read 2222 MB/s from a drive that writes 14 MB/s,
            // every pass after the first out of the file cache.
            if (!write) file.DropCache();
            const auto passStart = Clock::now();
            for (std::uint64_t offset = 0; ok && offset < written; offset += kStorageBlockBytes) {
                if (write) stampBlock();
                ok = write ? file.Write(offset, block, kStorageBlockBytes)
                           : file.Read(offset, block, kStorageBlockBytes);
            }
            if (write) file.Flush();
            const double seconds = secondsSince(passStart);
            const double rate =
                ok && seconds > 0.0 ? static_cast<double>(written) / seconds / 1.0e6 : 0.0;
            const bool agrees = pass >= 1 && rate >= 0.9 * best && best >= 0.9 * rate;
            best = (std::max)(best, rate);
            if (write && agrees) break;
            if (pass >= 1 && secondsSince(begin) >= kStorageSequentialSeconds) break;
            if (secondsSince(begin) >= 2.0 * kStorageSequentialSeconds) break;
        }
        return best;
    };
    // Random 4 KiB requests, one at a time. Returns requests per second.
    const std::uint64_t pages = written / kStoragePageBytes;
    std::uint64_t random = 0x9E3779B97F4A7C15ull;
    const auto randomAccess = [&](bool write) {
        if (!write) file.DropCache();
        std::uint64_t operations = 0;
        const auto begin = Clock::now();
        do {
            random ^= random << 13;
            random ^= random >> 7;
            random ^= random << 17;
            const std::uint64_t offset = random % pages * kStoragePageBytes;
            if (write) {
                ++stamp;
                std::memcpy(block, &stamp, sizeof(stamp));
            }
            ok = write ? file.Write(offset, block, kStoragePageBytes)
                       : file.Read(offset, block, kStoragePageBytes);
            ++operations;
        } while (ok && secondsSince(begin) < kStorageRandomSeconds);
        if (write) file.Flush();
        const double seconds = secondsSince(begin);
        return ok && seconds > 0.0 ? static_cast<double>(operations) / seconds : 0.0;
    };

    if (ok && pages > 0) storage.seqReadMBps = sequential(false);
    if (ok && pages > 0) storage.seqWriteMBps = sequential(true);
    if (ok && pages > 0) storage.randReadIops = randomAccess(false);
    if (ok && pages > 0) storage.randWriteIops = randomAccess(true);
    storage.randReadMBps = storage.randReadIops * static_cast<double>(kStoragePageBytes) / 1.0e6;
    storage.randWriteMBps = storage.randWriteIops * static_cast<double>(kStoragePageBytes) / 1.0e6;

    file.Close();
    FreePages(block, kStorageBlockBytes);
    if (!ok) {
        storage.skippedReason = "a read or write on the test file failed";
        return storage;
    }
    storage.measured = true;
    return storage;
}

// ---------------------------------------------------------------------------
// Sweep
// ---------------------------------------------------------------------------

struct ModePlan {
    std::string name;  // single | single_<class rank> | all
    std::vector<CpuLogicalProcessor> cpus;
    std::uint64_t maxBlockBytes = 0;
    // Decides which data sets also get the streaming-store kernels.
    std::vector<CpuCacheLevel> caches;
    std::uint32_t pinned = 0;
};

// Rounds down to a size every kernel accepts and the sweep can end on.
std::uint64_t RoundBlock(std::uint64_t bytes) {
    if (bytes >= kMiB) return bytes / kMiB * kMiB;
    return (std::max)(kMinBlockBytes, bytes / kMinBlockBytes * kMinBlockBytes);
}

void PrintCaches(const std::vector<CpuCacheLevel>& caches, std::ostream& out) {
    if (caches.empty()) {
        out << "sizes not reported by the OS; levels are read off the curve";
        return;
    }
    bool hasL4 = false;
    for (std::size_t i = 0; i < caches.size(); ++i) {
        const auto& cache = caches[i];
        hasL4 = hasL4 || cache.level == 4;
        out << (i == 0 ? "" : " | ") << "L" << cache.level << (cache.level == 1 ? "d " : " ")
            << FormatBytes(cache.sizeBytes);
        if (cache.sharedByCores <= 1) out << " per core";
        else out << " shared by " << cache.sharedByCores << " cores";
    }
    if (!hasL4) out << " | L4 not present";
}

void RunSweep(const CpuMemBwConfig& config, std::ostream& out, CpuMemBwReport& report) {
    const auto runStart = Clock::now();
    report.warmupRuns = config.warmupRuns;
    report.measuredRuns = config.measuredRuns;
    report.sampleMs = config.sampleMs;
    if (config.measuredRuns == 0 || config.sampleMs <= 0.0 ||
        config.maxDatasetBytes < kMinBlockBytes) {
        throw std::runtime_error("invalid CPU memory-bandwidth configuration");
    }

    const CpuBenchmarkReport topology = ProbeCpuTopology();
    if (topology.processors.empty()) throw std::runtime_error("no logical processors available");
    report.cpuName = topology.cpuName;
    report.logicalProcessors = static_cast<std::uint32_t>(topology.processors.size());

    // One thread per physical core.
    std::vector<CpuLogicalProcessor> cores;
    for (const auto& cpu : topology.processors) {
        if (cpu.smtIndex == 0) cores.push_back(cpu);
    }
    if (cores.empty()) cores.push_back(topology.processors.front());
    report.physicalCores = static_cast<std::uint32_t>(cores.size());
    if (config.debugSplitClasses > 1) {
        for (std::size_t i = 0; i < cores.size(); ++i) {
            cores[i].performanceLevel =
                static_cast<int>(i * config.debugSplitClasses / cores.size());
            cores[i].coreClass = "DebugClass" + std::to_string(cores[i].performanceLevel);
        }
    }

    const KernelSet kernels = SelectKernels(config.vectorBytes);
    report.vectorName = kernels.name;
    report.vectorBytes = kernels.vectorBytes;
    report.streamingStores = kernels.writeStream != nullptr && kernels.copyStream != nullptr;
    report.zeroFill = kernels.zeroFill != nullptr;

    // Core classes, fastest first; cores of unknown class form the last one.
    // The reference core of a class is its second core where it has one,
    // which keeps the sweep off the core that usually services interrupts.
    std::map<int, std::vector<CpuLogicalProcessor>> byLevel;
    for (const auto& cpu : cores) {
        byLevel[cpu.performanceLevel < 0 ? (std::numeric_limits<int>::max)() : cpu.performanceLevel]
            .push_back(cpu);
    }
    std::vector<CpuLogicalProcessor> references;
    for (const auto& entry : byLevel) {
        const auto& members = entry.second;
        const CpuLogicalProcessor& reference = members.size() > 1 ? members[1] : members.front();
        CpuMemBwClass cpuClass;
        cpuClass.rank = static_cast<std::uint32_t>(report.classes.size());
        cpuClass.name = reference.coreClass;
        cpuClass.coreCount = static_cast<std::uint32_t>(members.size());
        cpuClass.referenceLogicalIndex = reference.logicalIndex;
        if (!config.levelsFromCurve) {
            cpuClass.caches = ProbeCaches(reference, topology.processors, cpuClass.cacheSource);
            if (config.debugHideLastCache && !cpuClass.caches.empty()) cpuClass.caches.pop_back();
        }
        if (cpuClass.caches.empty()) cpuClass.cacheSource = "curve";
        report.classes.push_back(std::move(cpuClass));
        references.push_back(reference);
    }
    report.mixedCoreTypes = report.classes.size() > 1;
    report.referenceLogicalIndex = report.classes.front().referenceLogicalIndex;
    report.caches = report.classes.front().caches;
    report.cacheSource = report.classes.front().cacheSource;

    const MemoryInfo memory = QueryMemory();
    report.physicalMemoryBytes = memory.totalBytes;
    report.availableMemoryBytes = memory.availableBytes;

    // Never plan to touch more than half of the free memory.
    const std::uint64_t budget =
        memory.availableBytes > 0 ? memory.availableBytes / 2 : ~0ull;
    const std::uint32_t allThreads = static_cast<std::uint32_t>(cores.size());
    const std::uint64_t singleMax = RoundBlock((std::min)(config.maxDatasetBytes, budget));
    report.datasetLimitedByMemory = singleMax < RoundBlock(config.maxDatasetBytes);

    std::vector<ModePlan> plans;
    for (std::size_t c = 0; c < report.classes.size(); ++c) {
        ModePlan plan;
        plan.name = c == 0 ? "single" : "single_" + std::to_string(c);
        plan.cpus = {references[c]};
        plan.maxBlockBytes = singleMax;
        plan.caches = report.classes[c].caches;
        plans.push_back(std::move(plan));
    }
    const bool runAll = allThreads > 1;
    if (runAll) {
        ModePlan plan;
        plan.name = "all";
        plan.cpus = cores;
        plan.maxBlockBytes = RoundBlock((std::min)(
            config.maxDatasetBytes / 4, (std::min)(budget, kAllCoreMaxTotalBytes) / allThreads));
        // With mixed core types no single set of cache sizes fits all threads.
        if (!report.mixedCoreTypes) plan.caches = report.classes.front().caches;
        plans.push_back(std::move(plan));
    }

    out << "\n========== CPU Memory Bandwidth Sweep (" << kCpuMemBwWorkloadVersion
        << ") ==========\n"
        << "CPU:       " << report.cpuName << " (" << report.physicalCores << " cores / "
        << report.logicalProcessors << " threads)\n"
        << "Vectors:   " << kernels.name << ", " << kernels.vectorBytes
        << "-byte loads and stores\n";
    if (!report.mixedCoreTypes) {
        out << "Caches:    ";
        PrintCaches(report.caches, out);
        out << "\n";
    } else {
        for (const auto& cpuClass : report.classes) {
            out << "Class " << cpuClass.rank << ":   " << cpuClass.name << ", " << cpuClass.coreCount
                << (cpuClass.coreCount == 1 ? " core" : " cores") << ", reference logical processor "
                << cpuClass.referenceLogicalIndex << "\n           ";
            PrintCaches(cpuClass.caches, out);
            out << "\n";
        }
    }
    if (memory.totalBytes > 0) {
        out << "Memory:    " << std::fixed << std::setprecision(1)
            << static_cast<double>(memory.totalBytes) / static_cast<double>(kGiB)
            << " GiB installed";
        if (memory.availableBytes > 0) {
            out << ", " << static_cast<double>(memory.availableBytes) / static_cast<double>(kGiB)
                << " GiB free";
        }
        out << "\n";
    }
    out << "Sweep:     " << FormatBytes(kMinBlockBytes) << " - " << FormatBytes(singleMax)
        << " on one thread";
    if (report.mixedCoreTypes) out << " of each core class";
    if (runAll) {
        out << "; " << FormatBytes(kMinBlockBytes) << " - "
            << FormatBytes(plans.back().maxBlockBytes) << " per thread on " << allThreads
            << " threads";
    }
    out << (report.datasetLimitedByMemory ? " (ceiling lowered to fit free memory)" : "") << "\n"
        << "Timing:    wall clock, " << config.warmupRuns << " warm-up + " << config.measuredRuns
        << " measured samples of " << std::setprecision(0) << config.sampleMs
        << " ms per point, median reported\n"
        << "CPUMEM_META\tname=" << SanitizeField(report.cpuName)
        << "\tvector=" << kernels.name << "\tvector_bytes=" << kernels.vectorBytes
        << "\tphysical_count=" << report.physicalCores
        << "\tlogical_count=" << report.logicalProcessors
        << "\tall_threads=" << allThreads
        << "\treference_logical_index=" << report.referenceLogicalIndex
        << "\tclass_count=" << report.classes.size()
        << "\tcache_source=" << report.cacheSource << "\n";
    for (const auto& cpuClass : report.classes) {
        out << "CPUMEM_CLASS\trank=" << cpuClass.rank
            << "\tmode=" << (cpuClass.rank == 0 ? std::string("single")
                                                : "single_" + std::to_string(cpuClass.rank))
            << "\tname=" << SanitizeField(cpuClass.name) << "\tcores=" << cpuClass.coreCount
            << "\treference_logical_index=" << cpuClass.referenceLogicalIndex
            << "\tcache_source=" << cpuClass.cacheSource << "\n";
        for (const auto& cache : cpuClass.caches) {
            out << "CPUMEM_CACHE\tclass=" << cpuClass.rank << "\tlevel=" << cache.level
                << "\tsize_bytes=" << cache.sizeBytes
                << "\tshared_cores=" << cache.sharedByCores
                << "\tshared_threads=" << cache.sharedByThreads
                << "\tline_bytes=" << cache.lineBytes << "\n";
        }
    }
    out << std::flush;

    // One step per data set, plus the page-mapping and storage tests.
    std::size_t totalSteps = 1 + (config.storageTest ? 1 : 0);
    for (const auto& plan : plans) totalSteps += SweepBlocks(plan.maxBlockBytes).size();
    std::size_t doneSteps = 0;
    const auto progress = [&](const std::string& mode) {
        ++doneSteps;
        out << "CPUMEM_PROGRESS\tmode=" << mode << std::fixed << std::setprecision(4)
            << "\tfraction=" << static_cast<double>(doneSteps) / static_cast<double>(totalSteps)
            << "\n" << std::flush;
    };
    const double sampleSeconds = config.sampleMs / 1000.0;

    for (std::size_t p = 0; p < plans.size(); ++p) {
        ModePlan& plan = plans[p];
        const std::uint32_t threads = static_cast<std::uint32_t>(plan.cpus.size());
        const std::uint64_t memoryFrom = MemoryBandStart(plan.caches, threads);
        out << "\n[" << plan.name << "] ";
        if (threads == 1) {
            out << "one thread on logical processor " << plan.cpus.front().logicalIndex;
            if (report.mixedCoreTypes) {
                out << " (class " << p << ", " << report.classes[p].name << ")";
            }
        } else {
            out << threads << " threads, one per physical core, each with a private data set";
        }
        out << "\n    Data set       Read      Write       Copy   Write NT    Copy NT  Zero fill"
               "   GB/s (median)\n";

        for (const std::uint64_t block : SweepBlocks(plan.maxBlockBytes)) {
            BlockRunner runner(plan.cpus, static_cast<std::size_t>(block), kernels);
            if (!runner.allocated()) {
                out << std::setw(12) << FormatBytes(block)
                    << "   allocation failed; the sweep ends here\n";
                break;
            }
            plan.pinned = runner.pinned();

            std::vector<Kernel> order = {Kernel::Read, Kernel::Write, Kernel::Copy};
            if (report.streamingStores && block >= memoryFrom) {
                order.push_back(Kernel::WriteStream);
                order.push_back(Kernel::CopyStream);
            }
            // Last, because it leaves the block zeroed.
            if (report.zeroFill && block >= memoryFrom) order.push_back(Kernel::ZeroFill);
            double medians[kKernelCount] = {-1.0, -1.0, -1.0, -1.0, -1.0, -1.0};
            for (const Kernel kernel : order) {
                for (std::uint32_t run = 0; run < config.warmupRuns; ++run)
                    runner.Sample(kernel, sampleSeconds);
                // Keep the least disturbed round.
                double median = 0.0;
                double best = 0.0;
                for (std::uint32_t attempt = 0;; ++attempt) {
                    std::vector<double> rates;
                    for (std::uint32_t run = 0; run < config.measuredRuns; ++run)
                        rates.push_back(runner.Sample(kernel, sampleSeconds));
                    median = (std::max)(median, Median(rates));
                    best = (std::max)(best, *std::max_element(rates.begin(), rates.end()));
                    if (median >= kDisturbedRatio * best || attempt == kMaxRemeasure) break;
                    if (attempt == 0) ++report.remeasuredPoints;
                }

                CpuMemBwPoint point;
                point.mode = plan.name;
                point.kernel = KernelName(kernel);
                point.blockBytes = block;
                point.threads = threads;
                point.medianGBps = median / 1.0e9;
                point.bestGBps = best / 1.0e9;
                medians[static_cast<std::size_t>(kernel)] = point.medianGBps;
                report.points.push_back(point);
            }

            out << std::setw(12) << FormatBytes(block) << std::fixed << std::setprecision(1);
            for (const double value : medians) {
                if (value < 0.0) out << std::setw(11) << "-";
                else out << std::setw(11) << value;
            }
            out << "\n";
            for (std::size_t i = report.points.size() - order.size(); i < report.points.size(); ++i) {
                const auto& point = report.points[i];
                out << "CPUMEM_POINT\tmode=" << point.mode << "\tkernel=" << point.kernel
                    << "\tblock_bytes=" << point.blockBytes << "\tthreads=" << point.threads
                    << std::setprecision(3)
                    << "\tmedian_gbps=" << point.medianGBps << "\tbest_gbps=" << point.bestGBps
                    << "\n";
            }
            progress(plan.name);
        }
    }

    const CpuLogicalProcessor& reference = references.front();
    const std::uint64_t pageRegion = RoundBlock((std::min)(kPageFaultRegionBytes, budget / 4));
    report.pageFault = MeasurePageFaults(reference, pageRegion);
    progress("pagefault");
    if (config.storageTest) {
        report.storage = MeasureStorage(config);
        progress("storage");
    } else {
        report.storage.skippedReason = "switched off";
    }

    // Levels of every core class, then of all cores together.
    std::uint32_t wantedPins = 0;
    std::uint32_t pins = 0;
    for (std::size_t c = 0; c < report.classes.size(); ++c) {
        CpuMemBwClass& cpuClass = report.classes[c];
        cpuClass.summary = cpuClass.caches.empty()
            ? SummariseFromCurve(report.points, plans[c].name, 1)
            : SummariseMode(report.points, plans[c].name, 1, cpuClass.caches);
        cpuClass.summary.pinnedThreads = plans[c].pinned;
        wantedPins += 1;
        pins += plans[c].pinned;
    }
    report.single = report.classes.front().summary;
    if (runAll) {
        const ModePlan& plan = plans.back();
        report.all = plan.caches.empty()
            ? SummariseFromCurve(report.points, "all", allThreads)
            : SummariseMode(report.points, "all", allThreads, plan.caches);
        if (report.mixedCoreTypes) {
            // Only the memory figure applies to all threads alike.
            report.all.levels.erase(
                std::remove_if(report.all.levels.begin(), report.all.levels.end(),
                               [](const CpuMemBwLevelSummary& level) {
                                   return level.name != "memory";
                               }),
                report.all.levels.end());
        }
        report.all.pinnedThreads = plan.pinned;
        wantedPins += allThreads;
        pins += plan.pinned;
    }
    if (!PlatformCanPin()) report.affinity = "scheduler_managed";
    else report.affinity = pins == wantedPins ? "strict" : pins == 0 ? "failed" : "partial";

    for (const auto& cpuClass : report.classes) {
        std::string title = "one thread (logical processor " +
                            std::to_string(cpuClass.referenceLogicalIndex) + ")";
        if (report.mixedCoreTypes) {
            title = "class " + std::to_string(cpuClass.rank) + ", " + cpuClass.name + ", " + title;
        }
        PrintSummary(cpuClass.summary, title, out);
    }
    if (runAll) {
        PrintSummary(report.all,
                     "all cores (" + std::to_string(allThreads) +
                         " threads, data set sizes are per thread)",
                     out);
    }

    out << "\n--- Virtual memory ---\n";
    if (report.pageFault.measured) {
        out << std::fixed << std::setprecision(2)
            << "Mapping new pages: " << report.pageFault.gbps << " GB/s, "
            << std::setprecision(0) << report.pageFault.nsPerPage << " ns per "
            << FormatBytes(report.pageFault.pageBytes) << " page (first write to every page of a "
            << FormatBytes(report.pageFault.regionBytes) << " region, one thread)\n";
        if (report.pageFault.transparentHugePages) {
            out << "Transparent huge pages are set to 'always', so the kernel may map 2 MiB at"
                   " a time.\n";
        }
    } else {
        out << "Mapping new pages: not measured (allocation failed)\n";
    }
    out << "CPUMEM_PAGEFAULT\tmeasured=" << (report.pageFault.measured ? 1 : 0)
        << "\tpage_bytes=" << report.pageFault.pageBytes
        << "\tregion_bytes=" << report.pageFault.regionBytes
        << std::fixed << std::setprecision(3)
        << "\tgbps=" << report.pageFault.gbps << "\tns_per_page=" << report.pageFault.nsPerPage
        << "\ttransparent_huge_pages=" << (report.pageFault.transparentHugePages ? 1 : 0) << "\n";

    const CpuMemBwStorage& storage = report.storage;
    out << "\n--- Storage behind virtual memory ---\n";
    if (!storage.backing.empty()) out << "Paging target: " << storage.backing << "\n";
    if (storage.measured) {
        out << "Test file:     " << FormatBytes(storage.fileBytes) << " in " << storage.directory
            << (storage.onPagingDrive ? " (the drive that is paged to)" : "") << ", "
            << (storage.directIo ? "unbuffered I/O"
                                 : "buffered I/O: the OS file cache may inflate the figures")
            << "\n" << std::fixed << std::setprecision(0)
            << "Sequential 1 MiB:   read " << storage.seqReadMBps << " MB/s, write "
            << storage.seqWriteMBps << " MB/s\n"
            << std::setprecision(1)
            << "Random 4 KiB, QD1:  read " << storage.randReadMBps << " MB/s ("
            << std::setprecision(0) << storage.randReadIops << " IOPS), write "
            << std::setprecision(1) << storage.randWriteMBps << " MB/s ("
            << std::setprecision(0) << storage.randWriteIops << " IOPS)\n"
            << "Paging issues random 4 KiB requests, so that line is the closer estimate of\n"
            << "how fast memory comes back from the drive.\n";
    } else {
        out << "Not measured: " << storage.skippedReason << "\n";
    }
    out << "CPUMEM_STORAGE\tmeasured=" << (storage.measured ? 1 : 0)
        << "\tbacking=" << SanitizeField(storage.backing)
        << "\tdirectory=" << SanitizeField(storage.directory)
        << "\ton_paging_drive=" << (storage.onPagingDrive ? 1 : 0)
        << "\tdirect_io=" << (storage.directIo ? 1 : 0)
        << "\tfile_bytes=" << storage.fileBytes << std::fixed << std::setprecision(1)
        << "\tseq_read_mbps=" << storage.seqReadMBps << "\tseq_write_mbps=" << storage.seqWriteMBps
        << "\trand_read_mbps=" << storage.randReadMBps
        << "\trand_write_mbps=" << storage.randWriteMBps
        << "\trand_read_iops=" << storage.randReadIops
        << "\trand_write_iops=" << storage.randWriteIops
        << "\tskipped=" << SanitizeField(storage.skippedReason) << "\n";

    out << "\nNote: write and copy use ordinary stores. Outside the caches every stored line is\n"
        << "      first read from memory, so the memory traffic is 2x the write figure and 1.5x\n"
        << "      the copy figure. Streaming stores skip that read.\n";
    {
        // Streaming stores that gain nothing over ordinary ones are held back
        // by the CPU; the zero fill shows what the memory itself accepts.
        const CpuMemBwModeSummary& widest = runAll ? report.all : report.single;
        for (const auto& level : widest.levels) {
            if (level.name != "memory" || level.writeStreamGBps <= 0.0) continue;
            if (level.writeStreamGBps >= 1.5 * level.writeGBps) continue;
            out << std::fixed << std::setprecision(1)
                << "Note: streaming stores (" << level.writeStreamGBps
                << " GB/s) are not much faster than ordinary stores (" << level.writeGBps
                << " GB/s) on this CPU.\n";
            if (level.zeroFillGBps > level.writeStreamGBps) {
                out << "      CLZERO zero fill reaches " << level.zeroFillGBps
                    << " GB/s, so the memory accepts writes faster than streaming stores\n"
                    << "      deliver them.\n";
            }
        }
    }
    out << "Note: cache-level figures are limited by how fast the core issues " << kernels.vectorBytes
        << "-byte loads and\n      stores, so they depend on the vector size and the clock.\n";
    if (report.mixedCoreTypes) {
        out << "Note: this CPU mixes core types. Every class has its own single-thread sweep;\n"
            << "      the all-core sweep reports memory only, because the classes leave their\n"
            << "      caches at different data sets.\n";
    }
    if (report.affinity != "strict") {
        out << "Note: thread affinity is '" << report.affinity
            << "'; threads may have moved between cores.\n";
    }
    if (report.remeasuredPoints > 0) {
        out << "Note: " << report.remeasuredPoints << " of " << report.points.size()
            << " points were measured again because other activity on the machine\n"
            << "      disturbed their first round.\n";
    }
    report.durationSec = std::chrono::duration<double>(Clock::now() - runStart).count();
    out << "CPUMEM_DONE\taffinity=" << report.affinity
        << "\tremeasured_points=" << report.remeasuredPoints << std::setprecision(1)
        << "\tduration_sec=" << report.durationSec << "\n"
        << "Duration:  " << report.durationSec << " s\n"
        << "==========================================================\n" << std::flush;

    for (const auto& level : report.single.levels) {
        if (level.name == "memory" && level.measured) report.valid = true;
    }
}

}  // namespace

CpuMemBwModeSummary RereadCpuMemBwLevels(const std::vector<CpuMemBwPoint>& points,
                                         const std::string& mode, std::uint32_t threads,
                                         bool systemCacheAfterL3, const std::string& title,
                                         std::ostream& out) {
    CpuMemBwModeSummary summary = SummariseFromCurve(points, mode, threads, systemCacheAfterL3);
    PrintSummary(summary, title, out, /*protocol=*/false);
    return summary;
}

CpuMemBwReport RunCpuMemoryBandwidth(const CpuMemBwConfig& config, std::ostream& out) {
    CpuMemBwReport report;
    try {
        RunSweep(config, out, report);
    } catch (const std::exception& e) {
        report.valid = false;
        report.error = e.what();
    }
    return report;
}

}  // namespace gpu_bench
