#include "cinematic_liquid_v2_common.hlsli"

// ===== CSSurfaceClear =====
// Cinematic Liquid v2 render-density reconstruction, pass 1/3.
//
// Clear the fixed-point density accumulator independently of the MLS-MPM
// simulation grid.  Keeping this volume separate lets presentation use a
// smooth SPH kernel without changing the simulation or its score contract.



// Five float4 members deliberately preserve the 80-byte v2 Particle ABI.








// Shared 64-byte ABI for all three surface-reconstruction passes.


uint volumeIndex(uint3 coord) {
    return coord.x + volumeSizeAndCount.x *
        (coord.y + volumeSizeAndCount.y * coord.z);
}

[numthreads(8, 8, 4)]
void CSSurfaceClear(uint3 DTid : SV_DispatchThreadID)
{

    uint3 coord = DTid.xyz;
    if (any(coord >= volumeSizeAndCount.xyz)) return;

    densityAtomicBuffer[volumeIndex(coord)] = 0u;

}

