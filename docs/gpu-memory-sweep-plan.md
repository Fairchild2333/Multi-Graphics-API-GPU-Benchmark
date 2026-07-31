# GPU memory-hierarchy sweep — plan

A new test that measures where a GPU's bandwidth tiers actually are: the on-chip
caches, the device-local pool, and host-visible memory.

**Derived from the existing particle (`stream`) workload — same data layout,
same update math, same memory traffic pattern.** The existing test itself is not
modified: its kernel, its score formula, and its stored results are untouched.

Related: [`interpreting-stream-bandwidth.md`](interpreting-stream-bandwidth.md)
explains why the particle *score* cannot answer this question.

## 1. Can it be built on the particle test?

Yes — but not by simply sweeping `--particles`, and the reason is worth stating
up front because it dictates the whole design.

The existing kernel launches **one thread per particle**. Working-set size and
parallelism are therefore welded together: shrinking the buffer to reach the
caches also starves the GPU. Measured on an M4 Pro, headless:

| particles | threads | buffer | reported |
|---|---|---|---|
| 4,096 | 4,096 | 128 KB | **62.9 GB/s** |
| 16,384 | 16,384 | 512 KB | 167.2 GB/s |
| 65,536 | 65,536 | 2 MB | 319.4 GB/s |
| 262,144 | 262,144 | 8 MB | 321.2 GB/s |
| 1,048,576 | 1,048,576 | 32 MB | 157.5 GB/s |

The left half of that curve is **inverted**. A 128 KB working set sits in the
innermost cache and should be the fastest point in the sweep; it is the slowest.
That number is occupancy collapse, not cache latency or bandwidth.

So a pure `--particles` sweep can only see from roughly 2 MB upward: the memory
plateau and the top of the cache plateau, with nothing below. To reach the inner
tiers, thread count must be decoupled from working-set size.

That decoupling is the entire delta of this plan.

## 2. What is reused verbatim

Everything that defines *what is being measured* comes from the particle test:

- **The data layout.** `Particle { float4 position; float4 velocity; }`, 32
  bytes, from `gpu_common.h`. Unchanged.
- **The update math.** Position advanced by velocity, wrapped at a bound —
  byte-for-byte the body of the shipped kernel.
- **The traffic pattern.** Reads both halves (32 B), writes the position back
  (16 B): the same ~48 B/particle read-modify-write the real workload performs,
  not an idealised streaming read.
- **The buffer allocation path and storage-mode choices.**

This matters: a from-scratch read-only STREAM kernel would measure a different
thing and its tier boundaries would not necessarily be the ones the actual
workload experiences. Keeping the access pattern identical means the plateaus
found here are the plateaus the particle workload is subject to.

## 2b. Prototype results — the method works

Validated with a standalone Metal prototype before touching the repo build.
M4 Pro, 262,144 threads, 2 GiB of traffic per point, best of 5 dispatches,
device-local storage. Both access modes use the same `Particle` struct and the
same index walk; only the kernel body differs.

| working set | particles | read-modify-write GB/s | read-only GB/s |
|---|---|---|---|
| 64 KiB | 2,048 | 1243 † | 2817 |
| 128 KiB | 4,096 | 3904 | 2836 |
| 256 KiB | 8,192 | 3856 | 3243 |
| 1 MiB | 32,768 | 3803 | 3226 |
| 8 MiB | 262,144 | 3695 | 3171 |
| 16 MiB | 524,288 | 4723 † | 3214 |
| 32 MiB | 1,048,576 | 2964 | 1590 |
| 64 MiB | 2,097,152 | 1148 | 579 |
| 128 MiB | 4,194,304 | 409 | 353 |
| 256 MiB | 8,388,608 | 220 | 257 |
| 512 MiB | 16,777,216 | **175.7** | **251.2** |

What this establishes:

1. **Memory bandwidth is measured correctly.** The RMW figure at 512 MiB
   (175.7) matches an independent copy probe (177.8) and, converted to
   stream-score units, matches the shipped test: 147.4 vs 147.8 GB/s measured at
   16M particles. Three independent paths agree.
2. **The inversion is gone.** Small working sets are now far faster than large
   ones, which is the whole point of decoupling the grid from the buffer.
3. **A cache plateau is clearly resolved** at roughly 3200–3800 GB/s, about 18–20x
   the memory rate, holding from 128 KiB to 16 MiB.
