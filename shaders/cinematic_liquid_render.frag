#version 450

// Cinematic Liquid v1
// -------------------
// The MLS-MPM particles are reconstructed into a 3D density volume before
// this pass.  We raymarch the actual evolving free surface, then shade it with
// Fresnel reflection, refraction, Beer-Lambert absorption and a small opaque
// scene.  This is deliberately a real 3D liquid path rather than a coloured
// 2D velocity/dye field.

layout(location = 0) in  vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler3D densityVolume;

layout(push_constant) uniform LiquidRenderParams {
    vec4 cameraTime;        // camera position xyz, simulation time
    vec4 targetAspect;      // camera target xyz, viewport aspect
    vec4 volumeMinIso;      // simulation AABB min xyz, isosurface threshold
    vec4 volumeMaxStep;     // simulation AABB max xyz, ray step multiplier
    vec4 sphere;            // obstacle center xyz, radius
    uvec4 render;           // ray steps, shader version, viewport width, height
} rp;

const float PI = 3.14159265359;
const int MAX_PRIMARY_STEPS = 224;
const int MAX_THICKNESS_STEPS = 72;

float saturate(float v) { return clamp(v, 0.0, 1.0); }

vec2 intersectBox(vec3 ro, vec3 rd, vec3 bmin, vec3 bmax) {
    vec3 inv = 1.0 / rd;
    vec3 t0 = (bmin - ro) * inv;
    vec3 t1 = (bmax - ro) * inv;
    vec3 lo = min(t0, t1);
    vec3 hi = max(t0, t1);
    float nearT = max(max(lo.x, lo.y), lo.z);
    float farT  = min(min(hi.x, hi.y), hi.z);
    return vec2(nearT, farT);
}

float intersectSphere(vec3 ro, vec3 rd, vec4 s) {
    vec3 oc = ro - s.xyz;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - s.w * s.w;
    float h = b * b - c;
    if (h < 0.0) return -1.0;
    h = sqrt(h);
    float t = -b - h;
    return t > 0.001 ? t : (-b + h > 0.001 ? -b + h : -1.0);
}

vec3 sky(vec3 rd) {
    float h = saturate(rd.y * 0.5 + 0.5);
    vec3 horizon = vec3(0.20, 0.38, 0.62);
    vec3 zenith  = vec3(0.018, 0.070, 0.19);
    vec3 c = mix(horizon, zenith, pow(h, 0.65));
    vec3 sunDir = normalize(vec3(-0.45, 0.72, 0.38));
    float sun = pow(max(dot(rd, sunDir), 0.0), 720.0);
    c += vec3(1.0, 0.82, 0.56) * sun * 5.0;
    return c;
}

vec3 opaqueScene(vec3 ro, vec3 rd) {
    float best = 1e20;
    vec3 color = sky(rd);

    float ts = intersectSphere(ro, rd, rp.sphere);
    if (ts > 0.0 && ts < best) {
        best = ts;
        vec3 p = ro + rd * ts;
        vec3 n = normalize(p - rp.sphere.xyz);
        vec3 l = normalize(vec3(-0.45, 0.72, 0.38));
        float ndl = max(dot(n, l), 0.0);
        float rim = pow(1.0 - max(dot(n, -rd), 0.0), 3.0);
        vec3 metal = mix(vec3(0.11, 0.002, 0.001),
                         vec3(0.72, 0.020, 0.006), ndl);
        metal += sky(reflect(rd, n)) * (0.08 + 0.24 * rim);
        color = metal;
    }

    float floorY = rp.volumeMinIso.y;
    if (rd.y < -1e-5) {
        float tf = (floorY - ro.y) / rd.y;
        if (tf > 0.001 && tf < best) {
            vec3 p = ro + rd * tf;
            vec2 tileUv = p.xz * 2.2;
            float checker = mod(floor(tileUv.x) + floor(tileUv.y), 2.0);
            vec3 a = vec3(0.014, 0.023, 0.031);
            vec3 b = vec3(0.065, 0.078, 0.086);
            vec3 base = mix(a, b, checker);
            float gridLine = 1.0 - smoothstep(0.0, 0.055,
                min(abs(fract(tileUv.x) - 0.5), abs(fract(tileUv.y) - 0.5)));
            base += vec3(0.01, 0.10, 0.13) * gridLine * 0.22;
            float caustic = pow(0.5 + 0.5 * sin(p.x * 19.0 + sin(p.z * 14.0 + rp.cameraTime.w)) *
                                          sin(p.z * 17.0 - rp.cameraTime.w * 1.3), 8.0);
            color = base * (0.52 + 0.48 * max(dot(vec3(0,1,0),
                normalize(vec3(-0.45, 0.72, 0.38))), 0.0));
            color += vec3(0.03, 0.32, 0.40) * caustic * 0.16;
        }
    }
    return color;
}

