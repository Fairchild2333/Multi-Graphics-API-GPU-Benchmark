#include "cinematic_liquid_v2_common.hlsli"

// ===== CSResolveWhitewater =====
// Cinematic Liquid v2 density resolve.  This keeps the simulation grid in
// fixed-point storage and exposes a filterable R32F density field to the
// independent v2 presentation pipeline.

















uint gridIndex(uint3 c) {
    return c.x + gridSizeAndCount.x *
                 (c.y + gridSizeAndCount.y * c.z);
}

bool insideGrid(int3 c) {
    return all(c >= int3(0,0,0)) &&
           all(c < int3(gridSizeAndCount.xyz));
}

float fixedScale() { return max(abs(material.z), 1.0); }

float normalizedDensityAt(int3 c, float restCellMass) {
    if (!insideGrid(c)) return 0.0;
    float mass = float(max(gridBuffer[gridIndex(uint3(c))].mass, 0)) /
                 fixedScale();
    return clamp(mass / max(restCellMass, 1.0e-8), 0.0, 4.0);
}

float3 velocityAt(int3 c) {
    if (!insideGrid(c)) return float3(0,0,0);
    GridCell cell = gridBuffer[gridIndex(uint3(c))];
    if (cell.mass <= 0) return float3(0,0,0);
    // grid_update_v2 replaces accumulated momentum with encoded velocity.
    // Dividing by mass again here inflated free-surface velocity by 15x or
    // more and was the direct cause of the fake white patches in the pool.
    return float3(cell.vx, cell.vy, cell.vz) / fixedScale();
}

