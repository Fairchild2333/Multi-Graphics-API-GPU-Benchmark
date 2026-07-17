#include "cinematic_liquid_v2_common.hlsli"

// ===== CSG2P =====
// Cinematic Liquid v2 pass 5: APIC grid-to-particle transfer, advection and
// a particle-level compound-body tunnelling guard.  Any final velocity change
// made by that guard is returned to BodyImpulse, preserving two-way coupling.



static const int BODY_DUCK = 0;
static const int BODY_PLAY_BALL = 1;
static const int BODY_ANCHORED_BOAT = 2;
static const int BODY_SINK_BALL = 3;
static const uint MAX_BODIES = 32u;
static const float DAM_GATE_X = -0.66;
static const float DAM_GATE_HALF_Z = 1.36;
static const float DAM_GATE_TOP = 1.72;
// The scored choreography re-stages the complete particle block at 4 s, so
// the reference-style dam is released immediately instead of being allowed to
// slump vertically behind an invisible gate before its horizontal release.
static const float DAM_GATE_RELEASE_TIME = 0.0;
static const float kBodyContributionLimit = 16777215.0;














float gridFixedScale() { return max(abs(material.z), 1.0); }
float bodyFixedScale() { return max(abs(coupling.x), 1.0); }
float decodeGrid(int v) { return float(v) / gridFixedScale(); }
int encodeBody(float v) {
    return int(round(clamp(v * bodyFixedScale(),
                           -kBodyContributionLimit,
                            kBodyContributionLimit)));
}
float3 quadraticWeights(float x) {
    float a = 1.5 - x;
    float b = x - 1.0;
    float c = x - 0.5;
    return float3(0.5 * a * a, 0.75 - b * b, 0.5 * c * c);
}
bool insideGrid(int3 n, int3 s) {
    return all(n >= int3(0,0,0)) && all(n < s);
}
uint gridIndex(int3 n) {
    uint3 c = uint3(n);
    return c.x + gridSizeAndCount.x *
                 (c.y + gridSizeAndCount.y * c.z);
}

