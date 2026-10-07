// GPU Burn ES 2.0 legacy v1 - the triangle that covers the screen.
// GLSL ES 1.00 has no gl_VertexID, so the corners come from a vertex buffer.

attribute vec2 aPosition;

varying mediump vec2 vUV;

void main() {
    vUV = aPosition * 0.5 + 0.5;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
