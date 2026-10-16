# Interpreting the Cinematic Liquid score

Cinematic Liquid (`cinematic_liquid_v2_physical_scene_v8`) simulates 320,920
particles with MLS-MPM, 10 substeps a frame, and renders the result with a
raymarch through a density volume. The score is

```
MParticle-step/s ≈ 320,920 x 10 / GPU time of the whole frame
```

The frame time includes both the simulation and the rendering.

## What the simulation stresses

In every substep each particle adds its mass, momentum and stress into the 27
grid cells around it. The adds are integer atomics on fixed-point values. The
grid is updated, and each particle then reads the 27 cells back. The surface
pass splats density into a 128 x 64 x 96 volume, also with atomic adds.

The grid is 128 x 64 x 96 cells of 16 bytes, about 12.6 MB. The accesses are
scattered and many of them are atomic, so the simulation waits on the latency
of reads and atomics in a data set of about 12 MB. A cache large enough to hold
that grid matters more than memory bandwidth or arithmetic throughput.

The rendering is a fragment-shader raymarch through the 3 MB density volume. It
behaves more like GPU Burn: mostly arithmetic, with a working set small enough
for the caches of every GPU tested.

## A15 against A12X

The three GPU tests rank these two Apple chips differently:

| test | what it mostly measures | result |
|---|---|---|
| particles, 16M | streaming bandwidth | A12X (128-bit memory) ahead: 29.5 against 19.0 GB/s |
| GPU Burn | special-function throughput | level: 1.53 against 1.59 Gpix-step/s at 256 steps |
| Cinematic Liquid | latency and atomics on a 12 MB data set | A15 about twice as fast |

The liquid gap is in the simulation, not the rendering (Metal, 2026-10):

| device | simulation (ms/frame) | rendering (ms/frame) | score |
|---|---|---|---|
| A15, iPhone 13 Pro | 154 | 94 | 12.93 |
| A12X, iPad Pro 12.9" 3rd gen | 350 | 114 | 6.90 |
| A12X, iPad Pro 11" 1st gen | 418 | 112 | 6.06 |

The GPU cache diagnostic (dependent random reads, GPU-warm curve) shows why:

| data set | A15 | A12X (12.9") |
|---|---|---|
| 1 MB | 106 ns | 171 ns |
| 4 MB | 167 ns | 263 ns |
| 12 MB | 215 ns | 297 ns |
| 32 MB | 249 ns | 300 ns |
| 128 MB | 318 ns | 356 ns |

- On the A15 the curve stays in a cache tier up to 32 MB and reaches memory
  latency only beyond it. That matches the 32 MB system cache that published
  die analyses give the A15. The whole grid fits, so the atomics complete in the
  cache.
- On the A12X the curve is at its top level from about 6 MB on. At 12 MB every
  atomic add is already a read-modify-write in memory.

The two curves were measured with different chain lengths: 512 loads per chain
on the iPads, which could not run the longer version, against 8192 on the
iPhone. So the nanoseconds are not to be divided against each other; where each
curve flattens is comparable. The cache attribution of these curves is
unverified (`cacheAttribution=unverified`); the reading above rests on their
shape.

The A16 has a 24 MB system cache, which also holds the grid. Its one result
(9.14) ran right after a GPU Burn 256 that slowed by 16.7 % from heat, so it
does not show the chip's speed yet.

## Comparing across platforms

- Apple devices run a Metal port (`..._metal_preview`). Android, HarmonyOS and
  Windows run the Vulkan original. Compare across the two only roughly.
- Snapdragon phones with Adreno 6xx/7xx refuse the test: the engine requires
  linear-filterable R32_SFLOAT 3D images. Quest 2 and Quest 3 run it.
- The score includes rendering. On a GPU that is slow at fragment shading, the
  score understates its simulation speed.
