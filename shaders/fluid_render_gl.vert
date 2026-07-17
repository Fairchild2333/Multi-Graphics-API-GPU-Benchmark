#version 430

// Fluid render: fullscreen-triangle vertex shader (OpenGL 4.3).
// Identical to shaders/fluid_render.vert / volumetric_gl.vert.

out vec2 outUV;

void main() {
    vec2 uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    outUV = uv;
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
