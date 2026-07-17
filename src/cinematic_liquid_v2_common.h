#pragma once

#include "gpu_common.h"

#include <cstdint>
#include <vector>

namespace gpu_bench {

// Shared GPU ABI for Cinematic Liquid v2 (MLS-MPM) and the SPH presentation
// path.  Layouts must match the GLSL std430 / HLSL StructuredBuffer contracts.

struct alignas(16) MlsMpmParticleGpu {
    float position[4];
    float velocity[4];
    float c0[4];
    float c1[4];
    float c2[4];
};
static_assert(sizeof(MlsMpmParticleGpu) == 80,
              "MLS-MPM particle layout must match the GLSL std430 ABI");

struct alignas(16) CinematicLiquidBodyStateGpu {
    float positionType[4];
    float orientation[4];
    float linearVelocityInvMass[4];
    float angularVelocityInvInertia[4];
    float shape0[4];
    float shape1[4];
    float material[4];
    float color[4];
};
static_assert(sizeof(CinematicLiquidBodyStateGpu) == 128,
              "Cinematic Liquid v2 body state must match GLSL std430");

struct alignas(16) CinematicLiquidBodyImpulseGpu {
    std::int32_t linear[4];
    std::int32_t angular[4];
};
static_assert(sizeof(CinematicLiquidBodyImpulseGpu) == 32,
              "Cinematic Liquid v2 body impulse must match GLSL std430");

struct alignas(16) CinematicLiquidV2PushConstants {
    std::uint32_t gridSizeAndCount[4];
    float simulation[4];
    float material[4];
    float gridOriginDx[4];
    float collision[4];
    float coupling[4];
    std::uint32_t scene[4];
    float pool[4];
};
static_assert(sizeof(CinematicLiquidV2PushConstants) == 128,
              "Cinematic Liquid v2 compute push constants must match GLSL");

struct alignas(16) CinematicLiquidV2SurfacePushConstants {
    std::uint32_t volumeSizeAndCount[4];
    float volumeMinVoxel[4];
    float kernel[4];
    std::uint32_t contract[4];
};
static_assert(sizeof(CinematicLiquidV2SurfacePushConstants) == 64,
              "Cinematic Liquid v2 surface push constants must match GLSL");

struct alignas(16) CinematicLiquidV2RenderPushConstants {
    float cameraTime[4];
    float targetAspect[4];
    float volumeMinIso[4];
    float volumeMaxStep[4];
    float pool[4];
    float lighting[4];
    std::uint32_t render[4];
    std::uint32_t scene[4];
};
static_assert(sizeof(CinematicLiquidV2RenderPushConstants) == 128,
              "Cinematic Liquid v2 render push constants must match GLSL");

// World-space origin / material scales shared by compute, surface, and render.
constexpr float kLiquidV2OriginX = -2.56f;
constexpr float kLiquidV2OriginY = -0.12f;
constexpr float kLiquidV2OriginZ = -1.92f;
constexpr float kLiquidV2RestDensity = 1000.0f;
constexpr float kLiquidV2FixedPointScale = 65'536.0f;
constexpr float kLiquidV2BodyImpulseScale = 8'192.0f;

std::uint32_t liquidHash(std::uint32_t x);
float liquidJitter(std::uint32_t seed);

// Deterministic MLS-MPM dam-break lattice (not SPH).  Clears `out` then fills
// the play-pool bed + left reservoir (~320,920 particles).  `dx` and `mass`
// are part of the shared seed API for backends; seeding uses `spacing`.
void BuildCinematicLiquidV2ParticleSeed(std::vector<MlsMpmParticleGpu>& out,
                                        float dx, float spacing, float mass);

// Seven-body duck-family scene (mother, ball, boat, sink sphere, 3 ducklings).
void BuildCinematicLiquidV2BodySeed(
    std::vector<CinematicLiquidBodyStateGpu>& out);

void FillCinematicLiquidV2ComputePush(
    CinematicLiquidV2PushConstants& out,
    std::uint32_t gridX, std::uint32_t gridY, std::uint32_t gridZ,
    std::uint32_t particleCount,
    float substepDt,
    float particleMass,
    float dx,
    std::uint32_t bodyCount,
    std::uint32_t shaderVersion,
    float presentationTime);

void FillCinematicLiquidV2SurfacePush(
    CinematicLiquidV2SurfacePushConstants& out,
    std::uint32_t surfaceX, std::uint32_t surfaceY, std::uint32_t surfaceZ,
    std::uint32_t particleCount,
    float surfaceVoxelSize,
    float particleSpacing,
    float particleMass,
    std::uint32_t shaderVersion);

void FillCinematicLiquidV2RenderPush(
    CinematicLiquidV2RenderPushConstants& out,
    float presentationTime,
    float aspect,
    std::uint32_t gridX, std::uint32_t gridY, std::uint32_t gridZ,
    float dx,
    std::uint32_t raySteps,
    std::uint32_t shaderVersion,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t bodyCount,
    bool swapchainIsSrgb);

} // namespace gpu_bench
