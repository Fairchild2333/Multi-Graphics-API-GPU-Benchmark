// Auto-ported from gpu_burn.frag for Metal GPU Burn (same visual/sample contract).
// Do not early-exit the fixed maxIter loop. Host passes GpuBurnParams (16 bytes).

#include <metal_stdlib>
using namespace metal;

struct GpuBurnParams {
    float time;
    float passIndex;
    uint  maxIter;
    uint  version;
};

struct BurnVtxOut {
    float4 position [[position]];
    float2 uv;
};

vertex BurnVtxOut gpuBurnVertex(uint vid [[vertex_id]]) {
    BurnVtxOut out;
    float2 uv = float2((vid << 1) & 2, vid & 2);
    out.uv = uv;
    // Metal NDC Y points up; flip to match Vulkan/GLSL convention.
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y;
    out.position = float4(ndc, 0.0, 1.0);
    return out;
}


// GPU Burn v1 revision 2 - Plasma Bloom x Mangekyo Kaleidoscope.
// No textures or external assets. Every fragment executes exactly maxIter
// raymarch samples; there is no early break. A register-resident FP32/uint
// recurrence is embedded in every sample and affects the ray step/glow/output,
// so it cannot be removed as dead code.

// ABI-compatible location/size with gpu_stress.frag, but with product-specific
// field semantics. The host must provide exactly 16 bytes.
constant float kPi       = 3.141592653589793;
constant float kTau      = 6.283185307179586;
constant float kFar      = 6.0;
constant float kHitEps   = 0.006;

float2 rotate2(float2 p, float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return float2(c * p.x - s * p.y, s * p.x + c * p.y);
}

float3 kaleidoscopeBackground(float2 uv, float phase, float energySignal) {
    float2 p = uv * 0.86;
    float radius = length(p);
    float angle = atan2(p.y, p.x);

    // Soft woven violet substrate. The interference is deliberately broad so
    // it reads as a background material rather than another foreground object.
    float weaveA = 0.5 + 0.5 * sin(radius * 8.2
                 + sin(angle * 12.0 - phase * 0.055) * 1.35);
    float weaveB = 0.5 + 0.5 * sin(radius * 13.4
                 - sin(angle * 8.0 + phase * 0.043) * 1.15);
    float smoke = 0.5 + 0.5 * sin(radius * 4.6
                + sin(angle * 6.0 - radius * 2.2 - phase * 0.031));

    // Central Mangekyo spiral. Thin spokes fade into the larger woven field.
    float spiralPhase = angle * 20.0 + radius * 12.5 - phase * 0.23;
    float spiral = pow(1.0 - abs(sin(spiralPhase)), 8.0)
                 * exp(-radius * 1.28);
    float spiralSoft = pow(1.0 - abs(sin(angle * 10.0
                            - radius * 7.2 + phase * 0.12)), 3.0)
                     * exp(-radius * 0.90);

    // Three broad cable families reproduce the cyan/magenta/gold woven loops
    // from the selected v2 background without pretending they are 3D geometry.
    float cyanField = sin(angle * 6.0 - radius * 3.05
                    + sin(radius * 4.1 - phase * 0.032) * 0.72
                    + phase * 0.070);
    float cyanCable = 1.0 - smoothstep(0.055, 0.155, abs(cyanField));
    float magentaField = sin(angle * 5.0 + radius * 2.45
                       + sin(radius * 3.25 + phase * 0.025) * 0.88
                       - phase * 0.058 + 0.65);
    float magentaCable = 1.0 - smoothstep(0.060, 0.175, abs(magentaField));
    // Concentric ring families. Pure radial phase (no angular perturbation)
    // keeps every circle perfectly round, and the higher frequencies fit
    // several rings inside the visible radius: the old radius * 3.85 left
    // room for barely one loop, which read as a lone halo instead of the
    // intended concentric-circle backdrop.
    float goldField = sin(radius * 7.6 - phase * 0.033);
    float goldCable = 1.0 - smoothstep(0.035, 0.115, abs(goldField));
    float fineRing = 1.0 - smoothstep(0.020, 0.070,
        abs(sin(radius * 15.4 - phase * 0.041)));

    float vignette = clamp(1.0 - dot(uv * 0.29, uv * 0.29), 0.0, 1.0);
    float3 background = float3(0.003, 0.0015, 0.012)
                    + float3(0.095, 0.020, 0.145) * weaveA * 0.52
                    + float3(0.030, 0.020, 0.120) * weaveB * 0.44
                    + float3(0.090, 0.014, 0.110) * smoke * 0.36;
    background += float3(0.055, 0.44, 0.88) * cyanCable * 0.28;
    background += float3(0.72, 0.045, 0.50) * magentaCable * 0.20;
    background += float3(0.78, 0.30, 0.060) * goldCable * 0.13;
    background += float3(0.15, 0.28, 0.72) * fineRing * 0.14;
    background += float3(0.08, 0.52, 1.05) * spiral * 0.38;
    background += float3(0.86, 0.06, 0.56) * spiralSoft * 0.24;
    background *= 0.46 + vignette * 0.54;
    background *= 0.92 + energySignal * 0.06;
    return background;
}

