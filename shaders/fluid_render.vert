#version 450

// Fluid render: fullscreen-triangle vertex shader (identical to volumetric.vert).
// The dye field is read directly from the SSBO in the fragment shader.

layout(location = 0) out vec2 outUV;

void main() {
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    outUV = uv;
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
