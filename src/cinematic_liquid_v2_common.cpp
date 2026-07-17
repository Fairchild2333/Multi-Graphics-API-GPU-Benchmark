#include "cinematic_liquid_v2_common.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace gpu_bench {

std::uint32_t liquidHash(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    return x ^ (x >> 16);
}

float liquidJitter(std::uint32_t seed) {
    return (float(liquidHash(seed) & 0xffffu) / 65535.0f - 0.5f);
}

void BuildCinematicLiquidV2ParticleSeed(std::vector<MlsMpmParticleGpu>& out,
                                        float dx, float spacing, float mass) {
    (void)dx;
    (void)mass;
    // V7 redistributes the same ~321k budget into a deeper 14-layer play pool
    // so the 0.40 m sink sphere can generate a resolved entry crown, while
    // retaining a tall dam reservoir: 142*14*98 + 48*37*71 = 320,920.
    constexpr std::uint32_t baseX = 142, baseY = 14, baseZ = 98;
    constexpr std::uint32_t damX = 48, damY = 37, damZ = 71;
    constexpr std::size_t expectedParticles =
        std::size_t(baseX) * baseY * baseZ +
        std::size_t(damX) * damY * damZ;

    out.clear();
    out.reserve(expectedParticles);
    std::uint32_t serial = 0;
    auto appendLattice = [&](std::uint32_t countX, std::uint32_t countY,
                             std::uint32_t countZ, float centreX,
                             float startY, float centreZ) {
        for (std::uint32_t iz = 0; iz < countZ; ++iz) {
            for (std::uint32_t iy = 0; iy < countY; ++iy) {
                for (std::uint32_t ix = 0; ix < countX; ++ix, ++serial) {
                    const float x = centreX +
                        (float(ix) - 0.5f * float(countX - 1u)) * spacing;
                    const float y = startY + float(iy) * spacing;
                    const float z = centreZ +
                        (float(iz) - 0.5f * float(countZ - 1u)) * spacing;
                    const float jitterScale = spacing * 0.035f;
                    MlsMpmParticleGpu particle{};
                    particle.position[0] =
                        x + liquidJitter(0x51a7c0deu + serial * 3u) * jitterScale;
                    particle.position[1] =
                        y + liquidJitter(0x51a7c0dfu + serial * 3u) * jitterScale;
                    particle.position[2] =
                        z + liquidJitter(0x51a7c0e0u + serial * 3u) * jitterScale;
                    particle.position[3] = 1.0f;
                    out.push_back(particle);
                }
            }
        }
    };
    appendLattice(baseX, baseY, baseZ, 0.0f, 0.10f, 0.0f);
    appendLattice(damX, damY, damZ, -1.63f, 0.46f, 0.0f);
    if (out.size() != expectedParticles)
        throw std::runtime_error(
            "Cinematic Liquid v2 deterministic dam-break seed drifted");
    if (out.size() < 310'000u || out.size() > 330'000u)
        throw std::runtime_error(
            "Cinematic Liquid v2 particle contract drifted outside 310k-330k");
}

