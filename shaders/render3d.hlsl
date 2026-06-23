// True-3D instanced billboard (DX12 5_1 / DX11 5_0). Slot 0 = quad corner
// (per-vertex), slot 1 = particle pos/vel (per-instance). Column-major matrix.
cbuffer R3D : register(b0) {
    float4x4 viewProj;
    float4   camRight;
    float4   camUp;
    float    pointSize;
};
struct VSIn {
    float2 corner   : CORNER;
    float4 position : POSITION;
    float4 velocity : VELOCITY;
};
struct VSOut {
    float4 pos    : SV_POSITION;
    float2 corner : TEXCOORD0;
    float3 color  : COLOR;
};
VSOut VSMain(VSIn i) {
    float3 world = i.position.xyz
                 + camRight.xyz * (i.corner.x * pointSize)
                 + camUp.xyz    * (i.corner.y * pointSize);
    VSOut o;
    o.pos    = mul(viewProj, float4(world, 1.0));
    o.corner = i.corner;
    float speed = length(i.velocity.xyz);
    o.color = lerp(float3(0.1,0.4,1.0), float3(1.0,0.3,0.1), saturate(speed*5.0));
    return o;
}
float4 PSMain(VSOut i) : SV_TARGET {
    float r = length(i.corner);
    if (r > 1.0) discard;
    return float4(i.color, smoothstep(1.0, 0.0, r));
}
