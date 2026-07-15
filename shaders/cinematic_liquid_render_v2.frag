#version 450

// Cinematic Liquid v2 presentation shader.
//
// The density volume remains the source of truth for the water surface.  Pool
// and body intersections are traced independently, then depth-compared with
// the liquid entry.  Objects in front of the water are shaded directly while
// objects behind it are reached by the refracted ray, avoiding the common
// "toy painted on top of water" failure mode.

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 2) uniform sampler3D densityVolume;
layout(set = 0, binding = 4) uniform sampler3D whitewaterVolume;

struct BodyState {
    vec4 positionType;
    vec4 orientation;
    vec4 linearVelocityInvMass;
    vec4 angularVelocityInvInertia;
    vec4 shape0;
    vec4 shape1;
    vec4 material;
    vec4 color;
};

layout(set = 0, binding = 3, std430) readonly buffer BodyStateBuffer {
    BodyState bodies[];
} bodyStateBuffer;

layout(push_constant) uniform LiquidRenderV2Params {
    vec4 cameraTime;
    vec4 targetAspect;
    vec4 volumeMinIso;
    vec4 volumeMaxStep;
    vec4 pool;
    vec4 lighting;
    uvec4 render;
    uvec4 scene;
} rp;

const int BODY_DUCK = 0;
const int BODY_PLAY_BALL = 1;
const int BODY_ANCHORED_BOAT = 2;
const int BODY_SINK_BALL = 3;
const uint MAX_RENDER_BODIES = 32u;
// Leave enough headroom to march the fixed reconstruction volume at 0.75 of
// its smallest world-space voxel without skipping thin sheets or droplets.
const int MAX_PRIMARY_STEPS = 352;
// The primary ray only finds the air-to-water boundary. A second bounded
// march follows the transmitted ray until it reaches an object or the true
// water-to-air boundary. Without this second interface the old shader sampled
// the sky/floor while the ray was still inside water, producing a chrome-blue
// plastic appearance.
const int MAX_SECONDARY_STEPS = 256;
// Match the reference renderer's default optical contract: after the first
// air-to-water hit, keep following the important Fresnel branch through up to
// four real density-field interfaces.  This is deliberately a bounded path,
// not recursive ray tracing, so every scored pixel has a fixed upper cost.
const int MAX_REFRACTION_BOUNCES = 4;
const int REFRACTION_PROBE_STEPS = 4;
const int MAX_WHITEWATER_STEPS = 48;
const int MAX_SHADOW_STEPS = 32;
const int MAX_POOL_STEPS = 80;
const float PI = 3.14159265359;
const float FAR_DISTANCE = 1.0e20;

const int HIT_NONE = 0;
const int HIT_POOL = 1;
const int HIT_FLOOR = 2;
const int HIT_DUCK = 3;
const int HIT_BALL = 4;
const int HIT_BOAT = 5;
const int HIT_SINK_BALL = 6;
const int HIT_POOL_LINER = 7;

struct SceneHit {
    float t;
    vec3 normal;
    vec3 color;
    float roughness;
    int kind;
};

// The clear PVC side wall is deliberately not a SceneHit.  Treating it as an
// opaque pool hit would stop the ray before the water and hide the very volume
// the low hero camera is meant to show.  It is composited as one bounded thin
// dielectric layer after the first visible scene surface has been resolved.
struct ClearPoolHit {
    float t;
    vec3 normal;
    vec3 position;
    bool valid;
};