// A simply connected radial field: seven broad petals and crossed crystal
// crowns deform a solid core, while angular combs add cracks, fibres and spikes.
// The centre always remains solid, so the silhouette can never become a ring.
float plasmaField(float3 p, float phase, thread float& structureOut) {
    p.xz = rotate2(p.xz, phase * 0.61);
    p.xy = rotate2(p.xy, phase * 0.37 + 0.43);
    p.yz = rotate2(p.yz, phase * 0.23 - 0.28);

    float radiusFromCentre = length(p);
    float3 direction = p / max(radiusFromCentre, 1e-5);
    float azimuth = atan2(p.z, p.x);
    float elevation = atan2(p.y, length(p.xz));

    float petalWave = 0.5 + 0.5 * cos(azimuth * 7.0
                                    + sin(elevation * 3.0) * 1.8);
    float crownWave = 0.5 + 0.5 * cos(elevation * 6.0
                                    - azimuth * 2.0
                                    + sin(azimuth * 3.0) * 0.65);
    float bloom = pow(clamp(petalWave * 0.62 + crownWave * 0.38,
                            0.0, 1.0), 1.65);

    float facet = clamp((abs(direction.x) + abs(direction.y)
                       + abs(direction.z) - 1.0) * 1.25, 0.0, 1.0);
    facet *= facet;

    float spike = 1.0 - abs(sin(azimuth * 11.0 - elevation * 9.0
                              + sin(azimuth * 4.0) * 1.4));
    spike *= spike;
    spike *= spike;
    spike *= 0.35 + bloom * 0.65;

    float crack = 1.0 - abs(sin(azimuth * 41.0 + elevation * 59.0
                              + sin((azimuth - elevation) * 7.0) * 2.1));
    crack *= crack;
    crack *= crack;

    float fibre = 1.0 - abs(sin(azimuth * 67.0 - elevation * 37.0
                              + sin(elevation * 13.0) * 1.6));
    fibre *= fibre;
    fibre *= fibre;

    structureOut = clamp(crack * 0.62 + fibre * 0.48 + spike * 0.60,
                         0.0, 1.0);
    float crystalRadius = 0.67 + bloom * 0.25 + facet * 0.08
                        + spike * 0.15 + crack * 0.028 + fibre * 0.020;
    return radiusFromCentre - crystalRadius;
}

float3 plasmaNormal(float3 p, float phase) {
    const float e = 0.003;
    float scratch;
    float xp = plasmaField(p + float3(e, 0.0, 0.0), phase, scratch);
    float xn = plasmaField(p - float3(e, 0.0, 0.0), phase, scratch);
    float yp = plasmaField(p + float3(0.0, e, 0.0), phase, scratch);
    float yn = plasmaField(p - float3(0.0, e, 0.0), phase, scratch);
    float zp = plasmaField(p + float3(0.0, 0.0, e), phase, scratch);
    float zn = plasmaField(p - float3(0.0, 0.0, e), phase, scratch);
    return normalize(float3(xp - xn, yp - yn, zp - zn) + float3(1e-7, 0.0, 0.0));
}



