#version 450

// Fullscreen-triangle vertex shader for the volumetric raymarch test.
// Identical to fractal.vert: 3 verts from gl_VertexIndex, no vertex buffer.
// Draw with vkCmdDraw(cmd, 3, 1, 0, 0).

layout(location = 0) out vec2 outUV;

void main() {
    // (0,0), (2,0), (0,2) -> triangle that covers the [0,1]^2 viewport.
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    outUV = uv;
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
