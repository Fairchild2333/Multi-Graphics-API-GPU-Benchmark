#include <metal_stdlib>
using namespace metal;

struct Particle {
    float4 position;
    float4 velocity;
};

struct ComputeParams {
    float deltaTime;
    float bounds;
};

kernel void computeMain(device Particle* particles [[buffer(0)]],
                        constant ComputeParams& params [[buffer(1)]],
                        uint id [[thread_position_in_grid]]) {
    particles[id].position.xyz += particles[id].velocity.xyz * params.deltaTime;

    if (particles[id].position.x > params.bounds) {
        particles[id].position.x = -params.bounds;
    }
}

// All-pairs gravitational N-body kernel (compute-bound). Reuses the same
// particle buffer; tiles positions through threadgroup memory. See
// shaders/nbody.comp for notes.
struct NBodyParams {
    float deltaTime;
    float softening;
    uint  numBodies;
};

kernel void nbodyMain(device Particle* particles      [[buffer(0)]],
                      constant NBodyParams& params    [[buffer(1)]],
                      uint gid [[thread_position_in_grid]],
                      uint lid [[thread_position_in_threadgroup]]) {
    constexpr uint TILE = 256u;
    threadgroup float4 sharedPos[TILE];

    float3 myPos = particles[gid].position.xyz;
    float3 acc   = float3(0.0);
    float  soft2 = params.softening * params.softening;

    uint tiles = params.numBodies / TILE;
    for (uint t = 0u; t < tiles; ++t) {
        sharedPos[lid] = particles[t * TILE + lid].position;
        threadgroup_barrier(mem_flags::mem_threadgroup);

        for (uint j = 0u; j < TILE; ++j) {
            float3 d        = sharedPos[j].xyz - myPos;
            float  distSq   = dot(d, d) + soft2;
            float  invDist  = rsqrt(distSq);
            float  invDist3 = invDist * invDist * invDist;
            acc += d * invDist3;
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }

    float3 vel = particles[gid].velocity.xyz + acc * params.deltaTime;
    float3 pos = myPos + vel * params.deltaTime;
    particles[gid].velocity.xyz = vel;
    particles[gid].position.xyz = pos;
}

struct VertexOut {
    float4 position [[position]];
    float3 color;
    float  pointSize [[point_size]];
};

vertex VertexOut vertexMain(const device Particle* particles [[buffer(0)]],
                            uint vid [[vertex_id]]) {
    VertexOut out;
    out.position  = float4(particles[vid].position.xy, 0.0, 1.0);
    out.pointSize = 2.0;

    float speed = length(particles[vid].velocity.xy);
    out.color = mix(float3(0.1, 0.4, 1.0),
                    float3(1.0, 0.3, 0.1),
                    clamp(speed * 5.0, 0.0, 1.0));
    return out;
}

fragment float4 fragmentMain(VertexOut in [[stage_in]]) {
    return float4(in.color, 1.0);
}

// ---- Fractal stress test (fullscreen triangle + heavy fragment) ----
struct FractalParams {
    float time;
    float zoom;
    uint  maxIter;
};

struct FractalVtxOut {
    float4 position [[position]];
    float2 uv;
};

vertex FractalVtxOut fractalVertex(uint vid [[vertex_id]]) {
    FractalVtxOut out;
    float2 uv = float2((vid << 1) & 2, vid & 2);
    out.uv = uv;
    out.position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    return out;
}

fragment float4 fractalFragment(FractalVtxOut in [[stage_in]],
                                constant FractalParams& params [[buffer(0)]]) {
    float2 c = (in.uv - 0.5) * (3.0 / max(params.zoom, 0.0001));
    float2 z = c;
    float  acc = 0.0;

    for (uint k = 0u; k < params.maxIter; ++k) {
        z = float2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
        z = sin(z);
        acc += dot(z, z);
    }

    float v = fract(acc * 0.05 + params.time * 0.1);
    float3 col = 0.5 + 0.5 * cos(6.28318 * (v + float3(0.0, 0.33, 0.67)));
    return float4(col, 1.0);
}

// ---- SynthPeak: register-bound FMA loops (Apple GPUs have no FP64) ----
struct PeakParams { uint iters; float mul; float add; };

kernel void synthFp32(device float* outBuf       [[buffer(0)]],
                      constant PeakParams& p      [[buffer(1)]],
                      uint gid [[thread_position_in_grid]]) {
    float s = float(gid & 1023u) * 0.001 + 0.5;
    float a0=s,a1=s+0.1,a2=s+0.2,a3=s+0.3,a4=s+0.4,a5=s+0.5,a6=s+0.6,a7=s+0.7;
    float m=p.mul, c=p.add;
    for (uint i=0u;i<p.iters;++i){ a0=a0*m+c;a1=a1*m+c;a2=a2*m+c;a3=a3*m+c;a4=a4*m+c;a5=a5*m+c;a6=a6*m+c;a7=a7*m+c; }
    outBuf[gid] = a0+a1+a2+a3+a4+a5+a6+a7;
}

kernel void synthFp16(device float* outBuf       [[buffer(0)]],
                      constant PeakParams& p      [[buffer(1)]],
                      uint gid [[thread_position_in_grid]]) {
    half s = half(float(gid & 1023u) * 0.001 + 0.5);
    half2 a0=half2(s,s+0.1h),a1=half2(s+0.2h,s+0.3h),a2=half2(s+0.4h,s+0.5h),a3=half2(s+0.6h,s+0.7h);
    half2 a4=half2(s+0.8h,s+0.9h),a5=half2(s+1.0h,s+1.1h),a6=half2(s+1.2h,s+1.3h),a7=half2(s+1.4h,s+1.5h);
    half2 m=half2(half(p.mul)), c=half2(half(p.add));
    for (uint i=0u;i<p.iters;++i){ a0=a0*m+c;a1=a1*m+c;a2=a2*m+c;a3=a3*m+c;a4=a4*m+c;a5=a5*m+c;a6=a6*m+c;a7=a7*m+c; }
    half2 sum=a0+a1+a2+a3+a4+a5+a6+a7;
    outBuf[gid] = float(sum.x+sum.y);
}

kernel void synthInt32(device float* outBuf      [[buffer(0)]],
                       constant PeakParams& p     [[buffer(1)]],
                       uint gid [[thread_position_in_grid]]) {
    int s = int(gid & 1023u) + 1;
    int a0=s,a1=s+1,a2=s+2,a3=s+3,a4=s+4,a5=s+5,a6=s+6,a7=s+7;
    int m=int(p.mul*1000.0)+3, c=int(p.add*1000.0)+1;
    for (uint i=0u;i<p.iters;++i){ a0=a0*m+c;a1=a1*m+c;a2=a2*m+c;a3=a3*m+c;a4=a4*m+c;a5=a5*m+c;a6=a6*m+c;a7=a7*m+c; }
    outBuf[gid] = float(a0+a1+a2+a3+a4+a5+a6+a7);
}

// ---- Render3D: instanced camera-facing billboards with depth ----
struct R3DParams {
    float4x4 viewProj;
    float4   camRight;
    float4   camUp;
    float    pointSize;
};
struct R3DVtxOut {
    float4 position [[position]];
    float2 corner;
    float3 color;
};
vertex R3DVtxOut render3dVertex(uint vid [[vertex_id]],
                                uint iid [[instance_id]],
                                const device Particle* particles [[buffer(1)]],
                                constant R3DParams& p           [[buffer(2)]]) {
    const float2 corners[6] = { float2(-1,-1), float2(1,-1), float2(1,1),
                                float2(-1,-1), float2(1,1),  float2(-1,1) };
    float2 c = corners[vid];
    float3 world = particles[iid].position.xyz
                 + p.camRight.xyz * (c.x * p.pointSize)
                 + p.camUp.xyz    * (c.y * p.pointSize);
    R3DVtxOut o;
    o.position = p.viewProj * float4(world, 1.0);
    o.corner   = c;
    float speed = length(particles[iid].velocity.xyz);
    o.color = mix(float3(0.1,0.4,1.0), float3(1.0,0.3,0.1), clamp(speed*5.0, 0.0, 1.0));
    return o;
}
fragment float4 render3dFragment(R3DVtxOut in [[stage_in]]) {
    float r = length(in.corner);
    if (r > 1.0) discard_fragment();
    return float4(in.color, smoothstep(1.0, 0.0, r));
}
