#version 430

// Fullscreen-triangle vertex shader for the fractal stress test (OpenGL 4.3).
// Generates 3 vertices from gl_VertexID — no vertex buffer needed.

out vec2 outUV;

void main() {
    vec2 uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    outUV = uv;
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
