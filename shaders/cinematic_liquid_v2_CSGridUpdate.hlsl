#include "cinematic_liquid_v2_common.hlsli"

// ===== CSGridUpdate =====
// Cinematic Liquid v2 pass 4: convert nodal momentum to velocity, enforce the
// finite pool and moving compound-body boundaries, inject the motor-boat
// propeller wake, and atomically return equal-and-opposite impulses to the
// rigid bodies.



static const int BODY_DUCK = 0;
static const int BODY_PLAY_BALL = 1;
static const int BODY_ANCHORED_BOAT = 2;
static const int BODY_SINK_BALL = 3;
static const uint MAX_BODIES = 32u;
static const float DAM_GATE_X = -0.66;
static const float DAM_GATE_HALF_Z = 1.36;
static const float DAM_GATE_TOP = 1.72;
// Keep this identical to mls_mpm_g2p_v2.comp.  A one-time 4 s GPU-state reset
// in the host stages the high block; it then collapses immediately and
// physically, matching the reference scene rather than a delayed gate trick.
static const float DAM_GATE_RELEASE_TIME = 0.0;
static const float kFixedContributionLimit = 536870911.0;
static const float kBodyContributionLimit = 16777215.0;














float gridFixedScale() { return max(abs(material.z), 1.0); }
float bodyFixedScale() { return max(abs(coupling.x), 1.0); }
float decodeGrid(int v) { return float(v) / gridFixedScale(); }
int encodeGrid(float v) {
    return int(round(clamp(v * gridFixedScale(),
                           -kFixedContributionLimit,
                            kFixedContributionLimit)));
}
int encodeBody(float v) {
    return int(round(clamp(v * bodyFixedScale(),
                           -kBodyContributionLimit,
                            kBodyContributionLimit)));
}
uint gridIndex(uint3 c) {
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

// Keep this in sync with mls_mpm_g2p_v2.comp and the duck maths in
// cinematic_liquid_render_v2.frag: every displaced lobe is also the visible
// surface.  shape0.xyz is the belly envelope, shape0.w the righting strength,
// shape1 = (head radius, head forward, head height, beak length).
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
        float hullDistance = sdEllipsoid(
            p + float3(0.0, 0.28 * h.y, 0.0), h);
        float propRadius = max(body.shape1.x, 0.02);
        float aft = max(body.shape1.y, h.x);
        float3 shaftA = float3(-0.62 * h.x, -1.15 * h.y, 0.0);
        float3 shaftB = float3(-aft, -1.15 * h.y, 0.0);
        float shaftDistance = sdCapsule(
            p, shaftA, shaftB, 0.12 * propRadius);
        float hubDistance = sdSphere(p - shaftB, 0.22 * propRadius);
        return min(hullDistance, min(shaftDistance, hubDistance));
    }

    return 1.0e6;
}

float3 bodyNormal(BodyState body, float3 worldPoint, float epsilon) {
    float3 e = float3(max(epsilon, 1.0e-4), 0.0, 0.0);
    float3 gradient = float3(
        bodySdf(body, worldPoint + e.xyy) - bodySdf(body, worldPoint - e.xyy),
        bodySdf(body, worldPoint + e.yxy) - bodySdf(body, worldPoint - e.yxy),
        bodySdf(body, worldPoint + e.yyx) - bodySdf(body, worldPoint - e.yyx));
    float g2 = dot(gradient, gradient);
    return g2 > 1.0e-12 ? gradient * rsqrt(g2) : float3(0.0, 1.0, 0.0);
}

float3 collideStationary(float3 velocity, float3 inwardNormal) {
    float normalSpeed = dot(velocity, inwardNormal);
    if (normalSpeed >= 0.0) return velocity;
    float restitution = clamp(collision.x, 0.0, 1.0);
    float tangentialScale = 1.0 - clamp(collision.y, 0.0, 1.0);
    float3 tangential = velocity - normalSpeed * inwardNormal;
    return tangential * tangentialScale -
           restitution * normalSpeed * inwardNormal;
}

