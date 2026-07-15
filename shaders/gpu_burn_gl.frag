#version 430

// GPU Burn v1 - OpenGL 4.3 Plasma Bloom counterpart of gpu_burn.frag.
// Fixed-count energy-crystal raymarch; no textures and no loop early-exit.

in  vec2 outUV;
out vec4 outColor;

layout(binding = 1, std140) uniform GpuBurnV1Params {
    float time;
    float passIndex;
    uint  maxIter;
    uint  version;
};

const float kPi     = 3.141592653589793;
const float kFar    = 6.0;
const float kHitEps = 0.006;

vec2 rotate2(vec2 p, float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return vec2(c * p.x - s * p.y, s * p.x + c * p.y);
}

float plasmaField(vec3 p, float phase, out float structureOut) {
    p.xz = rotate2(p.xz, phase * 0.61);
    p.xy = rotate2(p.xy, phase * 0.37 + 0.43);
    p.yz = rotate2(p.yz, phase * 0.23 - 0.28);

    float radiusFromCentre = length(p);
    vec3 direction = p / max(radiusFromCentre, 1e-5);
    float azimuth = atan(p.z, p.x);
    float elevation = atan(p.y, length(p.xz));

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

vec3 plasmaNormal(vec3 p, float phase) {
    const float e = 0.003;
    float scratch;
    float xp = plasmaField(p + vec3(e, 0.0, 0.0), phase, scratch);
    float xn = plasmaField(p - vec3(e, 0.0, 0.0), phase, scratch);
    float yp = plasmaField(p + vec3(0.0, e, 0.0), phase, scratch);
    float yn = plasmaField(p - vec3(0.0, e, 0.0), phase, scratch);
    float zp = plasmaField(p + vec3(0.0, 0.0, e), phase, scratch);
    float zn = plasmaField(p - vec3(0.0, 0.0, e), phase, scratch);
    return normalize(vec3(xp - xn, yp - yn, zp - zn) + vec3(1e-7, 0.0, 0.0));
}

void main() {
    vec2 uv = outUV * 2.0 - 1.0;
    uv.x *= 16.0 / 9.0;

    vec3 rayOrigin = vec3(0.0, 0.0, 2.35);
    vec3 rayDir = normalize(vec3(uv, -1.75));
    float phase = time;

    uvec2 pixel = uvec2(gl_FragCoord.xy);
    uint y = min(pixel.y, 719u);
    y = min(y, 719u - y);
    uint drawIndex = uint(passIndex + 0.5);
    uint checksum = 0xB5297A4Du
                  ^ (pixel.x * 0x9E3779B9u)
                  ^ (y * 0x85EBCA6Bu)
                  ^ (drawIndex * 0xC2B2AE35u)
                  ^ (version * 0x27D4EB2Fu);

    vec4 core = fract(vec4(uv, time * 0.071,
                           float(drawIndex + 1u) * 0.173)
                    + vec4(0.13, 0.31, 0.57, 0.83));
    float coreEnergy = 0.0;

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

    // Fixed-count by score contract. Do not break/return/discard in this loop.
    for (uint i = 0u; i < maxIter; ++i) {
        vec3 p = rayOrigin + rayDir * t;
        float structure;
        float d = plasmaField(p, phase, structure);
        float ad = abs(d);
        nearestSurface = min(nearestSurface, ad);

        checksum ^= i + 0x9E3779B9u + (checksum << 6u) + (checksum >> 2u);
        checksum = checksum * 1664525u + 1013904223u;
        float jitter = float(checksum & 1023u) * (1.0 / 1024.0);

        core = fract(abs(core * vec4(1.6181, 1.4142, 1.7321, 1.3247)
                       + core.yzwx * 0.719
                       + vec4(ad, structure, jitter, t * 0.071)
                       + vec4(0.103, 0.217, 0.331, 0.449)));
        core.xy = sin((core.xy + core.zw) * (2.0 * kPi)) * 0.5 + 0.5;
        float coreStep = dot(core, vec4(0.17, 0.23, 0.29, 0.31));
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

    vec3 hitPos = rayOrigin + rayDir * hitT;
    float surfaceStructure;
    plasmaField(hitPos, phase, surfaceStructure);
    vec3 normal = plasmaNormal(hitPos, phase);

    vec3 lightDir = normalize(vec3(-0.48, 0.72, 0.50));
    vec3 viewDir = -rayDir;
    float diffuse = max(dot(normal, lightDir), 0.0);
    float facing = max(dot(normal, viewDir), 0.0);
    float rim = pow(1.0 - facing, 3.0);
    vec3 halfVector = normalize(lightDir + viewDir);
    float specular = pow(max(dot(normal, halfVector), 0.0), 56.0);

    vec3 plasmaBase = mix(vec3(0.006, 0.012, 0.095),
                          vec3(0.32, 0.045, 2.8), surfaceStructure);
    plasmaBase = mix(plasmaBase, vec3(0.12, 2.2, 6.8),
                     clamp(diffuse * diffuse
                         * (0.35 + surfaceStructure), 0.0, 1.0));

    float coreSignal = fract(coreEnergy * 0.00390625 + dot(core, core));
    vec3 checksumTint = vec3(float(checksum & 255u),
                             float((checksum >> 8u) & 255u),
                             float((checksum >> 16u) & 255u)) * (1.0 / 255.0);
    float hotVeins = pow(surfaceStructure, 2.4);
    vec3 surface = plasmaBase * (0.22 + diffuse * 1.38)
                 + vec3(0.18, 2.6, 8.4) * rim
                 + vec3(8.5, 9.2, 11.0) * specular
                 + vec3(5.8, 7.4, 10.0) * hotVeins
                   * (0.25 + diffuse * 0.75);
    surface *= 0.91 + coreSignal * 0.18;
    surface += checksumTint * 0.018;

    float invSteps = 1.0 / float(max(maxIter, 1u));
    float glow = glowAccum * invSteps;
    float haloMask = max(hit, 1.0 - smoothstep(0.025, 0.45, nearestSurface));

    float vignette = clamp(1.0 - dot(uv * 0.38, uv * 0.38), 0.0, 1.0);
    vec3 background = vec3(0.0004, 0.0008, 0.006)
                    + vec3(0.004, 0.002, 0.032) * vignette * vignette;
    background *= 0.96 + coreSignal * 0.08;
    vec3 bloom = vec3(0.035, 0.75, 5.8) * glow * 5.8
               + vec3(1.7, 0.08, 3.6) * glow * glow * 3.2
               + vec3(0.025, 0.16, 1.1) * haloMask * haloMask;

    vec3 baseLayer = background + bloom;
    vec3 crystalLayer = mix(baseLayer, surface + bloom, hit);
    float overlayPass = step(0.5, passIndex);
    vec3 hdr = mix(baseLayer, crystalLayer, overlayPass);
    vec3 mapped = vec3(1.0) - exp(-max(hdr, vec3(0.0)) * 1.18);
    mapped = pow(mapped, vec3(1.0 / 2.2));

    // Only compose after all fixed sample work. Pass 0 remains visible outside
    // the crystal/halo because pass 1 discards the obvious scene exterior here.
    if (overlayPass > 0.5 && haloMask < 0.01)
        discard;

    outColor = vec4(mapped, 1.0);
}
