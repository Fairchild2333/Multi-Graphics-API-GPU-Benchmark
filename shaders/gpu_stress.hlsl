// GPU Stress v1: DX12 SM5.1 / DX11 SM5.0 fullscreen GraphicsBurn.
// R/G carry an exact uint checksum signal; B consumes the FP32/SFU recurrence.
// The signal prevents dead-code elimination but is not read back in v1.

cbuffer GpuStressV1Params : register(b0) {
    float passIndex;
    float loadScale;
    uint  maxIter;
    uint  version;
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
    uint2 pixel = (uint2)input.pos.xy;
    uint y = min(pixel.y, 719u);
    y = min(y, 719u - y);
    uint drawIndex = (uint)passIndex;
    uint checksum = 0xA341316Cu
                  ^ (pixel.x * 0x9E3779B9u)
                  ^ (y * 0x85EBCA6Bu)
                  ^ (drawIndex * 0xC2B2AE35u)
                  ^ (version * 0x27D4EB2Fu);

    float seed = float(checksum & 0xFFFFu) * (1.0 / 65535.0);
    float4 a = frac(float4(seed, input.uv.x, input.uv.y, float(drawIndex + 1u) * 0.173)
                  + float4(0.11, 0.37, 0.61, 0.89));
    float4 b = frac(a.wxyz * float4(1.17, 1.31, 1.47, 1.73)
                  + float4(0.07, 0.19, 0.43, 0.67));
    float energy = 0.0;

    [loop]
    for (uint i = 0u; i < maxIter; ++i) {
        checksum ^= i + 0x9E3779B9u + (checksum << 6u) + (checksum >> 2u);
        checksum = checksum * 1664525u + 1013904223u;
        float jitter = float(checksum & 1023u) * (1.0 / 1024.0);

        a = frac(abs(a * float4(1.6181, 1.4142, 1.7321, 1.3247)
                   + b.yzwx * (0.731 + loadScale * 0.001)
                   + float4(0.103, 0.217, 0.331, 0.449)
                   + jitter * 0.0001));
        b = frac(abs(b * float4(1.2207, 1.3763, 1.5331, 1.6931)
                   + a.wxyz * 0.677
                   + float4(0.059, 0.181, 0.307, 0.479)));
        a.xy = sin((a.xy + b.zw) * 6.2831853) * 0.5 + 0.5;
        energy += dot(a, b);
    }

    float2 checksumRG = float2(float(checksum & 255u),
                               float((checksum >> 8u) & 255u)) * (1.0 / 255.0);
    float fpSignal = frac(energy * 0.00390625 + dot(a, b));
    return float4(checksumRG, fpSignal, 1.0);
}
