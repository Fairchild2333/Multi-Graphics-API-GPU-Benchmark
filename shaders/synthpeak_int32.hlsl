// Synthetic peak INT32 (DX12/DX11). Integer multiply-add (IMAD).
struct Particle { float4 position; float4 velocity; };
RWStructuredBuffer<Particle> particles : register(u0);
cbuffer PeakParams : register(b0) { uint iters; float mul; float add; };
[numthreads(256,1,1)]
void CSMain(uint3 dt : SV_DispatchThreadID) {
    uint gid = dt.x;
    int s = int(gid & 1023u) + 1;
    int a0=s, a1=s+1, a2=s+2, a3=s+3, a4=s+4, a5=s+5, a6=s+6, a7=s+7;
    int m = int(mul*1000.0)+3, c = int(add*1000.0)+1;
    [loop] for (uint i = 0u; i < iters; ++i) {
        a0=a0*m+c; a1=a1*m+c; a2=a2*m+c; a3=a3*m+c;
        a4=a4*m+c; a5=a5*m+c; a6=a6*m+c; a7=a7*m+c;
    }
    particles[gid].position.x = float(a0+a1+a2+a3+a4+a5+a6+a7);
}
