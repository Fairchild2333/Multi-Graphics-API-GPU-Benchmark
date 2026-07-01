// Volumetric raymarch shaders (DX12 5_1 / DX11 5_0). Fullscreen triangle VS
// (no vertex buffer) + heavy fixed-step-count PS. See shaders/volumetric.frag.

cbuffer VolumetricParams : register(b0) {
    float time;
    float stepSize;
    uint  steps;
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

float hash13(float3 p) {
    p = frac(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return frac((p.x + p.y) * p.z);
}

float noise3(float3 x) {
    float3 i = floor(x);
    float3 f = frac(x);
    f = f * f * (3.0 - 2.0 * f);

    float n000 = hash13(i + float3(0.0, 0.0, 0.0));
    float n100 = hash13(i + float3(1.0, 0.0, 0.0));
    float n010 = hash13(i + float3(0.0, 1.0, 0.0));
    float n110 = hash13(i + float3(1.0, 1.0, 0.0));
    float n001 = hash13(i + float3(0.0, 0.0, 1.0));
    float n101 = hash13(i + float3(1.0, 0.0, 1.0));
    float n011 = hash13(i + float3(0.0, 1.0, 1.0));
    float n111 = hash13(i + float3(1.0, 1.0, 1.0));

    float nx00 = lerp(n000, n100, f.x);
    float nx10 = lerp(n010, n110, f.x);
    float nx01 = lerp(n001, n101, f.x);
    float nx11 = lerp(n011, n111, f.x);

    float nxy0 = lerp(nx00, nx10, f.y);
    float nxy1 = lerp(nx01, nx11, f.y);

    return lerp(nxy0, nxy1, f.z) * 2.0 - 1.0;
}

float fbm(float3 p) {
    float a = 0.5;
    float s = 0.0;
    [unroll]
    for (int i = 0; i < 5; ++i) {
        s += a * noise3(p);
        p = p * 2.02 + float3(1.7, 9.2, 3.3);
        a *= 0.5;
    }
    return s;
}

float4 PSMain(VSOut input) : SV_TARGET {
    float2 uv = (input.uv - 0.5) * 2.0;
    uv.x *= 1.7777;

    float ang = time * 0.15;
    float3 camPos   = float3(cos(ang) * 3.0, 1.5, sin(ang) * 3.0);
    float3 camFwd   = normalize(-camPos);
    float3 camRight = normalize(cross(camFwd, float3(0.0, 1.0, 0.0)));
    float3 camUp    = cross(camRight, camFwd);

    float3 ro = camPos;
    float3 rd = normalize(camFwd + camRight * uv.x + camUp * uv.y);

    float density = 0.0;
    float t = 0.0;
    [loop]
    for (uint i = 0u; i < steps; ++i) {
        float3 p = ro + rd * t;
        float w = fbm(p * 0.5 + float3(0.0, time * 0.05, 0.0));
        float d = fbm(p * 0.5 + w * 0.6);
        d = smoothstep(0.0, 0.6, d + 0.3);
        density += d;
        t += stepSize;
    }

    float trans = exp(-density * 0.06);
    float v = clamp(density / (float)steps * 4.0, 0.0, 1.0);
    float3 cold = float3(0.45, 0.62, 0.85);
    float3 hot  = float3(1.00, 0.55, 0.30);
    float3 col = lerp(cold, hot, v) * (1.0 - trans) + float3(0.05, 0.08, 0.13) * trans;
    return float4(col, 1.0);
}
