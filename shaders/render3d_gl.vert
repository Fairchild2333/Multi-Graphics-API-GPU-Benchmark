#version 430
layout(location = 0) in vec2 inCorner;
layout(location = 1) in vec4 inPosition;
layout(location = 2) in vec4 inVelocity;
layout(location = 0) out vec2 vCorner;
layout(location = 1) out vec3 vColor;
layout(binding = 0, std140) uniform R3D {
    mat4  viewProj;
    vec4  camRight;
    vec4  camUp;
    float pointSize;
};
void main() {
    vec3 world = inPosition.xyz + camRight.xyz*(inCorner.x*pointSize) + camUp.xyz*(inCorner.y*pointSize);
    gl_Position = viewProj * vec4(world, 1.0);
    vCorner = inCorner;
    float speed = length(inVelocity.xyz);
    vColor = mix(vec3(0.1,0.4,1.0), vec3(1.0,0.3,0.1), clamp(speed*5.0, 0.0, 1.0));
}