float4 safeQuaternion(float4 q) {
    float q2 = dot(q, q);
    return q2 > 1.0e-10 ? q * rsqrt(q2) : float4(0.0, 0.0, 0.0, 1.0);
}
float3 rotateByQuaternion(float4 q, float3 v) {
    q = safeQuaternion(q);
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}
float3 inverseRotateByQuaternion(float4 q, float3 v) {
    q = safeQuaternion(q);
    q.xyz = -q.xyz;
    return rotateByQuaternion(q, v);
}
float sdSphere(float3 p, float r) { return length(p) - max(r, 1.0e-4); }
float sdEllipsoid(float3 p, float3 r) {
    r = max(abs(r), float3(1.0e-4, 1.0e-4, 1.0e-4));
    float k0 = length(p / r);
    float k1 = length(p / (r * r));
    return k0 * (k0 - 1.0) / max(k1, 1.0e-5);
}
float sdCapsule(float3 p, float3 a, float3 b, float r) {
    float3 pa = p - a;
    float3 ba = b - a;
    float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1.0e-8), 0.0, 1.0);
    return length(pa - ba * h) - max(r, 1.0e-4);
}
float smoothUnion(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / max(k, 1.0e-5), 0.0, 1.0);
    return lerp(b, a, h) - k * h * (1.0 - h);
}
// Keep identical to mls_mpm_grid_update_v2.comp (and the render frag): one
// plump belly, upswept tail, round head and flat beak, smooth-min blended.
float duckSdf(float3 p, float4 shape0, float4 shape1) {
    float3 bodyRadius = max(abs(shape0.xyz), float3(0.02,0.02,0.02));
    float headRadius = max(shape1.x, 0.02);
    float3 headCenter = float3(shape1.y, shape1.z, 0.0);
    float beakLength = max(shape1.w, 0.02);

    float belly = sdEllipsoid(
        p - float3(0.0, -0.04 * bodyRadius.y, 0.0),
        float3(bodyRadius.x, 0.84 * bodyRadius.y, bodyRadius.z));
    float tail = sdEllipsoid(
        p - float3(-0.72 * bodyRadius.x, 0.28 * bodyRadius.y, 0.0),
        bodyRadius * float3(0.44, 0.30, 0.42));
    float duck = smoothUnion(belly, tail, 0.30 * bodyRadius.y);
    duck = smoothUnion(duck, sdSphere(p - headCenter, headRadius),
                       0.45 * headRadius);
    float3 beakCenter = headCenter +
        float3(headRadius + 0.30 * beakLength, -0.20 * headRadius, 0.0);
    float beak = sdEllipsoid(
        p - beakCenter,
        float3(0.60 * beakLength, 0.24 * headRadius, 0.46 * headRadius));
    return smoothUnion(duck, beak, 0.12 * headRadius);
}
float bodyBoundingRadius(BodyState body) {
    int type = int(round(body.positionType.w));
    if (type == BODY_PLAY_BALL || type == BODY_SINK_BALL)
        return max(body.shape0.x, 0.01);
    if (type == BODY_DUCK) {
        float3 bodyRadius = max(abs(body.shape0.xyz), float3(0.02,0.02,0.02));
        float headReach = length(float2(body.shape1.y, body.shape1.z)) +
                          max(body.shape1.x, 0.02) + max(body.shape1.w, 0.02);
        return max(length(bodyRadius), headReach);
    }
    if (type == BODY_ANCHORED_BOAT) {
        float3 h = max(abs(body.shape0.xyz), float3(0.02,0.02,0.02));
        float aftReach = length(float2(max(body.shape1.y, h.x), 1.15 * h.y)) +
                         0.25 * max(body.shape1.x, 0.02);
        return max(length(h), aftReach);
    }
    return 0.01;
}
float bodySdf(BodyState body, float3 worldPoint) {
    float3 p = inverseRotateByQuaternion(
        body.orientation, worldPoint - body.positionType.xyz);
    int type = int(round(body.positionType.w));
    if (type == BODY_PLAY_BALL || type == BODY_SINK_BALL)
        return sdSphere(p, body.shape0.x);
    if (type == BODY_DUCK)
        return duckSdf(p, body.shape0, body.shape1);
    if (type == BODY_ANCHORED_BOAT) {
        float3 h = max(abs(body.shape0.xyz), float3(0.02,0.02,0.02));
        float propRadius = max(body.shape1.x, 0.02);
        float aft = max(body.shape1.y, h.x);
        float3 a = float3(-0.62 * h.x, -1.15 * h.y, 0.0);
        float3 b = float3(-aft, -1.15 * h.y, 0.0);
        return min(sdEllipsoid(p + float3(0.0, 0.28 * h.y, 0.0), h),
               min(sdCapsule(p, a, b, 0.12 * propRadius),
                   sdSphere(p - b, 0.22 * propRadius)));
    }
    return 1.0e6;
}
float3 bodyNormal(BodyState body, float3 p, float e) {
    float3 d = float3(max(e, 1.0e-4), 0.0, 0.0);
    float3 g = float3(
        bodySdf(body, p + d.xyy) - bodySdf(body, p - d.xyy),
        bodySdf(body, p + d.yxy) - bodySdf(body, p - d.yxy),
        bodySdf(body, p + d.yyx) - bodySdf(body, p - d.yyx));
    return dot(g, g) > 1.0e-12 ? normalize(g) : float3(0.0, 1.0, 0.0);
}

float3 collideVelocity(float3 velocity, float3 surfaceVelocity, float3 normal,
                     float restitution, float friction) {
    float3 relative = velocity - surfaceVelocity;
    float normalSpeed = dot(relative, normal);
    if (normalSpeed >= 0.0) return velocity;
    float3 tangential = relative - normalSpeed * normal;
    relative = tangential * (1.0 - clamp(friction, 0.0, 1.0)) -
               clamp(restitution, 0.0, 1.0) * normalSpeed * normal;
    return surfaceVelocity + relative;
}