vec3 worldToUv(vec3 p) {
    return (p - rp.volumeMinIso.xyz) /
           max(rp.volumeMaxStep.xyz - rp.volumeMinIso.xyz, vec3(1e-5));
}

float densityAt(vec3 p) {
    vec3 uvw = worldToUv(p);
    if (any(lessThan(uvw, vec3(0.0))) || any(greaterThan(uvw, vec3(1.0))))
        return 0.0;
    return texture(densityVolume, uvw).r;
}

vec3 surfaceNormal(vec3 p) {
    vec3 size = vec3(textureSize(densityVolume, 0));
    vec3 worldCell = (rp.volumeMaxStep.xyz - rp.volumeMinIso.xyz) / max(size, vec3(1.0));
    float dx = densityAt(p + vec3(worldCell.x, 0, 0)) - densityAt(p - vec3(worldCell.x, 0, 0));
    float dy = densityAt(p + vec3(0, worldCell.y, 0)) - densityAt(p - vec3(0, worldCell.y, 0));
    float dz = densityAt(p + vec3(0, 0, worldCell.z)) - densityAt(p - vec3(0, 0, worldCell.z));
    // Density grows toward the liquid interior, therefore -gradient is the
    // outward-facing surface normal.
    vec3 g = vec3(dx / max(worldCell.x, 1e-5),
                  dy / max(worldCell.y, 1e-5),
                  dz / max(worldCell.z, 1e-5));
    return -normalize(g + vec3(1e-7));
}

bool findSurface(vec3 ro, vec3 rd, float t0, float t1, out float hitT, out float stepLen) {
    uint steps = clamp(rp.render.x, 24u, uint(MAX_PRIMARY_STEPS));
    stepLen = max((t1 - t0) / float(steps), 0.001) * max(rp.volumeMaxStep.w, 0.25);
    float iso = rp.volumeMinIso.w;
    float prevT = t0;
    float prevD = densityAt(ro + rd * prevT) - iso;

    for (int i = 1; i <= MAX_PRIMARY_STEPS; ++i) {
        if (i > int(steps)) break;
        float t = min(t0 + float(i) * stepLen, t1);
        float d = densityAt(ro + rd * t) - iso;
        if (prevD <= 0.0 && d > 0.0) {
            float a = prevT;
            float b = t;
            for (int refine = 0; refine < 5; ++refine) {
                float m = 0.5 * (a + b);
                if (densityAt(ro + rd * m) > iso) b = m; else a = m;
            }
            hitT = 0.5 * (a + b);
            return true;
        }
        prevT = t;
        prevD = d;
        if (t >= t1) break;
    }
    return false;
}

float estimateThickness(vec3 ro, vec3 rd, float entryT, float farT, float stepLen) {
    float iso = rp.volumeMinIso.w;
    float thickness = 0.0;
    float stride = max(stepLen * 1.5, 0.002);
    bool entered = false;
    for (int i = 1; i <= MAX_THICKNESS_STEPS; ++i) {
        float t = entryT + float(i) * stride;
        if (t >= farT) break;
        float d = densityAt(ro + rd * t);
        if (d > iso) {
            entered = true;
            thickness += stride * saturate(d);
        } else if (entered) {
            break;
        }
    }
    return thickness;
}

