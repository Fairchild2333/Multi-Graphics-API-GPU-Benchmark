#include "cinematic_liquid_v2_common.hlsli"

// ===== CSSurfaceSplat =====
// Cinematic Liquid v2 render-density reconstruction, pass 2/3.
//
// One invocation splats one simulation particle into an independent density
// volume.  Density is accumulated as unsigned fixed point because Vulkan 1.1
// does not guarantee floating-point image atomics.



// Five float4 members deliberately preserve the 80-byte v2 Particle ABI.








// Shared 64-byte ABI for all three surface-reconstruction passes.


static const float PI = 3.14159265358979323846;

uint volumeIndex(uint3 coord) {
    return coord.x + volumeSizeAndCount.x *
        (coord.y + volumeSizeAndCount.y * coord.z);
}

float3 voxelCenter(int3 coord, float voxelSize) {
    return volumeMinVoxel.xyz +
        (float3(coord) + float3(0.5,0.5,0.5)) * voxelSize;
}

[numthreads(256, 1, 1)]
void CSSurfaceSplat(uint3 DTid : SV_DispatchThreadID)
{

    uint particleIndex = DTid.x;
    if (particleIndex >= volumeSizeAndCount.w) return;

    float h = kernel.x;
    float particleMassOverRestDensity = kernel.y;
    float fixedScale = kernel.z;
    float voxelSize = volumeMinVoxel.w;
    if (h <= 0.0 || particleMassOverRestDensity <= 0.0 ||
        fixedScale <= 0.0 || voxelSize <= 0.0 ||
        any(volumeSizeAndCount.xyz == uint3(0u,0u,0u))) return;

    float3 particlePosition = particleBuffer[particleIndex].position.xyz;

    // A voxel contributes when its exact world-space centre lies inside the
    // particle kernel.  These bounds are derived from that same centre map:
    //     centre = volumeMin + (index + 0.5) * voxelSize
    float3 lowerCoordinate =
        (particlePosition - float3(h, h, h) - volumeMinVoxel.xyz) / voxelSize - float3(0.5,0.5,0.5);
    float3 upperCoordinate =
        (particlePosition + float3(h, h, h) - volumeMinVoxel.xyz) / voxelSize - float3(0.5,0.5,0.5);
    int3 first = int3(ceil(lowerCoordinate));
    int3 last = int3(floor(upperCoordinate));
    int3 volumeMax = int3(volumeSizeAndCount.xyz) - int3(1,1,1);
    first = clamp(first, int3(0,0,0), volumeMax);
    last = clamp(last, int3(0,0,0), volumeMax);
    if (any(first > last)) return;

    float kernelCoefficient = 15.0 / (2.0 * PI * h * h * h * h * h);
    for (int z = first.z; z <= last.z; ++z) {
        for (int y = first.y; y <= last.y; ++y) {
            for (int x = first.x; x <= last.x; ++x) {
                int3 coord = int3(x, y, z);
                float distanceToParticle =
                    length(voxelCenter(coord, voxelSize) - particlePosition);
                if (distanceToParticle > h) continue;

                float hMinusR = h - distanceToParticle;
                float spikySquared = kernelCoefficient * hMinusR * hMinusR;
                float normalizedDensity =
                    particleMassOverRestDensity * spikySquared;
                uint encodedDensity = uint(round(normalizedDensity * fixedScale));
                if (encodedDensity == 0u) continue;

                InterlockedAdd(densityAtomicBuffer[
                    volumeIndex(uint3(coord))], encodedDensity);
            }
        }
    }

}