void addBodyImpulse(uint index, float3 impulse, float3 torque) {
    InterlockedAdd(bodyImpulseBuffer[index].linImpulse.x, encodeBody(impulse.x));
    InterlockedAdd(bodyImpulseBuffer[index].linImpulse.y, encodeBody(impulse.y));
    InterlockedAdd(bodyImpulseBuffer[index].linImpulse.z, encodeBody(impulse.z));
    InterlockedAdd(bodyImpulseBuffer[index].angImpulse.x, encodeBody(torque.x));
    InterlockedAdd(bodyImpulseBuffer[index].angImpulse.y, encodeBody(torque.y));
    InterlockedAdd(bodyImpulseBuffer[index].angImpulse.z, encodeBody(torque.z));
    InterlockedAdd(bodyImpulseBuffer[index].angImpulse.w, 1);
}

float roundedRectangleDistance(float2 p, float2 halfExtent, float radius) {
    radius = clamp(radius, 0.0, min(halfExtent.x, halfExtent.y));
    float2 q = abs(p) - halfExtent + float2(radius, radius);
    return length(max(q, float2(0,0))) +
           min(max(q.x, q.y), 0.0) - radius;
}

float2 roundedRectangleNormal(float2 p, float2 halfExtent,
                            float radius, float epsilon) {
    float2 ex = float2(max(epsilon, 1.0e-4), 0.0);
    float2 gradient = float2(
        roundedRectangleDistance(p + ex, halfExtent, radius) -
        roundedRectangleDistance(p - ex, halfExtent, radius),
        roundedRectangleDistance(p + ex.yx, halfExtent, radius) -
        roundedRectangleDistance(p - ex.yx, halfExtent, radius));
    float g2 = dot(gradient, gradient);
    return g2 > 1.0e-12 ? gradient * rsqrt(g2) : float2(1.0, 0.0);
}

void constrainSimulationDomain(float radius, inout float3 position,
                               inout float3 velocity) {
    float dx = max(abs(gridOriginDx.w), 1.0e-6);
    float margin = max(material.w, 1.5) * dx + radius;
    float3 domainMin = gridOriginDx.xyz + float3(margin, margin, margin);
    float3 domainMax = gridOriginDx.xyz +
        float3(gridSizeAndCount.xyz - uint3(1u,1u,1u)) * dx - float3(margin, margin, margin);
    domainMin.y = max(domainMin.y, pool.z + radius);

    if (position.x < domainMin.x) {
        position.x = domainMin.x;
        velocity = collideVelocity(velocity, float3(0,0,0), float3(1,0,0),
                                   collision.x, collision.y);
    } else if (position.x > domainMax.x) {
        position.x = domainMax.x;
        velocity = collideVelocity(velocity, float3(0,0,0), float3(-1,0,0),
                                   collision.x, collision.y);
    }
    if (position.y < domainMin.y) {
        position.y = domainMin.y;
        velocity = collideVelocity(velocity, float3(0,0,0), float3(0,1,0),
                                   collision.x, collision.y);
    } else if (position.y > domainMax.y) {
        position.y = domainMax.y;
        velocity = collideVelocity(velocity, float3(0,0,0), float3(0,-1,0),
                                   collision.x, collision.y);
    }
    if (position.z < domainMin.z) {
        position.z = domainMin.z;
        velocity = collideVelocity(velocity, float3(0,0,0), float3(0,0,1),
                                   collision.x, collision.y);
    } else if (position.z > domainMax.z) {
        position.z = domainMax.z;
        velocity = collideVelocity(velocity, float3(0,0,0), float3(0,0,-1),
                                   collision.x, collision.y);
    }

}

