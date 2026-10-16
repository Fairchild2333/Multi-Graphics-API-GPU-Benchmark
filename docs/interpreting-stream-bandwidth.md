# Interpreting the Particle (`stream`) bandwidth score

The Particle workload reports a score in GB/s. That number is **derived**, not
read from a hardware counter, and it is easy to misread in two specific ways.
This note records what it does and does not mean, and the measurements that
establish it.

All figures below were measured on an Apple M4 Pro (macOS 26.5, Metal). The
reasoning is platform-independent — the kernel and the score constant are shared
by every backend — but the absolute numbers are machine-specific.

## What the number is

```cpp
// src/app_base.cpp
r.score = 40.0 * n / computeSec / 1e9;   // ~40 bytes moved per particle
```

`n` is the particle count and `computeSec` is the average GPU time of the
compute command buffer. So the score is:

> assumed bytes per particle × particles ÷ measured compute time

Nothing measures actual memory traffic. The `40.0` is a hand-estimated constant.

## Caveat 1: the byte constant undercounts by ~17%

The particle is `float4 position + float4 velocity` = **32 bytes**. The update
kernel is the same on every backend (`shaders/compute.comp`,
`shaders/compute.hlsl`, `shaders/particle.metal`):

```metal
particles[id].position.xyz += particles[id].velocity.xyz * params.deltaTime;
if (particles[id].position.x > params.bounds) particles[id].position.x = -params.bounds;
```

It reads both halves (32 B) and writes the position back (16 B), so real traffic
is about **48 bytes per particle**, not 40. The reported figure therefore runs
roughly `48/40 = 1.2×` low.

This was confirmed with a standalone Metal probe over a 1 GB buffer, taking the
best of 12 dispatches:

| kernel | bytes/element assumed | measured |
|---|---|---|
| pure copy `dst[i] = src[i]`, 4 per thread | 32 (unambiguous) | 177.8 GB/s |
| the shipped particle kernel, 1 per thread | 48 | 180.5 GB/s |
| the particle kernel, 4 per thread | 48 | 180.8 GB/s |

The pure copy has no ambiguity about its byte count, and it lands within 2% of
the particle kernel scored at 48 B. Scoring the particle kernel at 40 B would
put it 19% below a copy with an identical access pattern, which is not credible.

**The constant has deliberately not been changed.** Correcting it would shift
every stored score on every platform by 17% and silently break comparability
with existing history. If it is ever changed, bump the result schema version so
old and new rows can be told apart.

## Caveat 2: small particle counts measure cache, not memory

The presets span a 256× range of working-set size, crossing the cache/memory
boundary:

| preset | particles | buffer | measured (headless) |
|---|---|---|---|
| Light | 65,536 | 2 MB | 317 GB/s |
| Medium | 1,048,576 | 32 MB | 155 GB/s |
| Heavy | 4,194,304 | 128 MB | 148 GB/s |
| Extreme | 16,777,216 | 512 MB | 149 GB/s |

The number plateaus from 128 MB upward — that plateau is the memory result. The
2 MB buffer sits in cache and reports 317 GB/s, which exceeds what the memory
system can deliver, so it is not a bandwidth reading at all.

**Only compare runs that used the same particle count.** The GUI now shows the
buffer size on each preset, warns when a cache-resident size is selected, and
defaults the Charts page to a single size instead of plotting several together.

## Caveat 3: read-modify-write costs about a third of peak

The M4 Pro is specified at 273 GB/s. The particle kernel reaches ~180 GB/s of
real traffic, about 66% of that.

The gap is **not** a hardware ceiling. It is the write-back. Measured with the
memory sweep (see `gpu-memory-sweep-plan.md`) on a 512 MiB working set, far
larger than any cache:

| access pattern | measured | of spec |
|---|---|---|
| read-modify-write (what the particle kernel does) | 175.7 GB/s | 64% |
| pure read, same struct, same walk | **251.2 GB/s** | **92%** |

So the memory subsystem does deliver close to its rating; a kernel that reads
and writes the same buffer simply cannot. Unrolling does not recover it
(180.8 vs 180.5 GB/s for 4 vs 1 element per thread), because the limit is
traffic, not instruction-level parallelism.

An earlier revision of this document claimed ~180 GB/s was "the practical
ceiling for this class of kernel". That was wrong — it was inferred from a copy
kernel, which also writes. Use 251 GB/s as this machine's read bandwidth and
~176 GB/s as its read-modify-write bandwidth.

## Comparing phones and tablets: the score follows the memory

Across mobile devices the 16M-particle headless score tracks each device's own
measured memory bandwidth (the VRAM / GPU memory test), not the strength of its
GPU. Measured 2026-10 (Apple: Metal, or MoltenVK where noted; Android and
HarmonyOS: Vulkan):

| device | 16M particles (GB/s) | GPU memory test (GB/s) | ratio |
|---|---|---|---|
| A12X, iPad Pro 12.9" 3rd gen | 29.5 | 59.9 | 0.49 |
| A12X, iPad Pro 11" 1st gen (MoltenVK) | 28.8 | 58.1 | 0.50 |
| XRing O1, Xiaomi 15S Pro | 27.8 | 44.6 | 0.62 |
| A16, iPhone 14 Pro Max | 27.5 | 41.0 | 0.67 |
| Snapdragon 8 Gen 2, MIX Fold 3 | 26.1 | 53.0 (no plateau) | 0.49 |
| Snapdragon 888+, MIX 4 | 20.9 | 32.7 | 0.64 |
| Snapdragon 8 Gen 1, Black Shark 5 Pro | 19.2 | 44.2 (no plateau) | 0.43 |
| A15, iPhone 13 Pro | 19.0 | 30.9 | 0.61 |
| Kirin 9000, Mate 40 Pro | 17.2 | 30.2 | 0.57 |
| Snapdragon 865, Black Shark 3 Pro | 13.5 | 23.2 | 0.58 |

The GPU memory column mixes versions of the memory test: V1 on Android and
HarmonyOS, the native Metal sweep on the A15, A16 and the 12.9" iPad, V2 on the
11" iPad. V1 and the current V3 agree within 0.2 - 3 % (RTX 5090, M4 Pro, A12X,
A15; table in [cli-reference.md](cli-reference.md), memory-bandwidth sweep).
V2 read too high only on the M4 Pro and is 2 % above V3 on the A12X. The MIX
Fold 3 and Black Shark 5 Pro values did not reach the memory plateau and were
measured before the data check existed (2026-10-07), so their ratios are less
certain; the others stand.

Apple's GPUs turn their bandwidth into particle throughput as well as the
others do (0.49 - 0.67 against 0.43 - 0.64). Where an Apple device ranks low
here, its memory is the reason: the A15 has 64-bit LPDDR4X (about 34 GB/s on
paper, 30.9 measured), the slowest memory of these flagships, while the A12X's
128-bit bus puts both iPads at the top. Games hide much of a narrow bus behind
large on-chip caches and tile memory; a 512 MB buffer streamed every frame
cannot. Two outliers sit
far below the line (Kirin 9000S 0.27, Kirin X90 0.30) and are not explained
yet.

## Summary

The score is useful for comparing **the same workload at the same particle
count across devices and APIs**. It is not a peak-bandwidth probe, and it should
not be read against a spec sheet. If a true peak-bandwidth number is ever
wanted, it needs a separate STREAM-style workload with split source and
destination buffers — though on this machine that measured 177.8 GB/s, i.e. no
better than the existing kernel.