4. **The capacity knee sits between 16 and 32 MiB**, where the rate first falls
   away sharply. That is the last-level cache boundary, inferred from the curve.
5. **Unified memory is confirmed**: device-local and host-visible converge to the
   same ~176 GB/s at the large end (full table in the prototype run), so there is
   no separate VRAM pool on this part — exactly what should be reported rather
   than a fabricated split.
6. **Read-only reaches 251 GB/s, 92% of the 273 GB/s rating.** This corrects an
   earlier claim in `interpreting-stream-bandwidth.md` that ~180 GB/s was the
   hardware ceiling. It is not; read-modify-write is what costs the third.

† Two artifacts remain, both understood:

- **64 KiB RMW (1243)** is write contention, not cache speed: 2,048 particles
  shared by 262,144 threads means ~128 threads writing each address. Inherent to
  reusing a kernel that writes. The read-only column has no such dip.
- **16 MiB RMW (4723)** is an isolated spike above its neighbours; likely a
  write-combining effect at the cache boundary. Needs a repeat run with more
  samples before the tier detector is allowed to treat it as a plateau.

### Why both access modes ship

The comparison is itself the useful result, and each mode answers a different
question:

- **Read-modify-write** is faithful to the particle workload. Its tiers are the
  tiers that workload actually experiences, and its memory figure cross-checks
  against the shipped score.
- **Read-only** removes write contention, so it resolves the small end cleanly
  and gives the honest peak the memory system can sustain.

Reporting only one would either misstate the cache region or misstate the peak.

## 3. What changes — the minimal delta

Only the launch geometry. Two parameters are added to the dispatch:

```metal
// Same body as the shipped kernel; only the loop around it is new.
kernel void particleSweep(device Particle* p, constant SweepParams& sp,
                          uint tid [[thread_position_in_grid]]) {
    for (uint r = 0u; r < sp.repeats; ++r) {
        for (uint i = tid; i < sp.count; i += sp.gridSize) {
            p[i].position.xyz += p[i].velocity.xyz * sp.deltaTime;
            if (p[i].position.x > sp.bounds) p[i].position.x = -sp.bounds;
        }
    }
}
```

- **`gridSize` is fixed and large** (start at 262,144 threads), independent of
  `count`. The GPU stays saturated whether the working set is 64 KB or 512 MB.
- **`repeats` re-walks the set** so every point moves the same total bytes
  regardless of size, keeping dispatch durations comparable and holding small
  sets resident in cache.
- The grid-stride step keeps neighbouring threads on neighbouring addresses, so
  access stays coalesced exactly as in the original kernel.

When `count >= gridSize` and `repeats == 1`, this degenerates to precisely the
shipped kernel's behaviour — the sweep's large-set end should reproduce the
existing `stream` numbers, which is validation check 1 below.

## 4. Byte accounting

Traffic is `count x repeats x 48 bytes` — 32 read, 16 written back.

The sweep reports the accurate 48 B figure, **and** the same value scaled by
40/48 labelled as *stream-score-equivalent*, so a point can be checked directly
against an existing `stream` run at the same particle count. This gives an
honest number without breaking the cross-check.

The existing score's 40 B constant is deliberately left alone; see
`interpreting-stream-bandwidth.md` for why.

Note that a constant byte-factor cancels out of tier *boundaries* entirely — it
scales the whole curve uniformly. Only absolute GB/s is affected.

## 5. Storage modes

| mode | Metal | meaning |
|---|---|---|
| device-local | `MTLResourceStorageModePrivate` | VRAM on a discrete GPU |
| host-visible | `MTLResourceStorageModeShared` | system RAM; PCIe-reached on a discrete GPU |

Reuses the allocation choice the particle backend already makes. On
unified-memory parts the two curves should coincide; the probe says so
explicitly when the ratio lands within ±10%, rather than implying a VRAM/RAM
split the hardware does not have. On a discrete GPU the host-visible curve
should sit far lower, and that ratio is the interesting result.

## 5b. Discrete GPUs and other platforms

The Apple part validated above has unified memory, so device-local and
host-visible collapse into one tier. A discrete GPU is the more interesting
case, and the more demanding one.

### What a discrete GPU should show

Three clearly separated plateaus instead of two:

