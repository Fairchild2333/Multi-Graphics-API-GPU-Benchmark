#include "cinematic_liquid_v2_common.hlsli"

// ===== CSP2GMass =====
// Cinematic Liquid v2 pass 2: APIC particle-to-grid mass and momentum.
















static const float kFixedContributionLimit = 536870911.0;

float fixedScale() { return max(abs(material.z), 1.0); }
int encodeFixedSigned(float v) {
    return int(round(clamp(v * fixedScale(),
                           -kFixedContributionLimit,
                            kFixedContributionLimit)));
}
int encodeFixedPositive(float v) {
    return int(round(clamp(v * fixedScale(), 0.0,
                           kFixedContributionLimit)));
}
float3 quadraticWeights(float x) {
    float a = 1.5 - x;
    float b = x - 1.0;
    float c = x - 0.5;
    return float3(0.5 * a * a, 0.75 - b * b, 0.5 * c * c);
}
bool insideGrid(int3 n, int3 s) {
    return all(n >= int3(0,0,0)) && all(n < s);
}
uint gridIndex(int3 n) {
    uint3 c = uint3(n);
    return c.x + gridSizeAndCount.x *
                 (c.y + gridSizeAndCount.y * c.z);
}

[numthreads(256, 1, 1)]
void CSP2GMass(uint3 DTid : SV_DispatchThreadID)
{

    uint particleIndex = DTid.x;
    if (particleIndex >= gridSizeAndCount.w ||
        any(gridSizeAndCount.xyz < uint3(3u,3u,3u))) return;

    float dx = max(abs(gridOriginDx.w), 1.0e-6);
    float particleMass = max(material.y, 0.0);
    if (particleMass == 0.0) return;

    Particle particle = particleBuffer[particleIndex];
    float3 gridPosition = (particle.position.xyz - gridOriginDx.xyz) / dx;
    int3 base = int3(floor(gridPosition - float3(0.5,0.5,0.5)));
    float3 fractional = gridPosition - float3(base);
    float3 wx = quadraticWeights(fractional.x);
    float3 wy = quadraticWeights(fractional.y);
    float3 wz = quadraticWeights(fractional.z);
    float3x3 C = Mat3Cols(particle.C0.xyz, particle.C1.xyz, particle.C2.xyz);
    int3 gridSize = int3(gridSizeAndCount.xyz);

    for (int z = 0; z < 3; ++z) {
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 3; ++x) {
                int3 offset = int3(x, y, z);
                int3 node = base + offset;
                if (!insideGrid(node, gridSize)) continue;
                float weight = wx[x] * wy[y] * wz[z];
                if (weight <= 0.0) continue;

                float3 dpos = (float3(offset) - fractional) * dx;
                float3 velocity = particle.velocity.xyz + mul(C, dpos);
                float3 momentum = weight * particleMass * velocity;
                uint index = gridIndex(node);
                InterlockedAdd(gridBuffer[index].vx, encodeFixedSigned(momentum.x));
                InterlockedAdd(gridBuffer[index].vy, encodeFixedSigned(momentum.y));
                InterlockedAdd(gridBuffer[index].vz, encodeFixedSigned(momentum.z));
                InterlockedAdd(gridBuffer[index].mass, encodeFixedPositive(weight * particleMass));
            }
        }
    }

}