void main() {
    vec2 p = inUV * 2.0 - 1.0;
    p.x *= rp.targetAspect.w;

    vec3 ro = rp.cameraTime.xyz;
    vec3 forward = normalize(rp.targetAspect.xyz - ro);
    vec3 right = normalize(cross(forward, vec3(0.0, 1.0, 0.0)));
    vec3 up = normalize(cross(right, forward));
    float tanHalfFov = tan(46.0 * PI / 360.0);
    vec3 rd = normalize(forward + tanHalfFov * (p.x * right - p.y * up));

    vec2 boxHit = intersectBox(ro, rd, rp.volumeMinIso.xyz, rp.volumeMaxStep.xyz);
    float nearT = max(boxHit.x, 0.0);
    float farT = boxHit.y;

    float hitT = 0.0;
    float stepLen = 0.0;
    bool hitLiquid = farT > nearT && findSurface(ro, rd, nearT, farT, hitT, stepLen);
    if (!hitLiquid) {
        vec3 c = opaqueScene(ro, rd);
        c *= 1.0 - 0.18 * dot(p * vec2(0.42, 0.68), p * vec2(0.42, 0.68));
        c = pow(vec3(1.0) - exp(-max(c, vec3(0.0)) * 0.95), vec3(1.0 / 2.2));
        outColor = vec4(c, 1.0);
        return;
    }

    vec3 hit = ro + rd * hitT;
    vec3 n = surfaceNormal(hit);
    if (dot(n, rd) > 0.0) n = -n;

    float thickness = estimateThickness(ro, rd, hitT, farT, stepLen);
    float cosTheta = saturate(dot(-rd, n));
    float fresnel = 0.0204 + (1.0 - 0.0204) * pow(1.0 - cosTheta, 5.0);

    vec3 reflected = opaqueScene(hit + n * 0.004, reflect(rd, n));
    vec3 refractedDir = refract(rd, n, 1.0 / 1.333);
    vec3 refracted = opaqueScene(hit - n * 0.008, refractedDir);

    vec3 absorption = vec3(0.58, 0.16, 0.045);
    vec3 transmittance = exp(-absorption * thickness * 5.8);
    vec3 deepWater = vec3(0.002, 0.075, 0.115);
    refracted = refracted * transmittance + deepWater * (1.0 - transmittance);

    vec3 lightDir = normalize(vec3(-0.45, 0.72, 0.38));
    vec3 halfVec = normalize(lightDir - rd);
    float spec = pow(max(dot(n, halfVec), 0.0), 160.0);
    float rim = pow(1.0 - cosTheta, 2.2);
    vec3 color = mix(refracted, reflected, fresnel);
    color += vec3(1.0, 0.82, 0.62) * spec * 2.6;
    color += vec3(0.02, 0.31, 0.40) * rim * 0.14;

    // Thin, high-gradient fragments read as spray without replacing the
    // physically reconstructed surface with a procedural foam pattern.
    vec3 texelWorld = (rp.volumeMaxStep.xyz - rp.volumeMinIso.xyz) /
                      vec3(textureSize(densityVolume, 0));
    float edgeDensity = densityAt(hit - n * length(texelWorld) * 0.75);
    float spray = smoothstep(rp.volumeMinIso.w, rp.volumeMinIso.w + 0.20, edgeDensity) *
                  (1.0 - smoothstep(rp.volumeMinIso.w + 0.20,
                                    rp.volumeMinIso.w + 0.85, edgeDensity));
    color = mix(color, vec3(0.72, 0.92, 0.96), spray * 0.18);

    float vignette = saturate(1.0 - 0.18 * dot(p * vec2(0.42, 0.70),
                                               p * vec2(0.42, 0.70)));
    color *= vignette;
    color = pow(vec3(1.0) - exp(-max(color, vec3(0.0)) * 0.95), vec3(1.0 / 2.2));
    outColor = vec4(color, 1.0);
}
