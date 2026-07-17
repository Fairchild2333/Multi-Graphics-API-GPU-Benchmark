#include "cinematic_liquid_v2_common.hlsli"

// ===== CSClearGrid =====
// Cinematic Liquid v2 pass 1: clear the MLS-MPM grid and the fixed-point
// rigid-body reaction accumulators.  Dispatch enough x workgroups to cover
// gridSize.x * gridSize.y * gridSize.z; that is always larger than bodyCount
// for the fixed 128x64x96 scene.





















// Shared 128-byte compute ABI for every Cinematic Liquid v2 pass.

[numthreads(256, 1, 1)]
void CSClearGrid(uint3 DTid : SV_DispatchThreadID)
{

    uint index = DTid.x;
    uint totalCells = gridSizeAndCount.x *
                      gridSizeAndCount.y *
                      gridSizeAndCount.z;

    if (index < totalCells) {
        gridBuffer[index].vx = 0;
        gridBuffer[index].vy = 0;
        gridBuffer[index].vz = 0;
        gridBuffer[index].mass = 0;
    }

    if (index < min(scene.x, 32u)) {
        bodyImpulseBuffer[index].linImpulse = int4(0,0,0,0);
        bodyImpulseBuffer[index].angImpulse = int4(0,0,0,0);
    }

}

