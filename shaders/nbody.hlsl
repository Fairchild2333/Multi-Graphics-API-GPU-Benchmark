// All-pairs gravitational N-body compute shader (DX12 cs_5_1 / DX11 cs_5_0).
// Compute-bound counterpart to compute.hlsl; see shaders/nbody.comp for notes.
// Reuses the same Particle structured buffer (2 x float4) — no extra buffers.

struct Particle {
    float4 position;
    float4 velocity;
};

cbuffer NBodyParams : register(b0) {
    float deltaTime;
    float softening;
    uint  numBodies;
};

RWStructuredBuffer<Particle> particles : register(u0);

#define TILE 256u
groupshared float3 sharedPos[TILE];

[numthreads(256, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID, uint GI : SV_GroupIndex) {
    uint  gid   = DTid.x;
    uint  lid   = GI;
    float3 myPos = particles[gid].position.xyz;
    float3 acc   = float3(0.0, 0.0, 0.0);
    float  soft2 = softening * softening;

    uint tiles = numBodies / TILE;
    for (uint t = 0u; t < tiles; ++t) {
        sharedPos[lid] = particles[t * TILE + lid].position.xyz;
        GroupMemoryBarrierWithGroupSync();

        [loop]
        for (uint j = 0u; j < TILE; ++j) {
            float3 d        = sharedPos[j] - myPos;
            float  distSq   = dot(d, d) + soft2;
            float  invDist  = rsqrt(distSq);
            float  invDist3 = invDist * invDist * invDist;
            acc += d * invDist3;
        }
        GroupMemoryBarrierWithGroupSync();
    }

    float3 vel = particles[gid].velocity.xyz + acc * deltaTime;
    float3 pos = myPos + vel * deltaTime;
    particles[gid].velocity.xyz = vel;
    particles[gid].position.xyz = pos;
}
