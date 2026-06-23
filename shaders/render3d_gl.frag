#version 430
layout(location = 0) in vec2 vCorner;
layout(location = 1) in vec3 vColor;
layout(location = 0) out vec4 outColor;
void main() {
    float r = length(vCorner);
    if (r > 1.0) discard;
    outColor = vec4(vColor, smoothstep(1.0, 0.0, r));
}
