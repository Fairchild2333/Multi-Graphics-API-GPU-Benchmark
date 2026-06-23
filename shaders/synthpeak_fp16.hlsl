// Synthetic peak FP16 (DX12, SM 6.2 via DXC, -enable-16bit-types). Packed half2.
// Precompiled to a signed DXIL .cso at build time; needs Native16BitShaderOps.
struct Particle { float4 position; float4 velocity; };
RWStructuredBuffer<Particle> particles : register(u0);
cbuffer PeakParams : register(b0) { uint iters; float mul; float add; };
[numthreads(256,1,1)]
void CSMain(uint3 dt : SV_DispatchThreadID) {
    uint gid = dt.x;
    half s = (half)((float)(gid & 1023u) * 0.001 + 0.5);
    half2 a0=half2(s,s+(half)0.1), a1=half2(s+(half)0.2,s+(half)0.3);
    half2 a2=half2(s+(half)0.4,s+(half)0.5), a3=half2(s+(half)0.6,s+(half)0.7);
    half2 a4=half2(s+(half)0.8,s+(half)0.9), a5=half2(s+(half)1.0,s+(half)1.1);
    half2 a6=half2(s+(half)1.2,s+(half)1.3), a7=half2(s+(half)1.4,s+(half)1.5);
    half2 m=(half2)(half)mul, c=(half2)(half)add;
    [loop] for (uint i=0u;i<iters;++i) {
        a0=a0*m+c; a1=a1*m+c; a2=a2*m+c; a3=a3*m+c; a4=a4*m+c; a5=a5*m+c; a6=a6*m+c; a7=a7*m+c;
    }
    half2 sum = a0+a1+a2+a3+a4+a5+a6+a7;
    particles[gid].position.x = (float)(sum.x + sum.y);
}