void BuildCinematicLiquidV2BodySeed(
    std::vector<CinematicLiquidBodyStateGpu>& out) {
    auto set4 = [](float (&dst)[4], float x, float y, float z, float w) {
        dst[0] = x; dst[1] = y; dst[2] = z; dst[3] = w;
    };

    out.assign(kCinematicLiquidV2BodyCount, CinematicLiquidBodyStateGpu{});

    // 0: mother rubber duck.
    set4(out[0].positionType, 0.05f, 0.56f, 0.30f, 0.0f);
    set4(out[0].orientation, 0.0f, -0.30071f, 0.0f, 0.95372f);
    set4(out[0].linearVelocityInvMass, 0.0f, 0.0f, 0.0f, 1.0f / 20.0f);
    set4(out[0].angularVelocityInvInertia, 0.0f, 0.0f, 0.0f, 1.2f);
    set4(out[0].shape0, 0.30f, 0.21f, 0.26f, 18.0f);
    set4(out[0].shape1, 0.15f, 0.12f, 0.26f, 0.12f);
    set4(out[0].material, 0.08f, 0.28f, 1.35f, 2.60f);
    set4(out[0].color, 1.00f, 0.66f, 0.035f, 0.30f);

    // 1: hollow play ball.
    set4(out[1].positionType, 0.78f, 0.55f, -0.32f, 1.0f);
    set4(out[1].orientation, 0.0f, 0.0f, 0.0f, 1.0f);
    set4(out[1].linearVelocityInvMass, 0.0f, 0.0f, 0.0f, 1.0f / 5.4f);
    set4(out[1].angularVelocityInvInertia, 0.0f, 0.0f, 0.0f, 4.0f);
    set4(out[1].shape0, 0.22f, 0.0f, 0.0f, 0.0f);
    set4(out[1].shape1, 0.0f, 0.0f, 0.0f, 0.0f);
    set4(out[1].material, 0.34f, 0.20f, 0.85f, 1.10f);
    set4(out[1].color, 0.96f, 0.10f, 0.18f, 0.24f);

    // 2: motorized toy boat.
    set4(out[2].positionType, 1.25f, 0.47f, 0.50f, 2.0f);
    set4(out[2].orientation, 0.0f, -0.08716f, 0.0f, 0.99619f);
    set4(out[2].linearVelocityInvMass, 0.0f, 0.0f, 0.0f, 1.0f / 34.0f);
    set4(out[2].angularVelocityInvInertia, 0.0f, 0.0f, 0.0f, 0.62f);
    set4(out[2].shape0, 0.52f, 0.16f, 0.24f, 0.10f);
    set4(out[2].shape1, 0.14f, 0.62f, 3.2f, 12.0f);
    set4(out[2].material, 0.04f, 0.34f, 0.90f, 1.70f);
    set4(out[2].color, 0.045f, 0.22f, 0.82f, 0.30f);

    // 3: 1.06x-water-density solid sphere (release choreography keyed to lane 3).
    set4(out[3].positionType, 0.38f, 1.65f, -1.25f, 3.0f);
    set4(out[3].orientation, 0.0f, 0.0f, 0.0f, 1.0f);
    set4(out[3].linearVelocityInvMass, 0.0f, 0.0f, 0.0f, 1.0f / 35.52f);
    set4(out[3].angularVelocityInvInertia, 0.0f, 0.0f, 0.0f, 1.76f);
    set4(out[3].shape0, 0.20f, 0.0f, 0.0f, 0.0f);
    set4(out[3].shape1, 0.0f, 0.0f, 0.0f, 0.0f);
    set4(out[3].material, 0.0f, 0.42f, 4.00f, 1.40f);
    set4(out[3].color, 0.12f, 0.15f, 0.19f, 0.20f);

    // 4-6: duckling trio.
    constexpr struct { float x, z, qy, qw; } kDucklings[3] = {
        {-0.62f, 0.78f,  0.21644f, 0.97630f},
        { 0.42f, 1.10f, -0.46175f, 0.88701f},
        {-0.20f, 1.15f, -0.60876f, 0.79335f},
    };
    for (std::size_t i = 0; i < 3; ++i) {
        auto& duckling = out[4 + i];
        set4(duckling.positionType, kDucklings[i].x, 0.52f, kDucklings[i].z, 0.0f);
        set4(duckling.orientation, 0.0f, kDucklings[i].qy, 0.0f, kDucklings[i].qw);
        set4(duckling.linearVelocityInvMass, 0.0f, 0.0f, 0.0f, 1.0f / 1.82f);
        set4(duckling.angularVelocityInvInertia, 0.0f, 0.0f, 0.0f, 9.0f);
        set4(duckling.shape0, 0.14f, 0.095f, 0.12f, 26.0f);
        set4(duckling.shape1, 0.068f, 0.054f, 0.118f, 0.055f);
        set4(duckling.material, 0.10f, 0.30f, 1.60f, 3.20f);
        set4(duckling.color, 1.00f, 0.66f, 0.035f, 0.30f);
    }
}

void FillCinematicLiquidV2ComputePush(
    CinematicLiquidV2PushConstants& out,
    std::uint32_t gridX, std::uint32_t gridY, std::uint32_t gridZ,
    std::uint32_t particleCount,
    float substepDt,
    float particleMass,
    float dx,
    std::uint32_t bodyCount,
    std::uint32_t shaderVersion,
    float presentationTime) {
    out = {};
    out.gridSizeAndCount[0] = gridX;
    out.gridSizeAndCount[1] = gridY;
    out.gridSizeAndCount[2] = gridZ;
    out.gridSizeAndCount[3] = particleCount;
    out.simulation[0] = substepDt;
    out.simulation[1] = -9.81f;
    out.simulation[2] = kLiquidV2RestDensity;
    out.simulation[3] = 45'000.0f;
    out.material[0] = 0.035f;
    out.material[1] = particleMass;
    out.material[2] = kLiquidV2FixedPointScale;
    out.material[3] = 2.5f;
    out.gridOriginDx[0] = kLiquidV2OriginX;
    out.gridOriginDx[1] = kLiquidV2OriginY;
    out.gridOriginDx[2] = kLiquidV2OriginZ;
    out.gridOriginDx[3] = dx;
    out.collision[0] = 0.45f;
    out.collision[1] = 0.035f;
    out.collision[2] = 0.035f;
    out.collision[3] = 8.0f;
    out.coupling[0] = kLiquidV2BodyImpulseScale;
    out.coupling[1] = 1.0f;
    out.coupling[2] = 1.0f;
    out.coupling[3] = 1.0f;
    out.scene[0] = bodyCount;
    out.scene[1] = 2u;
    out.scene[2] = 1u;  // two-way coupling + deterministic choreography
    out.scene[3] = shaderVersion;
    out.pool[0] = 0.30f;
    // Wall inset; must stay in sync with the render frag and SPH wall constants.
    out.pool[1] = 0.45f;
    out.pool[2] = 0.00f;
    out.pool[3] = presentationTime;
}