[numthreads(8, 8, 4)]
void CSResolveWhitewater(uint3 DTid : SV_DispatchThreadID)
{

    uint3 coord = DTid.xyz;
    if (any(coord >= gridSizeAndCount.xyz)) return;

    float dx = max(abs(gridOriginDx.w), 1.0e-6);
    float restCellMass = max(simulation.z * dx * dx * dx, 1.0e-8);
    float cellMass = float(max(gridBuffer[gridIndex(coord)].mass, 0)) /
                     fixedScale();
    float density = clamp(cellMass / restCellMass, 0.0, 4.0);
    // The visible density is rebuilt independently from final particle
    // positions in its own fixed volume. This grid pass now writes only whitewater;
    // `density` remains the physically useful surface-band classifier below.

    // jeantimex/fluid's whitewater system uses trapped-air and kinetic-energy
    // triggers.  MLS-MPM already gives us a velocity field, so derive the same
    // physical signals from six grid neighbours instead of copying its SPH
    // neighbour table or inventing screen-space paint.
    int3 c = int3(coord);
    const int3 directions[6] = {
        int3(1,0,0), int3(-1,0,0), int3(0,1,0),
        int3(0,-1,0), int3(0,0,1), int3(0,0,-1)};
    float3 velocity = velocityAt(c);
    float convergingFlow = 0.0;
    float velocityMismatch = 0.0;
    for (int i = 0; i < 6; ++i) {
        int3 neighbour = c + directions[i];
        float neighbourDensity = normalizedDensityAt(neighbour, restCellMass);
        float3 neighbourVelocity = velocityAt(neighbour);
        float neighbourWeight = smoothstep(0.025, 0.30, neighbourDensity);
        float3 direction = float3(directions[i]);
        float3 relativeVelocity = velocity - neighbourVelocity;
        convergingFlow += max(dot(relativeVelocity, direction), 0.0) *
                           neighbourWeight;
        velocityMismatch += length(relativeVelocity) * neighbourWeight;
    }
    convergingFlow *= 0.5;
    velocityMismatch /= 6.0;

    // Empty neighbours must not be treated as stationary fluid: doing so
    // creates a false curl sheet across every calm free surface.  Extend the
    // centre velocity into air for gradient reconstruction, while the
    // density band below still identifies the real surface.
    int3 cxm = c - int3(1,0,0), cxp = c + int3(1,0,0);
    int3 cym = c - int3(0,1,0), cyp = c + int3(0,1,0);
    int3 czm = c - int3(0,0,1), czp = c + int3(0,0,1);
    float3 vxMinus = normalizedDensityAt(cxm, restCellMass) > 0.055
        ? velocityAt(cxm) : velocity;
    float3 vxPlus = normalizedDensityAt(cxp, restCellMass) > 0.055
        ? velocityAt(cxp) : velocity;
    float3 vyMinus = normalizedDensityAt(cym, restCellMass) > 0.055
        ? velocityAt(cym) : velocity;
    float3 vyPlus = normalizedDensityAt(cyp, restCellMass) > 0.055
        ? velocityAt(cyp) : velocity;
    float3 vzMinus = normalizedDensityAt(czm, restCellMass) > 0.055
        ? velocityAt(czm) : velocity;
    float3 vzPlus = normalizedDensityAt(czp, restCellMass) > 0.055
        ? velocityAt(czp) : velocity;
    float invTwoDx = 0.5 / dx;
    float divergence = ((vxPlus.x - vxMinus.x) +
                        (vyPlus.y - vyMinus.y) +
                        (vzPlus.z - vzMinus.z)) * invTwoDx;
    float3 curl = float3(
        (vyPlus.z - vyMinus.z) - (vzPlus.y - vzMinus.y),
        (vzPlus.x - vzMinus.x) - (vxPlus.z - vxMinus.z),
        (vxPlus.y - vxMinus.y) - (vyPlus.x - vyMinus.x)) * invTwoDx;

    float surfaceBand = smoothstep(0.035, 0.18, density) *
                        (1.0 - smoothstep(0.62, 1.35, density));
    float kinetic = smoothstep(0.65, 6.00, dot(velocity, velocity));
    float trappedAir = smoothstep(0.55, 2.80,
        velocityMismatch + 0.60 * convergingFlow +
        0.025 * max(-divergence, 0.0));
    float turbulent = smoothstep(6.0, 30.0, length(curl));
    float spray = smoothstep(0.015, 0.24, density) *
                  (1.0 - smoothstep(0.24, 0.52, density)) *
                  smoothstep(1.10, 3.80, length(velocity));
    float whitewater = surfaceBand * kinetic *
        clamp(0.82 * trappedAir + 0.38 * turbulent, 0.0, 1.0);
    whitewater = max(whitewater, spray * (0.30 + 0.52 * turbulent));

    if (scene.x > 3u) {
        BodyState sinkBall = bodyStateBuffer[3];
        if (int(round(sinkBall.positionType.w)) == 3) {
            float3 worldPosition = gridOriginDx.xyz + float3(coord) * dx;
            float3 toNode = worldPosition - sinkBall.positionType.xyz;
            float radius = max(sinkBall.shape0.x, 0.01);
            float radialDistance = length(toNode);
            float shellDistance = abs(radialDistance - radius);
            float impactSpeed = max(-sinkBall.linVelInvMass.y, 0.0);
            float equator = radialDistance > 1.0e-5
                ? 1.0 - abs(toNode.y / radialDistance) : 0.0;
            float impactShell = 1.0 - smoothstep(0.35 * dx,
                                                 2.8 * dx,
                                                 shellDistance);
            float crown = impactShell * smoothstep(0.65, 3.20, impactSpeed) *
                          smoothstep(0.12, 0.82, equator) *
                          smoothstep(0.015, 0.42, density);
            // This signal is tied to the real GPU body state and local fluid
            // density; it highlights the coupled entry crown rather than
            // drawing a screen-space splash.
            whitewater = max(whitewater, crown);
        }
    }
    whitewater = pow(clamp(whitewater, 0.0, 1.0), 1.80);
    whitewaterVolume[int3(c)] = (float4(whitewater, 0.0, 0.0, 1.0)).x;

}

