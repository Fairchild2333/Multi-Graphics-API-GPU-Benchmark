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

## Summary

The score is useful for comparing **the same workload at the same particle
count across devices and APIs**. It is not a peak-bandwidth probe, and it should
not be read against a spec sheet. If a true peak-bandwidth number is ever
wanted, it needs a separate STREAM-style workload with split source and
destination buffers — though on this machine that measured 177.8 GB/s, i.e. no
better than the existing kernel.
