#include "cinematic_liquid_v2_common.hlsli"

// ===== CSRigidIntegrate =====
// Cinematic Liquid v2 rigid integration.  Exactly one 32-lane workgroup must
// be dispatched.  All body states are snapshotted in shared memory before
// integration and again before pair contacts, avoiding cross-workgroup races.



static const int BODY_DUCK = 0;
static const int BODY_PLAY_BALL = 1;
static const int BODY_ANCHORED_BOAT = 2;
static const int BODY_SINK_BALL = 3;
static const uint MAX_BODIES = 32u;
// The sphere must have enough real free-fall time to strike the shallow pool
// before the fixed 5 s capture.  Air drag is handled separately below; this is
// a genuine gravity release, not a prescribed crane trajectory.
static const float SINK_BALL_RELEASE_TIME = 4.28;
static const float SINK_BALL_START_Y = 1.65;
static const float2 BOAT_TETHER_ANCHOR_XZ = float2(1.25, 0.50);














groupshared BodyState initialStates[32];
groupshared BodyState predictedStates[32];

float bodyFixedScale() { return max(abs(coupling.x), 1.0); }
float3 decodeImpulse(int3 v) { return float3(v) / bodyFixedScale(); }

float4 safeQuaternion(float4 q) {
    float q2 = dot(q, q);
    return q2 > 1.0e-10 ? q * rsqrt(q2) : float4(0.0, 0.0, 0.0, 1.0);
}
float4 quaternionMultiply(float4 a, float4 b) {
    return float4(a.w * b.xyz + b.w * a.xyz + cross(a.xyz, b.xyz),
                a.w * b.w - dot(a.xyz, b.xyz));
}
float3 rotateByQuaternion(float4 q, float3 v) {
    q = safeQuaternion(q);
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}
float4 integrateOrientation(float4 q, float3 worldAngularVelocity, float dt) {
    q = safeQuaternion(q);
    q += 0.5 * dt * quaternionMultiply(float4(worldAngularVelocity, 0.0), q);
    return safeQuaternion(q);
}

float bodyBoundingRadius(BodyState body) {
    int type = int(round(body.positionType.w));
    if (type == BODY_PLAY_BALL || type == BODY_SINK_BALL)
        return max(body.shape0.x, 0.01);
    if (type == BODY_DUCK) {
        float bodyRadius = length(max(abs(body.shape0.xyz), float3(0.01,0.01,0.01)));
        float headRadius = max(body.shape1.x, 0.01);
        float headReach = length(float2(body.shape1.y, body.shape1.z)) +
                          headRadius + max(body.shape1.w, 0.0);
        return max(bodyRadius, headReach);
    }
    if (type == BODY_ANCHORED_BOAT)
        return length(max(abs(body.shape0.xyz), float3(0.01,0.01,0.01)));
    return 0.01;
}

float bodyBottomExtent(BodyState body) {
    int type = int(round(body.positionType.w));
    if (type == BODY_PLAY_BALL || type == BODY_SINK_BALL)
        return max(body.shape0.x, 0.01);
    if (type == BODY_ANCHORED_BOAT)
        return max(1.36 * abs(body.shape0.y), 0.04);
    if (type == BODY_DUCK)
        return max(abs(body.shape0.y), 0.04);
    return 0.01;
}

float3 collideVelocity(float3 velocity, float3 normal,
                     float restitution, float friction) {
    float normalSpeed = dot(velocity, normal);
    if (normalSpeed >= 0.0) return velocity;
    float3 tangent = velocity - normalSpeed * normal;
    return tangent * (1.0 - clamp(friction, 0.0, 1.0)) -
           clamp(restitution, 0.0, 1.0) * normalSpeed * normal;
}