| tier | expected | source |
|---|---|---|
| on-chip cache | multiple TB/s | L1 / L2 / Infinity Cache |
| device-local | 500–1000 GB/s | GDDR6/6X or HBM |
| host-visible | 25–60 GB/s | system RAM **across PCIe** |

**The host-visible number is the link, not the host's DRAM.** A GPU reading a
CPU-visible allocation is bounded by PCIe (~25–28 GB/s effective on 4.0 x16,
~55 GB/s on 5.0 x16), while the host's own DDR5 delivers 90+ GB/s. So this
measures *system memory as the GPU sees it* — which is exactly the wall a game
hits when VRAM overflows and resources are demoted — and must be labelled that
way, never as "system RAM bandwidth". Resizable BAR changes addressing, not the
link rate.

### The default max working set is too small for a discrete GPU

This is the one parameter that must change before the sweep is trusted on a
dGPU. Last-level caches are now enormous:

| part | LLC |
|---|---|
| RX 6800 XT | 128 MB Infinity Cache |
| RX 7900 XTX | 96 MB Infinity Cache |
| RTX 4090 | 72 MB L2 |

At the current 512 MiB ceiling, a 128 MB cache still covers a quarter of the
working set, so the "device-local plateau" would carry a substantial cache
component and read high. The maximum must rise to **2–4 GiB**, capped by
available device memory, giving a 16–32x margin over the largest LLC. Query the
device's memory budget and clamp; do not assume the allocation succeeds.

The host-visible pass should also get a smaller traffic target than the
device-local pass — at ~25 GB/s, 2 GiB per point costs 80 ms of pure transfer,
and a dozen sizes times five repeats becomes slow. Budget by time rather than by
bytes for that pass.

### Backend readiness

**All four backends already have GPU timestamps.** An earlier revision of this
plan claimed D3D11 and OpenGL had none; that was a bad grep (D3D11 reads results
through `GetData`, OpenGL uses `glQueryCounter`, neither of which matched the
strings searched). Timing is not the discriminator.

| backend | timestamp mechanism |
|---|---|
| Vulkan | `vkCmdWriteTimestamp` + `timestampPeriod` |
| D3D12 | `D3D12_QUERY_HEAP_TYPE_TIMESTAMP` + `GetTimestampFrequency` |
| D3D11 | `D3D11_QUERY_TIMESTAMP` + `D3D11_QUERY_TIMESTAMP_DISJOINT` |
| OpenGL | `glQueryCounter(GL_TIMESTAMP)` |

The real discriminators are host-visible allocation and maximum buffer size.

| backend | device-local sweep | host-visible sweep | max working set |
|---|---|---|---|
| Vulkan | ready | **ready** — `HOST_VISIBLE \| HOST_COHERENT` already wired to `--host-memory` | device memory |
| D3D12 | ready | read-only via UPLOAD heap; RMW needs a custom `MEMORY_POOL_L0` heap | device memory |
| D3D11 | ready | read-only only — UAVs require `USAGE_DEFAULT`, so an RMW buffer cannot live in system memory | capped at 2 GiB by the D3D11 resource-size rule |
| OpenGL | ready | flag exists but placement is unverified (see below) | `GL_MAX_SHADER_STORAGE_BLOCK_SIZE`, as low as 128 MB on some drivers |

So **every backend can measure the cache tiers and device-local memory** — the
core of the test. They diverge only on the host-visible tier, and on how large a
working set they can allocate.

#### The fixed grid sidesteps D3D11's dispatch limit

`dx11_backend.cpp` throws when a workload exceeds
`D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION` (65,535 groups), which the
one-thread-per-particle kernel hits at large counts. The sweep dispatches a
fixed 262,144 threads — 1,024 groups — no matter how large the working set is,
so it never approaches that limit. The decoupling introduced for occupancy turns
out to solve D3D11's dispatch ceiling for free.

#### Two allocation caveats worth verifying before trusting a number

- **OpenGL's `--host-memory` may not actually be in system memory.** It calls
  `glBufferStorage` with `GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT` but
  *without* `GL_CLIENT_STORAGE_BIT`, which is the hint that asks the driver to
  keep the allocation client-side. Placement is therefore the driver's choice
  and the resulting number may be device-local anyway. Confirm with a known-good
  PCIe reference before reporting an OpenGL host-visible figure.
