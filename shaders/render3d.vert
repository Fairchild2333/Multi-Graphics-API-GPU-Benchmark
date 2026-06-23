#version 450

// True-3D billboard vertex shader. Instanced: 6 vertices (a quad) per particle.
//   location 0 = quad corner (-1..1)        — per-vertex
//   location 1 = particle position (vec4)   — per-instance
//   location 2 = particle velocity (vec4)   — per-instance
// The quad is expanded camera-facing in world space, then projected by viewProj.

layout(location = 0) in vec2 inCorner;
layout(location = 1) in vec4 inPosition;
layout(location = 2) in vec4 inVelocity;

layout(location = 0) out vec2 outCorner;
layout(location = 1) out vec3 outColor;

layout(push_constant) uniform Render3DParams {
    mat4  viewProj;
    vec4  camRight;
    vec4  camUp;
    float pointSize;
} pc;

void main() {
    vec3 world = inPosition.xyz
               + pc.camRight.xyz * (inCorner.x * pc.pointSize)
               + pc.camUp.xyz    * (inCorner.y * pc.pointSize);
    gl_Position = pc.viewProj * vec4(world, 1.0);

    outCorner = inCorner;
    float speed = length(inVelocity.xyz);
    outColor = mix(vec3(0.1, 0.4, 1.0), vec3(1.0, 0.3, 0.1),
                   clamp(speed * 5.0, 0.0, 1.0));
}