void solveBodyPair(uint selfIndex, BodyState other,
                   inout BodyState current) {
    float inverseMassA = max(current.linVelInvMass.w, 0.0);
    float inverseMassB = max(other.linVelInvMass.w, 0.0);
    float inverseMassSum = inverseMassA + inverseMassB;
    if (inverseMassA <= 0.0 || inverseMassSum <= 0.0) return;

    float3 delta = current.positionType.xyz - other.positionType.xyz;
    float distanceSquared = dot(delta, delta);
    float minimumDistance = bodyBoundingRadius(current) +
                            bodyBoundingRadius(other);
    if (distanceSquared >= minimumDistance * minimumDistance) return;

    float distance = sqrt(max(distanceSquared, 1.0e-12));
    float3 normal = distance > 1.0e-5
        ? delta / distance
        : normalize(float3(0.37 + float(selfIndex) * 0.11, 0.71, 0.43));
    float3 relativeVelocity = current.linVelInvMass.xyz -
                            other.linVelInvMass.xyz;
    float normalSpeed = dot(relativeVelocity, normal);

    if (normalSpeed < 0.0) {
        float restitution = clamp(max(current.material.x, other.material.x),
                                  0.0, 1.0);
        float normalImpulse = -(1.0 + restitution) * normalSpeed /
                              inverseMassSum;
        current.linVelInvMass.xyz +=
            normal * (normalImpulse * inverseMassA);

        float3 tangent = relativeVelocity - normalSpeed * normal;
        float tangentLength = length(tangent);
        if (tangentLength > 1.0e-6) {
            tangent /= tangentLength;
            float friction = clamp(sqrt(max(current.material.y, 0.0) *
                                        max(other.material.y, 0.0)), 0.0, 1.0);
            float tangentImpulse = min(tangentLength / inverseMassSum,
                                       normalImpulse * friction);
            current.linVelInvMass.xyz -=
                tangent * (tangentImpulse * inverseMassA);
        }
    }

    float penetration = minimumDistance - distance;
    current.positionType.xyz += normal * penetration *
        (inverseMassA / inverseMassSum) * 0.72;
}

void constrainToPool(float horizontalRadius, float bottomExtent,
                     inout BodyState body) {
    float dx = max(abs(gridOriginDx.w), 1.0e-6);
    float padding = max(material.w, 0.0) * dx;
    float3 domainMinimum = gridOriginDx.xyz + float3(padding, padding, padding);
    float3 domainMaximum = gridOriginDx.xyz +
        float3(gridSizeAndCount.xyz - uint3(1u,1u,1u)) * dx -
        float3(padding, padding, padding);
    float3 rawMaximum = gridOriginDx.xyz +
        float3(gridSizeAndCount.xyz - uint3(1u,1u,1u)) * dx;
    float2 centre = 0.5 * (gridOriginDx.xz + rawMaximum.xz);
    float2 halfExtent = max(0.5 * (rawMaximum.xz - gridOriginDx.xz) -
                          float2(max(pool.y, 0.0) + horizontalRadius,
                                 max(pool.y, 0.0) + horizontalRadius),
                          float2(dx, dx));
    float3 minimum = float3(centre.x - halfExtent.x,
                        max(domainMinimum.y + bottomExtent,
                            pool.z + bottomExtent),
                        centre.y - halfExtent.y);
    float3 maximum = float3(centre.x + halfExtent.x,
                        domainMaximum.y - bottomExtent,
                        centre.y + halfExtent.y);

    float e = max(body.material.x, collision.x);
    float f = max(body.material.y, collision.y);
    if (body.positionType.x < minimum.x) {
        body.positionType.x = minimum.x;
        body.linVelInvMass.xyz = collideVelocity(
            body.linVelInvMass.xyz, float3(1,0,0), e, f);
    } else if (body.positionType.x > maximum.x) {
        body.positionType.x = maximum.x;
        body.linVelInvMass.xyz = collideVelocity(
            body.linVelInvMass.xyz, float3(-1,0,0), e, f);
    }
    if (body.positionType.z < minimum.z) {
        body.positionType.z = minimum.z;
        body.linVelInvMass.xyz = collideVelocity(
            body.linVelInvMass.xyz, float3(0,0,1), e, f);
    } else if (body.positionType.z > maximum.z) {
        body.positionType.z = maximum.z;
        body.linVelInvMass.xyz = collideVelocity(
            body.linVelInvMass.xyz, float3(0,0,-1), e, f);
    }
    if (body.positionType.y < minimum.y) {
        body.positionType.y = minimum.y;
        body.linVelInvMass.xyz = collideVelocity(
            body.linVelInvMass.xyz, float3(0,1,0), e, f);
        body.angVelInvInertia.xyz *= 1.0 - clamp(f, 0.0, 0.95);
    }

    float cornerRadius = clamp(pool.x - horizontalRadius, 0.0,
        max(min(halfExtent.x, halfExtent.y), 0.0));
    if (cornerRadius > dx) {
        float2 relative = body.positionType.xz - centre;
        float2 cornerCentre = sign(relative) *
            max(halfExtent - float2(cornerRadius, cornerRadius), float2(0,0));
        float2 radial = relative - cornerCentre;
        if (abs(relative.x) > halfExtent.x - cornerRadius &&
            abs(relative.y) > halfExtent.y - cornerRadius &&
            length(radial) > cornerRadius) {
            float2 inward = -normalize(radial + float2(1.0e-8, 1.0e-8));
            body.positionType.xz = centre + cornerCentre -
                                   inward * cornerRadius;
            body.linVelInvMass.xyz = collideVelocity(
                body.linVelInvMass.xyz,
                float3(inward.x, 0.0, inward.y), e, f);
        }
    }
}