- **UAVs cannot live in system memory on D3D11, and not on a D3D12 UPLOAD heap
  either.** Both APIs restrict read-write views to device-local (D3D11
  `USAGE_DEFAULT`) or read-only upload heaps. The read-only sweep variant works
  on both; the read-modify-write variant needs D3D12's custom heap with
  `D3D12_MEMORY_POOL_L0` + `WRITE_BACK`, and has no D3D11 equivalent.

This makes the read-only variant more than a cache-cleanliness measure: it is
the only host-visible mode available on D3D11 and on plain D3D12 upload heaps.

#### Size limits are the sharpest constraint

Section 5b argues the maximum working set must reach 2–4 GiB on a discrete GPU
to clear a 96–128 MB last-level cache. Two backends cannot necessarily get there:

- **D3D11** caps a single resource at 2 GiB, which is the bare minimum for a
  16x margin over a 128 MB Infinity Cache — workable, with no headroom.
- **OpenGL** must be queried at runtime: `GL_MAX_SHADER_STORAGE_BLOCK_SIZE` is
  as low as 128 MB on some drivers. At that size the working set never leaves
  the LLC on a modern AMD part, and the "memory" plateau would be pure cache.
  The sweep must query this limit, clamp to it, and **refuse to report a memory
  figure** when the achievable maximum is not several times the reported cache
  knee.

That refusal is the important behaviour: it is better to report "cannot reach
memory on this backend" than a cache number labelled as bandwidth.

**Vulkan remains the port target** — the only backend with everything already in
place, and the only one covering Windows and Linux at once. D3D12 second, for
Windows coverage where Vulkan is unavailable.

### Reusing the existing `--host-memory` switch

The two allocations the sweep needs are already implemented, by the existing
`--host-memory` flag (the "System memory" checkbox):

```cpp
// src/vulkan_backend.cpp — config_.hostMemory picks between exactly these
createBuffer(size, gpuUsage,
             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, …);
createBuffer(size, gpuUsage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, …);   // + staging upload
```

So the Vulkan port reuses that helper with the two property masks rather than
writing new allocation code. Three caveats:

- **The flag is a per-run boolean, not a sweep axis.** Today one mode is chosen
  for the whole run. The sweep must measure both *within a single invocation*,
  or the two curves are taken at different clock and thermal states and their
  ratio means nothing. That is a change to the driver loop, not to allocation.
- **Metal ignores the flag entirely** — `hostMemory` appears zero times in
  `metal_backend.mm`; the backend always uses shared storage and labels it
  `Unified-memory`. The Private/Shared comparison in the prototype is therefore
  capability the flag never provided on Apple parts.
- **D3D12 and D3D11 treat it as a no-op with a warning**, falling back to
  device-local, so the flag does not shorten the D3D12 work described above.

### Extra validation required on a discrete GPU

Beyond the checks in section 9:

7. **Device-local plateau must be flat across at least a 4x size range** above
   the cache knee. If it still slopes, the maximum working set is too small
   relative to the LLC.
8. **Host-visible plateau should land near the theoretical PCIe rate.** Landing
   far below suggests the allocation is not being read coherently, or the access
   is not coalescing across the link.
9. **Grid size must be re-tuned.** 262,144 threads suits an M4 Pro; a 4090 keeps
   ~196,000 threads resident, and the largest parts more. Check 5 (plateaus must
   not move when threads double) is the gate.

## 6. Structure — a side path, not a Workload

`--cpu-benchmark` is the precedent: a separate path in `main.cpp` that returns
before GLFW, GPU probing, shader discovery, or a window exist, emitting its own
records instead of a `BenchmarkResult`.

The sweep follows that shape so it cannot disturb the existing test:

- No new `Workload` enum value, so no backend has to handle or reject one, and
  the scoring/difficulty/results machinery is untouched.
- No frames, render pass, FPS, or difficulty label — almost every
  `BenchmarkResult` field would be meaningless.
- Nothing written to `results.json`, so History, Charts, and the comparability
  of stored scores are unaffected.

## 7. Output contract

Human-readable table plus TAB-separated records in the `CPU_*` house style,
every record flushed so a GUI can stream them.

```
MEMSWEEP_META    device=… grid_threads=… bytes_per_particle=48 repeats=…
MEMSWEEP_POINT   storage=device_local particles=… bytes=… gbps=… stream_equiv_gbps=… ms=…
MEMSWEEP_TIER    from_bytes=… to_bytes=… gbps=…
MEMSWEEP_RESULT  peak_cache_gbps=… peak_cache_bytes=… device_local_gbps=… host_visible_gbps=… tiers=…
MEMSWEEP_ERROR   message=…
```

