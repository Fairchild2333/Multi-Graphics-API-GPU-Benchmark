#version 430

// GPU Stress v1: OpenGL 4.3 counterpart of gpu_stress.frag.

in  vec2 outUV;
out vec4 outColor;

layout(binding = 1, std140) uniform GpuStressV1Params {
    float passIndex;
    float loadScale;
    uint  maxIter;
    uint  version;
};

void main() {
    uvec2 pixel = uvec2(gl_FragCoord.xy);
    uint y = min(pixel.y, 719u);
    y = min(y, 719u - y);
    uint drawIndex = uint(passIndex);
    uint checksum = 0xA341316Cu
                  ^ (pixel.x * 0x9E3779B9u)
                  ^ (y * 0x85EBCA6Bu)
                  ^ (drawIndex * 0xC2B2AE35u)
                  ^ (version * 0x27D4EB2Fu);

    float seed = float(checksum & 0xFFFFu) * (1.0 / 65535.0);
    vec4 a = fract(vec4(seed, outUV.x, outUV.y, float(drawIndex + 1u) * 0.173)
                 + vec4(0.11, 0.37, 0.61, 0.89));
    vec4 b = fract(a.wxyz * vec4(1.17, 1.31, 1.47, 1.73)
                 + vec4(0.07, 0.19, 0.43, 0.67));
    float energy = 0.0;

    for (uint i = 0u; i < maxIter; ++i) {
        checksum ^= i + 0x9E3779B9u + (checksum << 6u) + (checksum >> 2u);
        checksum = checksum * 1664525u + 1013904223u;
        float jitter = float(checksum & 1023u) * (1.0 / 1024.0);

        a = fract(abs(a * vec4(1.6181, 1.4142, 1.7321, 1.3247)
                    + b.yzwx * (0.731 + loadScale * 0.001)
                    + vec4(0.103, 0.217, 0.331, 0.449)
                    + jitter * 0.0001));
        b = fract(abs(b * vec4(1.2207, 1.3763, 1.5331, 1.6931)
                    + a.wxyz * 0.677
                    + vec4(0.059, 0.181, 0.307, 0.479)));
        a.xy = sin((a.xy + b.zw) * 6.2831853) * 0.5 + 0.5;
        energy += dot(a, b);
    }

    vec2 checksumRG = vec2(float(checksum & 255u),
                           float((checksum >> 8u) & 255u)) * (1.0 / 255.0);
    float fpSignal = fract(energy * 0.00390625 + dot(a, b));
    outColor = vec4(checksumRG, fpSignal, 1.0);
}
