#version 430

// Fractal stress-test fragment shader (OpenGL 4.3). See shaders/fractal.frag.

in  vec2 outUV;
out vec4 outColor;

layout(binding = 1, std140) uniform FractalParams {
    float time;
    float zoom;
    uint  maxIter;
};

void main() {
    vec2 c = (outUV - 0.5) * (3.0 / max(zoom, 0.0001));
    vec2 z = c;
    float acc = 0.0;

    for (uint k = 0u; k < maxIter; ++k) {
        z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
        z = sin(z);
        acc += dot(z, z);
    }

    float v = fract(acc * 0.05 + time * 0.1);
    vec3 col = 0.5 + 0.5 * cos(6.28318 * (v + vec3(0.0, 0.33, 0.67)));
    outColor = vec4(col, 1.0);
}
