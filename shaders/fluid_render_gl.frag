#version 430

// Cinematic Fluid v1 render (OpenGL 4.3). See shaders/fluid_render.frag.

in  vec2 outUV;
out vec4 outColor;

layout(std430, binding = 0) buffer FluidState {
    vec4 cells[];
};

layout(binding = 1, std140) uniform FluidRenderParams {
    uint  gridSize;
    float time;
    float exposure;
    uint  version;
} rp;

uint cellIndex(ivec2 c) {
    ivec2 hi = ivec2(int(rp.gridSize) - 1);
    ivec2 cc = clamp(c, ivec2(0), hi);
    return uint(cc.y) * rp.gridSize + uint(cc.x);
}

vec4 sampleFluid(vec2 uv) {
    vec2 grid = clamp(uv, vec2(0.0), vec2(1.0)) * float(rp.gridSize - 1u);
    ivec2 lo = ivec2(floor(grid));
    ivec2 hi = lo + ivec2(1);
    vec2 f = fract(grid);
    vec4 a = mix(cells[cellIndex(lo)],
                 cells[cellIndex(ivec2(hi.x, lo.y))], f.x);
    vec4 b = mix(cells[cellIndex(ivec2(lo.x, hi.y))],
                 cells[cellIndex(hi)], f.x);
    return mix(a, b, f.y);
}

float densityAt(vec2 uv) {
    vec2 pigment = max(sampleFluid(uv).zw, vec2(0.0));
    return pigment.x + pigment.y;
}

float hash21(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main() {
    vec2 screen = outUV * 2.0 - 1.0;
    float cameraDrift = 0.010 * sin(rp.time * 0.17);
    vec2 uv = vec2(0.5 + screen.x * 0.54 + cameraDrift,
                   0.5 + screen.y * 0.47);

    vec4 fluid = sampleFluid(uv);
    vec2 dye = max(fluid.zw, vec2(0.0));
    float density = dye.x + dye.y;
    float mixAmount = dye.y / max(density, 1e-4);
    float speed = length(fluid.xy);

    vec2 texel = vec2(1.0 / float(rp.gridSize));
    vec4 sampleL = sampleFluid(uv - vec2(texel.x, 0.0));
    vec4 sampleR = sampleFluid(uv + vec2(texel.x, 0.0));
    vec4 sampleD = sampleFluid(uv - vec2(0.0, texel.y));
    vec4 sampleU = sampleFluid(uv + vec2(0.0, texel.y));
    float hL = max(sampleL.z, 0.0) + max(sampleL.w, 0.0);
    float hR = max(sampleR.z, 0.0) + max(sampleR.w, 0.0);
    float hD = max(sampleD.z, 0.0) + max(sampleD.w, 0.0);
    float hU = max(sampleU.z, 0.0) + max(sampleU.w, 0.0);
    vec3 normal = normalize(vec3((hL - hR) * 2.8,
                                 (hD - hU) * 2.8, 0.32));

    vec2 glowStep = texel * 3.5;
    float glow = 0.0;
    glow += densityAt(uv + glowStep * vec2( 1.0,  0.0));
    glow += densityAt(uv + glowStep * vec2(-1.0,  0.0));
    glow += densityAt(uv + glowStep * vec2( 0.0,  1.0));
    glow += densityAt(uv + glowStep * vec2( 0.0, -1.0));
    glow += densityAt(uv + glowStep * vec2( 0.707,  0.707));
    glow += densityAt(uv + glowStep * vec2(-0.707,  0.707));
    glow += densityAt(uv + glowStep * vec2( 0.707, -0.707));
    glow += densityAt(uv + glowStep * vec2(-0.707, -0.707));
    glow *= 0.125;

    vec3 cyan    = vec3(0.008, 0.30, 1.00);
    vec3 magenta = vec3(0.92, 0.012, 0.38);
    vec3 gold    = vec3(1.00, 0.27, 0.018);
    vec3 hue = mix(cyan, mix(magenta, gold, 0.18 + 0.30 * dye.x), mixAmount);
    float visible = smoothstep(0.10, 0.30, density);
    float body = visible * (0.32 + 0.68 * (1.0 - exp(-density * 1.20)));

    vec3 lightDir = normalize(vec3(-0.42, 0.58, 0.70));
    float diffuse = 0.34 + 0.66 * max(dot(normal, lightDir), 0.0);
    float rim = pow(1.0 - max(normal.z, 0.0), 2.4);
    float collision = 4.0 * min(dye.x, dye.y) / max(density, 1e-4);
    float gradientEdge = clamp(length(vec2(hL - hR, hD - hU)) * 3.0, 0.0, 1.0);

    float curl = abs((sampleR.y - sampleL.y) - (sampleU.x - sampleD.x)) *
                 (0.5 * float(rp.gridSize));
    float strain = length(vec2(sampleR.x - sampleL.x,
                               sampleU.y - sampleD.y)) *
                   (0.5 * float(rp.gridSize));
    float shearRidge = smoothstep(0.10, 0.85, curl + strain);
    float microRipple = 0.5 + 0.5 * sin(dot(uv + fluid.xy * 0.04,
                                            vec2(71.0, -59.0)) + rp.time * 0.25);
    float filaments = 0.84 + 0.13 * shearRidge + 0.03 * microRipple;

    vec3 background = mix(vec3(0.0003, 0.0008, 0.0030),
                          vec3(0.0010, 0.0030, 0.0100),
                          0.5 + 0.5 * screen.y);
    vec2 starGrid = outUV * vec2(320.0, 180.0);
    vec2 starCell = floor(starGrid);
    vec2 starLocal = fract(starGrid) - 0.5;
    float starSeed = hash21(starCell);
    float star = smoothstep(0.998, 1.0, starSeed) *
                 (1.0 - smoothstep(0.035, 0.18, length(starLocal))) *
                 (0.55 + 0.45 * sin(rp.time * 0.7 + starSeed * 31.0));
    background += star * vec3(0.22, 0.34, 0.58);

    vec3 color = background;
    float glowVisible = smoothstep(0.08, 0.24, glow);
    float glowBody = glowVisible * (1.0 - exp(-max(glow - 0.06, 0.0) * 0.50));
    color += hue * body * filaments * (0.30 + 0.62 * diffuse);
    color += mix(cyan, magenta, mixAmount) * glowBody * 0.12;
    color += vec3(1.00, 0.24, 0.035) * collision * body * 0.14;
    color += mix(vec3(0.04, 0.35, 1.0), vec3(1.0, 0.04, 0.30), mixAmount) *
             gradientEdge * body * 0.24;
    color += vec3(0.12, 0.28, 0.72) * rim * body * 0.15;
    color += hue * min(speed * 1.5, 1.0) * body * 0.08;

    float vignette = clamp(1.0 - 0.34 * dot(screen * vec2(0.72, 1.0),
                                           screen * vec2(0.72, 1.0)), 0.30, 1.0);
    color *= vignette;
    float bars = 1.0 - smoothstep(0.93, 1.0, abs(screen.y));
    color *= bars;

    color = vec3(1.0) - exp(-max(color, vec3(0.0)) * rp.exposure);
    color = pow(color, vec3(1.0 / 2.2));
    outColor = vec4(color, 1.0);
}
