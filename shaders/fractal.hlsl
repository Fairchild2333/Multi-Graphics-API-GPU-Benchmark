// Fractal stress-test shaders (DX12 5_1 / DX11 5_0). Fullscreen triangle VS
// (no vertex buffer) + heavy fixed-iteration PS. See shaders/fractal.frag.

cbuffer FractalParams : register(b0) {
    float time;
    float zoom;
    uint  maxIter;
};

struct VSOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOut VSMain(uint vid : SV_VertexID) {
    VSOut o;
    float2 uv = float2((vid << 1) & 2, vid & 2);
    o.uv  = uv;
    o.pos = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    return o;
}

float4 PSMain(VSOut input) : SV_TARGET {
    float2 c = (input.uv - 0.5) * (3.0 / max(zoom, 0.0001));
    float2 z = c;
    float  acc = 0.0;

    [loop]
    for (uint k = 0u; k < maxIter; ++k) {
        z = float2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
        z = sin(z);
        acc += dot(z, z);
    }

    float v = frac(acc * 0.05 + time * 0.1);
    float3 col = 0.5 + 0.5 * cos(6.28318 * (v + float3(0.0, 0.33, 0.67)));
    return float4(col, 1.0);
}