float3 collideBody(float3 velocity, float3 surfaceVelocity, float3 outwardNormal,
                 BodyState body) {
    float3 relative = velocity - surfaceVelocity;
    float normalSpeed = dot(relative, outwardNormal);
    if (normalSpeed >= 0.0) return velocity;
    float restitution = clamp(max(collision.x, body.material.x), 0.0, 1.0);
    float friction = clamp(max(collision.y, body.material.y) *
                           max(coupling.z, 0.0), 0.0, 1.0);
    float3 tangential = relative - normalSpeed * outwardNormal;
    relative = tangential * (1.0 - friction) -
               restitution * normalSpeed * outwardNormal;
    return surfaceVelocity + relative;
}

void addBodyImpulse(uint index, float3 impulse, float3 torque,
                    float displacedMass, bool contact) {
    InterlockedAdd(bodyImpulseBuffer[index].linImpulse.x, encodeBody(impulse.x));
    InterlockedAdd(bodyImpulseBuffer[index].linImpulse.y, encodeBody(impulse.y));
    InterlockedAdd(bodyImpulseBuffer[index].linImpulse.z, encodeBody(impulse.z));
    if (displacedMass > 0.0)
        InterlockedAdd(bodyImpulseBuffer[index].linImpulse.w, max(encodeBody(displacedMass), 0));
    InterlockedAdd(bodyImpulseBuffer[index].angImpulse.x, encodeBody(torque.x));
    InterlockedAdd(bodyImpulseBuffer[index].angImpulse.y, encodeBody(torque.y));
    InterlockedAdd(bodyImpulseBuffer[index].angImpulse.z, encodeBody(torque.z));
    if (contact)
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

void applySimulationDomainBoundary(float3 worldPosition, float dx,
                                   inout float3 velocity) {
    float boundary = max(material.w, 0.0) * dx;
    float3 domainMin = gridOriginDx.xyz + float3(boundary, boundary, boundary);
    float3 domainMax = gridOriginDx.xyz +
        float3(gridSizeAndCount.xyz - uint3(1u,1u,1u)) * dx - float3(boundary, boundary, boundary);

    if (worldPosition.x < domainMin.x + dx)
        velocity = collideStationary(velocity, float3(1.0, 0.0, 0.0));
    if (worldPosition.x > domainMax.x - dx)
        velocity = collideStationary(velocity, float3(-1.0, 0.0, 0.0));
    if (worldPosition.y < max(domainMin.y, pool.z) + dx)
        velocity = collideStationary(velocity, float3(0.0, 1.0, 0.0));
    if (worldPosition.y > domainMax.y - dx)
        velocity = collideStationary(velocity, float3(0.0, -1.0, 0.0));
    if (worldPosition.z < domainMin.z + dx)
        velocity = collideStationary(velocity, float3(0.0, 0.0, 1.0));
    if (worldPosition.z > domainMax.z - dx)
        velocity = collideStationary(velocity, float3(0.0, 0.0, -1.0));
}

void applyFinitePoolWall(float3 worldPosition, float dx,
                         inout float3 velocity) {
    float3 rawMin = gridOriginDx.xyz;
    float3 rawMax = gridOriginDx.xyz +
        float3(gridSizeAndCount.xyz - uint3(1u,1u,1u)) * dx;
    float wallTop = lerp(rawMin.y, rawMax.y, 0.42);
    if (worldPosition.y > wallTop + 1.25 * dx ||
        worldPosition.y < pool.z - dx) return;

    float2 centre = 0.5 * (rawMin.xz + rawMax.xz);
    float2 halfExtent = max(0.5 * (rawMax.xz - rawMin.xz) -
                          float2(max(pool.y, 0.0), max(pool.y, 0.0)),
                          float2(2.0 * dx, 2.0 * dx));
    float radius = clamp(pool.x, 0.0,
                         max(min(halfExtent.x, halfExtent.y) - dx, 0.0));
    float2 relative = worldPosition.xz - centre;
    float wallDistance = roundedRectangleDistance(relative, halfExtent, radius);
    float contactBand = 1.35 * dx;
    if (abs(wallDistance) > contactBand) return;

    float2 outward2 = roundedRectangleNormal(relative, halfExtent,
                                            radius, 0.25 * dx);
    float3 outward = float3(outward2.x, 0.0, outward2.y);
    float outwardSpeed = dot(velocity, outward);
    if (wallDistance <= 0.0 && outwardSpeed > 0.0)
        velocity = collideStationary(velocity, -outward);
    else if (wallDistance > 0.0 && outwardSpeed < 0.0)
        velocity = collideStationary(velocity, outward);
}

void applyDamGate(float3 worldPosition, float dx, inout float3 velocity) {
    if (pool.w >= DAM_GATE_RELEASE_TIME ||
        abs(worldPosition.z) > DAM_GATE_HALF_Z ||
        worldPosition.y > DAM_GATE_TOP ||
        abs(worldPosition.x - DAM_GATE_X) > 1.35 * dx) return;

    // A two-sided temporary wall stores the tall left reservoir.  Removing
    // this constraint releases a genuine MLS-MPM dam-break; no render-only
    // displacement or procedural wave is added.
    if (worldPosition.x <= DAM_GATE_X && velocity.x > 0.0)
        velocity = collideStationary(velocity, float3(-1.0, 0.0, 0.0));
    else if (worldPosition.x > DAM_GATE_X && velocity.x < 0.0)
        velocity = collideStationary(velocity, float3(1.0, 0.0, 0.0));
}

void applyPropeller(BodyState boat, uint bodyIndex, float3 worldPosition,
                    float cellMass, float dt, inout float3 velocity) {
    float radius = max(boat.shape1.x, 0.0);
    if (radius <= 0.0 || coupling.w <= 0.0) return;

    float3 axis = rotateByQuaternion(boat.orientation, float3(-1.0, 0.0, 0.0));
    float3 propellerCentre = boat.positionType.xyz + rotateByQuaternion(
        boat.orientation,
        float3(-max(boat.shape1.y, boat.shape0.x),
             -1.15 * boat.shape0.y, 0.0));
    float3 delta = worldPosition - propellerCentre;
    float axial = dot(delta, axis);
    float3 radialVector = delta - axis * axial;
    float radialDistance = length(radialVector);
    float wakeLength = 5.0 * radius;
    float wakeRadius = radius * (1.0 + 0.18 * max(axial, 0.0) /
                                 max(radius, 1.0e-4));
    if (axial < -0.20 * radius || axial > wakeLength ||
        radialDistance >= wakeRadius) return;

    float diskFade = 1.0 - smoothstep(0.55 * wakeRadius,
                                      wakeRadius, radialDistance);
    float axialFade = exp(-1.7 * max(axial, 0.0) / max(wakeLength, 1.0e-4));
    float influence = diskFade * axialFade;
    float3 radialDirection = radialDistance > 1.0e-5
        ? radialVector / radialDistance
        : rotateByQuaternion(boat.orientation, float3(0.0, 0.0, 1.0));
    float3 swirlDirection = normalize(cross(axis, radialDirection));
    float axialAcceleration = max(boat.shape1.z, 0.0) * coupling.w;
    float swirlAcceleration = abs(boat.shape1.w) * radius *
                              coupling.w * 0.28;
    float3 deltaVelocity = (axis * axialAcceleration +
                          swirlDirection * swirlAcceleration) *
                         (dt * influence);
    velocity += deltaVelocity;

    float3 fluidImpulse = cellMass * deltaVelocity;
    float3 bodyImpulse = -fluidImpulse;
    addBodyImpulse(bodyIndex, bodyImpulse,
                   cross(propellerCentre - boat.positionType.xyz, bodyImpulse),
                   0.0, length(deltaVelocity) > 1.0e-6);
}

[numthreads(8, 8, 4)]
void CSGridUpdate(uint3 DTid : SV_DispatchThreadID)
{

    uint3 coord = DTid.xyz;
    if (any(coord >= gridSizeAndCount.xyz)) return;

    uint index = gridIndex(coord);
    float mass = decodeGrid(gridBuffer[index].mass);
    if (mass <= 0.0) {
        gridBuffer[index].vx = 0;
        gridBuffer[index].vy = 0;
        gridBuffer[index].vz = 0;
        return;
    }

    float dt = max(simulation.x, 0.0);
    float dx = max(abs(gridOriginDx.w), 1.0e-6);
    float3 worldPosition = gridOriginDx.xyz + float3(coord) * dx;
    float3 velocity = float3(decodeGrid(gridBuffer[index].vx),
                         decodeGrid(gridBuffer[index].vy),
                         decodeGrid(gridBuffer[index].vz)) / mass;
    velocity.y += simulation.y * dt;
    applySimulationDomainBoundary(worldPosition, dx, velocity);
    applyFinitePoolWall(worldPosition, dx, velocity);
    applyDamGate(worldPosition, dx, velocity);

    uint bodyCount = min(scene.x, MAX_BODIES);
    float restDensity = max(simulation.z, 1.0e-6);
    float restMassPerCell = restDensity * dx * dx * dx;
    float contactBand = 0.75 * dx;

    for (uint bodyIndex = 0u; bodyIndex < bodyCount; ++bodyIndex) {
        BodyState body = bodyStateBuffer[bodyIndex];
        int type = int(round(body.positionType.w));

        if (type == BODY_ANCHORED_BOAT && bodyIndex == scene.y)
            applyPropeller(body, bodyIndex, worldPosition, mass, dt, velocity);

        // Conservative sphere cull: with the duck family the body list grew
        // to seven, so only evaluate the full compound SDF for nodes that can
        // actually be within the contact band (margin covers the smooth-min
        // and ellipsoid-distance approximations).
        float3 toBody = worldPosition - body.positionType.xyz;
        float cullRadius = bodyBoundingRadius(body) + 2.0 * contactBand + dx;
        if (dot(toBody, toBody) > cullRadius * cullRadius) continue;

        float distance = bodySdf(body, worldPosition);
        if (distance >= contactBand) continue;

        float3 before = velocity;
        float3 normal = bodyNormal(body, worldPosition, 0.25 * dx);
        float3 arm = worldPosition - body.positionType.xyz;
        float3 surfaceVelocity = body.linVelInvMass.xyz +
            cross(body.angVelInvInertia.xyz, arm);

        // Occupied mass nodes approximate displaced volume.  The upward body
        // impulse is paired with an equal downward impulse on the liquid.
        float solidWeight = 1.0 - smoothstep(-contactBand,
                                             contactBand, distance);
        float occupancy = clamp(mass / restMassPerCell, 0.0, 1.0);
        float displacedVolume = dx * dx * dx * solidWeight * occupancy;
        float3 buoyancyImpulse = float3(
            0.0,
            -simulation.y * restDensity * displacedVolume * dt *
                max(coupling.y, 0.0),
            0.0);
        if (mass > 1.0e-8)
            velocity -= buoyancyImpulse / mass;

        velocity = collideBody(velocity, surfaceVelocity, normal, body);

        if (type == BODY_SINK_BALL && body.linVelInvMass.y < -0.75) {
            // PIC/APIC damps the high-frequency ring jet of a sphere entry.
            // Restore that missing pressure redirection in simulation space:
            // nodes around the moving equator receive an outward/upward crown
            // impulse, with the exact opposite included in bodyImpulse below.
            float impactSpeed = -body.linVelInvMass.y;
            float equatorWeight = smoothstep(0.18, 0.88,
                                             1.0 - abs(normal.y));
            float3 lateral = float3(normal.x, 0.0, normal.z);
            float lateralLength = length(lateral);
            if (lateralLength > 1.0e-5 && equatorWeight > 0.0) {
                lateral /= lateralLength;
                float3 crownDirection = normalize(lateral +
                                                float3(0.0, 0.72, 0.0));
                velocity += crownDirection *
                    (0.075 * impactSpeed * solidWeight * equatorWeight);
            }
        }
        // `velocity - before` already contains the explicit downward liquid
        // buoyancy reaction, so its opposite is the complete body impulse.
        // Adding buoyancyImpulse again here would violate momentum balance.
        float3 bodyImpulse = -mass * (velocity - before);
        addBodyImpulse(bodyIndex, bodyImpulse,
                       cross(arm, bodyImpulse),
                       restDensity * displacedVolume, true);
    }

    float speed = length(velocity);
    if (collision.w > 0.0 && speed > collision.w)
        velocity *= collision.w / max(speed, 1.0e-6);

    gridBuffer[index].vx = encodeGrid(velocity.x);
    gridBuffer[index].vy = encodeGrid(velocity.y);
    gridBuffer[index].vz = encodeGrid(velocity.z);

}

