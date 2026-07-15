#version 450

// Cinematic Fluid v1: a deterministic 2.5D presentation of the projected
// velocity field. Two transported pigments become emissive cyan and magenta/
// gold ribbons; density gradients provide a pseudo surface normal. Sampling
// and glow tap counts are fixed so the render cost remains benchmarkable.

layout(location = 0) in  vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0, std430) readonly buffer FluidState {
    vec4 cells[];   // xy = velocity, z/w = independent pigments
} state;

layout(push_constant) uniform FluidRenderParams {
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
    vec4 a = mix(state.cells[cellIndex(lo)],
                 state.cells[cellIndex(ivec2(hi.x, lo.y))], f.x);
    vec4 b = mix(state.cells[cellIndex(ivec2(lo.x, hi.y))],
                 state.cells[cellIndex(hi)], f.x);
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
    // A subtly moving wide-angle crop makes the square simulation read like a
    // deep horizontal chamber, without copying the familiar torus/donut scene.
    vec2 screen = inUV * 2.0 - 1.0;
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

    // Fixed eight-direction bloom. This is deliberately explicit rather than
    // a post-process texture so the captured frame shows one self-contained
    // render pass and its SSBO neighbourhood traffic.
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
    // Suppress the low-density numerical haze left by long semi-Lagrangian
    // runs, then build opacity from the actual transported pigments.
    float visible = smoothstep(0.10, 0.30, density);
    float body = visible * (0.32 + 0.68 * (1.0 - exp(-density * 1.20)));

    vec3 lightDir = normalize(vec3(-0.42, 0.58, 0.70));
    float diffuse = 0.34 + 0.66 * max(dot(normal, lightDir), 0.0);
    float rim = pow(1.0 - max(normal.z, 0.0), 2.4);
    float collision = 4.0 * min(dye.x, dye.y) / max(density, 1e-4);
    float gradientEdge = clamp(length(vec2(hL - hR, hD - hU)) * 3.0, 0.0, 1.0);

    // Neighbour velocity derivatives reveal real shear/curl in the projected
    // field. A tiny flow-warped ripple only breaks quantisation; it no longer
    // draws density-contour bands over the simulation.
    float curl = abs((sampleR.y - sampleL.y) - (sampleU.x - sampleD.x)) *
                 (0.5 * float(rp.gridSize));
    float strain = length(vec2(sampleR.x - sampleL.x,
                               sampleU.y - sampleD.y)) *
                   (0.5 * float(rp.gridSize));
    float shearRidge = smoothstep(0.10, 0.85, curl + strain);
    float microRipple = 0.5 + 0.5 * sin(dot(uv + fluid.xy * 0.04,
                                            vec2(71.0, -59.0)) + rp.time * 0.25);
    float filaments = 0.84 + 0.13 * shearRidge + 0.03 * microRipple;

    // Deep-space chamber background with sparse deterministic dust. It stays
    // inexpensive relative to the fluid sampling but gives bright ribbons a
    // readable silhouette at the edges of the viewport.
    vec3 background = mix(vec3(0.0003, 0.0008, 0.0030),
                          vec3(0.0010, 0.0030, 0.0100),
                          0.5 + 0.5 * screen.y);
    vec2 starGrid = inUV * vec2(320.0, 180.0);
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

    // Gentle optical framing: anamorphic edge falloff and letterbox bars.
    float vignette = clamp(1.0 - 0.34 * dot(screen * vec2(0.72, 1.0),
                                           screen * vec2(0.72, 1.0)), 0.30, 1.0);
    color *= vignette;
    float bars = 1.0 - smoothstep(0.93, 1.0, abs(screen.y));
    color *= bars;

    // Filmic exponential tone map followed by a mild display gamma. Exposure
    // is versioned in the C++ constants rather than inferred from frame data.
    color = vec3(1.0) - exp(-max(color, vec3(0.0)) * rp.exposure);
    color = pow(color, vec3(1.0 / 2.2));
    outColor = vec4(color, 1.0);
}
