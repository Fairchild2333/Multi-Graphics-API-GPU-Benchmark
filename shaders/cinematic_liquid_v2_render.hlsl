#include "cinematic_liquid_v2_common.hlsli"

// ===== Temporary fullscreen present (full raymarch port pending) =====
struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
VSOut VSMain(uint vid : SV_VertexID) {
    VSOut o;
    float2 uv = float2((vid << 1) & 2, vid & 2);
    o.uv = uv;
    o.pos = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    return o;
}
float4 PSMain(VSOut i) : SV_TARGET {
    return float4(0.04, 0.20 + 0.25 * i.uv.y, 0.45, 1.0);
}