fragment float4 gpuBurnFragment(BurnVtxOut in [[stage_in]],
                                constant GpuBurnParams& params [[buffer(0)]]) {
    float2 inUV = in.uv;
    float2 fragCoord = in.position.xy;

    float2 uv = inUV * 2.0 - 1.0;
    uv.x *= 16.0 / 9.0;

    // Close camera keeps the solid bloom large and visible at the 16-step floor.
    float3 rayOrigin = float3(0.0, 0.0, 2.35);
    float3 rayDir = normalize(float3(uv, -1.75));
    float phase = params.time;

    uint2 pixel = uint2(fragCoord);
    uint y = min(pixel.y, 719u);
    y = min(y, 719u - y);
    uint drawIndex = uint(params.passIndex + 0.5);
    uint checksum = 0xB5297A4Du
                  ^ (pixel.x * 0x9E3779B9u)
                  ^ (y * 0x85EBCA6Bu)
                  ^ (drawIndex * 0xC2B2AE35u)
                  ^ (params.version * 0x27D4EB2Fu);

    float4 core = fract(float4(uv, params.time * 0.071,
                           float(drawIndex + 1u) * 0.173)
                    + float4(0.13, 0.31, 0.57, 0.83));
    float coreEnergy = 0.0;

    // Analytic entry into a conservative sphere around the crystal. This is not
    // a scored-loop early exit: every pixel still executes all maxIter samples,
    // but rays aimed at the object do not waste the 16-step default in empty air.
    const float boundRadius = 1.32;
    float sphereB = dot(rayOrigin, rayDir);
    float sphereC = dot(rayOrigin, rayOrigin) - boundRadius * boundRadius;
    float sphereDisc = sphereB * sphereB - sphereC;
    float sphereEntry = max(-sphereB - sqrt(max(sphereDisc, 0.0)), 0.0);
    float t = mix(0.0, sphereEntry, step(0.0, sphereDisc));
    float hit = 0.0;
    float hitT = kFar;
    float nearestSurface = 10.0;
    float glowAccum = 0.0;

    // Fixed-count loop by contract. Do not add break/return/discard here.
    for (uint i = 0u; i < params.maxIter; ++i) {
        float3 p = rayOrigin + rayDir * t;
        float structure;
        float d = plasmaField(p, phase, structure);
        float ad = abs(d);
        nearestSurface = min(nearestSurface, ad);

        checksum ^= i + 0x9E3779B9u + (checksum << 6u) + (checksum >> 2u);
        checksum = checksum * 1664525u + 1013904223u;
        float jitter = float(checksum & 1023u) * (1.0 / 1024.0);

        core = fract(abs(core * float4(1.6181, 1.4142, 1.7321, 1.3247)
                       + core.yzwx * 0.719
                       + float4(ad, structure, jitter, t * 0.071)
                       + float4(0.103, 0.217, 0.331, 0.449)));
        core.xy = sin((core.xy + core.zw) * kTau) * 0.5 + 0.5;
        float coreStep = dot(core, float4(0.17, 0.23, 0.29, 0.31));
        coreEnergy += dot(core, core.wzyx);

        glowAccum += exp(-ad * 12.0) * (0.90 + coreStep * 0.18)
                   / (1.0 + t * t * 0.18);

        float withinFar = step(t, kFar);
        float newHit = (1.0 - hit) * (1.0 - step(kHitEps, ad)) * withinFar;
        hitT = mix(hitT, t, newHit);
        hit = max(hit, newHit);

        float marchWeight = (1.0 - hit) * withinFar;
        float stepLength = clamp(ad * (0.58 + coreStep * 0.055), 0.004, 0.28);
        t += marchWeight * stepLength;
    }

    float3 hitPos = rayOrigin + rayDir * hitT;
    float surfaceStructure;
    plasmaField(hitPos, phase, surfaceStructure);
    float3 normal = plasmaNormal(hitPos, phase);

    float3 lightDir = normalize(float3(-0.48, 0.72, 0.50));
    float3 viewDir = -rayDir;
    float diffuse = max(dot(normal, lightDir), 0.0);
    float facing = max(dot(normal, viewDir), 0.0);
    float rim = pow(1.0 - facing, 3.0);
    float3 halfVector = normalize(lightDir + viewDir);
    float specular = pow(max(dot(normal, halfVector), 0.0), 56.0);

    float3 plasmaBase = mix(float3(0.006, 0.012, 0.095),
                          float3(0.32, 0.045, 2.8), surfaceStructure);
    plasmaBase = mix(plasmaBase, float3(0.12, 2.2, 6.8),
                     clamp(diffuse * diffuse
                         * (0.35 + surfaceStructure), 0.0, 1.0));

    float coreSignal = fract(coreEnergy * 0.00390625 + dot(core, core));
    float3 checksumTint = float3(float(checksum & 255u),
                             float((checksum >> 8u) & 255u),
                             float((checksum >> 16u) & 255u)) * (1.0 / 255.0);
    float hotVeins = pow(surfaceStructure, 2.4);
    // Brighter core with a violet-leaning grade so the crystal's light sits
    // in the same family as the magenta/violet kaleidoscope backdrop instead
    // of reading as an icy-blue foreign object.
    float3 surface = plasmaBase * (0.22 + diffuse * 1.38)
                 + float3(1.2, 1.1, 8.2) * rim
                 + float3(8.5, 9.2, 11.0) * specular
                 + float3(5.8, 7.4, 10.0) * hotVeins
                   * (0.25 + diffuse * 0.75);
    surface *= 0.91 + coreSignal * 0.18;
    surface += checksumTint * 0.018;
    surface *= float3(0.58, 0.52, 1.00) * 0.42;
    surface += float3(0.30, 0.018, 0.58) * hotVeins
             * (0.16 + rim * 0.24);

    float invSteps = 1.0 / float(max(params.maxIter, 1u));
    float glow = glowAccum * invSteps;
    // Wider halo falloff makes the crystal light visibly spill into the
    // backdrop instead of hugging the silhouette.
    float haloMask = max(hit, 1.0 - smoothstep(0.05, 1.05, nearestSurface));

    float3 background = kaleidoscopeBackground(uv, params.time, coreSignal);
    float3 bloom = float3(0.85, 0.45, 5.4) * glow * 1.85
               + float3(1.7, 0.08, 3.6) * glow * glow * 0.72
               + float3(0.30, 0.14, 1.05) * haloMask * haloMask * 0.34;

    // Two opaque draws form one observable image: pass 0 owns the full-screen
    // background/glow; pass 1 owns the plasma crystal and its nearby halo.
    float3 baseLayer = background + bloom * 0.50;
    float3 crystalLayer = mix(baseLayer, surface + bloom * 0.88
                          + background * 0.08, hit);
    float overlayPass = step(0.5, params.passIndex);
    float3 hdr = mix(baseLayer, crystalLayer, overlayPass);
    hdr = max(hdr, float3(0.0));
    // Filmic-exposure mapping restored from the original Plasma Bloom v1:
    // 1-exp(-hdr*k) drives highlights to white far faster than Reinhard
    // (hdr 5 -> 0.998 vs 0.83), which is where most of the remembered v1
    // brightness lived. v1 additionally double-gamma-encoded by mistake;
    // that bug stays fixed, the legitimate exposure curve returns.
    float3 mapped = float3(1.0) - exp(-max(hdr, float3(0.0)) * 1.25);

    // Deliberately after the complete fixed-count loop and all shading. The
    // discarded area reveals pass 0; no fragment can skip scored sample work.
    if (overlayPass > 0.5 && haloMask < 0.01)
        discard_fragment();

    return float4(mapped, 1.0);

}