Deliberately avoids the four substrings the macOS GUI treats as score lines
(`Avg FPS:`, `Score:`, `VRAM rate:`, `RAM rate:`), so streaming this through the
existing log path cannot corrupt the summary field.

Tier detection: walk the points, group adjacent sizes whose rate stays within
12% of the running group mean, report groups of two or more as a plateau. Raw
points are always emitted so the curve can be re-read by hand.

## 8. Integration points

| # | change | file | status |
|---|---|---|---|
| 1 | probe API | `src/gpu_memory_sweep.h` | written, needs rework to the particle-derived kernel |
| 2 | Metal implementation + non-Metal stub | `src/gpu_memory_sweep.mm` | written, needs rework |
| 3 | add source, `-fobjc-arc` | `CMakeLists.txt` | todo |
| 4 | `--memory-sweep` flag + early dispatch | `src/main.cpp` | todo |
| 5 | `--help` entry | `src/main.cpp` | todo |
| 6 | GUI page (macOS) | `macos-gui/…` | todo, optional |
| 7 | measured reference curve | `docs/` | todo |

The two source files currently hold a from-scratch read-only kernel, written
before the "derive it from the particle test" requirement. They will be reworked
to the shared `Particle` struct and the update body above.

Flag surface, mirroring `--cpu-benchmark`:

```
--memory-sweep                 Run the GPU memory-hierarchy sweep and exit
--memory-sweep-min-kib N       Smallest working set (default 64)
--memory-sweep-max-kib N       Largest working set (default 524288 = 512 MiB on
                               unified memory; must be raised to 2-4 GiB on a
                               discrete GPU, see 5b)
--memory-sweep-traffic-mib N   Bytes moved per point (default 2048)
--memory-sweep-repeats N       Dispatches per point, best kept (default 5)
--memory-sweep-threads N       Fixed grid size (default 262144)
--memory-sweep-host-only       Skip the device-local pass
```

`--gpu N` is reused for device selection.

## 9. Validation

In order; the probe is only worth having if these hold.

1. **Large-set end reproduces the existing test.** At 1M particles with
   `repeats = 1`, the sweep's stream-equivalent figure should match a real
   `--workload stream --particles 1048576 --headless` run (157.5 GB/s on the
   M4 Pro). This is the check that the derivation is faithful.
2. **The inversion is gone.** Small working sets must now be *faster* than
   large ones. If 128 KB is still slower than 32 MB, the grid decoupling is not
   working and everything downstream is meaningless.
3. **Cache plateau clearly exceeds the memory plateau**, with the peak at a
   small working set.
4. **Unified memory: the two storage curves coincide.** Divergence on an Apple
   part means the storage-mode assumption is wrong.
5. **Grid size does not move the plateaus.** Re-run with 2x the threads; if the
   numbers shift, 262,144 is not saturating the device and the default must
   rise. This is the check most likely to fail when porting to a large discrete
   GPU.
6. **Repeatability**: rerun; plateau rates should move only a few percent.

## 10. Risks and limits

- **Metal only, initially.** Non-Metal builds compile a stub reporting
  `backend_not_implemented`. Vulkan and D3D12 ports are mechanical — same
  struct, same body, same sweep — but are out of scope here and untested.
- **The innermost tier may be broadcast, not cache bandwidth.** At 64 KB a
  262,144-thread grid covers only 2,048 particles, so hardware may service many
  lanes from one fetch. Reported as *peak on-chip read rate*, never as an "L1"
  figure.
- **Plateau detection is a heuristic.** 12% tolerance can merge close tiers or
  split a noisy one.
- **Cache capacities are inferred from knees, not queried.** Shared and
  system-level caches are also contended with the CPU and display engine.
- **`repeats` changes semantics slightly** — particles are updated several times
  per dispatch, so positions advance further than in a real frame. Harmless for
  a bandwidth measurement (same body, same traffic), but it is not a simulation.
- **No results.json integration**, by design.

## 11. Suggested order

1. Rework the two source files to the particle-derived kernel.
2. CMake + `--memory-sweep` dispatch; run validation checks 1–3.
3. Tune the default grid size against check 5.
4. Record the measured M4 Pro curve in `docs/`.
5. Only then consider a GUI page and other backends.
