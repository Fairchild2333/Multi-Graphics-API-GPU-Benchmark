struct Particle {
    float4 position;
    float4 velocity;
};

cbuffer ComputeParams : register(b0) {
    float deltaTime;
    float bounds;
#ifdef DX11_CHUNKED_DISPATCH
    uint particleOffset;
    uint particleCount;
#endif
};

RWStructuredBuffer<Particle> particles : register(u0);

[numthreads(256, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID) {
    uint idx = DTid.x;
#ifdef DX11_CHUNKED_DISPATCH
    idx += particleOffset;
    if (idx >= particleCount) return;
#endif

    particles[idx].position.xyz += particles[idx].velocity.xyz * deltaTime;

    if (particles[idx].position.x > bounds) {
        particles[idx].position.x = -bounds;
    }
}