float saturate(float x) { return clamp(x, 0.0, 1.0); }

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float valueNoise2(vec2 p) {
    vec2 cell = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = hash12(cell);
    float b = hash12(cell + vec2(1.0, 0.0));
    float c = hash12(cell + vec2(0.0, 1.0));
    float d = hash12(cell + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float lowFrequencyNoise2(vec2 p) {
    return 0.68 * valueNoise2(p) +
           0.32 * valueNoise2(p * 2.07 + vec2(7.1, -3.8));
}

vec3 finishColor(vec3 linearColor, float vignette) {
    // jeantimex/fluid keeps the raymarch result linear and applies only scene
    // exposure before the final linear->sRGB transfer.  The previous ACES fit
    // crushed neighbouring cyan/blue values into poster-like contour bands,
    // which made the reconstructed surface read as chrome or oil.
    vec3 mapped = clamp(max(linearColor, vec3(0.0)) *
                        max(rp.lighting.w, 0.01) * vignette,
                        vec3(0.0), vec3(1.0));
    // Vulkan normally selects an sRGB swapchain and performs the transfer
    // function on store.  Applying pow(1/2.2) as well was the cause of the
    // washed-out v2 captures.  scene.z records the actual attachment format
    // so the uncommon UNORM fallback still gets an explicit encode.
    if (rp.scene.z == 0u)
        mapped = pow(mapped, vec3(1.0 / 2.2));
    return mapped;
}

float dielectricFresnel(float cosIncident, float etaIncident,
                         float etaTransmitted) {
    cosIncident = clamp(abs(cosIncident), 0.0, 1.0);
    float sinTransmitted = etaIncident / etaTransmitted *
        sqrt(max(0.0, 1.0 - cosIncident * cosIncident));
    if (sinTransmitted >= 1.0) return 1.0;
    float cosTransmitted = sqrt(max(0.0,
        1.0 - sinTransmitted * sinTransmitted));
    float parallel = (etaTransmitted * cosIncident -
                      etaIncident * cosTransmitted) /
                     max(etaTransmitted * cosIncident +
                         etaIncident * cosTransmitted, 1.0e-6);
    float perpendicular = (etaIncident * cosIncident -
                           etaTransmitted * cosTransmitted) /
                          max(etaIncident * cosIncident +
                              etaTransmitted * cosTransmitted, 1.0e-6);
    return 0.5 * (parallel * parallel + perpendicular * perpendicular);
}

vec4 safeQuaternion(vec4 q) {
    float q2 = dot(q, q);
    return q2 > 1.0e-10 ? q * inversesqrt(q2) : vec4(0.0, 0.0, 0.0, 1.0);
}
vec3 rotateByQuaternion(vec4 q, vec3 v) {
    q = safeQuaternion(q);
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}
vec3 inverseRotateByQuaternion(vec4 q, vec3 v) {
    q = safeQuaternion(q);
    q.xyz = -q.xyz;
    return rotateByQuaternion(q, v);
}

vec2 intersectBox(vec3 ro, vec3 rd, vec3 boxMin, vec3 boxMax) {
    vec3 safeDirection = vec3(
        abs(rd.x) < 1.0e-7 ? (rd.x < 0.0 ? -1.0e-7 : 1.0e-7) : rd.x,
        abs(rd.y) < 1.0e-7 ? (rd.y < 0.0 ? -1.0e-7 : 1.0e-7) : rd.y,
        abs(rd.z) < 1.0e-7 ? (rd.z < 0.0 ? -1.0e-7 : 1.0e-7) : rd.z);
    vec3 t0 = (boxMin - ro) / safeDirection;
    vec3 t1 = (boxMax - ro) / safeDirection;
    vec3 lo = min(t0, t1);
    vec3 hi = max(t0, t1);
    return vec2(max(max(lo.x, lo.y), lo.z), min(min(hi.x, hi.y), hi.z));
}

float intersectSphereLocal(vec3 ro, vec3 rd, vec3 centre, float radius,
                           out vec3 normal) {
    vec3 oc = ro - centre;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float h = b * b - c;
    if (h < 0.0) return -1.0;
    h = sqrt(h);
    float t0 = -b - h;
    float t1 = -b + h;
    float t = t0 > 0.001 ? t0 : (t1 > 0.001 ? t1 : -1.0);
    if (t > 0.0) normal = normalize(ro + rd * t - centre);
    return t;
}

float intersectEllipsoidLocal(vec3 ro, vec3 rd, vec3 centre, vec3 radii,
                              out vec3 normal) {
    radii = max(abs(radii), vec3(1.0e-4));
    vec3 o = (ro - centre) / radii;
    vec3 d = rd / radii;
    float a = dot(d, d);
    float b = dot(o, d);
    float c = dot(o, o) - 1.0;
    float h = b * b - a * c;
    if (h < 0.0 || a < 1.0e-12) return -1.0;
    h = sqrt(h);
    float t0 = (-b - h) / a;
    float t1 = (-b + h) / a;
    float t = t0 > 0.001 ? t0 : (t1 > 0.001 ? t1 : -1.0);
    if (t > 0.0) {
        vec3 p = ro + rd * t - centre;
        normal = normalize(p / (radii * radii));
    }
    return t;
}

float intersectBoxLocal(vec3 ro, vec3 rd, vec3 centre, vec3 halfExtent,
                        out vec3 normal) {
    halfExtent = max(abs(halfExtent), vec3(1.0e-4));
    vec2 range = intersectBox(ro, rd, centre - halfExtent, centre + halfExtent);
    // A slab near value alone is not a hit: for a missed box one axis can
    // still produce a positive near while another already ended behind it.
    if (range.y <= 0.001 || range.y < max(range.x, 0.0)) return -1.0;
    float t = range.x > 0.001 ? range.x : (range.y > 0.001 ? range.y : -1.0);
    if (t > 0.0) {
        vec3 p = (ro + rd * t - centre) / halfExtent;
        vec3 a = abs(p);
        if (a.x > a.y && a.x > a.z) normal = vec3(sign(p.x), 0.0, 0.0);
        else if (a.y > a.z) normal = vec3(0.0, sign(p.y), 0.0);
        else normal = vec3(0.0, 0.0, sign(p.z));
    }
    return t;
}

float intersectCapsuleLocal(vec3 ro, vec3 rd, vec3 a, vec3 b, float radius,
                            out vec3 normal) {
    vec3 ba = b - a;
    vec3 oa = ro - a;
    float baba = dot(ba, ba);
    float bard = dot(ba, rd);
    float baoa = dot(ba, oa);
    float rdoa = dot(rd, oa);
    float oaoa = dot(oa, oa);
    float qa = baba - bard * bard;
    float qb = baba * rdoa - baoa * bard;
    float qc = baba * oaoa - baoa * baoa - radius * radius * baba;
    float h = qb * qb - qa * qc;
    float best = -1.0;
    if (h >= 0.0 && abs(qa) > 1.0e-9) {
        float t = (-qb - sqrt(h)) / qa;
        float y = baoa + t * bard;
        if (t > 0.001 && y > 0.0 && y < baba) {
            vec3 p = oa + t * rd - ba * (y / baba);
            normal = normalize(p);
            best = t;
        }
    }
    vec3 capNormal;
    float ta = intersectSphereLocal(ro, rd, a, radius, capNormal);
    if (ta > 0.0 && (best < 0.0 || ta < best)) {
        best = ta;
        normal = capNormal;
    }
    float tb = intersectSphereLocal(ro, rd, b, radius, capNormal);
    if (tb > 0.0 && (best < 0.0 || tb < best)) {
        best = tb;
        normal = capNormal;
    }
    return best;
}

float sdSphereSigned(vec3 p, float r) { return length(p) - max(r, 1.0e-4); }

float sdEllipsoidSigned(vec3 p, vec3 r) {
    r = max(abs(r), vec3(1.0e-4));
    float k0 = length(p / r);
    float k1 = length(p / (r * r));
    return k0 * (k0 - 1.0) / max(k1, 1.0e-5);
}

float smoothUnion(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / max(k, 1.0e-5), 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

// Identical shape maths to the duck collider in mls_mpm_grid_update_v2.comp /
// mls_mpm_g2p_v2.comp: what the liquid feels is exactly what is drawn.
float duckSdf(vec3 p, vec3 bodyRadius, float headRadius, vec3 headCenter,
              float beakLength) {
    float belly = sdEllipsoidSigned(
        p - vec3(0.0, -0.04 * bodyRadius.y, 0.0),
        vec3(bodyRadius.x, 0.84 * bodyRadius.y, bodyRadius.z));
    float tail = sdEllipsoidSigned(
        p - vec3(-0.72 * bodyRadius.x, 0.28 * bodyRadius.y, 0.0),
        bodyRadius * vec3(0.44, 0.30, 0.42));
    float duck = smoothUnion(belly, tail, 0.30 * bodyRadius.y);
    duck = smoothUnion(duck, sdSphereSigned(p - headCenter, headRadius),
                       0.45 * headRadius);
    vec3 beakCenter = headCenter +
        vec3(headRadius + 0.30 * beakLength, -0.20 * headRadius, 0.0);
    float beak = sdEllipsoidSigned(
        p - beakCenter,
        vec3(0.60 * beakLength, 0.24 * headRadius, 0.46 * headRadius));
    return smoothUnion(duck, beak, 0.12 * headRadius);
}

SceneHit emptyHit() {
    SceneHit hit;
    hit.t = FAR_DISTANCE;
    hit.normal = vec3(0.0, 1.0, 0.0);
    hit.color = vec3(0.0);
    hit.roughness = 1.0;
    hit.kind = HIT_NONE;
    return hit;
}

void acceptHit(float t, vec3 worldNormal, vec3 color, float roughness,
               int kind, inout SceneHit hit) {
    if (t > 0.001 && t < hit.t) {
        hit.t = t;
        hit.normal = normalize(worldNormal);
        hit.color = max(color, vec3(0.0));
        hit.roughness = clamp(roughness, 0.02, 1.0);
        hit.kind = kind;
    }
}

vec3 colourfulBallColor(vec3 localPoint, float radius, vec3 base) {
    vec3 n = normalize(localPoint / max(radius, 1.0e-4));
    float longitude = atan(n.z, n.x) / (2.0 * PI) + 0.5;
    float segment = floor(fract(longitude) * 6.0);
    vec3 palette[6] = vec3[6](
        vec3(0.96, 0.12, 0.08), vec3(1.00, 0.62, 0.04),
        vec3(0.96, 0.90, 0.08), vec3(0.06, 0.72, 0.30),
        vec3(0.05, 0.36, 0.96), vec3(0.66, 0.12, 0.92));
    vec3 stripe = palette[int(segment)];
    float cap = smoothstep(0.50, 0.75, abs(n.y));
    return mix(mix(max(base, vec3(0.08)), stripe, 0.82),
               vec3(0.96), cap * 0.78);
}

void traceBody(BodyState body, vec3 ro, vec3 rd, inout SceneHit hit) {
    vec4 q = safeQuaternion(body.orientation);
    vec3 localOrigin = inverseRotateByQuaternion(q, ro - body.positionType.xyz);
    vec3 localDirection = inverseRotateByQuaternion(q, rd);
    int type = int(round(body.positionType.w));
    vec3 localNormal;
    float t;

    if (type == BODY_PLAY_BALL || type == BODY_SINK_BALL) {
        float radius = max(body.shape0.x, 0.01);
        t = intersectSphereLocal(localOrigin, localDirection,
                                 vec3(0.0), radius, localNormal);
        if (t > 0.0) {
            vec3 color = type == BODY_PLAY_BALL
                ? colourfulBallColor(localOrigin + localDirection * t,
                                     radius, body.color.rgb)
                : mix(max(body.color.rgb, vec3(0.10)),
                      vec3(0.34, 0.39, 0.43), 0.55);
            acceptHit(t, rotateByQuaternion(q, localNormal), color,
                      type == BODY_PLAY_BALL ? 0.30 : 0.17,
                      type == BODY_PLAY_BALL ? HIT_BALL : HIT_SINK_BALL, hit);
        }
        return;
    }

    if (type == BODY_DUCK) {
        vec3 bodyRadius = max(abs(body.shape0.xyz), vec3(0.02));
        float headRadius = max(body.shape1.x, 0.02);
        vec3 headCenter = vec3(body.shape1.y, body.shape1.z, 0.0);
        float beakLength = max(body.shape1.w, 0.02);

        float bound = max(length(bodyRadius),
                          length(vec2(headCenter.x, headCenter.y)) +
                          headRadius + beakLength) + 0.015;
        float rayB = dot(localOrigin, localDirection);
        float rayC = dot(localOrigin, localOrigin) - bound * bound;
        float disc = rayB * rayB - rayC;
        if (disc <= 0.0) return;
        float root = sqrt(disc);
        float tNear = max(-rayB - root, 0.001);
        float tFar = min(-rayB + root, hit.t);
        if (tFar <= tNear) return;

        // Sphere-trace the smooth-min compound.  Hard analytic ellipsoid
        // unions crease at every lobe seam; the reference bath toy is one
        // continuous blended surface, which only an SDF march reproduces.
        t = tNear;
        bool surfaceHit = false;
        for (int i = 0; i < 72; ++i) {
            float d = duckSdf(localOrigin + localDirection * t, bodyRadius,
                              headRadius, headCenter, beakLength);
            if (d < 8.0e-4) { surfaceHit = true; break; }
            t += d * 0.85;
            if (t > tFar) break;
        }
        if (!surfaceHit) return;

        vec3 duckPoint = localOrigin + localDirection * t;
        vec2 e = vec2(1.0, -1.0) * 0.0016;
        vec3 sdfNormal = normalize(
            e.xyy * duckSdf(duckPoint + e.xyy, bodyRadius, headRadius,
                            headCenter, beakLength) +
            e.yyx * duckSdf(duckPoint + e.yyx, bodyRadius, headRadius,
                            headCenter, beakLength) +
            e.yxy * duckSdf(duckPoint + e.yxy, bodyRadius, headRadius,
                            headCenter, beakLength) +
            e.xxx * duckSdf(duckPoint + e.xxx, bodyRadius, headRadius,
                            headCenter, beakLength));

        // Painted-toy colour zones: orange beak, black eyes with one white
        // glint each.  Decals instead of extra geometry keep the silhouette
        // as smooth as the reference duck's printed face.
        vec3 yellow = max(body.color.rgb, vec3(1.0, 0.63, 0.02));
        vec3 beakCenter = headCenter +
            vec3(headRadius + 0.30 * beakLength, -0.20 * headRadius, 0.0);
        float beakDistance = sdEllipsoidSigned(
            duckPoint - beakCenter,
            vec3(0.60 * beakLength, 0.24 * headRadius, 0.46 * headRadius));
        float restDistance = min(
            sdSphereSigned(duckPoint - headCenter, headRadius),
            sdEllipsoidSigned(duckPoint - vec3(0.0, -0.04 * bodyRadius.y, 0.0),
                              vec3(bodyRadius.x, 0.84 * bodyRadius.y,
                                   bodyRadius.z)));
        float beakPaint = smoothstep(0.06 * headRadius, -0.06 * headRadius,
                                     beakDistance - restDistance);
        vec3 color = mix(yellow, vec3(1.0, 0.30, 0.02), beakPaint);
        float roughness = mix(0.30, 0.36, beakPaint);

        for (int side = -1; side <= 1; side += 2) {
            vec3 eyeDirection = normalize(vec3(0.60, 0.34, 0.74 * float(side)));
            vec3 eyeCentre = headCenter + eyeDirection * headRadius;
            float eyePaint = smoothstep(0.20 * headRadius, 0.15 * headRadius,
                                        length(duckPoint - eyeCentre));
            vec3 glintCentre = headCenter + normalize(
                eyeDirection + vec3(0.10, 0.16, 0.0)) * headRadius;
            float glintPaint = smoothstep(0.075 * headRadius,
                                          0.050 * headRadius,
                                          length(duckPoint - glintCentre));
            color = mix(color, vec3(0.006), eyePaint);
            roughness = mix(roughness, 0.10, eyePaint);
            color = mix(color, vec3(0.94), glintPaint);
        }

        acceptHit(t, rotateByQuaternion(q, sdfNormal), color, roughness,
                  HIT_DUCK, hit);
        return;
    }

    if (type == BODY_ANCHORED_BOAT) {
        vec3 h = max(abs(body.shape0.xyz), vec3(0.02));
        vec3 hullColor = max(body.color.rgb, vec3(0.10, 0.18, 0.58));
        t = intersectEllipsoidLocal(localOrigin, localDirection,
                                    vec3(0.0, -0.28 * h.y, 0.0), h,
                                    localNormal);
        if (t > 0.0) {
            vec3 hullPoint = localOrigin + localDirection * t;
            float upperHull = smoothstep(-0.62 * h.y, 0.34 * h.y,
                                         hullPoint.y);
            vec3 shapedHull = mix(hullColor * 0.44,
                                  hullColor * 1.12, upperHull);
            float waterlineStripe = 1.0 - smoothstep(
                0.055 * h.y, 0.16 * h.y,
                abs(hullPoint.y - 0.02 * h.y));
            shapedHull = mix(shapedHull, vec3(0.92, 0.95, 0.92),
                             waterlineStripe * 0.86);
            float bowPanel = smoothstep(0.42 * h.x, 0.82 * h.x,
                                        hullPoint.x);
            shapedHull = mix(shapedHull, vec3(0.94, 0.16, 0.055),
                             bowPanel * 0.72);
            acceptHit(t, rotateByQuaternion(q, localNormal), shapedHull,
                      max(body.color.a, 0.28), HIT_BOAT, hit);
        }

        // A broad cream deck breaks the ellipsoid silhouette into a readable
        // keel/deck/bow toy-boat profile without changing collision geometry.
        vec3 deckCentre = vec3(0.04 * h.x, 0.52 * h.y, 0.0);
        vec3 deckHalf = vec3(0.62 * h.x, 0.10 * h.y, 0.74 * h.z);
        t = intersectBoxLocal(localOrigin, localDirection, deckCentre,
                              deckHalf, localNormal);
        if (t > 0.0)
            acceptHit(t, rotateByQuaternion(q, localNormal),
                      vec3(0.92, 0.88, 0.69), 0.38, HIT_BOAT, hit);

        vec3 cabinCentre = vec3(0.10 * h.x, 0.80 * h.y, 0.0);
        vec3 cabinHalf = vec3(0.24 * h.x, 0.28 * h.y, 0.43 * h.z);
        t = intersectBoxLocal(localOrigin, localDirection, cabinCentre,
                              cabinHalf, localNormal);
        if (t > 0.0)
            acceptHit(t, rotateByQuaternion(q, localNormal),
                      vec3(0.88, 0.94, 0.98), 0.24, HIT_BOAT, hit);

        vec3 roofCentre = vec3(0.10 * h.x, 1.12 * h.y, 0.0);
        vec3 roofHalf = vec3(0.30 * h.x, 0.065 * h.y, 0.50 * h.z);
        t = intersectBoxLocal(localOrigin, localDirection, roofCentre,
                              roofHalf, localNormal);
        if (t > 0.0)
            acceptHit(t, rotateByQuaternion(q, localNormal),
                      vec3(0.93, 0.18, 0.045), 0.30, HIT_BOAT, hit);

        float propRadius = max(body.shape1.x, 0.02);
        float aft = max(body.shape1.y, h.x);
        vec3 shaftA = vec3(-0.62 * h.x, -1.15 * h.y, 0.0);
        vec3 shaftB = vec3(-aft, -1.15 * h.y, 0.0);
        t = intersectCapsuleLocal(localOrigin, localDirection,
                                  shaftA, shaftB, 0.12 * propRadius, localNormal);
        if (t > 0.0)
            acceptHit(t, rotateByQuaternion(q, localNormal),
                      vec3(0.20, 0.22, 0.25), 0.13, HIT_BOAT, hit);
        t = intersectSphereLocal(localOrigin, localDirection, shaftB,
                                 0.22 * propRadius, localNormal);
        if (t > 0.0)
            acceptHit(t, rotateByQuaternion(q, localNormal),
                      vec3(0.24, 0.26, 0.28), 0.11, HIT_BOAT, hit);

        float angle = rp.cameraTime.w * body.shape1.w;
        for (int blade = 0; blade < 2; ++blade) {
            float a = angle + float(blade) * 0.5 * PI;
            vec3 radial = vec3(0.0, cos(a), sin(a));
            vec3 bladeA = shaftB - radial * (0.13 * propRadius);
            vec3 bladeB = shaftB + radial * propRadius;
            t = intersectCapsuleLocal(localOrigin, localDirection,
                                      bladeA, bladeB, 0.11 * propRadius,
                                      localNormal);
            if (t > 0.0)
                acceptHit(t, rotateByQuaternion(q, localNormal),
                          vec3(0.96, 0.42, 0.035), 0.22, HIT_BOAT, hit);
        }
    }
}

float roundedRectangleDistance(vec2 p, vec2 halfExtent, float radius) {
    radius = clamp(radius, 0.0, min(halfExtent.x, halfExtent.y));
    vec2 q = abs(p) - halfExtent + vec2(radius);
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - radius;
}

void poolDistances(vec3 p, out float lowerRing, out float upperRing,
                   out float liner) {
    vec3 volumeCellSize = (rp.volumeMaxStep.xyz - rp.volumeMinIso.xyz) /
        max(vec3(textureSize(densityVolume, 0)), vec3(1.0));
    vec2 physicalMax = rp.volumeMaxStep.xz - volumeCellSize.xz;
    vec2 centre = 0.5 * (rp.volumeMinIso.xz + physicalMax);
    vec2 halfExtent = 0.5 * (physicalMax - rp.volumeMinIso.xz);
    float tube = max(rp.pool.y, 0.015);
    // 5.29 * 0.085 = 0.45: keep the drawn ring exactly on the physical
    // finite-wall inset (grid_update/g2p pool.y and the SPH wall constants)
    // so the resting water edge meets the visible rim.  The wider inset
    // leaves a readable grass catch band around the pool for landed spray.
    float poolInset = min(5.29 * tube,
        max(min(halfExtent.x, halfExtent.y) - 2.0 * tube, 0.0));
    halfExtent = max(halfExtent - vec2(poolInset), vec2(2.0 * tube));
    float cornerRadius = max(rp.pool.x, 0.50 * tube);
    // ring separation is derived from tube size; rp.pool.z is the physical
    // ground/pool-floor height shared with the compute collision contract.
    float separation = 2.0 * tube;
    float perimeter = roundedRectangleDistance(
        p.xz - centre, halfExtent, cornerRadius);
    // The fixed v2 fill occupies roughly the lower third of the grid.  Stack
    // both inflatable rings around that waterline rather than at grid origin;
    // the grid includes extra air above the pool for the falling sphere.
    // 0.42 raises the pool wall (user request); must match the physical
    // wallTop in mls_mpm_g2p_v2.comp / mls_mpm_grid_update_v2.comp and the
    // SPH wall constants.
    float upperY = mix(rp.volumeMinIso.y, rp.volumeMaxStep.y, 0.42);
    float lowerY = upperY - separation;
    lowerRing = length(vec2(perimeter, p.y - lowerY)) - tube;
    upperRing = length(vec2(perimeter, p.y - upperY)) - tube;

    float linerThickness = max(rp.pool.w, 0.008);
    float linerPlanar = roundedRectangleDistance(
        p.xz - centre, max(halfExtent - vec2(0.30 * tube), vec2(tube)),
        max(cornerRadius - 0.30 * tube, tube));
    float floorY = rp.pool.z - 0.5 * linerThickness;
    float vertical = abs(p.y - floorY) - 0.5 * linerThickness;
    float bottomLiner = max(linerPlanar, vertical);

    // Keep the floor liner, but do not paint an opaque wall between the rings.
    // The physical grid still owns collision; visually, the open/clear side is
    // what lets the low hero camera reveal the water volume like the reference
    // tank instead of hiding it behind a large cyan rectangle.
    liner = bottomLiner;
}

float poolSdf(vec3 p) {
    float lowerRing, upperRing, liner;
    poolDistances(p, lowerRing, upperRing, liner);
    // One restrained top ring is enough to read as an inflatable pool. The
    // old stacked front rings covered the very water volume this test exists
    // to inspect from the low camera.
    return min(upperRing, liner);
}

vec3 poolNormal(vec3 p) {
    float e = max(length(rp.volumeMaxStep.xyz - rp.volumeMinIso.xyz) *
                  0.00025, 0.0005);
    vec3 d = vec3(e, 0.0, 0.0);
    vec3 g = vec3(poolSdf(p + d.xyy) - poolSdf(p - d.xyy),
                  poolSdf(p + d.yxy) - poolSdf(p - d.yxy),
                  poolSdf(p + d.yyx) - poolSdf(p - d.yyx));
    return normalize(g + vec3(1.0e-8));
}

void tracePool(vec3 ro, vec3 rd, inout SceneHit hit) {
    float t = 0.005;
    float maximumDistance = min(hit.t, 40.0);
    for (int i = 0; i < MAX_POOL_STEPS; ++i) {
        if (t >= maximumDistance) break;
        vec3 p = ro + rd * t;
        float distance = poolSdf(p);
        if (abs(distance) < 0.0015) {
            float lowerRing, upperRing, liner;
            poolDistances(p, lowerRing, upperRing, liner);
            vec3 color;
            float roughness;
            if (liner < lowerRing && liner < upperRing) {
                color = vec3(0.025, 0.085, 0.105);
                roughness = 0.44;
            } else {
                color = vec3(0.020, 0.105, 0.255);
                roughness = 0.27;
            }
            int hitKind = liner < lowerRing && liner < upperRing
                ? HIT_POOL_LINER : HIT_POOL;
            acceptHit(t, poolNormal(p), color, roughness, hitKind, hit);
            break;
        }
        t += max(abs(distance) * 0.82, 0.0015);
    }
}

void poolMembraneDimensions(out vec2 centre, out vec2 wallHalfExtent,
                            out float cornerRadius, out float bottomY,
                            out float topY, out float thickness) {
    vec3 volumeCellSize = (rp.volumeMaxStep.xyz - rp.volumeMinIso.xyz) /
        max(vec3(textureSize(densityVolume, 0)), vec3(1.0));
    vec2 physicalMax = rp.volumeMaxStep.xz - volumeCellSize.xz;
    centre = 0.5 * (rp.volumeMinIso.xz + physicalMax);
    vec2 volumeHalfExtent = 0.5 * (physicalMax - rp.volumeMinIso.xz);
    float tube = max(rp.pool.y, 0.015);
    // Keep the visual membrane aligned with the internal physical pool wall:
    // 5.29 * the current 0.085 m tube is the widened 0.45 m inset.  The
    // remaining band belongs to escaped spray rather than the pool interior.
    float poolInset = min(5.29 * tube,
        max(min(volumeHalfExtent.x, volumeHalfExtent.y) - 2.0 * tube, 0.0));
    wallHalfExtent = max(volumeHalfExtent - vec2(poolInset), vec2(2.0 * tube));
    cornerRadius = max(rp.pool.x, 0.50 * tube);
    bottomY = rp.pool.z;
    float upperRingY = mix(rp.volumeMinIso.y, rp.volumeMaxStep.y, 0.42);
    topY = upperRingY - 0.10 * tube;
    thickness = max(0.012, 0.14 * tube);
}

float poolMembraneSdf(vec3 p) {
    vec2 centre, halfExtent;
    float cornerRadius, bottomY, topY, thickness;
    poolMembraneDimensions(centre, halfExtent, cornerRadius,
                           bottomY, topY, thickness);
    float perimeter = roundedRectangleDistance(
        p.xz - centre, halfExtent, cornerRadius);
    float membrane = abs(perimeter) - 0.5 * thickness;
    float middleY = 0.5 * (bottomY + topY);
    float vertical = abs(p.y - middleY) - 0.5 * (topY - bottomY);
    return max(membrane, vertical);
}

vec3 poolMembraneNormal(vec3 p) {
    float e = max(length(rp.volumeMaxStep.xyz - rp.volumeMinIso.xyz) *
                  0.00016, 0.00045);
    vec3 d = vec3(e, 0.0, 0.0);
    vec3 g = vec3(
        poolMembraneSdf(p + d.xyy) - poolMembraneSdf(p - d.xyy),
        poolMembraneSdf(p + d.yxy) - poolMembraneSdf(p - d.yxy),
        poolMembraneSdf(p + d.yyx) - poolMembraneSdf(p - d.yyx));
    return normalize(g + vec3(1.0e-8));
}

ClearPoolHit traceClearPool(vec3 ro, vec3 rd, float visibleDistance) {
    ClearPoolHit hit;
    hit.t = FAR_DISTANCE;
    hit.normal = vec3(0.0, 1.0, 0.0);
    hit.position = vec3(0.0);
    hit.valid = false;
    float maximumDistance = min(visibleDistance, 40.0);
    float t = 0.005;
    for (int i = 0; i < MAX_POOL_STEPS; ++i) {
        if (t >= maximumDistance) break;
        vec3 p = ro + rd * t;
        float distance = poolMembraneSdf(p);
        if (abs(distance) < 0.0011) {
            hit.t = t;
            hit.position = p;
            hit.normal = poolMembraneNormal(p);
            hit.valid = true;
            break;
        }
        t += max(abs(distance) * 0.78, 0.0011);
    }
    return hit;
}

vec3 sky(vec3 direction) {
    direction = normalize(direction);
    float altitude = max(direction.y, 0.0);
    vec3 horizon = vec3(0.88, 0.94, 1.00);
    vec3 zenith = vec3(0.055, 0.28, 0.66);
    vec3 upper = mix(horizon, zenith, pow(altitude, 0.52));
    vec3 lower = mix(vec3(0.095, 0.19, 0.055), horizon,
                     smoothstep(-0.24, 0.015, direction.y));
    vec3 color = direction.y >= 0.0 ? upper : lower;

    // Two low-frequency analytic layers provide a portable skybox-like cloud
    // field without a cubemap asset or another descriptor binding.
    if (direction.y > 0.015) {
        vec2 cloudUv = direction.xz /
            (0.30 + direction.y) * 1.25 +
            vec2(rp.cameraTime.w * 0.010, rp.cameraTime.w * 0.003);
        float cloudField = lowFrequencyNoise2(cloudUv);
        cloudField += 0.10 * sin(cloudUv.x * 2.1 + cloudUv.y * 1.4);
        float altitudeMask = smoothstep(0.025, 0.13, direction.y) *
            (1.0 - smoothstep(0.72, 0.98, direction.y));
        float cloud = smoothstep(0.56, 0.73, cloudField) * altitudeMask;
        vec3 cloudShade = mix(vec3(0.57, 0.67, 0.74),
                              vec3(1.0, 0.98, 0.93),
                              saturate(direction.y * 1.7));
        color = mix(color, cloudShade, cloud * 0.62);
    }

    vec3 sunDirection = normalize(dot(rp.lighting.xyz, rp.lighting.xyz) > 0.01
        ? rp.lighting.xyz : vec3(-0.45, 0.72, 0.38));
    float sunDot = max(dot(direction, sunDirection), 0.0);
    float sunDisc = direction.y >= -0.01 ? pow(sunDot, 1400.0) : 0.0;
    float sunHalo = direction.y >= -0.03 ? pow(sunDot, 10.0) : 0.0;
    color += vec3(1.0, 0.75, 0.43) * sunHalo * 0.18;
    color += vec3(1.0, 0.91, 0.72) * sunDisc * 4.2;

    // Atmospheric perspective keeps the infinite grass plane and sky joined
    // at the horizon, including in water and PVC reflections.
    float horizonFog = exp(-abs(direction.y) * 24.0);
    color = mix(color, horizon, horizonFog * 0.22);
    return color;
}

vec3 applyClearPvc(vec3 background, vec3 ro, vec3 rd,
                   float firstVisibleDistance) {
    ClearPoolHit hit = traceClearPool(ro, rd, firstVisibleDistance);
    if (!hit.valid) return background;

    vec2 centre, halfExtent;
    float cornerRadius, bottomY, topY, thickness;
    poolMembraneDimensions(centre, halfExtent, cornerRadius,
                           bottomY, topY, thickness);
    // A transparent surface behind an already visible object or liquid entry
    // must not be painted over it.  The small positive allowance only rejects
    // numerically co-planar/behind hits; it does not turn the membrane opaque.
    float depthAllowance = max(0.002, 0.18 * thickness);
    if (hit.t + depthAllowance >= firstVisibleDistance) return background;

    vec3 n = dot(hit.normal, rd) < 0.0 ? hit.normal : -hit.normal;
    vec3 p = hit.position;
    vec3 wrinkle = vec3(
        sin(p.y * 43.0 + p.z * 8.5),
        0.72 * sin(p.y * 31.0 + p.x * 5.4 - p.z * 6.1),
        cos(p.y * 39.0 - p.x * 8.0));
    wrinkle -= n * dot(wrinkle, n);
    n = normalize(n + wrinkle * 0.032);

    const float pvcIor = 1.50;
    float cosine = saturate(dot(-rd, n));
    float fresnel = dielectricFresnel(cosine, 1.0, pvcIor);
    float opticalPath = min(thickness / max(cosine, 0.12), 0.12);
    // Clear soft PVC absorbs red slightly faster than blue: almost neutral at
    // normal incidence, subtly cyan at thickness/grazing angles.
    vec3 transmission = exp(-vec3(1.35, 0.38, 0.20) * opticalPath);
    vec3 reflectedDirection = normalize(reflect(rd, n));
    vec3 reflected = sky(reflectedDirection);
    vec3 color = background * transmission * (1.0 - fresnel) +
                 reflected * fresnel;

    float weldDistance = min(abs(p.y - bottomY), abs(p.y - topY));
    float weldedEdge = 1.0 - smoothstep(0.008, 0.030, weldDistance);
    color = mix(color, vec3(0.58, 0.82, 0.90), weldedEdge * 0.055);
    vec3 sunDirection = normalize(dot(rp.lighting.xyz, rp.lighting.xyz) > 0.01
        ? rp.lighting.xyz : vec3(-0.45, 0.72, 0.38));
    float glint = pow(max(dot(reflectedDirection, sunDirection), 0.0), 260.0);
    color += vec3(1.0, 0.91, 0.76) * glint *
             mix(0.025, 0.22, fresnel);
    return color;
}

vec3 grassAlbedo(vec2 p, float distanceToCamera) {
    float macro = lowFrequencyNoise2(p * 0.22);
    float clump = valueNoise2(p * 1.65 + vec2(4.7, -2.3));
    float fine = hash12(floor(p * 18.0));
    float detailFade = 1.0 - smoothstep(10.0, 34.0, distanceToCamera);
    float lushness = saturate(0.24 + 0.62 * macro +
                              0.18 * clump * detailFade);
    vec3 color = mix(vec3(0.026, 0.095, 0.018),
                     vec3(0.155, 0.345, 0.060), lushness);
    float mowingStripe = 0.5 + 0.5 * sin((p.x + 0.16 * p.y) * 1.22);
    color *= mix(0.92, 1.06, mowingStripe);
    color *= mix(0.90, 1.08, fine * detailFade);
    float distanceHaze = smoothstep(30.0, 105.0, distanceToCamera);
    return mix(color, vec3(0.28, 0.43, 0.19), distanceHaze * 0.68);
}

vec3 grassNormal(vec2 p, float distanceToCamera) {
    float detailFade = 1.0 - smoothstep(9.0, 32.0, distanceToCamera);
    float nx = sin(p.x * 11.0 + p.y * 4.3) +
               0.45 * sin(p.x * 23.0 - p.y * 8.0);
    float nz = cos(p.y * 12.0 - p.x * 3.7) +
               0.45 * cos(p.y * 21.0 + p.x * 7.0);
    return normalize(vec3(0.040 * nx * detailFade, 1.0,
                          0.040 * nz * detailFade));
}

bool insidePoolFootprint(vec2 p) {
    vec2 centre, halfExtent;
    float cornerRadius, bottomY, topY, thickness;
    poolMembraneDimensions(centre, halfExtent, cornerRadius,
                           bottomY, topY, thickness);
    return roundedRectangleDistance(p - centre, halfExtent,
                                    cornerRadius) < 0.0;
}

SceneHit traceOpaque(vec3 ro, vec3 rd) {
    SceneHit hit = emptyHit();
    uint bodyCount = min(rp.scene.x, MAX_RENDER_BODIES);
    for (uint index = 0u; index < bodyCount; ++index)
        traceBody(bodyStateBuffer.bodies[index], ro, rd, hit);
    tracePool(ro, rd, hit);

    float floorY = rp.pool.z - max(rp.pool.w, 0.008) - 0.002;
    if (rd.y < -1.0e-6) {
        float t = (floorY - ro.y) / rd.y;
        if (t > 0.001 && t < hit.t) {
            vec3 p = ro + rd * t;
            // The analytic plane is infinite.  Multi-scale colour variation
            // and a distance-faded micro-normal read as grass near the pool
            // while converging smoothly into the atmospheric horizon.
            vec3 color = grassAlbedo(p.xz, t);
            vec3 normal = grassNormal(p.xz, t);
            acceptHit(t, normal, color, 0.91,
                      HIT_FLOOR, hit);
        }
    }
    return hit;
}

float densityAt(vec3 p);
float fluidSunVisibility(vec3 p, vec3 lightDirection);

vec3 shadeOpaqueHit(SceneHit hit, vec3 ro, vec3 rd) {
    if (hit.kind == HIT_NONE) return sky(rd);
    vec3 p = ro + rd * hit.t;
    vec3 n = dot(hit.normal, rd) < 0.0 ? hit.normal : -hit.normal;
    vec3 lightDirection = normalize(dot(rp.lighting.xyz, rp.lighting.xyz) > 0.01
        ? rp.lighting.xyz : vec3(-0.45, 0.72, 0.38));
    float visibility = 1.0;
    if (hit.kind == HIT_FLOOR || hit.kind == HIT_POOL ||
        hit.kind == HIT_POOL_LINER)
        visibility = fluidSunVisibility(p + n * 0.012, lightDirection);
    float shadowAmbient = hit.kind == HIT_FLOOR ? 0.52 : 0.66;
    float shadowFactor = mix(shadowAmbient, 1.0, visibility);
    float diffuse = max(dot(n, lightDirection), 0.0) * shadowFactor;
    vec3 halfVector = normalize(lightDirection - rd);
    float specularPower = mix(180.0, 18.0, hit.roughness);
    float specular = pow(max(dot(n, halfVector), 0.0), specularPower);
    float rim = pow(1.0 - saturate(dot(n, -rd)), 3.0);
    vec3 ambient = sky(n) * (0.13 + 0.08 * rim);
    vec3 color = hit.color * (0.22 + 0.78 * diffuse) + ambient * hit.color;
    color += vec3(1.0, 0.86, 0.68) * specular * shadowFactor *
             mix(0.90, 0.16, hit.roughness);
    // The opaque pool liner is the first surface below the water, so the
    // underwater caustic belongs on it rather than on the hidden grass plane.
    if (hit.kind == HIT_POOL_LINER) {
        float caustic = pow(0.5 + 0.5 *
            sin(p.x * 17.0 + sin(p.z * 13.0 + rp.cameraTime.w)) *
            sin(p.z * 19.0 - rp.cameraTime.w * 1.2), 9.0);
        color += vec3(0.04, 0.32, 0.42) * caustic * 0.18;
    }
    if (hit.kind == HIT_FLOOR) {
        float atmosphericHaze = smoothstep(24.0, 105.0, hit.t);
        vec3 horizonDirection = normalize(vec3(rd.x, 0.012, rd.z));
        color = mix(color, sky(horizonDirection),
                    atmosphericHaze * 0.84);
    }
    return color;
}

vec3 sampleOpaque(vec3 ro, vec3 rd) {
    return shadeOpaqueHit(traceOpaque(ro, rd), ro, rd);
}

vec3 worldToUv(vec3 p) {
    return (p - rp.volumeMinIso.xyz) /
           max(rp.volumeMaxStep.xyz - rp.volumeMinIso.xyz, vec3(1.0e-5));
}
float densityAt(vec3 p) {
    vec3 uvw = worldToUv(p);
    // Do not sample the clamped border texels as liquid.  Treating a non-zero
    // edge voxel as extending forever created planar box faces at the pool
    // sides/bottom and was a major source of the dark solid-slab appearance.
    const float boundaryEpsilon = 1.0e-4;
    if (any(lessThanEqual(uvw, vec3(boundaryEpsilon))) ||
        any(greaterThanEqual(uvw, vec3(1.0 - boundaryEpsilon)))) return 0.0;
    return texture(densityVolume, uvw).r;
}

float whitewaterAt(vec3 p) {
    vec3 uvw = worldToUv(p);
    if (any(lessThan(uvw, vec3(0.0))) ||
        any(greaterThan(uvw, vec3(1.0)))) return 0.0;
    return texture(whitewaterVolume, uvw).r;
}

float fluidSunVisibility(vec3 p, vec3 lightDirection) {
    vec2 range = intersectBox(p, lightDirection, rp.volumeMinIso.xyz,
                              rp.volumeMaxStep.xyz);
    float startT = max(range.x, 0.0);
    float endT = range.y;
    if (endT <= startT) return 1.0;

    float stepLength = max(0.085, (endT - startT) /
                                    float(MAX_SHADOW_STEPS));
    float opticalDepth = 0.0;
    float t = startT + 0.5 * stepLength;
    for (int i = 0; i < MAX_SHADOW_STEPS; ++i) {
        if (t >= endT) break;
        opticalDepth += max(densityAt(p + lightDirection * t) - 0.035, 0.0) *
                        stepLength;
        if (opticalDepth > 2.8) break;
        t += stepLength;
    }
    return exp(-1.25 * opticalDepth);
}

float whitewaterAlongRay(vec3 ro, vec3 rd, float nearT, float farT) {
    if (farT <= nearT) return 0.0;
    float stepLength = max((farT - nearT) /
                           float(MAX_WHITEWATER_STEPS), 0.018);
    float t = nearT + 0.5 * stepLength;
    float opticalDepth = 0.0;
    for (int i = 0; i < MAX_WHITEWATER_STEPS; ++i) {
        if (t >= farT) break;
        float foam = whitewaterAt(ro + rd * t);
        opticalDepth += foam * stepLength * 1.35;
        if (opticalDepth > 2.4) break;
        t += stepLength;
    }
    return 1.0 - exp(-opticalDepth);
}
vec3 liquidNormal(vec3 p) {
    vec3 volumeSize = rp.volumeMaxStep.xyz - rp.volumeMinIso.xyz;
    vec3 cellSize = volumeSize /
                    max(vec3(textureSize(densityVolume, 0)), vec3(1.0));
    // The five-tap reconstruction already removes particle-cell dimples, so a
    // four-voxel shading gradient only turns refraction into broad oil-slick
    // colour patches. Keep one-voxel silhouette detail and blend toward a
    // modest 2.5-voxel gradient where the field is reliable.
    vec3 detailOffset = cellSize * 1.00;
    vec3 smoothOffset = cellSize * 2.50;
    vec3 detailGradient = vec3(
        (densityAt(p + vec3(detailOffset.x, 0, 0)) -
         densityAt(p - vec3(detailOffset.x, 0, 0))) /
            max(detailOffset.x, 1.0e-5),
        (densityAt(p + vec3(0, detailOffset.y, 0)) -
         densityAt(p - vec3(0, detailOffset.y, 0))) /
            max(detailOffset.y, 1.0e-5),
        (densityAt(p + vec3(0, 0, detailOffset.z)) -
         densityAt(p - vec3(0, 0, detailOffset.z))) /
            max(detailOffset.z, 1.0e-5));
    vec3 smoothGradient = vec3(
        (densityAt(p + vec3(smoothOffset.x, 0, 0)) -
         densityAt(p - vec3(smoothOffset.x, 0, 0))) /
            max(smoothOffset.x, 1.0e-5),
        (densityAt(p + vec3(0, smoothOffset.y, 0)) -
         densityAt(p - vec3(0, smoothOffset.y, 0))) /
            max(smoothOffset.y, 1.0e-5),
        (densityAt(p + vec3(0, 0, smoothOffset.z)) -
         densityAt(p - vec3(0, 0, smoothOffset.z))) /
            max(smoothOffset.z, 1.0e-5));
    float smoothReliability = smoothstep(0.035, 0.35,
                                         length(smoothGradient));
    vec3 gradient = mix(detailGradient, smoothGradient,
                        0.72 * smoothReliability);
    return -normalize(gradient + vec3(1.0e-7));
}

bool findLiquidSurface(vec3 ro, vec3 rd, float nearT, float farT,
                       out float hitT, out float stepLength) {
    uint requestedSteps = clamp(rp.render.x, 24u,
                                uint(MAX_PRIMARY_STEPS));
    vec3 volumeSize = rp.volumeMaxStep.xyz - rp.volumeMinIso.xyz;
    vec3 voxelSize = volumeSize /
                     max(vec3(textureSize(densityVolume, 0)), vec3(1.0));
    float minimumVoxelSize = max(min(voxelSize.x,
                                     min(voxelSize.y, voxelSize.z)),
                                 1.0e-5);
    float requestedStepLength = (farT - nearT) / float(requestedSteps) *
                                max(rp.volumeMaxStep.w, 0.25);
    // Uniform 0.75-voxel-or-smaller marching is deliberately conservative.
    // In particular, do not reintroduce an empty-space multiplier here unless
    // it is guarded by a conservative occupancy/max-density hierarchy.
    stepLength = max(min(requestedStepLength,
                         minimumVoxelSize * 0.75), 1.0e-5);
    int marchSteps = min(int(ceil((farT - nearT) / stepLength)) + 2,
                         MAX_PRIMARY_STEPS);
    float iso = rp.volumeMinIso.w;
    float entryDensity = densityAt(ro + rd * nearT) - iso;

    // When the camera starts inside liquid, treat the box entry as a hit.
    if (entryDensity > 0.0) {
        hitT = nearT;
        return true;
    }

    // Stable sub-step jitter suppresses banding.  Its amplitude plus the
    // 0.75-voxel base stride still keeps the largest first interval below one
    // minimum world-space voxel.  All later intervals use the same stride.
    float jitter = (hash12(gl_FragCoord.xy) - 0.5) * stepLength * 0.20;
    float previousT = clamp(nearT + jitter, nearT, farT);
    float previousDensity = densityAt(ro + rd * previousT) - iso;

    for (int i = 0; i < MAX_PRIMARY_STEPS; ++i) {
        if (i >= marchSteps || previousT >= farT) break;
        float t = min(previousT + stepLength, farT);
        float density = densityAt(ro + rd * t) - iso;
        if (previousDensity <= 0.0 && density > 0.0) {
            float a = previousT;
            float b = t;
            for (int refine = 0; refine < 5; ++refine) {
                float m = 0.5 * (a + b);
                if (densityAt(ro + rd * m) > iso) b = m; else a = m;
            }
            hitT = 0.5 * (a + b);
            return true;
        }
        previousT = t;
        previousDensity = density;
        if (t >= farT) break;
    }
    return false;
}

bool traceLiquidInterior(vec3 ro, vec3 rd, float maximumDistance,
                         float stride, out float exitT,
                         out float opticalThickness) {
    opticalThickness = 0.0;
    exitT = 0.0;
    vec2 range = intersectBox(ro, rd, rp.volumeMinIso.xyz,
                              rp.volumeMaxStep.xyz);
    float farT = min(range.y, maximumDistance);
    if (farT <= 0.0) return false;

    float iso = rp.volumeMinIso.w;
    stride = max(stride, 0.002);
    float previousT = 0.0;
    float previousDensity = densityAt(ro) - iso;
    bool observedInterior = previousDensity > 0.0;

    for (int i = 0; i < MAX_SECONDARY_STEPS; ++i) {
        if (previousT >= farT) break;
        float t = min(previousT + stride, farT);
        float density = densityAt(ro + rd * t) - iso;
        if (observedInterior) {
            float midpointDensity = max(0.5 *
                (max(previousDensity, 0.0) + max(density, 0.0)), 0.0);
            opticalThickness += (t - previousT) *
                saturate(midpointDensity / max(1.0 - iso, 1.0e-4));
        } else if (density > 0.0) {
            observedInterior = true;
        }

        if (observedInterior && previousDensity > 0.0 && density <= 0.0) {
            float a = previousT;
            float b = t;
            for (int refine = 0; refine < 5; ++refine) {
                float m = 0.5 * (a + b);
                if (densityAt(ro + rd * m) > iso) a = m; else b = m;
            }
            exitT = 0.5 * (a + b);
            return true;
        }
        previousT = t;
        previousDensity = density;
    }
    // If the reconstructed liquid reaches the finite density-volume boundary,
    // that boundary is still a water-to-air event.  Returning "no exit" here
    // would shade the environment without applying Snell's law.  densityAt()
    // normally forces the edge negative, but retain this explicit fallback for
    // coarse strides and floating-point edge cases.
    if (observedInterior && previousDensity > 0.0 &&
        previousT >= farT - 1.0e-5 &&
        range.y <= maximumDistance + 1.0e-5) {
        exitT = farT;
        return true;
    }
    return false;
}

vec3 liquidTransmittance(float opticalThickness) {
    // The reference splat stores density in the hundreds and multiplies the
    // density excess before Beer-Lambert evaluation. Our normalized field
    // otherwise under-attenuates by several times, letting saturated floor
    // tiles shine through thick water. Preserve the reference's red-heavy
    // spectral ratio while calibrating it to this scene's metre scale.
    // Lightened from (30,10,8): with the deeper pool the old extinction went
    // near-black and hid the sunken sphere; the pale-blue balance keeps red
    // absorbed first while letting submerged toys stay readable.
    return exp(-max(opticalThickness, 0.0) * vec3(12.0, 3.6, 2.5));
}

float probeLiquidThickness(vec3 ro, vec3 rd, float maximumDistance) {
    vec2 range = intersectBox(ro, rd, rp.volumeMinIso.xyz,
                              rp.volumeMaxStep.xyz);
    float nearT = max(range.x, 0.0);
    float farT = min(range.y, maximumDistance);
    if (farT <= nearT || dot(rd, rd) < 0.5) return 0.0;

    float segmentLength = farT - nearT;
    float stepLength = segmentLength / float(REFRACTION_PROBE_STEPS);
    float iso = rp.volumeMinIso.w;
    float thickness = 0.0;
    for (int i = 0; i < REFRACTION_PROBE_STEPS; ++i) {
        float t = nearT + (float(i) + 0.5) * stepLength;
        float normalizedDensity =
            max(densityAt(ro + rd * t) - iso, 0.0) /
            max(1.0 - iso, 1.0e-4);
        thickness += saturate(normalizedDensity) * stepLength;
    }
    return thickness;
}

float integrateLiquidThickness(vec3 ro, vec3 rd, float maximumDistance) {
    vec2 range = intersectBox(ro, rd, rp.volumeMinIso.xyz,
                              rp.volumeMaxStep.xyz);
    float nearT = max(range.x, 0.0);
    float farT = min(range.y, maximumDistance);
    if (farT <= nearT || dot(rd, rd) < 0.5) return 0.0;

    float stepLength = (farT - nearT) / float(MAX_SHADOW_STEPS);
    float iso = rp.volumeMinIso.w;
    float thickness = 0.0;
    for (int i = 0; i < MAX_SHADOW_STEPS; ++i) {
        float t = nearT + (float(i) + 0.5) * stepLength;
        float normalizedDensity =
            max(densityAt(ro + rd * t) - iso, 0.0) /
            max(1.0 - iso, 1.0e-4);
        thickness += saturate(normalizedDensity) * stepLength;
    }
    return thickness;
}

bool findNextLiquidEntry(vec3 ro, vec3 rd, float maximumDistance,
                         out float entryT, out float stepLength) {
    vec2 range = intersectBox(ro, rd, rp.volumeMinIso.xyz,
                              rp.volumeMaxStep.xyz);
    float nearT = max(range.x, 0.0);
    float farT = min(range.y, maximumDistance);
    if (farT <= nearT) {
        entryT = 0.0;
        stepLength = 0.0;
        return false;
    }
    return findLiquidSurface(ro, rd, nearT, farT, entryT, stepLength);
}

void main() {
    vec2 screen = inUV * 2.0 - 1.0;
    screen.x *= rp.targetAspect.w;
    vec3 ro = rp.cameraTime.xyz;
    vec3 forward = normalize(rp.targetAspect.xyz - ro);
    vec3 worldUp = abs(forward.y) > 0.985
        ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(forward, worldUp));
    vec3 up = normalize(cross(right, forward));
    float tanHalfFov = tan(46.0 * PI / 360.0);
    vec3 rd = normalize(forward + tanHalfFov *
                        (screen.x * right - screen.y * up));

    SceneHit opaque = traceOpaque(ro, rd);
    vec2 volumeHit = intersectBox(ro, rd, rp.volumeMinIso.xyz,
                                 rp.volumeMaxStep.xyz);
    float nearT = max(volumeHit.x, 0.0);
    float farT = volumeHit.y;
    float liquidHitT = 0.0;
    float primaryStep = 0.0;
    bool hitLiquid = farT > nearT && findLiquidSurface(
        ro, rd, nearT, farT, liquidHitT, primaryStep);

    // This comparison is the essential water-front/water-back ordering rule.
    if (!hitLiquid || opaque.t < liquidHitT) {
        vec3 color = shadeOpaqueHit(opaque, ro, rd);
        float foamLimit = farT;
        if (opaque.kind != HIT_NONE) foamLimit = min(foamLimit, opaque.t);
        float airborneWhitewater = farT > nearT
            ? whitewaterAlongRay(ro, rd, nearT, foamLimit) : 0.0;
        color = mix(color, vec3(0.76, 0.90, 0.94),
                    saturate(airborneWhitewater * 0.30));
        float firstVisibleDistance = opaque.kind != HIT_NONE
            ? opaque.t : FAR_DISTANCE;
        color = applyClearPvc(color, ro, rd, firstVisibleDistance);
        float vignette = saturate(1.0 - 0.16 *
            dot(screen * vec2(0.42, 0.68), screen * vec2(0.42, 0.68)));
        outColor = vec4(finishColor(color, vignette), 1.0);
        return;
    }

    vec3 surface = ro + rd * liquidHitT;
    vec3 cellSize = (rp.volumeMaxStep.xyz - rp.volumeMinIso.xyz) /
                    max(vec3(textureSize(densityVolume, 0)), vec3(1.0));
    float interfaceNudge = max(0.22 * min(cellSize.x,
                                          min(cellSize.y, cellSize.z)),
                               0.004);
    float minimumVoxelSize = min(cellSize.x, min(cellSize.y, cellSize.z));
    // Keep the secondary march below one voxel, but large enough that its
    // fixed 256-step budget covers the 6.9 m volume diagonal.  Coupling this
    // stride to the much smaller interface nudge previously exhausted the
    // loop after ~2.25 m and could invent a premature exit.
    float secondaryStride = max(0.70 * minimumVoxelSize, 0.002);

    // Iterative bounded Fresnel path, adapted from the reference algorithm.
    // At every real density-field interface, accumulate the cheaper branch's
    // environment contribution and continue along the branch most likely to
    // encounter liquid. This captures internal reflections and re-entry
    // without an exponential recursive ray tree.
    vec3 radiance = vec3(0.0);
    vec3 throughput = vec3(1.0);
    vec3 rayPosition = surface;
    vec3 rayDirection = rd;
    bool travellingThroughLiquid = false;
    bool pathFinished = false;
    vec3 firstNormal = liquidNormal(surface);
    if (dot(firstNormal, rd) > 0.0) firstNormal = -firstNormal;

    for (int bounce = 0; bounce < MAX_REFRACTION_BOUNCES; ++bounce) {
        vec3 interfaceNormal = liquidNormal(rayPosition);
        if (dot(interfaceNormal, rayDirection) > 0.0)
            interfaceNormal = -interfaceNormal;

        float etaIncident = travellingThroughLiquid ? 1.333 : 1.0;
        float etaTransmitted = travellingThroughLiquid ? 1.0 : 1.333;
        float cosine = saturate(dot(-rayDirection, interfaceNormal));
        float reflectWeight = dielectricFresnel(
            cosine, etaIncident, etaTransmitted);
        vec3 reflectedDirection = normalize(
            reflect(rayDirection, interfaceNormal));
        vec3 refractedDirection = refract(
            rayDirection, interfaceNormal,
            etaIncident / etaTransmitted);
        bool hasRefraction = dot(refractedDirection,
                                 refractedDirection) > 1.0e-8;
        if (hasRefraction)
            refractedDirection = normalize(refractedDirection);
        else
            reflectWeight = 1.0;
        float refractWeight = 1.0 - reflectWeight;

        // Move each branch onto the correct side of the interface.  A nudge
        // only along the ray has almost no normal displacement at grazing
        // angles and can immediately hit the same surface again; on a thin
        // sheet it can also jump across both sides. interfaceNormal points
        // into the incident medium, so reflection stays on +N and refraction
        // begins on -N.
        vec3 reflectedOrigin = rayPosition +
            interfaceNormal * interfaceNudge +
            reflectedDirection * (0.10 * interfaceNudge);
        vec3 refractedOrigin = hasRefraction
            ? rayPosition - interfaceNormal * interfaceNudge +
              refractedDirection * (0.10 * interfaceNudge)
            : rayPosition;

        const float probeDistance = 0.80;
        float reflectedThickness = probeLiquidThickness(
            reflectedOrigin, reflectedDirection, probeDistance);
        float refractedThickness = hasRefraction
            ? probeLiquidThickness(
                refractedOrigin, refractedDirection, probeDistance)
            : 0.0;

        float reflectedImportance = reflectWeight * reflectedThickness;
        float refractedImportance = refractWeight * refractedThickness;
        bool probesAreEmpty = max(reflectedThickness,
                                  refractedThickness) < 1.0e-5;
        bool followRefraction = hasRefraction && (probesAreEmpty
            ? refractWeight > reflectWeight
            : refractedImportance > reflectedImportance);

        vec3 discardedDirection = followRefraction
            ? reflectedDirection : refractedDirection;
        float discardedWeight = followRefraction
            ? reflectWeight : refractWeight;
        float discardedThickness = followRefraction
            ? reflectedThickness : refractedThickness;
        if (discardedWeight > 1.0e-5 &&
            dot(discardedDirection, discardedDirection) > 0.5) {
            vec3 discardedOrigin = followRefraction
                ? reflectedOrigin : refractedOrigin;
            radiance += throughput * discardedWeight *
                liquidTransmittance(discardedThickness) *
                sampleOpaque(discardedOrigin, discardedDirection);
        }

        float followedWeight = followRefraction
            ? refractWeight : reflectWeight;
        rayDirection = followRefraction
            ? refractedDirection : reflectedDirection;
        if (followRefraction)
            travellingThroughLiquid = !travellingThroughLiquid;
        throughput *= followedWeight;

        vec3 nextOrigin = followRefraction
            ? refractedOrigin : reflectedOrigin;
        if (bounce == MAX_REFRACTION_BOUNCES - 1 ||
            max(throughput.r, max(throughput.g, throughput.b)) < 0.005) {
            SceneHit tailHit = traceOpaque(nextOrigin, rayDirection);
            float tailLimit = tailHit.kind != HIT_NONE
                ? tailHit.t : FAR_DISTANCE;
            float tailThickness = travellingThroughLiquid
                ? integrateLiquidThickness(nextOrigin, rayDirection,
                                           tailLimit)
                : 0.0;
            radiance += throughput * liquidTransmittance(tailThickness) *
                shadeOpaqueHit(tailHit, nextOrigin, rayDirection);
            pathFinished = true;
            break;
        }

        SceneHit nextOpaque = traceOpaque(nextOrigin, rayDirection);
        float eventLimit = nextOpaque.kind != HIT_NONE
            ? nextOpaque.t : FAR_DISTANCE;
        float nextSurfaceT = 0.0;
        float segmentThickness = 0.0;
        float nextStep = secondaryStride;
        bool foundNextSurface;
        if (travellingThroughLiquid) {
            foundNextSurface = traceLiquidInterior(
                nextOrigin, rayDirection, eventLimit, secondaryStride,
                nextSurfaceT, segmentThickness);
            throughput *= liquidTransmittance(segmentThickness);
        } else {
            foundNextSurface = findNextLiquidEntry(
                nextOrigin, rayDirection, eventLimit,
                nextSurfaceT, nextStep);
        }

        if (!foundNextSurface) {
            radiance += throughput * shadeOpaqueHit(
                nextOpaque, nextOrigin, rayDirection);
            pathFinished = true;
            break;
        }
        rayPosition = nextOrigin + rayDirection * nextSurfaceT;
    }

    if (!pathFinished) {
        vec3 tailOrigin = rayPosition + rayDirection * interfaceNudge;
        SceneHit tailHit = traceOpaque(tailOrigin, rayDirection);
        float tailLimit = tailHit.kind != HIT_NONE
            ? tailHit.t : FAR_DISTANCE;
        float tailThickness = travellingThroughLiquid
            ? integrateLiquidThickness(tailOrigin, rayDirection, tailLimit)
            : 0.0;
        radiance += throughput * liquidTransmittance(tailThickness) *
            shadeOpaqueHit(tailHit, tailOrigin, rayDirection);
    }

    vec3 color = radiance;

    float foamAtSurface = max(
        whitewaterAt(surface),
        whitewaterAt(surface - firstNormal * length(cellSize) * 0.85));
    float airborneWhitewater = whitewaterAlongRay(
        ro, rd, nearT, min(farT, liquidHitT + 2.2 * primaryStep));
    float whitewater = saturate(
        0.14 * smoothstep(0.22, 0.90, foamAtSurface) +
        0.07 * airborneWhitewater);
    color = mix(color, vec3(0.76, 0.91, 0.95), whitewater);
    color = applyClearPvc(color, ro, rd, liquidHitT);

    float vignette = saturate(1.0 - 0.16 *
        dot(screen * vec2(0.42, 0.70), screen * vec2(0.42, 0.70)));
    outColor = vec4(finishColor(color, vignette), 1.0);
}