void FillCinematicLiquidV2SurfacePush(
    CinematicLiquidV2SurfacePushConstants& out,
    std::uint32_t surfaceX, std::uint32_t surfaceY, std::uint32_t surfaceZ,
    std::uint32_t particleCount,
    float surfaceVoxelSize,
    float particleSpacing,
    float particleMass,
    std::uint32_t shaderVersion) {
    out = {};
    out.volumeSizeAndCount[0] = surfaceX;
    out.volumeSizeAndCount[1] = surfaceY;
    out.volumeSizeAndCount[2] = surfaceZ;
    out.volumeSizeAndCount[3] = particleCount;
    out.volumeMinVoxel[0] = kLiquidV2OriginX;
    out.volumeMinVoxel[1] = kLiquidV2OriginY;
    out.volumeMinVoxel[2] = kLiquidV2OriginZ;
    out.volumeMinVoxel[3] = surfaceVoxelSize;
    out.kernel[0] = 1.70f * particleSpacing;
    out.kernel[1] = particleMass / kLiquidV2RestDensity;
    out.kernel[2] = kLiquidV2FixedPointScale;
    out.kernel[3] = 4.0f;
    out.contract[0] = shaderVersion;
    out.contract[1] = 1u;  // normalized Spiky^2 particle reconstruction
}

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
    bool swapchainIsSrgb) {
    // Piecewise smoothstep path: overview -> low side-on 5 s hero hold ->
    // propeller side -> high rear.  Segment derivatives reach zero at joins.
    struct CameraKey { float t, degrees, radius, height; };
    constexpr CameraKey keys[] = {
        {0.0f,  -35.0f, 5.35f, 2.05f},
        {3.0f,   34.0f, 5.00f, 1.78f},
        {4.6f,   72.0f, 4.25f, 1.76f},
        {5.5f,   84.0f, 4.30f, 1.82f},
        {10.0f, 105.0f, 4.85f, 2.30f},
        {15.0f, 165.0f, 5.35f, 2.65f}
    };
    constexpr std::size_t keyCount = sizeof(keys) / sizeof(keys[0]);
    const float cameraT = std::clamp(presentationTime, 0.0f, 15.0f);
    CameraKey camera = keys[keyCount - 1u];
    for (std::size_t i = 0; i + 1u < keyCount; ++i) {
        if (cameraT <= keys[i + 1u].t) {
            float u = (cameraT - keys[i].t) / (keys[i + 1u].t - keys[i].t);
            u = std::clamp(u, 0.0f, 1.0f);
            u = u * u * (3.0f - 2.0f * u);
            camera.t = cameraT;
            camera.degrees =
                keys[i].degrees + (keys[i + 1u].degrees - keys[i].degrees) * u;
            camera.radius =
                keys[i].radius + (keys[i + 1u].radius - keys[i].radius) * u;
            camera.height =
                keys[i].height + (keys[i + 1u].height - keys[i].height) * u;
            break;
        }
    }
    const float angle = camera.degrees * 3.14159265359f / 180.0f;
    const float cameraX = std::cos(angle) * camera.radius;
    const float cameraZ = std::sin(angle) * camera.radius;

    out = {};
    out.cameraTime[0] = cameraX;
    out.cameraTime[1] = camera.height;
    out.cameraTime[2] = cameraZ;
    out.cameraTime[3] = presentationTime;
    out.targetAspect[0] = 0.0f;
    out.targetAspect[1] = 0.82f;
    out.targetAspect[2] = 0.0f;
    out.targetAspect[3] = aspect;
    out.volumeMinIso[0] = kLiquidV2OriginX;
    out.volumeMinIso[1] = kLiquidV2OriginY;
    out.volumeMinIso[2] = kLiquidV2OriginZ;
    // jeantimex/fluid uses densityOffset/targetDensity ~= 200/630.
    out.volumeMinIso[3] = 0.32f;
    out.volumeMaxStep[0] = kLiquidV2OriginX + float(gridX) * dx;
    out.volumeMaxStep[1] = kLiquidV2OriginY + float(gridY) * dx;
    out.volumeMaxStep[2] = kLiquidV2OriginZ + float(gridZ) * dx;
    out.volumeMaxStep[3] = 1.0f;
    out.pool[0] = 0.30f;
    out.pool[1] = 0.085f;
    out.pool[2] = 0.00f;  // physical pool/grass ground height
    out.pool[3] = 0.025f;
    out.lighting[0] = -0.42f;
    out.lighting[1] = 0.78f;
    out.lighting[2] = 0.46f;
    out.lighting[3] = 1.08f;
    out.render[0] = raySteps;
    out.render[1] = shaderVersion;
    out.render[2] = width;
    out.render[3] = height;
    out.scene[0] = bodyCount;
    out.scene[1] = 2u;
    out.scene[2] = swapchainIsSrgb ? 1u : 0u;
    out.scene[3] = shaderVersion;
}

} // namespace gpu_bench