void constrainFinitePoolWall(float particleRadius, float3 previousPosition,
                             inout float3 position, inout float3 velocity) {
    float dx = max(abs(gridOriginDx.w), 1.0e-6);
    float3 rawMin = gridOriginDx.xyz;
    float3 rawMax = gridOriginDx.xyz +
        float3(gridSizeAndCount.xyz - uint3(1u,1u,1u)) * dx;
    float wallTop = lerp(rawMin.y, rawMax.y, 0.42);

    // Once a particle has cleared the rim, let it fall on whichever side it
    // reached. This is the real overflow path; the outer simulation boundary
    // remains as a safety catch band.
    if (position.y > wallTop + particleRadius ||
        (previousPosition.y > wallTop + particleRadius &&
         position.y <= wallTop + particleRadius)) return;

    float2 centre = 0.5 * (rawMin.xz + rawMax.xz);
    float2 halfExtent = max(0.5 * (rawMax.xz - rawMin.xz) -
                          float2(max(pool.y, 0.0), max(pool.y, 0.0)),
                          float2(2.0 * dx, 2.0 * dx));
    float cornerRadius = clamp(pool.x, 0.0,
        max(min(halfExtent.x, halfExtent.y) - dx, 0.0));
    float previousDistance = roundedRectangleDistance(
        previousPosition.xz - centre, halfExtent, cornerRadius);
    float nextDistance = roundedRectangleDistance(
        position.xz - centre, halfExtent, cornerRadius);

    float2 outward2 = roundedRectangleNormal(position.xz - centre, halfExtent,
                                            cornerRadius, 0.20 * dx);
    float3 outward = float3(outward2.x, 0.0, outward2.y);
    if (previousDistance <= 0.0 && nextDistance > -particleRadius) {
        position.xz -= outward2 * (nextDistance + particleRadius);
        velocity = collideVelocity(velocity, float3(0,0,0), -outward,
                                   collision.x, collision.y);
    } else if (previousDistance > 0.0 && nextDistance < particleRadius) {
        position.xz += outward2 * (particleRadius - nextDistance);
        velocity = collideVelocity(velocity, float3(0,0,0), outward,
                                   collision.x, collision.y);
    }
}

void constrainDamGate(float radius, float3 previousPosition,
                      inout float3 position, inout float3 velocity) {
    if (pool.w >= DAM_GATE_RELEASE_TIME ||
        abs(position.z) > DAM_GATE_HALF_Z + radius ||
        position.y > DAM_GATE_TOP + radius) return;

    if (previousPosition.x <= DAM_GATE_X &&
        position.x + radius > DAM_GATE_X) {
        position.x = DAM_GATE_X - radius;
        velocity = collideVelocity(velocity, float3(0,0,0),
                                   float3(-1.0, 0.0, 0.0),
                                   collision.x, collision.y);
    } else if (previousPosition.x > DAM_GATE_X &&
               position.x - radius < DAM_GATE_X) {
        position.x = DAM_GATE_X + radius;
        velocity = collideVelocity(velocity, float3(0,0,0),
                                   float3(1.0, 0.0, 0.0),
                                   collision.x, collision.y);
    }
}