bool finiteVector(float3 v) {
    return !any(isnan(v)) && !any(isinf(v));
}

[numthreads(32, 1, 1)]
void CSRigidIntegrate(uint3 DTid : SV_DispatchThreadID,
                      uint3 Gid : SV_GroupID,
                      uint3 GTid : SV_GroupThreadID)
{

    if (Gid.x != 0u || Gid.y != 0u || Gid.z != 0u) return;

    uint lane = GTid.x;
    uint bodyCount = min(scene.x, MAX_BODIES);
    if (lane < bodyCount)
        initialStates[lane] = bodyStateBuffer[lane];
    GroupMemoryBarrierWithGroupSync();

    if (lane < bodyCount) {
        BodyState body = initialStates[lane];
        int type = int(round(body.positionType.w));
        float inverseMass = max(body.linVelInvMass.w, 0.0);
        float inverseInertia = max(body.angVelInvInertia.w, 0.0);
        float dt = clamp(simulation.x, 0.0, 1.0 / 30.0);

        bool heldSinkBall = lane == 3u && type == BODY_SINK_BALL &&
                            pool.w < SINK_BALL_RELEASE_TIME;
        // A zero inverse mass is the only hard anchor.  The motor boat now has
        // finite mass and receives the equal-and-opposite propeller impulse;
        // a soft horizontal tether below keeps it in the hero composition.
        bool anchored = inverseMass <= 0.0;
        if (heldSinkBall || anchored) {
            if (heldSinkBall) {
                // Hold the solid sphere above the pool, then hand it to
                // gravity.  The previous smoothstep crane reached the water
                // with zero velocity, so it displaced water slowly instead of
                // producing a physically coupled entry splash.
                body.positionType.y = SINK_BALL_START_Y;
                body.linVelInvMass.xyz = float3(0,0,0);
            } else {
                body.linVelInvMass.xyz = float3(0,0,0);
            }
            body.angVelInvInertia.xyz = float3(0,0,0);
            predictedStates[lane] = body;
        } else {
            BodyImpulse encoded = bodyImpulseBuffer[lane];
            float3 impulse = decodeImpulse(encoded.linImpulse.xyz);
            float3 angularImpulse = decodeImpulse(encoded.angImpulse.xyz);
            float displacedMass = max(float(encoded.linImpulse.w) /
                                      bodyFixedScale(), 0.0);
            float submergedFraction = clamp(displacedMass * inverseMass,
                                             0.0, 1.0);
            body.linVelInvMass.xyz += impulse * inverseMass;
            body.angVelInvInertia.xyz += angularImpulse * inverseInertia;
            body.linVelInvMass.y += simulation.y * dt;

            if (type == BODY_DUCK && body.shape0.w > 0.0) {
                float3 bodyUp = rotateByQuaternion(body.orientation,
                                                 float3(0.0, 1.0, 0.0));
                float3 restoringAxis = cross(bodyUp, float3(0.0, 1.0, 0.0));
                body.angVelInvInertia.xyz += restoringAxis *
                    (body.shape0.w * inverseInertia * dt);
            }

            if (type == BODY_ANCHORED_BOAT) {
                float2 offset = body.positionType.xz - BOAT_TETHER_ANCHOR_XZ;
                // A compliant mooring permits visible thrust, bob and yaw but
                // avoids a deterministic benchmark scene losing the boat.
                float2 tetherForce = -18.0 * offset -
                                   5.0 * body.linVelInvMass.xz;
                body.linVelInvMass.xz +=
                    tetherForce * (inverseMass * dt);
                float3 bodyUp = rotateByQuaternion(body.orientation,
                                                 float3(0.0, 1.0, 0.0));
                float3 restoringAxis = cross(bodyUp, float3(0.0, 1.0, 0.0));
                body.angVelInvInertia.xyz += restoringAxis *
                    (24.0 * inverseInertia * dt);
            }

            float linearDamping = max(body.material.z, 0.0);
            float angularDamping = max(body.material.w, 0.0);
            if (type == BODY_SINK_BALL) {
                // material.z/w describe water drag.  Applying them in air gave
                // the ball a fake 2.45 m/s terminal velocity despite -9.81 m/s².
                linearDamping = 0.015 + linearDamping * submergedFraction;
                angularDamping = 0.025 + angularDamping * submergedFraction;
            } else if (type == BODY_ANCHORED_BOAT) {
                linearDamping = 0.04 + linearDamping * submergedFraction;
                angularDamping = 0.06 + angularDamping * submergedFraction;
            }
            body.linVelInvMass.xyz *= exp(-linearDamping * dt);
            body.angVelInvInertia.xyz *= exp(-angularDamping * dt);

            float maximumSpeed = max(collision.w, 0.0);
            float speed = length(body.linVelInvMass.xyz);
            if (maximumSpeed > 0.0 && speed > maximumSpeed)
                body.linVelInvMass.xyz *= maximumSpeed / speed;
            float angularSpeed = length(body.angVelInvInertia.xyz);
            float maximumAngularSpeed = 16.0;
            if (angularSpeed > maximumAngularSpeed)
                body.angVelInvInertia.xyz *= maximumAngularSpeed /
                                                       angularSpeed;

            body.positionType.xyz += body.linVelInvMass.xyz * dt;
            body.orientation = integrateOrientation(
                body.orientation, body.angVelInvInertia.xyz, dt);
            predictedStates[lane] = body;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    if (lane >= bodyCount) return;
    BodyState current = predictedStates[lane];
    int type = int(round(current.positionType.w));
    bool heldSinkBall = lane == 3u && type == BODY_SINK_BALL &&
                        pool.w < SINK_BALL_RELEASE_TIME;
    bool anchored = current.linVelInvMass.w <= 0.0;

    if (!heldSinkBall && !anchored) {
        for (uint otherIndex = 0u; otherIndex < bodyCount; ++otherIndex) {
            if (otherIndex == lane) continue;
            solveBodyPair(lane, predictedStates[otherIndex], current);
        }
        constrainToPool(bodyBoundingRadius(current),
                        bodyBottomExtent(current), current);
    }

    if (!finiteVector(current.positionType.xyz) ||
        !finiteVector(current.linVelInvMass.xyz) ||
        !finiteVector(current.angVelInvInertia.xyz) ||
        any(isnan(current.orientation)) || any(isinf(current.orientation))) {
        current = initialStates[lane];
        current.linVelInvMass.xyz = float3(0,0,0);
        current.angVelInvInertia.xyz = float3(0,0,0);
        current.orientation = safeQuaternion(current.orientation);
    }

    bodyStateBuffer[lane] = current;

}

