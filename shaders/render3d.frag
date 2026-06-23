#version 450

// Soft round particle: radial alpha falloff from the billboard centre.
layout(location = 0) in vec2 inCorner;
layout(location = 1) in vec3 inColor;
layout(location = 0) out vec4 outFragColor;

void main() {
    float r = length(inCorner);
    if (r > 1.0) discard;
    float alpha = smoothstep(1.0, 0.0, r);
    outFragColor = vec4(inColor, alpha);
}