[numthreads(256, 1, 1)]
void CSG2P(uint3 DTid : SV_DispatchThreadID)
{

    uint particleIndex = DTid.x;
    if (particleIndex >= gridSizeAndCount.w ||
        any(gridSizeAndCount.xyz < uint3(3u,3u,3u))) return;

    float dx = max(abs(gridOriginDx.w), 1.0e-6);
    float dt = max(simulation.x, 0.0);
    Particle particle = particleBuffer[particleIndex];
    float3 gridPosition = (particle.position.xyz - gridOriginDx.xyz) / dx;
    int3 base = int3(floor(gridPosition - float3(0.5,0.5,0.5)));
    float3 fractional = gridPosition - float3(base);
    float3 wx = quadraticWeights(fractional.x);
    float3 wy = quadraticWeights(fractional.y);
    float3 wz = quadraticWeights(fractional.z);
    int3 gridSize = int3(gridSizeAndCount.xyz);

    float3 newVelocity = float3(0,0,0);
    float3x3 newC = kMat3Zero;
    float validWeight = 0.0;
    float affineScale = 4.0 / (dx * dx);

    for (int z = 0; z < 3; ++z) {
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 3; ++x) {
                int3 offset = int3(x, y, z);
                int3 node = base + offset;
                if (!insideGrid(node, gridSize)) continue;
                uint index = gridIndex(node);
                if (gridBuffer[index].mass <= 0) continue;
                float weight = wx[x] * wy[y] * wz[z];
                float3 gridVelocity = float3(
                    decodeGrid(gridBuffer[index].vx),
                    decodeGrid(gridBuffer[index].vy),
                    decodeGrid(gridBuffer[index].vz));
                float3 dpos = (float3(offset) - fractional) * dx;
                newVelocity += weight * gridVelocity;
                newC += affineScale * weight * OuterProduct(gridVelocity, dpos);
                validWeight += weight;
            }
        }
    }

    if (validWeight > 1.0e-6) {
        newVelocity /= validWeight;
        newC /= validWeight;
    } else {
        newVelocity = float3(0,0,0);
        newC = kMat3Zero;
    }

    float3 newPosition = particle.position.xyz + dt * newVelocity;
    float particleRadius = 0.42 * dx;
    uint bodyCount = min(scene.x, MAX_BODIES);
    float particleMass = max(material.y, 0.0);

    for (uint bodyIndex = 0u; bodyIndex < bodyCount; ++bodyIndex) {
        BodyState body = bodyStateBuffer[bodyIndex];
        // Conservative sphere cull mirroring mls_mpm_grid_update_v2.comp so
        // the seven-body loop stays cheap for the vast majority of particles.
        float3 toBody = newPosition - body.positionType.xyz;
        float cullRadius = bodyBoundingRadius(body) + particleRadius + dx;
        if (dot(toBody, toBody) > cullRadius * cullRadius) continue;
        float distance = bodySdf(body, newPosition);
        if (distance >= particleRadius) continue;
        float3 normal = bodyNormal(body, newPosition, 0.20 * dx);
        newPosition += normal * (particleRadius - distance);
        float3 arm = newPosition - body.positionType.xyz;
        float3 surfaceVelocity = body.linVelInvMass.xyz +
            cross(body.angVelInvInertia.xyz, arm);
        float3 before = newVelocity;
        newVelocity = collideVelocity(
            newVelocity, surfaceVelocity, normal,
            max(collision.x, body.material.x),
            max(collision.y, body.material.y) * max(coupling.z, 0.0));
        float3 bodyImpulse = -particleMass * (newVelocity - before);
        if (dot(bodyImpulse, bodyImpulse) > 1.0e-16)
            addBodyImpulse(bodyIndex, bodyImpulse, cross(arm, bodyImpulse));
    }

    constrainDamGate(particleRadius, particle.position.xyz,
                     newPosition, newVelocity);
    constrainFinitePoolWall(particleRadius, particle.position.xyz,
                            newPosition, newVelocity);
    constrainSimulationDomain(particleRadius, newPosition, newVelocity);
    float speed = length(newVelocity);
    if (collision.w > 0.0 && speed > collision.w)
        newVelocity *= collision.w / max(speed, 1.0e-6);

    particleBuffer[particleIndex].position.xyz = newPosition;
    particleBuffer[particleIndex].velocity.xyz = newVelocity;
    particleBuffer[particleIndex].C0 = float4(newC[0], 0.0);
    particleBuffer[particleIndex].C1 = float4(newC[1], 0.0);
    particleBuffer[particleIndex].C2 = float4(newC[2], 0.0);

}

