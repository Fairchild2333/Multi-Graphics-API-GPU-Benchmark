#version 450

// Fractal stress-test fragment shader (fill-rate + fragment ALU/SFU bound).
//
// Every pixel runs EXACTLY maxIter iterations with no early-out, so the work
// per frame is constant and scales linearly with resolution x maxIter — ideal
// for a sustained max-load stress test. sin() keeps the orbit bounded (no
// NaN/Inf) and loads the special-function units; the result is a swirly
// fractal-like field. `time` only rotates the colour palette, not the workload.

layout(location = 0) in  vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform FractalParams {
    float time;
    float zoom;
    uint  maxIter;
} params;

void main() {
    vec2 c = (inUV - 0.5) * (3.0 / max(params.zoom, 0.0001));
    vec2 z = c;
    float acc = 0.0;

    for (uint i = 0u; i < params.maxIter; ++i) {
        // Mandelbrot step, then sin() to keep bounded + add SFU load.
        z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
        z = sin(z);
        acc += dot(z, z);
    }

    float v = fract(acc * 0.05 + params.time * 0.1);
    vec3 col = 0.5 + 0.5 * cos(6.28318 * (v + vec3(0.0, 0.33, 0.67)));
    outColor = vec4(col, 1.0);
}
